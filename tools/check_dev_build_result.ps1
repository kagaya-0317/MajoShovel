param(
    [string[]]$ChangedPath = @(),
    [string]$RequiredConfig = ""
)

$ErrorActionPreference = "Stop"
$Root = [System.IO.Path]::GetFullPath((Resolve-Path (Join-Path $PSScriptRoot "..")))
. (Join-Path $PSScriptRoot "build_support.ps1")

function Stop-NotReusable([string]$Message) {
    Write-Host "[dev-build-check] not reusable: $Message"
    exit 1
}

function Get-RepositoryRelativePath([string]$Path) {
    try {
        return Get-MajoShovelRepositoryRelativePath $Root $Path
    }
    catch {
        Stop-NotReusable "changed path is outside the repository: $Path"
    }
}

$evidencePath = Join-Path (Get-MajoShovelDevBuildEvidenceRoot $Root) "latest-result.json"
if (-not (Test-Path -LiteralPath $evidencePath -PathType Leaf)) {
    Stop-NotReusable "latest-result.json does not exist"
}

try {
    $result = Get-Content -LiteralPath $evidencePath -Raw | ConvertFrom-Json
}
catch {
    Stop-NotReusable "latest-result.json could not be read: $($_.Exception.Message)"
}

if ($result.schemaVersion -ne 1) {
    Stop-NotReusable "unsupported evidence schema: $($result.schemaVersion)"
}
if ($result.status -ne "succeeded" -or -not $result.reusable) {
    Stop-NotReusable "latest build status is '$($result.status)' (reusable=$($result.reusable))"
}
if (-not [string]::IsNullOrWhiteSpace($RequiredConfig) -and
    -not [string]::Equals($result.config, $RequiredConfig, [System.StringComparison]::OrdinalIgnoreCase)) {
    Stop-NotReusable "build config '$($result.config)' does not match required config '$RequiredConfig'"
}
if (-not [string]::Equals(
        [System.IO.Path]::GetFullPath($result.repositoryRoot),
        $Root,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    Stop-NotReusable "evidence belongs to another repository root"
}

$watcher = Get-CimInstance Win32_Process -Filter "ProcessId = $($result.watcherPid)" -ErrorAction SilentlyContinue
if ($null -eq $watcher -or
    $watcher.Name -notin @("powershell.exe", "pwsh.exe") -or
    [string]::IsNullOrWhiteSpace($watcher.CommandLine) -or
    $watcher.CommandLine.IndexOf("dev_auto_reload.ps1", [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
    Stop-NotReusable "the dev_auto_reload process that produced this result is not running"
}
$watcherStartedAtUtc = ([DateTime]$watcher.CreationDate).ToUniversalTime()
$recordedWatcherStartUtc = ([DateTime]$result.watcherStartedAtUtc).ToUniversalTime()
if ([Math]::Abs(($watcherStartedAtUtc - $recordedWatcherStartUtc).TotalSeconds) -gt 2.0) {
    Stop-NotReusable "watcher PID was reused by a different process instance"
}

$currentSnapshot = Get-MajoShovelBuildInputSnapshot $Root
if ($null -eq $result.inputSnapshot -or
    $currentSnapshot.fingerprint -ne $result.inputSnapshot.fingerprint -or
    $currentSnapshot.fingerprint -ne $result.finalInputFingerprint) {
    Stop-NotReusable "source/CMake inputs changed after the recorded build"
}

if ($null -eq $result.output -or -not (Test-Path -LiteralPath $result.output.path -PathType Leaf)) {
    Stop-NotReusable "recorded executable is missing"
}
$currentOutput = Get-MajoShovelStableFileSnapshot $result.output.path
if ($currentOutput.length -ne $result.output.length -or
    $currentOutput.lastWriteUtcTicks -ne $result.output.lastWriteUtcTicks -or
    $currentOutput.sha256 -ne $result.output.sha256) {
    Stop-NotReusable "recorded executable changed after the build"
}

$inputFiles = @{}
foreach ($file in @($result.inputSnapshot.files)) {
    $inputFiles[[string]$file.path] = $file
}
$recompiledSources = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($source in @($result.recompiledSources)) {
    [void]$recompiledSources.Add([string]$source)
}

foreach ($path in $ChangedPath) {
    if ([string]::IsNullOrWhiteSpace($path)) {
        continue
    }
    $relativePath = Get-RepositoryRelativePath $path
    if (-not $inputFiles.ContainsKey($relativePath)) {
        Stop-NotReusable "changed build input is absent from the recorded snapshot: $relativePath"
    }
    $extension = [System.IO.Path]::GetExtension($relativePath).ToLowerInvariant()
    if ($extension -in @(".cpp", ".c", ".cc", ".cxx") -and
        -not $recompiledSources.Contains($relativePath)) {
        Stop-NotReusable "changed translation unit was not recompiled: $relativePath"
    }
    if ($extension -in @(".hpp", ".h", ".inl") -and $recompiledSources.Count -eq 0) {
        Stop-NotReusable "header changed but no translation unit was recompiled: $relativePath"
    }
}

[pscustomobject]@{
    reusable = $true
    buildId = $result.buildId
    config = $result.config
    inputFingerprint = $currentSnapshot.fingerprint
    changedPaths = @($ChangedPath | ForEach-Object { Get-RepositoryRelativePath $_ })
    recompiledSources = @($result.recompiledSources)
    executable = $result.output.path
    logPath = $result.logPath
} | ConvertTo-Json -Depth 5

exit 0
