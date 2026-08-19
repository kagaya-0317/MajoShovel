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

function New-MajoShovelSourceCoordinationMutex([string]$Root) {
    $mutexName = Get-MajoShovelBuildMutexName $Root "SourceCoordination"
    return [System.Threading.Mutex]::new($false, $mutexName)
}

function Try-Enter-MajoShovelSourceCoordinationLock([string]$Root) {
    $mutex = New-MajoShovelSourceCoordinationMutex $Root
    $acquired = $false
    try {
        try {
            $acquired = $mutex.WaitOne(0)
        }
        catch [System.Threading.AbandonedMutexException] {
            $acquired = $true
        }
        if ($acquired) {
            return $mutex
        }
        $mutex.Dispose()
        return $null
    }
    catch {
        if (-not $acquired) {
            $mutex.Dispose()
        }
        throw
    }
}

function Enter-MajoShovelSourceCoordinationLock(
    [string]$Root,
    [string]$LogPrefix,
    [int]$WaitSeconds = 3600
) {
    $mutex = New-MajoShovelSourceCoordinationMutex $Root
    $acquired = $false
    $reportedWait = $false
    $deadline = if ($WaitSeconds -le 0) { [DateTime]::MaxValue } else { (Get-Date).AddSeconds($WaitSeconds) }
    try {
        while (-not $acquired) {
            try {
                $acquired = $mutex.WaitOne(1000)
            }
            catch [System.Threading.AbandonedMutexException] {
                $acquired = $true
            }
            if ($acquired) {
                break
            }
            if (-not $reportedWait) {
                Write-Host "$LogPrefix a source verification build is running; waiting before opening an edit session."
                $reportedWait = $true
            }
            if ((Get-Date) -ge $deadline) {
                throw "Timed out waiting for the source coordination lock for '$Root'."
            }
        }
        if ($reportedWait) {
            Write-Host "$LogPrefix the source verification build finished; editing may continue."
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

function Get-MajoShovelSha256HexFromBytes([byte[]]$Bytes) {
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hash = $sha256.ComputeHash($Bytes)
        return [System.BitConverter]::ToString($hash).Replace("-", "").ToLowerInvariant()
    }
    finally {
        $sha256.Dispose()
    }
}

function Get-MajoShovelFileSha256(
    [string]$Path,
    [System.Security.Cryptography.HashAlgorithm]$Hasher = $null
) {
    $stream = [System.IO.File]::Open(
        $Path,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite)
    $ownsHasher = $null -eq $Hasher
    if ($ownsHasher) {
        $Hasher = [System.Security.Cryptography.SHA256]::Create()
    }
    try {
        $hash = $Hasher.ComputeHash($stream)
        return [System.BitConverter]::ToString($hash).Replace("-", "").ToLowerInvariant()
    }
    finally {
        $stream.Dispose()
        if ($ownsHasher) {
            $Hasher.Dispose()
        }
    }
}

function Get-MajoShovelStableFileSnapshot(
    [string]$Path,
    [System.Security.Cryptography.HashAlgorithm]$Hasher = $null
) {
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    for ($attempt = 0; $attempt -lt 3; $attempt++) {
        $before = Get-Item -LiteralPath $fullPath -ErrorAction Stop
        $hash = Get-MajoShovelFileSha256 $fullPath $Hasher
        $after = Get-Item -LiteralPath $fullPath -ErrorAction Stop
        if ($before.Length -eq $after.Length -and
            $before.LastWriteTimeUtc.Ticks -eq $after.LastWriteTimeUtc.Ticks) {
            return [pscustomobject]@{
                fullPath = $fullPath
                length = [int64]$after.Length
                lastWriteUtcTicks = [int64]$after.LastWriteTimeUtc.Ticks
                sha256 = $hash
            }
        }
        Start-Sleep -Milliseconds 40
    }
    throw "Build input changed while it was being hashed: $fullPath"
}

function Get-MajoShovelDevBuildEvidenceRoot([string]$Root) {
    $base = $env:LOCALAPPDATA
    if ([string]::IsNullOrWhiteSpace($base)) {
        $base = Join-Path $Root ".local"
    }
    return Join-Path $base "MajoShovel\build-logs\dev-auto-reload"
}

function Get-MajoShovelRepositoryRelativePath([string]$Root, [string]$Path) {
    $rootPath = [System.IO.Path]::GetFullPath($Root).TrimEnd([System.IO.Path]::DirectorySeparatorChar)
    $fullPath = if ([System.IO.Path]::IsPathRooted($Path)) {
        [System.IO.Path]::GetFullPath($Path)
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $rootPath $Path))
    }
    $rootPrefix = $rootPath + [System.IO.Path]::DirectorySeparatorChar
    if (-not $fullPath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside the repository: $Path"
    }
    return $fullPath.Substring($rootPrefix.Length).Replace('\', '/')
}

function Get-MajoShovelCodexEditSessionRoot([string]$Root) {
    $base = $env:LOCALAPPDATA
    if ([string]::IsNullOrWhiteSpace($base)) {
        $base = Join-Path $Root ".local"
    }
    $rootKey = (Get-MajoShovelBuildMutexName $Root "EditSessions").Split('.')[-1]
    return Join-Path $base "MajoShovel\codex-edit-sessions\$rootKey"
}

function Get-MajoShovelCodexEditSessionPath([string]$Root, [string]$SessionId) {
    $sessionBytes = [System.Text.Encoding]::UTF8.GetBytes($SessionId)
    $sessionKey = Get-MajoShovelSha256HexFromBytes $sessionBytes
    return Join-Path (Get-MajoShovelCodexEditSessionRoot $Root) "active\$sessionKey.json"
}

function Write-MajoShovelJsonFile([string]$Path, $Value) {
    $directory = Split-Path $Path -Parent
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $temporaryPath = "$Path.$PID.$([Guid]::NewGuid().ToString('N')).tmp"
    $json = $Value | ConvertTo-Json -Depth 8
    $utf8Bom = [System.Text.UTF8Encoding]::new($true)
    try {
        [System.IO.File]::WriteAllText($temporaryPath, $json + [Environment]::NewLine, $utf8Bom)
        Move-Item -LiteralPath $temporaryPath -Destination $Path -Force
    }
    finally {
        Remove-Item -LiteralPath $temporaryPath -Force -ErrorAction SilentlyContinue
    }
}

function Get-MajoShovelCodexEditSessions([string]$Root, [bool]$RemoveExpired = $false) {
    $activeRoot = Join-Path (Get-MajoShovelCodexEditSessionRoot $Root) "active"
    if (-not (Test-Path -LiteralPath $activeRoot -PathType Container)) {
        return @()
    }

    $rootPath = [System.IO.Path]::GetFullPath($Root)
    $nowUtc = [DateTime]::UtcNow
    $sessions = [System.Collections.Generic.List[object]]::new()
    foreach ($file in @(Get-ChildItem -LiteralPath $activeRoot -Filter "*.json" -File -ErrorAction SilentlyContinue)) {
        try {
            $session = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
            $matchesRoot = [string]::Equals(
                [System.IO.Path]::GetFullPath([string]$session.repositoryRoot),
                $rootPath,
                [System.StringComparison]::OrdinalIgnoreCase)
            $expired = ([DateTime]$session.expiresAtUtc).ToUniversalTime() -le $nowUtc
            if ($session.schemaVersion -ne 1 -or -not $matchesRoot -or $expired) {
                if ($RemoveExpired) {
                    Remove-Item -LiteralPath $file.FullName -Force -ErrorAction SilentlyContinue
                }
                continue
            }
            Add-Member -InputObject $session -NotePropertyName leasePath -NotePropertyValue $file.FullName -Force
            $sessions.Add($session)
        }
        catch {
            if ($RemoveExpired) {
                Remove-Item -LiteralPath $file.FullName -Force -ErrorAction SilentlyContinue
            }
        }
    }
    return @($sessions)
}

function Test-MajoShovelCodexEditSessionBlocksBuild($Session) {
    foreach ($path in @($Session.paths)) {
        $normalized = ([string]$path).Replace('\', '/').TrimStart('/')
        if ([string]::Equals($normalized, "CMakeLists.txt", [System.StringComparison]::OrdinalIgnoreCase) -or
            $normalized.StartsWith("src/", [System.StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }
    return $false
}

function Enter-MajoShovelSourceVerificationWindow(
    [string]$Root,
    [string]$LogPrefix,
    [int]$WaitSeconds = 3600
) {
    $deadline = if ($WaitSeconds -le 0) { [DateTime]::MaxValue } else { (Get-Date).AddSeconds($WaitSeconds) }
    $reportedWait = $false
    while ($true) {
        $mutex = Enter-MajoShovelSourceCoordinationLock $Root $LogPrefix $WaitSeconds
        $sessions = @(Get-MajoShovelCodexEditSessions $Root $true | Where-Object {
            Test-MajoShovelCodexEditSessionBlocksBuild $_
        })
        if ($sessions.Count -eq 0) {
            if ($reportedWait) {
                Write-Host "$LogPrefix all Codex edit sessions finished; starting source verification."
            }
            return $mutex
        }

        Exit-MajoShovelMutex $mutex
        if (-not $reportedWait) {
            $sessionIds = @($sessions | ForEach-Object { $_.sessionId }) -join ", "
            Write-Host "$LogPrefix Codex edit sessions are active; postponing source verification: $sessionIds"
            $reportedWait = $true
        }
        if ((Get-Date) -ge $deadline) {
            throw "Timed out waiting for Codex edit sessions to finish for '$Root'."
        }
        Start-Sleep -Seconds 1
    }
}

function Get-MajoShovelBuildInputSnapshot([string]$Root) {
    $rootPath = [System.IO.Path]::GetFullPath($Root).TrimEnd([System.IO.Path]::DirectorySeparatorChar)
    $sourcePath = Join-Path $rootPath "src"
    $paths = @()
    if (Test-Path -LiteralPath $sourcePath -PathType Container) {
        $paths += @(Get-ChildItem -LiteralPath $sourcePath -Recurse -File -ErrorAction Stop |
            Select-Object -ExpandProperty FullName)
    }
    $cmakeLists = Join-Path $rootPath "CMakeLists.txt"
    if (Test-Path -LiteralPath $cmakeLists -PathType Leaf) {
        $paths += $cmakeLists
    }

    $entries = [System.Collections.Generic.List[object]]::new()
    $fingerprintText = [System.Text.StringBuilder]::new()
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        foreach ($path in @($paths | Sort-Object -Unique)) {
            $file = Get-MajoShovelStableFileSnapshot $path $sha256
            $relativePath = Get-MajoShovelRepositoryRelativePath $rootPath $file.fullPath
            $entry = [pscustomobject]@{
                path = $relativePath
                length = $file.length
                lastWriteUtcTicks = $file.lastWriteUtcTicks
                sha256 = $file.sha256
            }
            $entries.Add($entry)
            [void]$fingerprintText.Append($relativePath)
            [void]$fingerprintText.Append("`t")
            [void]$fingerprintText.Append($file.sha256)
            [void]$fingerprintText.Append("`n")
        }
    }
    finally {
        $sha256.Dispose()
    }

    $fingerprintBytes = [System.Text.Encoding]::UTF8.GetBytes($fingerprintText.ToString())
    return [pscustomobject]@{
        algorithm = "sha256-path-content-v1"
        capturedAtUtc = [DateTime]::UtcNow.ToString("o")
        fingerprint = Get-MajoShovelSha256HexFromBytes $fingerprintBytes
        files = @($entries)
    }
}

function Get-MajoShovelObjectWriteTimes(
    [string]$BuildPath,
    [string]$Config,
    [string]$TargetName
) {
    $targetDirectory = Join-Path $BuildPath "$TargetName.dir\$Config"
    $writeTimes = @{}
    if (-not (Test-Path -LiteralPath $targetDirectory -PathType Container)) {
        return ,$writeTimes
    }

    foreach ($object in Get-ChildItem -LiteralPath $targetDirectory -Filter "*.obj" -File -Recurse -ErrorAction SilentlyContinue) {
        $writeTimes[$object.FullName.ToUpperInvariant()] = [int64]$object.LastWriteTimeUtc.Ticks
    }
    return ,$writeTimes
}

function Get-MajoShovelRecompiledSources(
    [string]$Root,
    [string]$BuildPath,
    [string]$Config,
    [string]$TargetName,
    [hashtable]$ObjectWriteTimesBefore
) {
    $targetDirectory = Join-Path $BuildPath "$TargetName.dir\$Config"
    $itemsPath = Join-Path $targetDirectory "$TargetName.tlog\Cl.items.tlog"
    if (-not (Test-Path -LiteralPath $itemsPath -PathType Leaf)) {
        return @()
    }

    $rootPath = [System.IO.Path]::GetFullPath($Root).TrimEnd([System.IO.Path]::DirectorySeparatorChar)
    $rootPrefix = $rootPath + [System.IO.Path]::DirectorySeparatorChar
    $sources = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($line in [System.IO.File]::ReadAllLines($itemsPath)) {
        $parts = $line.Split([char[]]@(';'), 2)
        if ($parts.Length -ne 2) {
            continue
        }
        $sourcePath = [System.IO.Path]::GetFullPath($parts[0])
        $objectPath = [System.IO.Path]::GetFullPath($parts[1])
        if (-not (Test-Path -LiteralPath $objectPath -PathType Leaf)) {
            continue
        }
        $object = Get-Item -LiteralPath $objectPath
        $objectKey = $object.FullName.ToUpperInvariant()
        $previousTicks = if ($null -ne $ObjectWriteTimesBefore -and $ObjectWriteTimesBefore.ContainsKey($objectKey)) {
            [int64]$ObjectWriteTimesBefore[$objectKey]
        } else {
            0
        }
        if ($object.LastWriteTimeUtc.Ticks -le $previousTicks) {
            continue
        }
        $relativePath = if ($sourcePath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            $sourcePath.Substring($rootPrefix.Length).Replace('\', '/')
        } else {
            $sourcePath.Replace('\', '/')
        }
        [void]$sources.Add($relativePath)
    }
    return @($sources | Sort-Object)
}

function Get-MajoShovelCommandArgumentValue([string[]]$Arguments, [string]$Name) {
    for ($index = 0; $index -lt $Arguments.Count - 1; $index++) {
        if ($Arguments[$index] -eq $Name) {
            return $Arguments[$index + 1]
        }
    }
    return ""
}

function Get-MajoShovelVisualStudioCompileProgress(
    [string[]]$Arguments
) {
    $buildPath = Get-MajoShovelCommandArgumentValue $Arguments "--build"
    if ([string]::IsNullOrWhiteSpace($buildPath)) {
        return $null
    }

    $config = Get-MajoShovelCommandArgumentValue $Arguments "--config"
    $targetName = Get-MajoShovelCommandArgumentValue $Arguments "--target"
    if ([string]::IsNullOrWhiteSpace($config) -or [string]::IsNullOrWhiteSpace($targetName)) {
        return $null
    }

    $targetDirectory = Join-Path $buildPath "$targetName.dir\$config"
    $tlogDirectory = Join-Path $targetDirectory "$targetName.tlog"
    $itemsPath = Join-Path $tlogDirectory "Cl.items.tlog"
    $projectPath = Join-Path $buildPath "$targetName.vcxproj"
    if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) {
        return $null
    }

    $compileItems = [System.Collections.Generic.List[object]]::new()
    if (Test-Path -LiteralPath $itemsPath -PathType Leaf) {
        foreach ($line in [System.IO.File]::ReadAllLines($itemsPath)) {
            $parts = $line.Split([char[]]@(';'), 2)
            if ($parts.Length -ne 2) {
                continue
            }
            $compileItems.Add([pscustomobject]@{
                sourcePath = [System.IO.Path]::GetFullPath($parts[0])
                objectPath = [System.IO.Path]::GetFullPath($parts[1])
            })
        }
    } else {
        [xml]$project = [System.IO.File]::ReadAllText($projectPath)
        $namespace = [System.Xml.XmlNamespaceManager]::new($project.NameTable)
        $namespace.AddNamespace("msbuild", "http://schemas.microsoft.com/developer/msbuild/2003")
        foreach ($node in $project.SelectNodes("//msbuild:ClCompile[@Include]", $namespace)) {
            $sourcePath = [System.IO.Path]::GetFullPath($node.Include)
            $compileItems.Add([pscustomobject]@{
                sourcePath = $sourcePath
                objectPath = Join-Path $targetDirectory ([System.IO.Path]::GetFileNameWithoutExtension($sourcePath) + ".obj")
            })
        }
    }

    if ($compileItems.Count -eq 0) {
        return $null
    }

    $dependenciesBySource = @{}
    $dependencyPath = Join-Path $tlogDirectory "Microsoft.Build.CPPTasks.CL.read.1.tlog"
    if (Test-Path -LiteralPath $dependencyPath -PathType Leaf) {
        $currentSources = @()
        foreach ($line in [System.IO.File]::ReadAllLines($dependencyPath)) {
            if ($line.StartsWith("^")) {
                $currentSources = @($line.Substring(1).Split([char[]]@('|'), [System.StringSplitOptions]::RemoveEmptyEntries))
                foreach ($source in $currentSources) {
                    $sourceKey = [System.IO.Path]::GetFullPath($source).ToUpperInvariant()
                    if (-not $dependenciesBySource.ContainsKey($sourceKey)) {
                        $dependenciesBySource[$sourceKey] = [System.Collections.Generic.List[string]]::new()
                    }
                }
                continue
            }
            if ([string]::IsNullOrWhiteSpace($line)) {
                continue
            }
            foreach ($source in $currentSources) {
                $sourceKey = [System.IO.Path]::GetFullPath($source).ToUpperInvariant()
                $dependenciesBySource[$sourceKey].Add($line)
            }
        }
    }

    $cleanFirst = $Arguments -contains "--clean-first"
    $plannedObjects = [System.Collections.Generic.List[object]]::new()
    foreach ($item in $compileItems) {
        $objectTicks = 0
        $needsCompile = $cleanFirst -or -not [System.IO.File]::Exists($item.objectPath)
        if (-not $needsCompile) {
            $objectTicks = [System.IO.File]::GetLastWriteTimeUtc($item.objectPath).Ticks
            $inputPaths = [System.Collections.Generic.List[string]]::new()
            $inputPaths.Add($item.sourcePath)
            $sourceKey = $item.sourcePath.ToUpperInvariant()
            if ($dependenciesBySource.ContainsKey($sourceKey)) {
                $inputPaths.AddRange($dependenciesBySource[$sourceKey])
            }
            foreach ($inputPath in $inputPaths) {
                if (-not [System.IO.File]::Exists($inputPath)) {
                    continue
                }
                if ([System.IO.File]::GetLastWriteTimeUtc($inputPath).Ticks -gt $objectTicks) {
                    $needsCompile = $true
                    break
                }
            }
        }
        if ($needsCompile) {
            $plannedObjects.Add([pscustomobject]@{
                path = $item.objectPath
                previousWriteUtcTicks = [int64]$objectTicks
            })
        }
    }

    return [pscustomobject]@{
        kind = "VisualStudio"
        plannedObjects = @($plannedObjects)
        completed = 0
        total = $plannedObjects.Count
        phase = if ($plannedObjects.Count -gt 0) { "compiling" } else { "checking" }
    }
}

function New-MajoShovelNativeProgressContext([string[]]$Arguments) {
    try {
        $visualStudioProgress = Get-MajoShovelVisualStudioCompileProgress $Arguments
        if ($null -ne $visualStudioProgress) {
            return $visualStudioProgress
        }
    }
    catch {
        Write-Verbose "Could not prepare detailed build progress: $($_.Exception.Message)"
    }
    return [pscustomobject]@{
        kind = "NativeOutput"
        completed = 0
        total = 0
        phase = "running"
    }
}

function Update-MajoShovelNativeProgressFromLine($Context, [string]$Line) {
    if ($Context.kind -eq "NativeOutput" -and $Line -match '^\[\s*(?<completed>\d+)\s*/\s*(?<total>\d+)\]') {
        $Context.completed = [int]$Matches.completed
        $Context.total = [int]$Matches.total
        $Context.phase = "building"
    } elseif ($Context.kind -eq "VisualStudio" -and $Line -match '\.vcxproj\s+->\s+') {
        $Context.phase = "finished"
    }
}

function Write-MajoShovelNativeProgress($Context, [string]$Activity, [TimeSpan]$Elapsed) {
    $elapsedText = "elapsed $([int]$Elapsed.TotalSeconds)s"
    if ($Context.kind -eq "VisualStudio" -and $Context.total -gt 0) {
        $completed = $Context.total
        if ($Context.phase -ne "linking" -and $Context.phase -ne "finished") {
            $completed = 0
            foreach ($plannedObject in $Context.plannedObjects) {
                if (-not [System.IO.File]::Exists($plannedObject.path)) {
                    continue
                }
                if ([System.IO.File]::GetLastWriteTimeUtc($plannedObject.path).Ticks -gt $plannedObject.previousWriteUtcTicks) {
                    $completed++
                }
            }
            $Context.completed = $completed
            if ($completed -ge $Context.total) {
                $Context.phase = "linking"
            }
        }
        $compilePercent = [Math]::Min(100, [int][Math]::Floor(($completed * 100.0) / $Context.total))
        $percent = if ($Context.phase -eq "finished") { 100 } elseif ($Context.phase -eq "linking") { 99 } else { $compilePercent }
        $status = if ($Context.phase -eq "finished") {
            "finished - compiled $completed/$($Context.total) - $elapsedText"
        } elseif ($Context.phase -eq "linking") {
            "linking - compiled $completed/$($Context.total) (compile 100%) - $elapsedText"
        } else {
            "compiled $completed/$($Context.total) (compile $compilePercent%) - $elapsedText"
        }
        Write-Progress -Activity $Activity -Status $status -PercentComplete $percent
        return
    }
    if ($Context.total -gt 0) {
        $percent = [Math]::Min(100, [int][Math]::Floor(($Context.completed * 100.0) / $Context.total))
        Write-Progress -Activity $Activity -Status "$($Context.completed)/$($Context.total) ($percent%) - $elapsedText" -PercentComplete $percent
        return
    }
    if ($Context.kind -eq "VisualStudio" -and $Context.phase -eq "finished") {
        Write-Progress -Activity $Activity -Status "finished - $elapsedText" -PercentComplete 100
        return
    }
    $status = if ($Context.kind -eq "VisualStudio" -and $Context.phase -eq "linking") {
        "linking - $elapsedText"
    } elseif ($Context.kind -eq "VisualStudio") {
        "checking dependencies - $elapsedText"
    } else {
        $elapsedText
    }
    Write-Progress -Activity $Activity -Status $status -PercentComplete -1
}

function Invoke-MajoShovelNativeCommandWithProgress(
    [string]$FilePath,
    [string[]]$Arguments,
    [string]$Activity,
    [string]$WorkingDirectory,
    [string]$LogPath = ""
) {
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo.FileName = $FilePath
    $process.StartInfo.Arguments = Join-MajoShovelCommandLineArguments $Arguments
    $process.StartInfo.WorkingDirectory = $WorkingDirectory
    $process.StartInfo.UseShellExecute = $false
    $captureOutput = -not [string]::IsNullOrWhiteSpace($LogPath)
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.StartInfo.StandardOutputEncoding = [System.Text.Encoding]::UTF8
    $process.StartInfo.StandardErrorEncoding = [System.Text.Encoding]::UTF8

    $startedAt = Get-Date
    $progressContext = New-MajoShovelNativeProgressContext $Arguments
    $standardOutput = [System.Text.StringBuilder]::new()
    $standardError = [System.Text.StringBuilder]::new()
    $exitCode = 1
    [void]$process.Start()
    $standardOutputTask = $process.StandardOutput.ReadLineAsync()
    $standardErrorTask = $process.StandardError.ReadLineAsync()
    $standardOutputEnded = $false
    $standardErrorEnded = $false

    try {
        while (-not $process.HasExited -or -not $standardOutputEnded -or -not $standardErrorEnded) {
            while (-not $standardOutputEnded -and $standardOutputTask.IsCompleted) {
                $line = $standardOutputTask.GetAwaiter().GetResult()
                if ($null -eq $line) {
                    $standardOutputEnded = $true
                    break
                }
                Update-MajoShovelNativeProgressFromLine $progressContext $line
                [void]$standardOutput.AppendLine($line)
                if (-not $captureOutput) {
                    Write-Host $line
                }
                $standardOutputTask = $process.StandardOutput.ReadLineAsync()
            }
            while (-not $standardErrorEnded -and $standardErrorTask.IsCompleted) {
                $line = $standardErrorTask.GetAwaiter().GetResult()
                if ($null -eq $line) {
                    $standardErrorEnded = $true
                    break
                }
                Update-MajoShovelNativeProgressFromLine $progressContext $line
                [void]$standardError.AppendLine($line)
                if (-not $captureOutput) {
                    Write-Host $line
                }
                $standardErrorTask = $process.StandardError.ReadLineAsync()
            }
            $elapsed = (Get-Date) - $startedAt
            Write-MajoShovelNativeProgress $progressContext $Activity $elapsed
            if (-not $process.HasExited) {
                [void]$process.WaitForExit(200)
            } elseif (-not $standardOutputEnded -or -not $standardErrorEnded) {
                Start-Sleep -Milliseconds 10
            }
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

    if ($captureOutput) {
        $utf8Bom = New-Object System.Text.UTF8Encoding($true)
        $writer = New-Object System.IO.StreamWriter($LogPath, $true, $utf8Bom)
        try {
            if ($standardOutput.Length -gt 0) {
                $writer.Write($standardOutput.ToString())
                [Console]::Out.Write($standardOutput.ToString())
            }
            if ($standardError.Length -gt 0) {
                $writer.Write($standardError.ToString())
                [Console]::Error.Write($standardError.ToString())
            }
        }
        finally {
            $writer.Dispose()
        }
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
