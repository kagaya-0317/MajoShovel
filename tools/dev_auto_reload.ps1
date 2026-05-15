param(
    [string]$BuildDir = "",
    [string]$Config = "Release",
    [int]$Jobs = 0
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$TargetName = "MajoShovel"
$GameProcess = $null
$PendingBuild = $true
$LastChangeTime = Get-Date
$IsBuilding = $false
$IgnoreEventsUntil = Get-Date
$NeedsConfigure = $false
$RunRoot = $null
$GameExePath = $null
$AutoReloadBlocked = $false
$RebuildRestartExitCode = 85

try {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class MajoShovelDevWin32 {
    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool IsWindow(IntPtr hWnd);
}
"@
} catch {
    Write-Host "[dev] foreground preservation disabled: Win32 helper could not be loaded."
}

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

function Get-ForegroundWindowHandle {
    if (-not ("MajoShovelDevWin32" -as [type])) {
        return [IntPtr]::Zero
    }
    return [MajoShovelDevWin32]::GetForegroundWindow()
}

function Get-WindowProcessId([IntPtr]$Handle) {
    if ($Handle -eq [IntPtr]::Zero -or -not ("MajoShovelDevWin32" -as [type])) {
        return 0
    }

    [uint32]$processId = 0
    [void][MajoShovelDevWin32]::GetWindowThreadProcessId($Handle, [ref]$processId)
    return [int]$processId
}

function Test-GameIsForeground {
    if (-not $script:GameProcess -or $script:GameProcess.HasExited) {
        return $false
    }

    $foregroundHandle = Get-ForegroundWindowHandle
    return (Get-WindowProcessId $foregroundHandle) -eq $script:GameProcess.Id
}

function Restore-ForegroundWindow([IntPtr]$Handle) {
    if ($Handle -eq [IntPtr]::Zero -or -not ("MajoShovelDevWin32" -as [type])) {
        return
    }
    if (-not [MajoShovelDevWin32]::IsWindow($Handle)) {
        return
    }

    [void][MajoShovelDevWin32]::SetForegroundWindow($Handle)
}

function Wait-ProcessMainWindow([System.Diagnostics.Process]$Process, [int]$TimeoutMs = 2000) {
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    while ((Get-Date) -lt $deadline) {
        if (-not $Process -or $Process.HasExited) {
            return [IntPtr]::Zero
        }

        $Process.Refresh()
        if ($Process.MainWindowHandle -ne [IntPtr]::Zero) {
            return $Process.MainWindowHandle
        }

        Start-Sleep -Milliseconds 50
    }

    return [IntPtr]::Zero
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
$NeedsConfigure = Test-ConfigureNeeded $BuildPath
$RunRoot = Join-Path $BuildPath ".dev-run"

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

function Start-Game([bool]$ActivateGame = $true, [IntPtr]$PreviousForegroundWindow = [IntPtr]::Zero) {
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
    $script:GameProcess = Start-Process -FilePath $runExe -ArgumentList @("--test-play", "--dev-auto-reload") -WorkingDirectory $Root -PassThru
    if (-not $ActivateGame) {
        [void](Wait-ProcessMainWindow $script:GameProcess)
        Restore-ForegroundWindow $PreviousForegroundWindow
    }
    Remove-OldRunDirs $runDir
    Start-Sleep -Milliseconds 300
    if ($script:GameProcess.HasExited) {
        Write-Host "[dev] game exited immediately with code $($script:GameProcess.ExitCode)."
        $script:GameProcess = $null
        $script:GameExePath = $null
    }
}

function Invoke-GameBuild {
    $cmake = Find-CMake
    $script:IsBuilding = $true
    try {
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
        & $cmake --build $BuildPath --config $Config --target $TargetName --parallel $Jobs
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[dev] build failed. Fix errors and save again."
            return $false
        }

        return $true
    }
    finally {
        $script:IsBuilding = $false
        $script:IgnoreEventsUntil = (Get-Date).AddSeconds(2)
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
    if ($script:IsBuilding -or (Get-Date) -lt $script:IgnoreEventsUntil) {
        return
    }

    if (Test-RebuildPath $path) {
        $cmakePath = [System.IO.Path]::GetFullPath((Join-Path $Root "CMakeLists.txt"))
        $fullPath = [System.IO.Path]::GetFullPath($path)
        if ([System.String]::Equals($fullPath, $cmakePath, [System.StringComparison]::OrdinalIgnoreCase)) {
            $script:NeedsConfigure = $true
        }

        $script:PendingBuild = $true
        $script:LastChangeTime = Get-Date
        Write-Host "[dev] code changed: $path"
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
Write-Host "[dev] code changes rebuild while the current game keeps running; successful builds restart the game copy."
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
            if (Invoke-GameBuild) {
                $foregroundBeforeRestart = Get-ForegroundWindowHandle
                $hadRunningGame = $hasRunningGame
                $activateRestartedGame = -not $hadRunningGame -or (Test-GameIsForeground)
                Stop-Game
                Start-Game $activateRestartedGame $foregroundBeforeRestart
            }
        }

        if ($GameProcess -and $GameProcess.HasExited) {
            $exitCode = $GameProcess.ExitCode
            $GameProcess = $null
            if ($exitCode -eq $RebuildRestartExitCode) {
                Write-Host "[dev] F5 restart requested: rebuilding before restart..."
                if (Invoke-GameBuild) {
                    Start-Game $true
                }
            }
        }

        Start-Sleep -Milliseconds 150
    }
}
finally {
    Stop-Game
    Get-EventSubscriber | Where-Object { $_.SourceObject -is [System.IO.FileSystemWatcher] } | Unregister-Event
    foreach ($watcher in $watchers) {
        $watcher.Dispose()
    }
}
