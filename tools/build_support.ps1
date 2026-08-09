function Get-MajoShovelNormalizedDirectoryPath([string]$Path) {
    return [System.IO.Path]::GetFullPath($Path).TrimEnd([System.IO.Path]::DirectorySeparatorChar) +
        [System.IO.Path]::DirectorySeparatorChar
}

function Get-MajoShovelBuildMutexName([string]$Path, [string]$Purpose) {
    $normalizedPath = (Get-MajoShovelNormalizedDirectoryPath $Path).ToUpperInvariant()
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $pathBytes = [System.Text.Encoding]::UTF8.GetBytes($normalizedPath)
        $hash = $sha256.ComputeHash($pathBytes)
    }
    finally {
        $sha256.Dispose()
    }

    $hashText = ($hash | ForEach-Object { $_.ToString("x2") }) -join ""
    return "Local\MajoShovel.$Purpose.$hashText"
}

function Enter-MajoShovelDevAutoReloadInstanceLock([string]$Path) {
    $mutexName = Get-MajoShovelBuildMutexName $Path "DevAutoReload"
    $mutex = [System.Threading.Mutex]::new($false, $mutexName)
    $acquired = $false
    try {
        try {
            $acquired = $mutex.WaitOne(0)
        }
        catch [System.Threading.AbandonedMutexException] {
            $acquired = $true
        }

        if (-not $acquired) {
            throw "Another dev_auto_reload process is already using build output '$Path'. Close it before starting a second watcher."
        }
        return $mutex
    }
    catch {
        if (-not $acquired) {
            $mutex.Dispose()
        }
        throw
    }
}

function Enter-MajoShovelBuildOperationLock([string]$Path, [string]$LogPrefix) {
    $mutexName = Get-MajoShovelBuildMutexName $Path "BuildOperation"
    $mutex = [System.Threading.Mutex]::new($false, $mutexName)
    $acquired = $false
    $reportedWait = $false
    try {
        while (-not $acquired) {
            try {
                $acquired = $mutex.WaitOne(1000)
            }
            catch [System.Threading.AbandonedMutexException] {
                $acquired = $true
            }

            if (-not $acquired -and -not $reportedWait) {
                Write-Host "$LogPrefix another build is using '$Path'; waiting for it to finish."
                $reportedWait = $true
            }
        }

        if ($reportedWait) {
            Write-Host "$LogPrefix the other build finished; continuing."
        }
        return $mutex
    }
    catch {
        if (-not $acquired) {
            $mutex.Dispose()
        }
        throw
    }
}

function Exit-MajoShovelMutex($Mutex) {
    if ($null -eq $Mutex) {
        return
    }

    try {
        $Mutex.ReleaseMutex()
    }
    finally {
        $Mutex.Dispose()
    }
}

function ConvertTo-MajoShovelCommandLineArgument([string]$Value) {
    if ($null -eq $Value) {
        return '""'
    }

    if ($Value.Length -eq 0 -or $Value.IndexOfAny([char[]]" `t`n`r`")") -ge 0) {
        return '"' + ($Value -replace '"', '\"') + '"'
    }

    return $Value
}

function Join-MajoShovelCommandLineArguments([string[]]$Arguments) {
    return ($Arguments | ForEach-Object { ConvertTo-MajoShovelCommandLineArgument $_ }) -join " "
}

