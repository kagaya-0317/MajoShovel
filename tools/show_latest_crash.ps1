# Show the newest MajoShovel crash report and the related dump path.
$ErrorActionPreference = "Stop"

$root = Join-Path $env:LOCALAPPDATA "MajoShovel\crashes"
if (-not (Test-Path -LiteralPath $root)) {
    Write-Host "No crash directory: $root"
    exit 0
}

$report = Get-ChildItem -LiteralPath $root -Filter "crash-*.txt" -File |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if ($null -eq $report) {
    Write-Host "No crash report found in $root"
    exit 0
}

Write-Host "Latest crash report: $($report.FullName)"
Write-Host ""
Get-Content -LiteralPath $report.FullName -Encoding UTF8 |
    Select-Object -First 260
