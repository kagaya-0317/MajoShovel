[CmdletBinding()]
param(
    [string]$SourceDirectory = "",
    [string]$OutputDirectory = "",
    [string]$MappingPath = "",
    [double]$SilenceThresholdDb = -50.0,
    [double]$MinimumSilenceSeconds = 0.010,
    [double]$PreservedLeadSeconds = 0.005,
    [int]$OutputSampleRate = 48000
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$invariantCulture = [Globalization.CultureInfo]::InvariantCulture
$utf8Bom = [Text.UTF8Encoding]::new($true)

if ([string]::IsNullOrWhiteSpace($SourceDirectory)) {
    $SourceDirectory = Join-Path $PSScriptRoot "..\効果音素材"
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $SourceDirectory "効果音素材_converted"
}
if ([string]::IsNullOrWhiteSpace($MappingPath)) {
    $MappingPath = Join-Path $PSScriptRoot "sound_material_names.tsv"
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
        "-show_entries", "stream=codec_name,sample_rate,channels:format=duration",
        "-of", "json",
        $Path
    )
    $metadata = $json | ConvertFrom-Json
    if ($null -eq $metadata.streams -or @($metadata.streams).Count -ne 1) {
        throw "Expected exactly one primary audio stream in '$Path'."
    }

    $stream = @($metadata.streams)[0]
    return [pscustomobject]@{
        CodecName = $stream.codec_name.ToString()
        SampleRate = [int]$stream.sample_rate
        Channels = [int]$stream.channels
        DurationSeconds = Convert-ToInvariantDouble $metadata.format.duration
    }
}

function Get-LeadingSilenceSeconds {
    param(
        [Parameter(Mandatory)]
        [string]$Path,
        [Parameter(Mandatory)]
        [string]$FfmpegPath,
        [Parameter(Mandatory)]
        [double]$ThresholdDb,
        [Parameter(Mandatory)]
        [double]$MinimumDuration
    )

    $thresholdText = $ThresholdDb.ToString("0.###", $invariantCulture)
    $durationText = $MinimumDuration.ToString("0.######", $invariantCulture)
    $analysis = Invoke-NativeCapture -Executable $FfmpegPath -Operation "Analyzing leading silence in '$Path'" -Arguments @(
        "-hide_banner",
        "-nostdin",
        "-i", $Path,
        "-af", "silencedetect=noise=${thresholdText}dB:d=$durationText",
        "-f", "null",
        "NUL"
    )

    $match = [regex]::Match(
        $analysis,
        "silence_start:\s*([0-9.]+).*?silence_end:\s*([0-9.]+)",
        [Text.RegularExpressions.RegexOptions]::Singleline
    )
    if (-not $match.Success) {
        return 0.0
    }

    $start = Convert-ToInvariantDouble $match.Groups[1].Value
    $end = Convert-ToInvariantDouble $match.Groups[2].Value
    if ($start -gt 0.0005 -or $end -le $start) {
        return 0.0
    }
    return $end
}

function Get-MaxVolumeDb {
    param(
        [Parameter(Mandatory)]
        [string]$Path,
        [Parameter(Mandatory)]
        [string]$FfmpegPath
    )

    $analysis = Invoke-NativeCapture -Executable $FfmpegPath -Operation "Measuring volume in '$Path'" -Arguments @(
        "-hide_banner",
        "-nostdin",
        "-i", $Path,
        "-af", "volumedetect",
        "-f", "null",
        "NUL"
    )
    $match = [regex]::Match($analysis, "max_volume:\s*(-?(?:inf|[0-9.]+))\s*dB")
    if (-not $match.Success -or $match.Groups[1].Value -eq "-inf") {
        return [double]::NegativeInfinity
    }
    return Convert-ToInvariantDouble $match.Groups[1].Value
}

