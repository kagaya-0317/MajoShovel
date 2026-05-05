param(
    [string]$BuildDir = "",
    [string]$Config = "Release",
    [int]$Jobs = 0,
    [switch]$Run
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$TargetName = "MajoShovel"

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
$cmake = Find-CMake
if ($Jobs -le 0) {
    $Jobs = [Math]::Max(1, [Environment]::ProcessorCount)
}

Write-Host "[build] source: $Root"
Write-Host "[build] output: $BuildPath"
Write-Host "[build] cmake: $cmake"
Write-Host "[build] jobs: $Jobs"

& $cmake -S $Root -B $BuildPath -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $cmake --build $BuildPath --config $Config --target $TargetName --parallel $Jobs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$exe = Join-Path $BuildPath "$Config\$TargetName.exe"
if (-not (Test-Path $exe)) {
    Write-Error "Build finished without producing $exe"
    exit 1
}

if ($Run) {
    Start-Process -FilePath $exe -WorkingDirectory $Root
}
