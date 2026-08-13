param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Start", "Heartbeat", "Stop", "Status")]
    [string]$Action,
    [string[]]$Path = @(),
    [string]$SessionId = "",
    [int]$LeaseSeconds = 900,
    [int]$WaitForBuildSeconds = 3600
)

$ErrorActionPreference = "Stop"
$Root = [System.IO.Path]::GetFullPath((Resolve-Path (Join-Path $PSScriptRoot "..")))
. (Join-Path $PSScriptRoot "build_support.ps1")

function Resolve-SessionId {
    if (-not [string]::IsNullOrWhiteSpace($SessionId)) {
        return $SessionId.Trim()
    }
    if (-not [string]::IsNullOrWhiteSpace($env:CODEX_THREAD_ID)) {
        return $env:CODEX_THREAD_ID.Trim()
    }
    throw "SessionId is required outside a Codex task. Pass -SessionId explicitly."
}

function Get-NormalizedSessionPaths([string[]]$InputPaths) {
    $normalized = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($item in $InputPaths) {
        if ([string]::IsNullOrWhiteSpace($item)) {
            continue
        }
        [void]$normalized.Add((Get-MajoShovelRepositoryRelativePath $Root $item))
    }
    return @($normalized | Sort-Object)
}

function Read-Session([string]$Id) {
    $sessionPath = Get-MajoShovelCodexEditSessionPath $Root $Id
    if (-not (Test-Path -LiteralPath $sessionPath -PathType Leaf)) {
        return $null
    }
    $session = Get-Content -LiteralPath $sessionPath -Raw | ConvertFrom-Json
    $valid = $session.schemaVersion -eq 1 -and
        [string]::Equals([string]$session.sessionId, $Id, [System.StringComparison]::OrdinalIgnoreCase) -and
        ([DateTime]$session.expiresAtUtc).ToUniversalTime() -gt [DateTime]::UtcNow
    if (-not $valid) {
        Remove-Item -LiteralPath $sessionPath -Force -ErrorAction SilentlyContinue
        return $null
    }
    return $session
}

function Assert-NoPathConflicts([string]$Id, [string[]]$Paths) {
    $conflicts = [System.Collections.Generic.List[string]]::new()
    foreach ($session in @(Get-MajoShovelCodexEditSessions $Root $true)) {
        if ([string]::Equals([string]$session.sessionId, $Id, [System.StringComparison]::OrdinalIgnoreCase)) {
            continue
        }
        $otherPaths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
        foreach ($otherPath in @($session.paths)) {
            [void]$otherPaths.Add([string]$otherPath)
        }
        foreach ($pathItem in $Paths) {
            if ($otherPaths.Contains($pathItem)) {
                $conflicts.Add("$pathItem (session $($session.sessionId))")
            }
        }
    }
    if ($conflicts.Count -gt 0) {
        throw "Another Codex task has an active edit lease for: $($conflicts -join ', ')"
    }
}

function Write-SessionLease([string]$Id, [string[]]$Paths, [DateTime]$StartedAtUtc) {
    $nowUtc = [DateTime]::UtcNow
    $lease = [pscustomobject]@{
        schemaVersion = 1
        sessionId = $Id
        processId = $PID
        repositoryRoot = $Root
        startedAtUtc = $StartedAtUtc.ToString("o")
        heartbeatAtUtc = $nowUtc.ToString("o")
        expiresAtUtc = $nowUtc.AddSeconds([Math]::Max(60, $LeaseSeconds)).ToString("o")
        paths = @($Paths)
    }
    Write-MajoShovelJsonFile (Get-MajoShovelCodexEditSessionPath $Root $Id) $lease
    return $lease
}