function Export-CsvUtf8Bom {
    param(
        [Parameter(Mandatory)]
        [object[]]$Rows,
        [Parameter(Mandatory)]
        [string]$Path
    )

    $lines = @($Rows | ConvertTo-Csv -NoTypeInformation)
    [IO.File]::WriteAllLines($Path, $lines, $utf8Bom)
}

function Assert-Mapping {
    param(
        [Parameter(Mandatory)]
        [object[]]$Rows,
        [Parameter(Mandatory)]
        [IO.FileInfo[]]$SourceFiles
    )

    if ($Rows.Count -eq 0) {
        throw "The filename mapping is empty."
    }

    $duplicateSources = @($Rows | Group-Object source | Where-Object Count -gt 1)
    $duplicateOutputs = @($Rows | Group-Object output | Where-Object Count -gt 1)
    $invalidOutputs = @($Rows | Where-Object output -CNotMatch "^0_[A-Z][A-Za-z]*[0-9]*\.wav$")
    $mappedSources = @($Rows | ForEach-Object source)
    $sourceNames = @($SourceFiles | ForEach-Object Name)
    $unmapped = @($sourceNames | Where-Object { $_ -notin $mappedSources })
    $missing = @($mappedSources | Where-Object { $_ -notin $sourceNames })

    $problems = @()
    if ($duplicateSources.Count -gt 0) {
        $problems += "Duplicate source names: $($duplicateSources.Name -join ', ')"
    }
    if ($duplicateOutputs.Count -gt 0) {
        $problems += "Duplicate output names: $($duplicateOutputs.Name -join ', ')"
    }
    if ($invalidOutputs.Count -gt 0) {
        $problems += "Invalid output names: $($invalidOutputs.output -join ', ')"
    }
    if ($unmapped.Count -gt 0) {
        $problems += "Unmapped source files: $($unmapped -join ', ')"
    }
    if ($missing.Count -gt 0) {
        $problems += "Mapped source files not found: $($missing -join ', ')"
    }
    if ($Rows.Count -ne $SourceFiles.Count) {
        $problems += "Mapping row count $($Rows.Count) does not match source file count $($SourceFiles.Count)."
    }

    if ($problems.Count -gt 0) {
        throw "Filename mapping validation failed:`n$($problems -join [Environment]::NewLine)"
    }
}

$sourcePath = (Resolve-Path -LiteralPath $SourceDirectory).Path
$mappingFullPath = (Resolve-Path -LiteralPath $MappingPath).Path
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$stagingPath = "$outputPath.staging-$([guid]::NewGuid().ToString('N'))"

if (Test-Path -LiteralPath $outputPath) {
    throw "Output directory already exists: '$outputPath'. Move or rename it before running again."
}

$ffmpeg = Find-MediaTool "ffmpeg"
$ffprobe = Find-MediaTool "ffprobe"
$ffmpegVersion = (& $ffmpeg -hide_banner -version 2>&1 | Select-Object -First 1).ToString()
$ffprobeVersion = (& $ffprobe -hide_banner -version 2>&1 | Select-Object -First 1).ToString()

$mapping = @(Import-Csv -LiteralPath $mappingFullPath -Delimiter "`t" -Encoding UTF8)
$sourceFiles = @(Get-ChildItem -LiteralPath $sourcePath -File -Filter "*.mp3" | Sort-Object Name)
Assert-Mapping -Rows $mapping -SourceFiles $sourceFiles

New-Item -ItemType Directory -Path $stagingPath | Out-Null
$results = [Collections.Generic.List[object]]::new()

