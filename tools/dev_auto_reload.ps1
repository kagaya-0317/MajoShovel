param(
    [string]$BuildDir = "",
    [string]$Config = "Release",
    [int]$Jobs = 0
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$TargetName = "MajoShovel"
. (Join-Path $PSScriptRoot "build_support.ps1")
$GameProcess = $null
$PendingBuild = $true
$LastChangeTime = Get-Date
$IsBuilding = $false
$NeedsConfigure = $false
$RunRoot = $null
$GameExePath = $null
$AutoReloadBlocked = $false
$RebuildRestartExitCode = 85
$DevAutoReloadMutex = $null

function Find-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $candidates = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\CMake\bin\cmake.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    throw "cmake.exe was not found. Install CMake or Visual Studio Build Tools with the C++ CMake tools component."
}

function Resolve-BuildPath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        $base = $env:LOCALAPPDATA
        if ([string]::IsNullOrWhiteSpace($base)) {
            $base = Join-Path $Root ".local"
        }
        return Join-Path $base "MajoShovel\build-nopch"
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path $Root $Path
}

function Test-ConfigureNeeded([string]$Path) {
    $cache = Join-Path $Path "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cache)) {
        return $true
    }

    $stamp = Join-Path $Path "CMakeFiles\generate.stamp"
    if (-not (Test-Path -LiteralPath $stamp)) {
        return $true
    }

    $cmakeLists = Join-Path $Root "CMakeLists.txt"
    if ((Test-Path -LiteralPath $cmakeLists) -and
        ((Get-Item -LiteralPath $cmakeLists).LastWriteTimeUtc -gt (Get-Item -LiteralPath $stamp).LastWriteTimeUtc)) {
        return $true
    }

    return $false
}

function Get-DevBuildConfigPath {
    $base = $env:LOCALAPPDATA
    if ([string]::IsNullOrWhiteSpace($base)) {
        $base = Join-Path $Root ".local"
    }
    return Join-Path $base "MajoShovel\dev_build_config.txt"
}

function Get-AutoReloadBlockPath {
    $base = $env:LOCALAPPDATA
    if ([string]::IsNullOrWhiteSpace($base)) {
        $base = Join-Path $Root ".local"
    }
    return Join-Path $base "MajoShovel\dev_auto_reload_blocked.txt"
}

function Get-BuildStatusPath {
    $base = $env:LOCALAPPDATA
    if ([string]::IsNullOrWhiteSpace($base)) {
        $base = Join-Path $Root ".local"
    }
    return Join-Path $base "MajoShovel\dev_build_status.txt"
}

function Get-RestartRequestPath {
    $base = $env:LOCALAPPDATA
    if ([string]::IsNullOrWhiteSpace($base)) {
        $base = Join-Path $Root ".local"
    }
    return Join-Path $base "MajoShovel\dev_restart_request.txt"
}

function Clear-BuildStatus {
    $path = Get-BuildStatusPath
    if (Test-Path -LiteralPath $path) {
        try {
            Remove-Item -LiteralPath $path -Force -ErrorAction Stop
        } catch {
            Write-Host "[dev] could not clear build status: $($_.Exception.Message)"
        }
    }
}

function Clear-RestartRequest {
    $path = Get-RestartRequestPath
    if (Test-Path -LiteralPath $path) {
        try {
            Remove-Item -LiteralPath $path -Force -ErrorAction Stop
        } catch {
            Write-Host "[dev] could not clear restart request: $($_.Exception.Message)"
        }
    }
}

function Take-RestartRequest {
    $path = Get-RestartRequestPath
    if (-not (Test-Path -LiteralPath $path)) {
        return $false
    }

    try {
        Remove-Item -LiteralPath $path -Force -ErrorAction Stop
        return $true
    } catch {
        Write-Host "[dev] could not consume restart request: $($_.Exception.Message)"
        return $false
    }
}

