param(
    [string]$BuildDir = "",
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$TargetName = "MajoShovel"
$GameProcess = $null
$PendingBuild = $true
$LastChangeTime = Get-Date
$IsBuilding = $false
$IgnoreEventsUntil = Get-Date

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

$BuildPath = Resolve-BuildPath $BuildDir

function Stop-Game {
    if ($script:GameProcess -and -not $script:GameProcess.HasExited) {
        Write-Host "[dev] stopping running game..."
        Stop-Process -Id $script:GameProcess.Id -Force
        $script:GameProcess = $null
    }
}

function Start-Game {
    $exe = Join-Path $BuildPath "$Config\$TargetName.exe"
    if (-not (Test-Path $exe)) {
        Write-Host "[dev] executable not found: $exe"
        return
    }

    Write-Host "[dev] starting game..."
    $script:GameProcess = Start-Process -FilePath $exe -WorkingDirectory $Root -PassThru
    Start-Sleep -Milliseconds 300
    if ($script:GameProcess.HasExited) {
        Write-Host "[dev] game exited immediately with code $($script:GameProcess.ExitCode)."
        $script:GameProcess = $null
    }
}

function Invoke-GameBuild {
    $cmake = Find-CMake
    $script:IsBuilding = $true
    try {
        Write-Host "[dev] configuring..."
        & $cmake -S $Root -B $BuildPath -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[dev] configure failed."
            return $false
        }

        Write-Host "[dev] building..."
        & $cmake --build $BuildPath --config $Config --target $TargetName --parallel 1
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
Write-Host "[dev] code changes rebuild and restart the game process; the window will be recreated."
Write-Host "[dev] data-only runtime reload keeps the existing game window."

try {
    while ($true) {
        if ($PendingBuild -and ((Get-Date) - $LastChangeTime).TotalMilliseconds -ge 700) {
            $PendingBuild = $false
            Stop-Game
            if (Invoke-GameBuild) {
                Start-Game
            }
        }

        if ($GameProcess -and $GameProcess.HasExited) {
            $GameProcess = $null
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
