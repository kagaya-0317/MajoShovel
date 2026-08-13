param(
    [string]$SessionId = "",
    [string]$RequiredConfig = ""
)

$ErrorActionPreference = "Stop"
$Root = [System.IO.Path]::GetFullPath((Resolve-Path (Join-Path $PSScriptRoot "..")))
. (Join-Path $PSScriptRoot "build_support.ps1")

function Stop-NotReusable([string]$Message) {
    Write-Host "[edit-build-check] not reusable: $Message"
    exit 1
}

if ([string]::IsNullOrWhiteSpace($SessionId)) {
    $SessionId = $env:CODEX_THREAD_ID
}
if ([string]::IsNullOrWhiteSpace($SessionId)) {
    Stop-NotReusable "SessionId is required outside a Codex task"
}

$receiptRoot = Join-Path (Get-MajoShovelCodexEditSessionRoot $Root) "completed"
$receipts = if (Test-Path -LiteralPath $receiptRoot -PathType Container) {
    @(Get-ChildItem -LiteralPath $receiptRoot -Filter "*.json" -File | Sort-Object LastWriteTimeUtc -Descending)
} else { @() }
$receipt = $null
$receiptPath = ""
foreach ($file in $receipts) {
    try {
        $candidate = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
        if ([string]::Equals([string]$candidate.sessionId, $SessionId, [System.StringComparison]::OrdinalIgnoreCase)) {
            $receipt = $candidate
            $receiptPath = $file.FullName
            break
        }
    }
    catch {
    }
}
if ($null -eq $receipt) {
    Stop-NotReusable "no completed edit receipt exists for session '$SessionId'"
}
if ($receipt.schemaVersion -ne 2) {
    Stop-NotReusable "the latest edit receipt uses an unsupported schema"
}

function Test-BuildCoversReceipt($Build, $Receipt, [ref]$VerifiedPaths) {
    if ($Build.schemaVersion -ne 1 -or $Build.status -ne "succeeded" -or -not $Build.reusable -or
        $null -eq $Build.inputSnapshot -or
        ([DateTime]$Build.startedAtUtc).ToUniversalTime() -lt ([DateTime]$Receipt.stoppedAtUtc).ToUniversalTime()) {
        return $false
    }
    if (-not [string]::IsNullOrWhiteSpace($RequiredConfig) -and
        -not [string]::Equals([string]$Build.config, $RequiredConfig, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $false
    }

    $buildFiles = @{}
    foreach ($file in @($Build.inputSnapshot.files)) {
        $buildFiles[[string]$file.path] = $file
    }
    $recompiled = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($source in @($Build.recompiledSources)) {
        [void]$recompiled.Add([string]$source)
    }

    $matchedPaths = [System.Collections.Generic.List[string]]::new()
    $hasBuildInputEvidence = $false
    foreach ($pathEvidence in @($Receipt.paths)) {
        if ($pathEvidence.isBuildInputPath) {
            $hasBuildInputEvidence = $true
            $pathItem = [string]$pathEvidence.path
            $existsInBuild = $buildFiles.ContainsKey($pathItem)
            if (($pathEvidence.existsAtStop -and
                    (-not $existsInBuild -or [string]$buildFiles[$pathItem].sha256 -ne [string]$pathEvidence.sha256)) -or
                (-not $pathEvidence.existsAtStop -and $existsInBuild)) {
                return $false
            }
            $extension = [System.IO.Path]::GetExtension($pathItem).ToLowerInvariant()
            if ($pathEvidence.existsAtStop -and
                $extension -in @(".cpp", ".c", ".cc", ".cxx") -and
                -not $recompiled.Contains($pathItem)) {
                return $false
            }
            if ($extension -in @(".hpp", ".h", ".inl") -and $recompiled.Count -eq 0) {
                return $false
            }
            $matchedPaths.Add($pathItem)
        }
    }
    if (-not $hasBuildInputEvidence) {
        return $false
    }
    $VerifiedPaths.Value = @($matchedPaths)
    return $true
}

$evidenceRoot = Get-MajoShovelDevBuildEvidenceRoot $Root
$candidateFiles = [System.Collections.Generic.List[System.IO.FileInfo]]::new()
$latestPath = Join-Path $evidenceRoot "latest-result.json"
if (Test-Path -LiteralPath $latestPath -PathType Leaf) {
    $candidateFiles.Add((Get-Item -LiteralPath $latestPath))
}
$successRoot = Join-Path $evidenceRoot "successes"
if (Test-Path -LiteralPath $successRoot -PathType Container) {
    foreach ($file in @(Get-ChildItem -LiteralPath $successRoot -Filter "*.json" -File | Sort-Object LastWriteTimeUtc -Descending)) {
        if (-not [string]::Equals($file.FullName, $latestPath, [System.StringComparison]::OrdinalIgnoreCase)) {
            $candidateFiles.Add($file)
        }
    }
}

$matchedBuild = $null
$matchedEvidencePath = ""
$verifiedPaths = @()
foreach ($file in $candidateFiles) {
    try {
        $candidate = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
        $candidateVerifiedPaths = @()
        if (Test-BuildCoversReceipt $candidate $receipt ([ref]$candidateVerifiedPaths)) {
            $matchedBuild = $candidate
            $matchedEvidencePath = $file.FullName
            $verifiedPaths = @($candidateVerifiedPaths)
            break
        }
    }
    catch {
    }
}
if ($null -eq $matchedBuild) {
    Stop-NotReusable "no retained successful auto-reload build covers the completed edit session"
}

[pscustomobject]@{
    reusable = $true
    sessionId = $SessionId
    receiptPath = $receiptPath
    evidencePath = $matchedEvidencePath
    buildId = $matchedBuild.buildId
    config = $matchedBuild.config
    verifiedPaths = @($verifiedPaths)
    recompiledSources = @($matchedBuild.recompiledSources)
    executable = $matchedBuild.output.path
    logPath = $matchedBuild.logPath
} | ConvertTo-Json -Depth 6

exit 0