function Publish-BuildStatus([string]$Status) {
    if ($Status -ne "ready" -and $Status -ne "failed") {
        throw "Unknown build status: $Status"
    }

    $path = Get-BuildStatusPath
    $temporaryPath = "$path.$PID.tmp"
    try {
        $directory = Split-Path $path -Parent
        New-Item -ItemType Directory -Force -Path $directory | Out-Null

        $exe = Join-Path (Get-BuildConfigPath) "$TargetName.exe"
        $exeWriteTicks = if (Test-Path -LiteralPath $exe) {
            (Get-Item -LiteralPath $exe).LastWriteTimeUtc.Ticks
        } else {
            0
        }
        $token = "$Status`t$PID-$([DateTime]::UtcNow.Ticks)-$exeWriteTicks"
        $utf8Bom = New-Object System.Text.UTF8Encoding($true)
        [System.IO.File]::WriteAllText($temporaryPath, $token + [Environment]::NewLine, $utf8Bom)
        Move-Item -LiteralPath $temporaryPath -Destination $path -Force
        return $true
    } catch {
        Remove-Item -LiteralPath $temporaryPath -Force -ErrorAction SilentlyContinue
        Write-Host "[dev] could not publish build status '$Status': $($_.Exception.Message)"
        return $false
    }
}

function Get-BuildStatusKind {
    $path = Get-BuildStatusPath
    if (-not (Test-Path -LiteralPath $path)) {
        return ""
    }
    $raw = Get-Content -LiteralPath $path -TotalCount 1 -ErrorAction SilentlyContinue
    if ($null -eq $raw) {
        return ""
    }
    return ($raw -split "`t", 2)[0].Trim().ToLowerInvariant()
}

function Test-BuildReady {
    return (Get-BuildStatusKind) -eq "ready"
}

function Invalidate-BuildReady {
    if (Test-BuildReady) {
        Clear-BuildStatus
    }
}

function Get-AutoReloadBlocked {
    $path = Get-AutoReloadBlockPath
    if (-not (Test-Path -LiteralPath $path)) {
        return $false
    }
    $raw = Get-Content -LiteralPath $path -TotalCount 1 -ErrorAction SilentlyContinue
    if ($null -eq $raw) {
        return $false
    }
    $value = $raw.Trim().ToLowerInvariant()
    return $value -eq "1" -or $value -eq "true" -or $value -eq "on" -or $value -eq "yes"
}

function Normalize-BuildConfig([string]$Value) {
    if ([string]::Equals($Value, "Debug", [System.StringComparison]::OrdinalIgnoreCase)) {
        return "Debug"
    }
    if ([string]::Equals($Value, "Release", [System.StringComparison]::OrdinalIgnoreCase)) {
        return "Release"
    }
    return ""
}

$ConfigProvided = $PSBoundParameters.ContainsKey("Config")
$ConfigFilePath = Get-DevBuildConfigPath
if (-not $ConfigProvided -and (Test-Path -LiteralPath $ConfigFilePath)) {
    $saved = Get-Content -LiteralPath $ConfigFilePath -TotalCount 1 -ErrorAction SilentlyContinue
    if ($null -ne $saved) {
        $savedConfig = Normalize-BuildConfig ($saved.Trim())
        if (-not [string]::IsNullOrEmpty($savedConfig)) {
            $Config = $savedConfig
            Write-Host "[dev] using saved build config: $Config"
        } elseif (-not [string]::IsNullOrWhiteSpace($saved)) {
            Write-Host "[dev] ignoring invalid saved build config: $saved"
        }
    }
}

$normalizedConfig = Normalize-BuildConfig $Config
if ([string]::IsNullOrEmpty($normalizedConfig)) {
    Write-Host "[dev] unknown build config '$Config'; falling back to Release."
    $Config = "Release"
} else {
    $Config = $normalizedConfig
}
$AutoReloadBlocked = Get-AutoReloadBlocked

$BuildPath = Resolve-BuildPath $BuildDir
if ($Jobs -le 0) {
    $Jobs = [Math]::Max(1, [Environment]::ProcessorCount)
}
$DevAutoReloadMutex = Enter-MajoShovelDevAutoReloadInstanceLock $BuildPath
$NeedsConfigure = Test-ConfigureNeeded $BuildPath
$RunRoot = Join-Path $BuildPath ".dev-run"
Clear-BuildStatus
Clear-RestartRequest

function Get-BuildConfigPath {
    return Join-Path $BuildPath $Config
}

