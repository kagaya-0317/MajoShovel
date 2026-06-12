param(
    [string]$BuildDir = "",
    [string]$Config = "Release",
    [int]$Jobs = 0,
    [ValidateSet("Auto", "VisualStudio", "Ninja")]
    [string]$Generator = "Auto",
    [ValidateSet("Auto", "Off", "Sccache", "Clcache")]
    [string]$CompilerCache = "Auto",
    [int]$CodexBuildSlots = 0,
    [int]$CodexBuildSlotWaitSeconds = 900,
    [switch]$Run
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$TargetName = "MajoShovel"
$CodexBuildSlotsProvided = $PSBoundParameters.ContainsKey("CodexBuildSlots")

function Get-LocalBuildBase {
    $base = $env:LOCALAPPDATA
    if ([string]::IsNullOrWhiteSpace($base)) {
        $base = Join-Path $Root ".local"
    }
    return $base
}

function Test-CodexBuildContext {
    return $env:CODEX_SHELL -eq "1" -or -not [string]::IsNullOrWhiteSpace($env:CODEX_THREAD_ID)
}

function Get-SafePathName([string]$Value) {
    if ([string]::IsNullOrWhiteSpace($Value)) {
        return "unknown"
    }
    return $Value -replace '[^A-Za-z0-9_.-]', '_'
}

function Get-RootFingerprint {
    $rootPath = [System.IO.Path]::GetFullPath($Root).TrimEnd('\').ToLowerInvariant()
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($rootPath)
        $hash = $sha.ComputeHash($bytes)
        return (($hash | Select-Object -First 8 | ForEach-Object { $_.ToString("x2") }) -join "")
    }
    finally {
        $sha.Dispose()
    }
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
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    throw "cmake.exe was not found. Install CMake or Visual Studio Build Tools with the C++ CMake tools component."
}

function Find-Ninja {
    $cmd = Get-Command ninja -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $candidates = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "C:\Program Files\CMake\bin\ninja.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    return ""
}

function Find-CompilerCache([string]$Preference) {
    if ($Preference -eq "Off") {
        return $null
    }

    $names = if ($Preference -eq "Sccache") {
        @("sccache")
    } elseif ($Preference -eq "Clcache") {
        @("clcache")
    } else {
        @("sccache", "clcache")
    }

    foreach ($name in $names) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) {
            return [pscustomobject]@{
                Name = $name
                Path = $cmd.Source
            }
        }
    }

    if ($Preference -ne "Auto") {
        throw "$Preference was requested, but the executable was not found on PATH."
    }

    return $null
}

function Find-VsDevCmd {
    $cmd = Get-Command VsDevCmd.bat -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $candidates = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    return ""
}

function Import-VsDevEnvironment {
    if (Get-Command cl -ErrorAction SilentlyContinue) {
        return
    }

    $vsDevCmd = Find-VsDevCmd
    if ([string]::IsNullOrWhiteSpace($vsDevCmd)) {
        throw "Ninja generator needs the MSVC command-line environment, but VsDevCmd.bat was not found."
    }

    Write-Host "[build] importing MSVC environment: $vsDevCmd"
    $environment = & cmd.exe /s /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 >nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw "VsDevCmd.bat failed with exit code $LASTEXITCODE."
    }

    foreach ($line in $environment) {
        $index = $line.IndexOf("=")
        if ($index -le 0) {
            continue
        }

        $name = $line.Substring(0, $index)
        $value = $line.Substring($index + 1)
        Set-Item -Path "env:$name" -Value $value
    }
}

function Get-CodexBuildSlotCount {
    if ($CodexBuildSlotsProvided) {
        return [Math]::Max(0, $CodexBuildSlots)
    }

    if (-not (Test-CodexBuildContext)) {
        return 0
    }

    $envSlots = 0
    if ([int]::TryParse($env:MAJOSHOVEL_CODEX_BUILD_SLOTS, [ref]$envSlots)) {
        return [Math]::Max(0, $envSlots)
    }

    return 4
}

function Write-LockFileContent([System.IO.FileStream]$Stream, [string]$Text) {
    $encoding = New-Object System.Text.UTF8Encoding($true)
    $preamble = $encoding.GetPreamble()
    $body = $encoding.GetBytes($Text)
    $Stream.SetLength(0)
    if ($preamble.Length -gt 0) {
        $Stream.Write($preamble, 0, $preamble.Length)
    }
    if ($body.Length -gt 0) {
        $Stream.Write($body, 0, $body.Length)
    }
    $Stream.Flush()
}

