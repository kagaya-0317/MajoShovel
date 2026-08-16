[CmdletBinding()]
param(
    [string]$SourceDirectory = "",
    [string]$OutputDirectory = "",
    [string]$MappingPath = "",
    [string]$ManifestPath = "",
    [int]$OutputSampleRate = 48000,
    [switch]$VerifyOnly,
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$invariantCulture = [Globalization.CultureInfo]::InvariantCulture

if ([string]::IsNullOrWhiteSpace($SourceDirectory)) {
    $SourceDirectory = Join-Path $PSScriptRoot "..\BGM素材"
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $PSScriptRoot "..\assets\audio\bgm"
}
if ([string]::IsNullOrWhiteSpace($MappingPath)) {
    $MappingPath = Join-Path $PSScriptRoot "bgm_materials.tsv"
}
if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $ManifestPath = Join-Path $PSScriptRoot "..\assets\audio\audio_manifest.tsv"
}

function Find-MediaTool {
    param(
        [Parameter(Mandatory)]
        [string]$Name
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    $packageRoot = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"
    if (Test-Path -LiteralPath $packageRoot) {
        $match = Get-ChildItem -LiteralPath $packageRoot -Filter "$Name.exe" -File -Recurse -ErrorAction SilentlyContinue |
            Where-Object FullName -Match "Gyan\.FFmpeg" |
            Sort-Object FullName -Descending |
            Select-Object -First 1
        if ($null -ne $match) {
            return $match.FullName
        }
    }

    throw "$Name was not found. Install the Gyan.FFmpeg WinGet package first."
}

function Invoke-NativeCapture {
    param(
        [Parameter(Mandatory)]
        [string]$Executable,
        [Parameter(Mandatory)]
        [string[]]$Arguments,
        [Parameter(Mandatory)]
        [string]$Operation
    )

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = & $Executable @Arguments 2>&1
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    $text = ($output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
    if ($exitCode -ne 0) {
        throw "$Operation failed with exit code $exitCode.`n$text"
    }
    return $text
}

function Convert-ToInvariantDouble {
    param(
        [Parameter(Mandatory)]
        [object]$Value
    )

    return [double]::Parse($Value.ToString(), $invariantCulture)
}

function Convert-SecondsToFrames {
    param(
        [Parameter(Mandatory)]
        [object]$Value,
        [Parameter(Mandatory)]
        [int]$SampleRate
    )

    $seconds = Convert-ToInvariantDouble $Value
    if ($seconds -lt 0.0) {
        throw "Loop time cannot be negative: '$Value'."
    }
    return [uint64][math]::Round($seconds * $SampleRate, [MidpointRounding]::AwayFromZero)
}

function Get-AudioMetadata {
    param(
        [Parameter(Mandatory)]
        [string]$Path,
        [Parameter(Mandatory)]
        [string]$FfprobePath
    )

    $json = Invoke-NativeCapture -Executable $FfprobePath -Operation "Probing '$Path'" -Arguments @(
        "-v", "error",
        "-select_streams", "a:0",
        "-show_entries", "stream=codec_name,sample_rate,channels,duration_ts,time_base:format=duration:format_tags=title",
        "-of", "json",
        $Path
    )
    $metadata = $json | ConvertFrom-Json
    if ($null -eq $metadata.streams -or @($metadata.streams).Count -ne 1) {
        throw "Expected exactly one primary audio stream in '$Path'."
    }

    $stream = @($metadata.streams)[0]
    $durationSeconds = Convert-ToInvariantDouble $metadata.format.duration
    $frameCount = [uint64][math]::Round($durationSeconds * [int]$stream.sample_rate)
    if ($null -ne $stream.duration_ts -and $stream.time_base -eq "1/$($stream.sample_rate)") {
        $frameCount = [uint64]$stream.duration_ts
    }
    $title = ""
    if ($null -ne $metadata.format.tags -and $null -ne $metadata.format.tags.title) {
        $title = $metadata.format.tags.title.ToString()
    }

    return [pscustomobject]@{
        CodecName = $stream.codec_name.ToString()
        SampleRate = [int]$stream.sample_rate
        Channels = [int]$stream.channels
        DurationSeconds = $durationSeconds
        FrameCount = $frameCount
        Title = $title
    }
}

function Assert-ChildPath {
    param(
        [Parameter(Mandatory)]
        [string]$Parent,
        [Parameter(Mandatory)]
        [string]$Child
    )

    $parentFullPath = [IO.Path]::GetFullPath($Parent).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    $childFullPath = [IO.Path]::GetFullPath($Child)
    if (-not $childFullPath.StartsWith($parentFullPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path escapes its expected parent: '$childFullPath' is not under '$parentFullPath'."
    }
}

function Assert-Mapping {
    param(
        [Parameter(Mandatory)]
        [object[]]$Rows,
        [Parameter(Mandatory)]
        [IO.FileInfo[]]$SourceFiles,
        [Parameter(Mandatory)]
        [int]$SampleRate
    )

    $requiredColumns = @(
        "source", "output", "cue_id", "display_name",
        "loop_start_seconds", "loop_end_seconds", "loop_crossfade_ms", "notes"
    )
    if ($Rows.Count -eq 0) {
        throw "The BGM mapping is empty."
    }
    foreach ($column in $requiredColumns) {
        if ($Rows[0].PSObject.Properties.Name -notcontains $column) {
            throw "The BGM mapping is missing the '$column' column."
        }
    }

    $problems = [Collections.Generic.List[string]]::new()
    foreach ($group in @($Rows | Group-Object source | Where-Object Count -gt 1)) {
        $problems.Add("Duplicate source name: $($group.Name)")
    }
    foreach ($group in @($Rows | Group-Object output | Where-Object Count -gt 1)) {
        $problems.Add("Duplicate output name: $($group.Name)")
    }
    foreach ($group in @($Rows | Group-Object cue_id | Where-Object Count -gt 1)) {
        $problems.Add("Duplicate cue ID: $($group.Name)")
    }

    $sourceNames = @($SourceFiles | ForEach-Object Name)
    $mappedSources = @($Rows | ForEach-Object source)
    foreach ($name in @($sourceNames | Where-Object { $_ -notin $mappedSources })) {
        $problems.Add("Unmapped source file: $name")
    }
    foreach ($name in @($mappedSources | Where-Object { $_ -notin $sourceNames })) {
        $problems.Add("Mapped source file not found: $name")
    }

    foreach ($row in $Rows) {
        if ($row.output -cnotmatch "^[a-z0-9]+(?:_[a-z0-9]+)*\.wav$") {
            $problems.Add("Output name must be lower snake case ASCII: $($row.output)")
        }
        if ($row.cue_id -cnotmatch "^bgm\.asset\.[a-z0-9]+(?:_[a-z0-9]+)*$") {
            $problems.Add("Invalid BGM cue ID: $($row.cue_id)")
        }
        if ([string]::IsNullOrWhiteSpace($row.display_name) -or $row.display_name.Contains("`t")) {
            $problems.Add("Invalid display name for '$($row.source)'.")
        }
        try {
            $startFrame = Convert-SecondsToFrames $row.loop_start_seconds $SampleRate
            $crossfadeFrames = Convert-SecondsToFrames ((Convert-ToInvariantDouble $row.loop_crossfade_ms) / 1000.0) $SampleRate
            if ($row.loop_end_seconds -ne "full") {
                $endFrame = Convert-SecondsToFrames $row.loop_end_seconds $SampleRate
                if ($endFrame -le $startFrame) {
                    $problems.Add("Loop end must be after loop start for '$($row.source)'.")
                }
                if ($crossfadeFrames * 2 -ge ($endFrame - $startFrame)) {
                    $problems.Add("Loop crossfade is too long for '$($row.source)'.")
                }
            }
        }
        catch {
            $problems.Add("Invalid loop values for '$($row.source)': $($_.Exception.Message)")
        }
    }

    if ($Rows.Count -ne $SourceFiles.Count) {
        $problems.Add("Mapping row count $($Rows.Count) does not match source file count $($SourceFiles.Count).")
    }
    if ($problems.Count -gt 0) {
        throw "BGM mapping validation failed:`n$($problems -join [Environment]::NewLine)"
    }
}

function Get-ExpectedLoopData {
    param(
        [Parameter(Mandatory)]
        [object]$Row,
        [Parameter(Mandatory)]
        [uint64]$FrameCount,
        [Parameter(Mandatory)]
        [int]$SampleRate
    )

    $startFrame = Convert-SecondsToFrames $Row.loop_start_seconds $SampleRate
    $endFrame = if ($Row.loop_end_seconds -eq "full") {
        $FrameCount
    }
    else {
        Convert-SecondsToFrames $Row.loop_end_seconds $SampleRate
    }
    $crossfadeFrames = Convert-SecondsToFrames ((Convert-ToInvariantDouble $Row.loop_crossfade_ms) / 1000.0) $SampleRate
    if ($startFrame -ge $FrameCount -or $endFrame -gt $FrameCount -or $endFrame -le $startFrame) {
        throw "Loop region is outside '$($Row.output)': start=$startFrame end=$endFrame frames=$FrameCount."
    }
    if ($crossfadeFrames * 2 -ge ($endFrame - $startFrame)) {
        throw "Loop crossfade is too long for '$($Row.output)'."
    }
    return [pscustomobject]@{
        StartFrame = $startFrame
        EndFrame = $endFrame
        ManifestEndFrame = if ($Row.loop_end_seconds -eq "full") { [uint64]0 } else { $endFrame }
        CrossfadeFrames = $crossfadeFrames
    }
}

function Assert-ManifestMatchesMapping {
    param(
        [Parameter(Mandatory)]
        [object[]]$Rows,
        [Parameter(Mandatory)]
        [object[]]$Results,
        [Parameter(Mandatory)]
        [string]$Path
    )

    $manifestRows = @(Import-Csv -LiteralPath $Path -Delimiter "`t" -Encoding UTF8)
    foreach ($row in $Rows) {
        $manifest = @($manifestRows | Where-Object id -CEQ $row.cue_id)
        if ($manifest.Count -ne 1) {
            throw "Manifest must contain exactly one '$($row.cue_id)' row."
        }
        $result = @($Results | Where-Object cue_id -CEQ $row.cue_id)[0]
        $expectedPath = "bgm/$($row.output)"
        if ($manifest[0].type -cne "bgm" -or
            $manifest[0].path -cne $expectedPath -or
            $manifest[0].display_name -cne $row.display_name -or
            [uint64]$manifest[0].loop_start_frame -ne [uint64]$result.loop_start_frame -or
            [uint64]$manifest[0].loop_end_frame -ne [uint64]$result.manifest_loop_end_frame -or
            [uint64]$manifest[0].loop_crossfade_frames -ne [uint64]$result.loop_crossfade_frames) {
            throw "Manifest values do not match the BGM mapping for '$($row.cue_id)'."
        }
    }
}

$sourcePath = (Resolve-Path -LiteralPath $SourceDirectory).Path
$mappingFullPath = (Resolve-Path -LiteralPath $MappingPath).Path
$manifestFullPath = (Resolve-Path -LiteralPath $ManifestPath).Path
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$outputParent = Split-Path -Parent $outputPath
$stagingPath = Join-Path $outputParent (".bgm-staging-" + [guid]::NewGuid().ToString("N"))

$ffmpeg = Find-MediaTool "ffmpeg"
$ffprobe = Find-MediaTool "ffprobe"
$mapping = @(Import-Csv -LiteralPath $mappingFullPath -Delimiter "`t" -Encoding UTF8)
$sourceFiles = @(Get-ChildItem -LiteralPath $sourcePath -File -Filter "*.mp3" | Sort-Object Name)
Assert-Mapping -Rows $mapping -SourceFiles $sourceFiles -SampleRate $OutputSampleRate

New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
Assert-ChildPath -Parent $outputParent -Child $stagingPath
$results = [Collections.Generic.List[object]]::new()

try {
    if (-not $VerifyOnly) {
        $existingOutputs = @($mapping | ForEach-Object { Join-Path $outputPath $_.output } | Where-Object { Test-Path -LiteralPath $_ })
        if ($existingOutputs.Count -gt 0 -and -not $Force) {
            throw "Mapped BGM outputs already exist. Use -VerifyOnly to check them or -Force to replace only mapped outputs."
        }
        New-Item -ItemType Directory -Path $stagingPath | Out-Null
    }

    $index = 0
    foreach ($row in $mapping) {
        $index++
        $inputPath = Join-Path $sourcePath $row.source
        $finalPath = Join-Path $outputPath $row.output
        $candidatePath = if ($VerifyOnly) { $finalPath } else { Join-Path $stagingPath $row.output }
        Assert-ChildPath -Parent $outputPath -Child $finalPath
        if (-not $VerifyOnly) {
            Write-Progress -Activity "Preparing BGM assets" -Status "$index/$($mapping.Count): $($row.source)" -PercentComplete (($index / $mapping.Count) * 100)
            Invoke-NativeCapture -Executable $ffmpeg -Operation "Converting '$inputPath'" -Arguments @(
                "-hide_banner", "-nostdin", "-loglevel", "error", "-y",
                "-fflags", "+bitexact",
                "-i", $inputPath,
                "-map", "0:a:0",
                "-map_metadata", "-1",
                "-metadata", "title=$($row.display_name)",
                "-vn", "-sn", "-dn",
                "-ac", "2",
                "-ar", $OutputSampleRate.ToString($invariantCulture),
                "-c:a", "pcm_s16le",
                "-flags:a", "+bitexact",
                $candidatePath
            ) | Out-Null
        }
        if (-not (Test-Path -LiteralPath $candidatePath -PathType Leaf)) {
            throw "Prepared BGM is missing: '$candidatePath'."
        }

        $metadata = Get-AudioMetadata -Path $candidatePath -FfprobePath $ffprobe
        $checks = [Collections.Generic.List[string]]::new()
        if ($metadata.CodecName -ne "pcm_s16le") { $checks.Add("codec=$($metadata.CodecName)") }
        if ($metadata.SampleRate -ne $OutputSampleRate) { $checks.Add("sampleRate=$($metadata.SampleRate)") }
        if ($metadata.Channels -ne 2) { $checks.Add("channels=$($metadata.Channels)") }
        if ($metadata.FrameCount -eq 0) { $checks.Add("empty audio") }
        if ($metadata.Title -cne $row.display_name) { $checks.Add("title='$($metadata.Title)'") }
        $loop = Get-ExpectedLoopData -Row $row -FrameCount $metadata.FrameCount -SampleRate $OutputSampleRate
        if ($checks.Count -gt 0) {
            throw "Verification failed for '$($row.output)': $($checks -join '; ')"
        }

        $results.Add([pscustomobject][ordered]@{
            cue_id = $row.cue_id
            output = $row.output
            display_name = $row.display_name
            duration_seconds = [math]::Round($metadata.DurationSeconds, 6)
            frame_count = $metadata.FrameCount
            loop_start_frame = $loop.StartFrame
            loop_end_frame = $loop.EndFrame
            manifest_loop_end_frame = $loop.ManifestEndFrame
            loop_crossfade_frames = $loop.CrossfadeFrames
            sha256 = (Get-FileHash -LiteralPath $candidatePath -Algorithm SHA256).Hash
        })
    }
    Write-Progress -Activity "Preparing BGM assets" -Completed

    if (-not $VerifyOnly) {
        foreach ($row in $mapping) {
            $candidatePath = Join-Path $stagingPath $row.output
            $finalPath = Join-Path $outputPath $row.output
            Move-Item -LiteralPath $candidatePath -Destination $finalPath -Force
        }
    }

    Assert-ManifestMatchesMapping -Rows $mapping -Results @($results) -Path $manifestFullPath
    @($results) | Format-Table cue_id, duration_seconds, loop_start_frame, loop_end_frame, loop_crossfade_frames -AutoSize
    Write-Output "BGM assets verified: $($results.Count) files"
    Write-Output "Output: $outputPath"
}
finally {
    Write-Progress -Activity "Preparing BGM assets" -Completed
    if (Test-Path -LiteralPath $stagingPath) {
        Assert-ChildPath -Parent $outputParent -Child $stagingPath
        Remove-Item -LiteralPath $stagingPath -Recurse -Force
    }
}