function Remove-OldRunDirs([string]$KeepPath = "") {
    if (-not (Test-Path $RunRoot)) {
        return
    }

    $runRootFull = [System.IO.Path]::GetFullPath($RunRoot)
    $keepFull = if ([string]::IsNullOrWhiteSpace($KeepPath)) { "" } else { [System.IO.Path]::GetFullPath($KeepPath) }

    Get-ChildItem -LiteralPath $RunRoot -Directory -ErrorAction SilentlyContinue | ForEach-Object {
        $dirFull = [System.IO.Path]::GetFullPath($_.FullName)
        $dirName = $_.FullName
        $insideRunRoot = $dirFull.StartsWith($runRootFull, [System.StringComparison]::OrdinalIgnoreCase)
        $isKept = $keepFull -and [System.String]::Equals($dirFull, $keepFull, [System.StringComparison]::OrdinalIgnoreCase)
        if ($insideRunRoot -and -not $isKept) {
            try {
                Remove-Item -LiteralPath $dirName -Recurse -Force -ErrorAction Stop
            } catch {
                Write-Host "[dev] could not remove old run directory: $dirName"
            }
        }
    }
}

function Stop-Game {
    if ($script:GameProcess -and -not $script:GameProcess.HasExited) {
        Write-Host "[dev] stopping running game..."
        Stop-Process -Id $script:GameProcess.Id -Force
        $script:GameProcess = $null
    }

    $buildConfigPath = Get-BuildConfigPath
    $exe = Join-Path $buildConfigPath "$TargetName.exe"
    $runRootFull = [System.IO.Path]::GetFullPath($RunRoot)
    $buildConfigFull = [System.IO.Path]::GetFullPath($buildConfigPath)
    $running = Get-Process -Name $TargetName -ErrorAction SilentlyContinue |
        Where-Object {
            if (-not $_.Path) {
                return $false
            }
            $processPath = [System.IO.Path]::GetFullPath($_.Path)
            return [System.String]::Equals($processPath, $exe, [System.StringComparison]::OrdinalIgnoreCase) -or
                $processPath.StartsWith($runRootFull, [System.StringComparison]::OrdinalIgnoreCase) -or
                $processPath.StartsWith($buildConfigFull, [System.StringComparison]::OrdinalIgnoreCase)
        }

    foreach ($process in $running) {
        Write-Host "[dev] stopping existing game process $($process.Id)..."
        Stop-Process -Id $process.Id -Force
    }

    Start-Sleep -Milliseconds 150
    Remove-OldRunDirs
}

function Start-Game {
    $buildConfigPath = Get-BuildConfigPath
    $exe = Join-Path $buildConfigPath "$TargetName.exe"
    if (-not (Test-Path $exe)) {
        Write-Host "[dev] executable not found: $exe"
        return
    }

    New-Item -ItemType Directory -Force -Path $RunRoot | Out-Null
    $runDir = Join-Path $RunRoot ("run-" + (Get-Date -Format "yyyyMMdd-HHmmss-fff"))
    New-Item -ItemType Directory -Force -Path $runDir | Out-Null
    $runExe = Join-Path $runDir "$TargetName.exe"
    Copy-Item -LiteralPath $exe -Destination $runExe -Force
    Get-ChildItem -LiteralPath $buildConfigPath -Filter "*.dll" -File -ErrorAction SilentlyContinue |
        ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $runDir -Force }

    Write-Host "[dev] starting game..."
    $script:GameExePath = $runExe
    Clear-RestartRequest
    $script:GameProcess = Start-Process -FilePath $runExe -ArgumentList @("--test-play", "--dev-auto-reload") -WorkingDirectory $Root -PassThru
    Remove-OldRunDirs $runDir
    Start-Sleep -Milliseconds 300
    if ($script:GameProcess.HasExited) {
        Write-Host "[dev] game exited immediately with code $($script:GameProcess.ExitCode)."
        $script:GameProcess = $null
        $script:GameExePath = $null
    }
}

function Restart-ExistingGameCopy {
    if ([string]::IsNullOrWhiteSpace($script:GameExePath) -or -not (Test-Path -LiteralPath $script:GameExePath)) {
        Write-Host "[dev] previous game copy is unavailable; cannot restore it after build failure."
        return $false
    }

    Write-Host "[dev] restarting the previous game copy after build failure..."
    Clear-RestartRequest
    $script:GameProcess = Start-Process -FilePath $script:GameExePath -ArgumentList @("--test-play", "--dev-auto-reload") -WorkingDirectory $Root -PassThru
    Remove-OldRunDirs (Split-Path $script:GameExePath -Parent)
    Start-Sleep -Milliseconds 300
    if ($script:GameProcess.HasExited) {
        Write-Host "[dev] previous game copy exited immediately with code $($script:GameProcess.ExitCode)."
        $script:GameProcess = $null
        return $false
    }
    return $true
}