function Try-AcquireBuildSlot([string]$LockPath, [string]$BuildPath, [int]$SlotNumber) {
    try {
        $stream = [System.IO.File]::Open($LockPath, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
        $metadata = @(
            "pid=$PID",
            "thread=$env:CODEX_THREAD_ID",
            "slot=$SlotNumber",
            "root=$Root",
            "build=$BuildPath",
            "started=$((Get-Date).ToString("o"))"
        ) -join "`r`n"
        Write-LockFileContent $stream ($metadata + "`r`n")
        return $stream
    }
    catch [System.IO.IOException] {
        return $null
    }
}

function Acquire-CodexBuildSlot([string]$PoolPath, [int]$SlotCount, [int]$WaitSeconds) {
    $locksPath = Join-Path $PoolPath ".locks"
    New-Item -ItemType Directory -Force -Path $locksPath | Out-Null

    $deadline = if ($WaitSeconds -le 0) { Get-Date } else { (Get-Date).AddSeconds($WaitSeconds) }
    do {
        for ($slot = 1; $slot -le $SlotCount; ++$slot) {
            $buildPath = Join-Path $PoolPath ("slot-" + $slot)
            $lockPath = Join-Path $locksPath ("slot-" + $slot + ".lock")
            $stream = Try-AcquireBuildSlot $lockPath $buildPath $slot
            if ($null -ne $stream) {
                return [pscustomobject]@{
                    BuildPath = $buildPath
                    Lock = $stream
                    Slot = $slot
                }
            }
        }

        if ((Get-Date) -ge $deadline) {
            break
        }

        Write-Host "[build] all $SlotCount Codex build slot(s) are busy; waiting..."
        Start-Sleep -Seconds 2
    } while ($true)

    throw "Timed out waiting for a free Codex build slot in $PoolPath."
}

function Resolve-GeneratorPlan([string]$RequestedGenerator, $CacheTool, [string]$NinjaPath, [bool]$AllowAutoNinja) {
    if ($RequestedGenerator -eq "Ninja") {
        if ([string]::IsNullOrWhiteSpace($NinjaPath)) {
            throw "Ninja generator was requested, but ninja.exe was not found."
        }
        return "Ninja"
    }

    if ($RequestedGenerator -eq "VisualStudio") {
        return "VisualStudio"
    }

    if ($AllowAutoNinja -and $null -ne $CacheTool -and -not [string]::IsNullOrWhiteSpace($NinjaPath)) {
        return "Ninja"
    }

    return "VisualStudio"
}

function Get-BuildFlavor([string]$GeneratorKind, $CacheTool) {
    if ($GeneratorKind -eq "Ninja") {
        if ($null -ne $CacheTool) {
            return "ninja-" + (Get-SafePathName $CacheTool.Name.ToLowerInvariant())
        }
        return "ninja"
    }

    return "vs"
}

function Resolve-BuildPath([string]$Path, [string]$BuildFlavor, [int]$SlotCount, [ref]$SlotLease) {
    if (-not [string]::IsNullOrWhiteSpace($Path)) {
        if ([System.IO.Path]::IsPathRooted($Path)) {
            return $Path
        }

        return Join-Path $Root $Path
    }

    $base = Get-LocalBuildBase
    if (Test-CodexBuildContext) {
        if ($SlotCount -gt 0) {
            $rootHash = Get-RootFingerprint
            $poolPath = Join-Path $base "MajoShovel\build-codex\pools\$rootHash\$BuildFlavor"
            $lease = Acquire-CodexBuildSlot $poolPath $SlotCount $CodexBuildSlotWaitSeconds
            $SlotLease.Value = $lease
            Write-Host "[build] Codex slot: $($lease.Slot) / $SlotCount"
            return $lease.BuildPath
        }

        $codexBuildId = $env:CODEX_THREAD_ID
        if ([string]::IsNullOrWhiteSpace($codexBuildId)) {
            $codexBuildId = "pid-$PID"
        }

        $safeCodexBuildId = Get-SafePathName $codexBuildId
        $buildName = if ($BuildFlavor -eq "vs") { $safeCodexBuildId } else { "$safeCodexBuildId-$BuildFlavor" }
        return Join-Path $base "MajoShovel\build-codex\$buildName"
    }

    return Join-Path $base "MajoShovel\build-nopch"
}

function Get-CMakeCacheValue([string]$Path, [string]$Name) {
    $cachePath = Join-Path $Path "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath)) {
        return ""
    }

    $prefix = $Name + ":"
    $line = Get-Content -LiteralPath $cachePath -ErrorAction SilentlyContinue |
        Where-Object { $_.StartsWith($prefix, [System.StringComparison]::Ordinal) } |
        Select-Object -First 1
    if ($null -eq $line) {
        return ""
    }

    $equals = $line.IndexOf("=")
    if ($equals -lt 0) {
        return ""
    }

    return $line.Substring($equals + 1)
}

