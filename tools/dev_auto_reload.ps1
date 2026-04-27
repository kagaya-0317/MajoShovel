param(
    [string]$BuildDir = "build-nopch",
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildPath = Join-Path $Root $BuildDir
$TargetName = "MajoShovel"
$GameProcess = $null
$PendingBuild = $true
$LastChangeTime = Get-Date

function Find-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $vsCMake = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $vsCMake) {
        return $vsCMake
    }

    throw "cmake.exe was not found. Install CMake or Visual Studio BuildTools CMake."
}

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
}

function Invoke-GameBuild {
    $cmake = Find-CMake
    Write-Host "[dev] configuring..."
    & $cmake -S $Root -B $BuildPath -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[dev] configure failed."
        return $false
    }

    Write-Host "[dev] building..."
    & $cmake --build $BuildPath --config $Config --target $TargetName
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[dev] build failed. Fix errors and save again."
        return $false
    }

    return $true
}

function Request-Rebuild($path) {
    $script:PendingBuild = $true
    $script:LastChangeTime = Get-Date
    Write-Host "[dev] changed: $path"
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