function Invoke-GameBuild {
    $cmake = Find-CMake
    $script:IsBuilding = $true
    $buildOperationMutex = $null
    try {
        $buildOperationMutex = Enter-MajoShovelBuildOperationLock $BuildPath "[dev]"
        Wait-MajoShovelBuildOutputAvailability $BuildPath $TargetName "[dev]"

        if ($script:NeedsConfigure) {
            Write-Host "[dev] configuring..."
            & $cmake -S $Root -B $BuildPath -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON
            if ($LASTEXITCODE -ne 0) {
                Write-Host "[dev] configure failed."
                return $false
            }
            $script:NeedsConfigure = $false
        }

        Write-Host "[dev] building with $Jobs job(s)..."
        $cleanFirst = Test-MajoShovelVisualStudioDependencyTrackingIncomplete $BuildPath $Config $TargetName
        if ($cleanFirst) {
            Write-Host "[dev] incomplete compiler dependency tracking detected; rebuilding once to prevent mixed object layouts."
        }
        $buildArgs = Get-MajoShovelVisualStudioBuildArguments $BuildPath $Config $TargetName $Jobs $cleanFirst
        $buildExitCode = Invoke-MajoShovelNativeCommandWithProgress $cmake $buildArgs "[dev] building with $Jobs job(s)" $Root
        if ($buildExitCode -ne 0) {
            Write-Host "[dev] build failed. Fix errors and save again."
            return $false
        }
        if (Test-MajoShovelVisualStudioDependencyTrackingIncomplete $BuildPath $Config $TargetName) {
            Write-Host "[dev] build finished, but compiler dependency tracking is still incomplete; refusing to launch a mixed binary."
            return $false
        }

        return $true
    }
    finally {
        Exit-MajoShovelMutex $buildOperationMutex
        $script:IsBuilding = $false
    }
}

function Test-RebuildPath($path) {
    $fullPath = [System.IO.Path]::GetFullPath($path)
    $srcPath = [System.IO.Path]::GetFullPath((Join-Path $Root "src"))
    $cmakePath = [System.IO.Path]::GetFullPath((Join-Path $Root "CMakeLists.txt"))

    return $fullPath.StartsWith($srcPath, [System.StringComparison]::OrdinalIgnoreCase) -or
        [System.String]::Equals($fullPath, $cmakePath, [System.StringComparison]::OrdinalIgnoreCase)
}

function Request-Rebuild($path) {
    if (Test-RebuildPath $path) {
        Invalidate-BuildReady
        $cmakePath = [System.IO.Path]::GetFullPath((Join-Path $Root "CMakeLists.txt"))
        $fullPath = [System.IO.Path]::GetFullPath($path)
        if ([System.String]::Equals($fullPath, $cmakePath, [System.StringComparison]::OrdinalIgnoreCase)) {
            $script:NeedsConfigure = $true
        }

        $script:PendingBuild = $true
        $script:LastChangeTime = Get-Date
        if ($script:IsBuilding) {
            Write-Host "[dev] code changed during build; queued another build: $path"
        } else {
            Write-Host "[dev] code changed: $path"
        }
        return
    }

    Write-Host "[dev] data changed: $path"
}

$watchers = @()

function Add-DirectoryWatcher($relativePath) {
    $fullPath = Join-Path $Root $relativePath
    if (-not (Test-Path $fullPath)) {
        return
    }

    $watcher = New-Object System.IO.FileSystemWatcher
    $watcher.Path = $fullPath
    $watcher.IncludeSubdirectories = $true
    $watcher.NotifyFilter = [System.IO.NotifyFilters]'FileName, DirectoryName, LastWrite, Size'
    $watcher.EnableRaisingEvents = $true

    Register-ObjectEvent $watcher Changed -Action { Request-Rebuild $Event.SourceEventArgs.FullPath } | Out-Null
    Register-ObjectEvent $watcher Created -Action { Request-Rebuild $Event.SourceEventArgs.FullPath } | Out-Null
    Register-ObjectEvent $watcher Deleted -Action { Request-Rebuild $Event.SourceEventArgs.FullPath } | Out-Null
    Register-ObjectEvent $watcher Renamed -Action { Request-Rebuild $Event.SourceEventArgs.FullPath } | Out-Null
    $script:watchers += $watcher
}