function Invoke-MajoShovelNativeCommandWithProgress(
    [string]$FilePath,
    [string[]]$Arguments,
    [string]$Activity,
    [string]$WorkingDirectory
) {
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo.FileName = $FilePath
    $process.StartInfo.Arguments = Join-MajoShovelCommandLineArguments $Arguments
    $process.StartInfo.WorkingDirectory = $WorkingDirectory
    $process.StartInfo.UseShellExecute = $false

    $startedAt = Get-Date
    $frames = @("|", "/", "-", "\")
    $exitCode = 1
    [void]$process.Start()

    try {
        while (-not $process.WaitForExit(200)) {
            $elapsed = (Get-Date) - $startedAt
            $frame = $frames[[int]($elapsed.TotalMilliseconds / 200) % $frames.Count]
            $percent = [int](($elapsed.TotalSeconds * 8) % 100)
            Write-Progress -Activity $Activity -Status "$frame elapsed $([int]$elapsed.TotalSeconds)s" -PercentComplete $percent
        }
        $exitCode = $process.ExitCode
    }
    finally {
        Write-Progress -Activity $Activity -Completed
        if (-not $process.HasExited) {
            $process.Kill()
        }
        $process.Dispose()
    }

    return $exitCode
}

function Get-MajoShovelTrackerArgumentFile([string]$CommandLine) {
    if ([string]::IsNullOrWhiteSpace($CommandLine)) {
        return ""
    }

    $matches = [regex]::Matches($CommandLine, '@(?:"(?<quoted>[^"]+)"|(?<plain>\S+))')
    foreach ($match in $matches) {
        $candidate = if ($match.Groups["quoted"].Success) {
            $match.Groups["quoted"].Value
        } else {
            $match.Groups["plain"].Value
        }
        if ([System.IO.Path]::GetExtension($candidate) -ieq ".tmp" -and
            (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return $candidate
        }
    }
    return ""
}

function Test-MajoShovelTrackerTargetsBuildPath($TrackerProcess, [string]$Path, [string]$TargetName) {
    if ($null -eq $TrackerProcess -or $TrackerProcess.Name -ine "Tracker.exe") {
        return $false
    }

    $argumentFile = Get-MajoShovelTrackerArgumentFile $TrackerProcess.CommandLine
    if ([string]::IsNullOrEmpty($argumentFile)) {
        return $false
    }

    try {
        $arguments = [System.IO.File]::ReadAllText($argumentFile)
    }
    catch {
        return $false
    }

    $targetMarker = (Get-MajoShovelNormalizedDirectoryPath $Path) + "$TargetName.dir\"
    return $arguments.IndexOf($targetMarker, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
}

function Test-MajoShovelSameProcessInstance($ExpectedProcess, $CurrentProcess) {
    return $null -ne $ExpectedProcess -and
        $null -ne $CurrentProcess -and
        $ExpectedProcess.ProcessId -eq $CurrentProcess.ProcessId -and
        $ExpectedProcess.Name -ieq $CurrentProcess.Name -and
        $ExpectedProcess.CreationDate.ToString("o") -eq $CurrentProcess.CreationDate.ToString("o")
}

function Stop-MajoShovelVerifiedBuildProcess($ExpectedProcess, [string]$Description, [string]$LogPrefix) {
    $current = Get-CimInstance Win32_Process -Filter "ProcessId = $($ExpectedProcess.ProcessId)" -ErrorAction SilentlyContinue
    if ($null -eq $current) {
        return
    }
    if (-not (Test-MajoShovelSameProcessInstance $ExpectedProcess $current)) {
        throw "Refusing to stop PID $($ExpectedProcess.ProcessId): the process identity changed during stale-build cleanup."
    }

    Stop-Process -Id $current.ProcessId -Force -ErrorAction Stop
    Write-Host "$LogPrefix stopped orphaned $Description (PID $($current.ProcessId))."
}

function Clear-MajoShovelOrphanedBuildCompilerProcesses(
    [string]$Path,
    [string]$TargetName,
    [string]$LogPrefix
) {
    $processes = @(Get-CimInstance Win32_Process -ErrorAction Stop)
    $processById = @{}
    foreach ($process in $processes) {
        $processById[[int]$process.ProcessId] = $process
    }

    $activeTrackerCount = 0
    foreach ($tracker in ($processes | Where-Object { $_.Name -ieq "Tracker.exe" })) {
        if (-not (Test-MajoShovelTrackerTargetsBuildPath $tracker $Path $TargetName)) {
            continue
        }

        $parent = $processById[[int]$tracker.ParentProcessId]
        $hasOriginalMsBuildParent = $null -ne $parent -and
            $parent.Name -ieq "MSBuild.exe" -and
            $parent.CreationDate -le $tracker.CreationDate
        if ($hasOriginalMsBuildParent) {
            $activeTrackerCount++
            continue
        }

        $compilerChildren = @($processes | Where-Object {
            $_.ParentProcessId -eq $tracker.ProcessId -and $_.Name -ieq "cl.exe"
        })
        foreach ($compiler in $compilerChildren) {
            Stop-MajoShovelVerifiedBuildProcess $compiler "compiler" $LogPrefix
        }

        Start-Sleep -Milliseconds 200
        Stop-MajoShovelVerifiedBuildProcess $tracker "compiler tracker" $LogPrefix

        $trackedIds = @($tracker.ProcessId) + @($compilerChildren | Select-Object -ExpandProperty ProcessId)
        $deadline = (Get-Date).AddSeconds(2)
        do {
            $remaining = @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
                Where-Object { $_.ProcessId -in $trackedIds })
            if ($remaining.Count -eq 0) {
                break
            }
            Start-Sleep -Milliseconds 100
        } while ((Get-Date) -lt $deadline)

        if ($remaining.Count -ne 0) {
            throw "Orphaned compiler processes for '$Path' could not be stopped safely."
        }
    }

    return $activeTrackerCount
}

function Wait-MajoShovelBuildOutputAvailability(
    [string]$Path,
    [string]$TargetName,
    [string]$LogPrefix
) {
    $reportedWait = $false
    while ($true) {
        $activeTrackerCount = Clear-MajoShovelOrphanedBuildCompilerProcesses $Path $TargetName $LogPrefix
        if ($activeTrackerCount -eq 0) {
            if ($reportedWait) {
                Write-Host "$LogPrefix the non-cooperating build finished; continuing."
            }
            return
        }

        if (-not $reportedWait) {
            Write-Host "$LogPrefix a live build that predates the shared lock is using '$Path'; waiting instead of terminating it."
            $reportedWait = $true
        }
        Start-Sleep -Seconds 1
    }
}

function Test-MajoShovelVisualStudioDependencyTrackingIncomplete(
    [string]$Path,
    [string]$Config,
    [string]$TargetName
) {
    $targetDirectory = Join-Path $Path "$TargetName.dir\$Config"
    $tlogPath = Join-Path $targetDirectory "$TargetName.tlog\Microsoft.Build.CPPTasks.CL.read.1.tlog"
    if (-not (Test-Path -LiteralPath $tlogPath -PathType Leaf)) {
        return $false
    }

    try {
        $lines = [System.IO.File]::ReadAllLines($tlogPath)
    }
    catch {
        Write-Host "[build] could not inspect compiler dependency tracking: $($_.Exception.Message)"
        return $false
    }

    for ($index = 0; $index -lt $lines.Length; $index++) {
        $marker = $lines[$index]
        if (-not $marker.StartsWith("^") -or
            -not $marker.EndsWith(".CPP", [System.StringComparison]::OrdinalIgnoreCase)) {
            continue
        }

        $hasDependencies = $false
        for ($dependencyIndex = $index + 1; $dependencyIndex -lt $lines.Length; $dependencyIndex++) {
            $dependency = $lines[$dependencyIndex]
            if ($dependency.StartsWith("^")) {
                break
            }
            if (-not [string]::IsNullOrWhiteSpace($dependency)) {
                $hasDependencies = $true
                break
            }
        }
        if ($hasDependencies) {
            continue
        }

        $sourcePath = $marker.Substring(1)
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
            continue
        }

        try {
            $sourceText = [System.IO.File]::ReadAllText($sourcePath)
        }
        catch {
            continue
        }
        if ($sourceText -notmatch '(?m)^\s*#\s*include\s*[<"]') {
            continue
        }

        $objectName = [System.IO.Path]::GetFileNameWithoutExtension($sourcePath) + ".obj"
        if (Test-Path -LiteralPath (Join-Path $targetDirectory $objectName) -PathType Leaf) {
            return $true
        }
    }

    return $false
}

function Get-MajoShovelVisualStudioBuildArguments(
    [string]$Path,
    [string]$Config,
    [string]$TargetName,
    [int]$Jobs,
    [bool]$CleanFirst = $false
) {
    $arguments = @(
        "--build", $Path,
        "--config", $Config,
        "--target", $TargetName,
        "--parallel", "1"
    )
    if ($CleanFirst) {
        $arguments += "--clean-first"
    }
    $arguments += @(
        "--",
        "/p:UseMultiToolTask=true",
        "/p:MultiProcMaxCount=$Jobs",
        "/p:EnforceProcessCountAcrossBuilds=true"
    )
    return $arguments
}