try {
    $index = 0
    foreach ($row in $mapping) {
        $index++
        $inputPath = Join-Path $sourcePath $row.source
        $outputFilePath = Join-Path $stagingPath $row.output
        Write-Progress -Activity "Converting sound materials" -Status "$index/$($mapping.Count): $($row.source) -> $($row.output)" -PercentComplete (($index / $mapping.Count) * 100)

        $inputMetadata = Get-AudioMetadata -Path $inputPath -FfprobePath $ffprobe
        if ($inputMetadata.Channels -lt 1 -or $inputMetadata.Channels -gt 2) {
            throw "Unsupported channel count $($inputMetadata.Channels) in '$inputPath'."
        }

        $leadingSilence = Get-LeadingSilenceSeconds `
            -Path $inputPath `
            -FfmpegPath $ffmpeg `
            -ThresholdDb $SilenceThresholdDb `
            -MinimumDuration $MinimumSilenceSeconds
        $trimSeconds = [math]::Max(0.0, $leadingSilence - $PreservedLeadSeconds)
        $trimText = $trimSeconds.ToString("0.######", $invariantCulture)

        $ffmpegArguments = @(
            "-hide_banner",
            "-nostdin",
            "-loglevel", "error",
            "-i", $inputPath,
            "-map_metadata", "-1",
            "-vn",
            "-sn",
            "-dn"
        )
        if ($trimSeconds -gt 0.0) {
            $ffmpegArguments += @("-af", "atrim=start=$trimText,asetpts=N/SR/TB")
        }
        $ffmpegArguments += @(
            "-ar", $OutputSampleRate.ToString($invariantCulture),
            "-c:a", "pcm_s16le",
            $outputFilePath
        )
        Invoke-NativeCapture -Executable $ffmpeg -Arguments $ffmpegArguments -Operation "Converting '$inputPath'" | Out-Null

        $outputMetadata = Get-AudioMetadata -Path $outputFilePath -FfprobePath $ffprobe
        $remainingLeadingSilence = Get-LeadingSilenceSeconds `
            -Path $outputFilePath `
            -FfmpegPath $ffmpeg `
            -ThresholdDb $SilenceThresholdDb `
            -MinimumDuration $MinimumSilenceSeconds
        $maxVolumeDb = Get-MaxVolumeDb -Path $outputFilePath -FfmpegPath $ffmpeg

        $checks = @()
        if ($outputMetadata.CodecName -ne "pcm_s16le") {
            $checks += "codec=$($outputMetadata.CodecName)"
        }
        if ($outputMetadata.SampleRate -ne $OutputSampleRate) {
            $checks += "sampleRate=$($outputMetadata.SampleRate)"
        }
        if ($outputMetadata.Channels -ne $inputMetadata.Channels) {
            $checks += "channels=$($outputMetadata.Channels), expected=$($inputMetadata.Channels)"
        }
        if ($outputMetadata.DurationSeconds -le 0.010) {
            $checks += "output is too short"
        }
        if ($outputMetadata.DurationSeconds -gt ($inputMetadata.DurationSeconds + 0.050)) {
            $checks += "output is longer than input"
        }
        if ([double]::IsNegativeInfinity($maxVolumeDb) -or $maxVolumeDb -lt -80.0) {
            $checks += "output is silent or nearly silent"
        }
        if ($remainingLeadingSilence -gt ($PreservedLeadSeconds + $MinimumSilenceSeconds)) {
            $checks += "leading silence remains"
        }

        $status = if ($checks.Count -eq 0) { "PASS" } else { "FAIL" }
        $reviewReasons = @()
        if ($trimSeconds -ge 0.100) {
            $reviewReasons += "large leading trim"
        }
        if ($maxVolumeDb -lt -30.0) {
            $reviewReasons += "quiet output"
        }
        if ($outputMetadata.DurationSeconds -le 0.100) {
            $reviewReasons += "very short output"
        }

        $results.Add([pscustomobject][ordered]@{
            source = $row.source
            output = $row.output
            sourcecodec = $inputMetadata.CodecName
            sourcehz = $inputMetadata.SampleRate
            outputcodec = $outputMetadata.CodecName
            outputhz = $outputMetadata.SampleRate
            channels = $outputMetadata.Channels
            sourcedurationms = [math]::Round($inputMetadata.DurationSeconds * 1000.0, 3)
            outputdurationms = [math]::Round($outputMetadata.DurationSeconds * 1000.0, 3)
            detectedsilencems = [math]::Round($leadingSilence * 1000.0, 3)
            appliedtrimms = [math]::Round($trimSeconds * 1000.0, 3)
            remainingdetectedsilencems = [math]::Round($remainingLeadingSilence * 1000.0, 3)
            maxvolumedb = $maxVolumeDb
            sourcebytes = (Get-Item -LiteralPath $inputPath).Length
            outputbytes = (Get-Item -LiteralPath $outputFilePath).Length
            sourcesha256 = (Get-FileHash -LiteralPath $inputPath -Algorithm SHA256).Hash
            outputsha256 = (Get-FileHash -LiteralPath $outputFilePath -Algorithm SHA256).Hash
            status = $status
            notes = $checks -join "; "
            review = $reviewReasons -join "; "
        })

        if ($status -ne "PASS") {
            throw "Verification failed for '$($row.source)': $($checks -join '; ')"
        }
    }
    Write-Progress -Activity "Converting sound materials" -Completed

    $wavFiles = @(Get-ChildItem -LiteralPath $stagingPath -File -Filter "*.wav")
    if ($wavFiles.Count -ne $mapping.Count) {
        throw "Expected $($mapping.Count) WAV files, found $($wavFiles.Count)."
    }
    $unexpectedFiles = @($wavFiles | Where-Object Name -NotIn @($mapping.output))
    if ($unexpectedFiles.Count -gt 0) {
        throw "Unexpected WAV files were generated: $($unexpectedFiles.Name -join ', ')"
    }

    $mappingReport = @($mapping | ForEach-Object {
        [pscustomobject][ordered]@{
            source = $_.source
            output = $_.output
        }
    })
    Export-CsvUtf8Bom -Rows $mappingReport -Path (Join-Path $stagingPath "filename_mapping.csv")
    Export-CsvUtf8Bom -Rows @($results) -Path (Join-Path $stagingPath "conversion_report.csv")
    Export-CsvUtf8Bom -Rows @($results | Where-Object review) -Path (Join-Path $stagingPath "review_candidates.csv")

    $trimmedCount = @($results | Where-Object appliedtrimms -GT 0).Count
    $reviewCount = @($results | Where-Object review).Count
    $summaryLines = @(
        "Sound material conversion summary",
        "Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')",
        "Source: $sourcePath",
        "Output: $outputPath",
        "Files: $($results.Count)",
        "Verified: $(@($results | Where-Object status -EQ 'PASS').Count)",
        "Leading silence trimmed: $trimmedCount",
        "Review candidates: $reviewCount",
        "Silence threshold: $SilenceThresholdDb dB",
        "Minimum detected silence: $MinimumSilenceSeconds seconds",
        "Preserved lead: $PreservedLeadSeconds seconds",
        "Output: PCM signed 16-bit WAV, $OutputSampleRate Hz, source channel count preserved",
        "FFmpeg: $ffmpegVersion",
        "FFprobe: $ffprobeVersion"
    )
    [IO.File]::WriteAllLines((Join-Path $stagingPath "conversion_summary.txt"), $summaryLines, $utf8Bom)

    Move-Item -LiteralPath $stagingPath -Destination $outputPath
    Write-Output "Conversion complete: $outputPath"
    Write-Output "Files verified: $($results.Count)"
    Write-Output "Files trimmed: $trimmedCount"
    Write-Output "Review candidates: $reviewCount"
}
catch {
    Write-Progress -Activity "Converting sound materials" -Completed
    if ($results.Count -gt 0 -and (Test-Path -LiteralPath $stagingPath)) {
        Export-CsvUtf8Bom -Rows @($results) -Path (Join-Path $stagingPath "conversion_report_partial.csv")
    }
    throw
}