function Publish-SessionReceipt([string]$Id, [string[]]$Paths, [DateTime]$StartedAtUtc) {
    $pathEvidence = [System.Collections.Generic.List[object]]::new()
    foreach ($pathItem in $Paths) {
        $normalized = $pathItem.Replace('\', '/').TrimStart('/')
        $isBuildInputPath = [string]::Equals($normalized, "CMakeLists.txt", [System.StringComparison]::OrdinalIgnoreCase) -or
            $normalized.StartsWith("src/", [System.StringComparison]::OrdinalIgnoreCase)
        $fullPath = Join-Path $Root $pathItem
        $entry = if ($isBuildInputPath -and (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
            Get-MajoShovelStableFileSnapshot $fullPath
        } else {
            $null
        }
        $pathEvidence.Add([pscustomobject]@{
            path = $pathItem
            isBuildInputPath = $isBuildInputPath
            existsAtStop = $null -ne $entry
            sha256 = if ($null -ne $entry) { [string]$entry.sha256 } else { "" }
        })
    }
    $receipt = [pscustomobject]@{
        schemaVersion = 2
        sessionId = $Id
        repositoryRoot = $Root
        startedAtUtc = $StartedAtUtc.ToString("o")
        stoppedAtUtc = [DateTime]::UtcNow.ToString("o")
        paths = @($pathEvidence)
    }
    $receiptRoot = Join-Path (Get-MajoShovelCodexEditSessionRoot $Root) "completed"
    New-Item -ItemType Directory -Force -Path $receiptRoot | Out-Null
    $receiptPath = Join-Path $receiptRoot (([DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss-fff")) + "-" + (Get-MajoShovelSha256HexFromBytes ([System.Text.Encoding]::UTF8.GetBytes($Id))).Substring(0, 16) + ".json")
    Write-MajoShovelJsonFile $receiptPath $receipt
    return $receiptPath
}

$resolvedSessionId = if ($Action -eq "Status" -and [string]::IsNullOrWhiteSpace($SessionId) -and [string]::IsNullOrWhiteSpace($env:CODEX_THREAD_ID)) { "" } else { Resolve-SessionId }

switch ($Action) {
    "Start" {
        $normalizedPaths = @(Get-NormalizedSessionPaths $Path)
        if ($normalizedPaths.Count -eq 0) {
            throw "Start requires at least one repository path."
        }
        $sourceLock = Enter-MajoShovelSourceCoordinationLock $Root "[edit-session]" $WaitForBuildSeconds
        try {
            Assert-NoPathConflicts $resolvedSessionId $normalizedPaths
            $existing = Read-Session $resolvedSessionId
            $startedAtUtc = if ($null -ne $existing) { ([DateTime]$existing.startedAtUtc).ToUniversalTime() } else { [DateTime]::UtcNow }
            $mergedPaths = @($normalizedPaths)
            if ($null -ne $existing) {
                $mergedPaths = @(Get-NormalizedSessionPaths (@($existing.paths) + $normalizedPaths))
            }
            $lease = Write-SessionLease $resolvedSessionId $mergedPaths $startedAtUtc
            $lease | ConvertTo-Json -Depth 5
        }
        finally {
            Exit-MajoShovelMutex $sourceLock
        }
    }
    "Heartbeat" {
        $existing = Read-Session $resolvedSessionId
        if ($null -eq $existing) {
            throw "No active edit session exists for '$resolvedSessionId'. Start it again."
        }
        $lease = Write-SessionLease $resolvedSessionId @($existing.paths) (([DateTime]$existing.startedAtUtc).ToUniversalTime())
        $lease | ConvertTo-Json -Depth 5
    }
    "Stop" {
        $existing = Read-Session $resolvedSessionId
        if ($null -eq $existing) {
            [pscustomobject]@{ stopped = $false; sessionId = $resolvedSessionId; reason = "not-active" } | ConvertTo-Json
            break
        }
        $receiptPath = Publish-SessionReceipt $resolvedSessionId @($existing.paths) (([DateTime]$existing.startedAtUtc).ToUniversalTime())
        Remove-Item -LiteralPath (Get-MajoShovelCodexEditSessionPath $Root $resolvedSessionId) -Force
        [pscustomobject]@{ stopped = $true; sessionId = $resolvedSessionId; receiptPath = $receiptPath; paths = @($existing.paths) } | ConvertTo-Json -Depth 5
    }
    "Status" {
        $sessions = @(Get-MajoShovelCodexEditSessions $Root $true)
        if (-not [string]::IsNullOrWhiteSpace($resolvedSessionId)) {
            $sessions = @($sessions | Where-Object { [string]::Equals([string]$_.sessionId, $resolvedSessionId, [System.StringComparison]::OrdinalIgnoreCase) })
        }
        [pscustomobject]@{ repositoryRoot = $Root; activeCount = $sessions.Count; sessions = @($sessions) } | ConvertTo-Json -Depth 6
    }
}