function Assert-BuildDirectoryMatchesPlan([string]$Path, [string]$GeneratorKind) {
    $existingGenerator = Get-CMakeCacheValue $Path "CMAKE_GENERATOR"
    if ([string]::IsNullOrWhiteSpace($existingGenerator)) {
        return
    }

    $isExistingNinja = $existingGenerator -eq "Ninja"
    $wantsNinja = $GeneratorKind -eq "Ninja"
    if ($isExistingNinja -ne $wantsNinja) {
        throw "Build directory '$Path' was configured with '$existingGenerator'. Use a different -BuildDir or remove that directory before switching generators."
    }
}

function Get-ExecutablePath([string]$BuildPath, [string]$GeneratorKind, [string]$BuildConfig) {
    if ($GeneratorKind -eq "Ninja") {
        return Join-Path $BuildPath "$TargetName.exe"
    }

    return Join-Path $BuildPath "$BuildConfig\$TargetName.exe"
}

function Invoke-MajoShovelBuild {
    $slotLease = $null
    try {
        $cmake = Find-CMake
        $ninja = Find-Ninja
        $cacheTool = Find-CompilerCache $CompilerCache
        if ($null -ne $cacheTool -and [string]::IsNullOrWhiteSpace($ninja)) {
            if ($CompilerCache -eq "Auto") {
                Write-Host "[build] compiler cache found, but ninja.exe was not found; cache disabled."
                $cacheTool = $null
            } else {
                throw "Compiler cache '$($cacheTool.Name)' needs the Ninja generator, but ninja.exe was not found."
            }
        }

        $generatorKind = Resolve-GeneratorPlan $Generator $cacheTool $ninja (Test-CodexBuildContext)
        if ($generatorKind -ne "Ninja" -and $null -ne $cacheTool) {
            if ($CompilerCache -eq "Auto") {
                Write-Host "[build] compiler cache disabled because Visual Studio generator is selected."
                $cacheTool = $null
            } else {
                throw "Compiler cache '$($cacheTool.Name)' requires -Generator Ninja."
            }
        }

        if ($generatorKind -eq "Ninja") {
            Import-VsDevEnvironment
        }

        $slotCount = Get-CodexBuildSlotCount
        $buildFlavor = Get-BuildFlavor $generatorKind $cacheTool
        $leaseRef = [ref]$slotLease
        $buildPath = Resolve-BuildPath $BuildDir $buildFlavor $slotCount $leaseRef
        $slotLease = $leaseRef.Value
        Assert-BuildDirectoryMatchesPlan $buildPath $generatorKind

        if ($Jobs -le 0) {
            $script:Jobs = [Math]::Max(1, [Environment]::ProcessorCount)
        }

        $configureArgs = @("-S", "$Root", "-B", $buildPath, "-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON")
        if ($generatorKind -eq "Ninja") {
            $configureArgs += @("-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=$ninja", "-DCMAKE_BUILD_TYPE=$Config")
        }
        if ($null -ne $cacheTool) {
            $configureArgs += @("-DCMAKE_C_COMPILER_LAUNCHER=$($cacheTool.Path)", "-DCMAKE_CXX_COMPILER_LAUNCHER=$($cacheTool.Path)")
        }

        $buildArgs = @("--build", $buildPath, "--target", $TargetName, "--parallel", "$Jobs")
        if ($generatorKind -ne "Ninja") {
            $buildArgs = @("--build", $buildPath, "--config", $Config, "--target", $TargetName, "--parallel", "$Jobs")
        }

        Write-Host "[build] source: $Root"
        Write-Host "[build] output: $buildPath"
        Write-Host "[build] cmake: $cmake"
        Write-Host "[build] generator: $generatorKind"
        if ($generatorKind -eq "Ninja") {
            Write-Host "[build] ninja: $ninja"
        }
        if ($null -ne $cacheTool) {
            Write-Host "[build] compiler cache: $($cacheTool.Name) ($($cacheTool.Path))"
        } else {
            Write-Host "[build] compiler cache: off"
        }
        Write-Host "[build] jobs: $Jobs"

        & $cmake @configureArgs
        if ($LASTEXITCODE -ne 0) {
            return $LASTEXITCODE
        }

        & $cmake @buildArgs
        if ($LASTEXITCODE -ne 0) {
            return $LASTEXITCODE
        }

        $exe = Get-ExecutablePath $buildPath $generatorKind $Config
        if (-not (Test-Path -LiteralPath $exe)) {
            Write-Error "Build finished without producing $exe"
            return 1
        }

        if ($Run) {
            Start-Process -FilePath $exe -WorkingDirectory $Root
        }

        return 0
    }
    finally {
        if ($null -ne $slotLease -and $null -ne $slotLease.Lock) {
            $slotLease.Lock.Dispose()
        }
    }
}

exit (Invoke-MajoShovelBuild)