function Add-FileWatcher($relativePath) {
    $fullPath = Join-Path $Root $relativePath
    $parent = Split-Path $fullPath -Parent
    $leaf = Split-Path $fullPath -Leaf
    if (-not (Test-Path $parent)) {
        return
    }

    $watcher = New-Object System.IO.FileSystemWatcher
    $watcher.Path = $parent
    $watcher.Filter = $leaf
    $watcher.IncludeSubdirectories = $false
    $watcher.NotifyFilter = [System.IO.NotifyFilters]'FileName, LastWrite, Size'
    $watcher.EnableRaisingEvents = $true

    Register-ObjectEvent $watcher Changed -Action { Request-Rebuild $Event.SourceEventArgs.FullPath } | Out-Null
    Register-ObjectEvent $watcher Created -Action { Request-Rebuild $Event.SourceEventArgs.FullPath } | Out-Null
    Register-ObjectEvent $watcher Deleted -Action { Request-Rebuild $Event.SourceEventArgs.FullPath } | Out-Null
    Register-ObjectEvent $watcher Renamed -Action { Request-Rebuild $Event.SourceEventArgs.FullPath } | Out-Null
    $script:watchers += $watcher
}

Add-DirectoryWatcher "src"
Add-DirectoryWatcher "data"
Add-DirectoryWatcher "assets"
Add-FileWatcher "CMakeLists.txt"

Write-Host "[dev] watching source and data. Press Ctrl+C to stop."
Write-Host "[dev] build output: $BuildPath"
Write-Host "[dev] run copies: $RunRoot"
Write-Host "[dev] build jobs: $Jobs"
Write-Host "[dev] build config: $Config"
Write-Host "[dev] auto reload block: $AutoReloadBlocked (toggle in game with F2)"
Write-Host "[dev] code changes rebuild while the current game keeps running."
Write-Host "[dev] successful builds wait for F5 before applying the prepared game copy."
Write-Host "[dev] data/assets runtime reload keeps the existing game window."

try {
    while ($true) {
        $blockedNow = Get-AutoReloadBlocked
        if ($blockedNow -ne $AutoReloadBlocked) {
            $AutoReloadBlocked = $blockedNow
            Write-Host "[dev] auto reload block changed: $AutoReloadBlocked"
        }

        $hasRunningGame = $GameProcess -and -not $GameProcess.HasExited
        $canRunPendingBuild = -not $AutoReloadBlocked -or -not $hasRunningGame
        if ($canRunPendingBuild -and $PendingBuild -and ((Get-Date) - $LastChangeTime).TotalMilliseconds -ge 700) {
            $PendingBuild = $false
            $hadRunningGame = $hasRunningGame
            $buildSucceeded = Invoke-GameBuild
            if ($buildSucceeded) {
                if ($hadRunningGame) {
                    if ($PendingBuild) {
                        Write-Host "[dev] a newer code change is queued; waiting for its build before publishing."
                    } elseif (Publish-BuildStatus "ready") {
                        Write-Host "[dev] build ready. Press F5 in the game to apply it."
                    }
                } else {
                    Clear-BuildStatus
                    Start-Game
                }
            } elseif ($hadRunningGame -and -not $PendingBuild) {
                if (Publish-BuildStatus "failed") {
                    Write-Host "[dev] build failure published to the running game."
                }
            }
        }

        if ($GameProcess -and $GameProcess.HasExited) {
            $exitCode = $GameProcess.ExitCode
            $GameProcess = $null
            $restartRequestFound = Take-RestartRequest
            Write-Host "[dev] game exited with code $exitCode (restart request: $restartRequestFound)."
            if ($restartRequestFound -or $exitCode -eq $RebuildRestartExitCode) {
                if (Test-BuildReady) {
                    Write-Host "[dev] F5 apply requested: starting the prepared build..."
                    Clear-BuildStatus
                    Start-Game
                } else {
                    Write-Host "[dev] F5 restart requested without a prepared build: building before restart..."
                    if (Invoke-GameBuild) {
                        Clear-BuildStatus
                        Start-Game
                    } else {
                        [void](Publish-BuildStatus "failed")
                        [void](Restart-ExistingGameCopy)
                    }
                }
            }
        }

        Start-Sleep -Milliseconds 150
    }
}
finally {
    Clear-BuildStatus
    Clear-RestartRequest
    Stop-Game
    Get-EventSubscriber | Where-Object { $_.SourceObject -is [System.IO.FileSystemWatcher] } | Unregister-Event
    foreach ($watcher in $watchers) {
        $watcher.Dispose()
    }
    if ($DevAutoReloadMutex) {
        Exit-MajoShovelMutex $DevAutoReloadMutex
    }
}
