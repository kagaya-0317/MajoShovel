param(
    [string]$ConfigPath = "",
    [string]$OutputDir = "",
    [double]$DefaultTargetHits = 5.0,
    [int]$ExpectedAttackBonus = 0,
    [int]$TopCount = 30
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
    $ConfigPath = Join-Path $Root "data\google_sheet_source.cfg"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $Root "docs\enemy_hp_balance"
}

function Write-Utf8BomFile([string]$Path, [string]$Text) {
    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    $encoding = [System.Text.UTF8Encoding]::new($true)
    [System.IO.File]::WriteAllText($Path, $Text, $encoding)
}

function Read-KeyValueFile([string]$Path) {
    if (-not (Test-Path $Path)) {
        throw "Config file not found: $Path"
    }
    $result = @{}
    foreach ($line in Get-Content -LiteralPath $Path -Encoding UTF8) {
        $trimmed = $line.Trim()
        if ($trimmed.Length -eq 0 -or $trimmed.StartsWith("#")) {
            continue
        }
        $separator = $trimmed.IndexOf("=")
        if ($separator -lt 0) {
            continue
        }
        $key = $trimmed.Substring(0, $separator).Trim()
        $value = $trimmed.Substring($separator + 1).Trim()
        if ($key.Length -gt 0) {
            $result[$key] = $value
        }
    }
    return $result
}

function Remove-Utf8Bom([string]$Text) {
    if ($Text.Length -gt 0 -and [int][char]$Text[0] -eq 0xFEFF) {
        return $Text.Substring(1)
    }
    return $Text
}

function Get-GoogleSheetRows([string]$SpreadsheetId, [string]$SheetName) {
    $escapedSheet = [Uri]::EscapeDataString($SheetName)
    $url = "https://docs.google.com/spreadsheets/d/$SpreadsheetId/gviz/tq?tqx=out:csv&sheet=$escapedSheet"
    $response = Invoke-WebRequest -UseBasicParsing -Uri $url -TimeoutSec 30
    $csv = Remove-Utf8Bom ([string]$response.Content)
    return $csv -split "`r?`n" | ConvertFrom-Csv
}

function Get-Cell($Row, [string[]]$Names) {
    foreach ($name in $Names) {
        $property = $Row.PSObject.Properties[$name]
        if ($null -ne $property) {
            return [string]$property.Value
        }
    }
    return ""
}

function Get-Number([string]$Text, [double]$Default = 0.0) {
    if ([string]::IsNullOrWhiteSpace($Text)) {
        return $Default
    }
    $value = 0.0
    $styles = [System.Globalization.NumberStyles]::Float
    $culture = [System.Globalization.CultureInfo]::InvariantCulture
    if ([double]::TryParse($Text.Trim(), $styles, $culture, [ref]$value)) {
        return $value
    }
    return $Default
}

function Get-StageSpecs {
    return @(
        [pscustomobject]@{ StageId = "stage_01_stardust"; Prefix = "ST"; Name = "星くずのダンジョン"; MaxDepth = 3; Levels = @(1, 2, 4) },
        [pscustomobject]@{ StageId = "stage_02_junk_magic"; Prefix = "JK"; Name = "ジャンク魔窟"; MaxDepth = 3; Levels = @(5, 7, 9) },
        [pscustomobject]@{ StageId = "stage_03_star_core"; Prefix = "SC"; Name = "星核洞"; MaxDepth = 3; Levels = @(10, 13, 16) },
        [pscustomobject]@{ StageId = "stage_04_astral_mine"; Prefix = "AS"; Name = "不可思議の迷宮"; MaxDepth = 9; Levels = @(3, 5, 7, 9, 11, 13, 15, 17, 20) }
    )
}

function Get-EstimatedLevel($Stage, [int]$Depth) {
    $index = [Math]::Max(0, [Math]::Min($Stage.Levels.Count - 1, $Depth - 1))
    return [int]$Stage.Levels[$index]
}

function Get-DamageTypeMultiplier([string]$DamageType) {
    switch ($DamageType) {
        { $_ -eq "fire" -or $_ -eq "thunder" -or $_ -eq "magic" } { return 1.10 }
        "earth" { return 1.05 }
        { $_ -eq "ice" -or $_ -eq "water" } { return 0.95 }
        default { return 1.0 }
    }
}

function Get-ChestKindWeights([int]$Depth, [int]$MaxDepth) {
    $progress = ([double]$Depth - 0.5) / [double]$MaxDepth
    $rareWeight = 18 + [int][Math]::Truncate($progress * 10.0)
    $superRareWeight = 4 + [int][Math]::Truncate($progress * 4.0)
    $commonWeight = [Math]::Max(1, 100 - $rareWeight - $superRareWeight)
    $totalWeight = $commonWeight + $rareWeight + $superRareWeight

    return @(
        [pscustomobject]@{ Code = "C"; Probability = [double]$commonWeight / $totalWeight; AverageRolls = 1.5 },
        [pscustomobject]@{ Code = "R"; Probability = [double]$rareWeight / $totalWeight; AverageRolls = 2.0 },
        [pscustomobject]@{ Code = "S"; Probability = [double]$superRareWeight / $totalWeight; AverageRolls = 2.5 }
    )
}

function Get-WeightedQuantile([System.Collections.IEnumerable]$Samples, [double]$Quantile) {
    $items = @($Samples | ForEach-Object { $_ })
    if ($items.Count -eq 0) {
        return 0.0
    }
    $totalWeight = ($items | Measure-Object -Property Weight -Sum).Sum
    if ($totalWeight -le 0.0) {
        return 0.0
    }
    $threshold = $totalWeight * $Quantile
    $running = 0.0
    foreach ($sample in ($items | Sort-Object Value)) {
        $running += $sample.Weight
        if ($running -ge $threshold) {
            return [double]$sample.Value
        }
    }
    return [double]($items | Sort-Object Value | Select-Object -Last 1).Value
}

function Round-NiceHp([double]$Value) {
    $clamped = [Math]::Max(1.0, $Value)
    if ($clamped -lt 30.0) {
        return [int][Math]::Round($clamped)
    }
    if ($clamped -lt 100.0) {
        return [int]([Math]::Round($clamped / 5.0) * 5.0)
    }
    return [int]([Math]::Round($clamped / 10.0) * 10.0)
}

function Get-EnemyTags($Enemy) {
    $tagText = Get-Cell $Enemy @("敵特殊タグ", "enemy_tags", "tags")
    return $tagText -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_.Length -gt 0 }
}

function Test-HasTag([string[]]$Tags, [string[]]$Wanted) {
    foreach ($tag in $Wanted) {
        if ($Tags -contains $tag) {
            return $true
        }
    }
    return $false
}

function Get-EnemyTargetHits($Enemy, [double]$DefaultHits) {
    $tags = @(Get-EnemyTags $Enemy)
    $behavior = Get-Cell $Enemy @("敵挙動コード", "enemy_behavior_code")
    $ai = Get-Cell $Enemy @("敵AI", "enemy_ai")
    $hits = $DefaultHits
    $notes = New-Object System.Collections.Generic.List[string]

    if (Test-HasTag $tags @("boss", "boss_only")) {
        return [pscustomobject]@{ Hits = 45.0; Notes = "boss" }
    }

    if (Test-HasTag $tags @("small")) {
        $hits -= 0.5
        $notes.Add("small")
    }
    if (Test-HasTag $tags @("swarm")) {
        $hits -= 1.0
        $notes.Add("swarm")
    }
    if (Test-HasTag $tags @("soft")) {
        $hits -= 0.3
        $notes.Add("soft")
    }
    if (Test-HasTag $tags @("flying", "hover", "floating", "airborne")) {
        $hits -= 0.3
        $notes.Add("mobile")
    }
    if ($behavior -match "shoot|projectile|ranged" -or $ai -match "ranged") {
        $hits -= 0.4
        $notes.Add("ranged")
    }
    if (Test-HasTag $tags @("support")) {
        $hits -= 0.4
        $notes.Add("support")
    }
    if (Test-HasTag $tags @("shield")) {
        $hits += 2.0
        $notes.Add("shield")
    }
    if (Test-HasTag $tags @("heavy")) {
        $hits += 1.0
        $notes.Add("heavy")
    }
    if (Test-HasTag $tags @("hard", "rugged", "metal", "rock")) {
        $hits += 0.8
        $notes.Add("hard")
    }
    if ($behavior -match "front_guard|physical_resist|magic_body") {
        $hits += 0.8
        $notes.Add("resist")
    }

    $hits = [Math]::Max(3.0, [Math]::Min(9.0, $hits))
    $noteText = if ($notes.Count -gt 0) { [string]::Join("+", $notes) } else { "normal" }
    return [pscustomobject]@{ Hits = $hits; Notes = $noteText }
}

function Get-AttackDistribution($Objects, $Stage, [int]$Depth, [int]$AttackBonus) {
    $samples = New-Object System.Collections.Generic.List[object]
    $topCandidates = New-Object System.Collections.Generic.List[object]
    foreach ($chest in Get-ChestKindWeights $Depth $Stage.MaxDepth) {
        $column = "$($Stage.Prefix)_$($chest.Code)$Depth"
        $sourceWeight = [double]$chest.Probability * [double]$chest.AverageRolls
        foreach ($object in $Objects) {
            $weight = Get-Number (Get-Cell $object @($column)) 0.0
            if ($weight -lt 1.0) {
                continue
            }
            $attackPower = [int](Get-Number (Get-Cell $object @("攻撃力", "attack_power", "attackPower", "attack", "power")) 0.0)
            $damageType = (Get-Cell $object @("ダメージ種別", "damage_type", "damageType")).Trim()
            if ($attackPower -le 0 -or $damageType.Length -eq 0 -or $damageType -eq "none") {
                continue
            }
            $effectiveAttack = [Math]::Max(0, $attackPower + $AttackBonus)
            $damage = [int][Math]::Ceiling([double]$effectiveAttack * (Get-DamageTypeMultiplier $damageType))
            $combinedWeight = $weight * $sourceWeight
            $name = Get-Cell $object @("名前", "name", "ID", "id")
            $samples.Add([pscustomobject]@{ Value = $damage; Weight = $combinedWeight }) | Out-Null
            $topCandidates.Add([pscustomobject]@{
                Name = $name
                Damage = $damage
                Weight = $combinedWeight
                Chest = $chest.Code
            }) | Out-Null
        }
    }

    if ($samples.Count -eq 0) {
        return $null
    }

    $totalWeight = ($samples | Measure-Object -Property Weight -Sum).Sum
    $mean = ($samples | ForEach-Object { $_.Value * $_.Weight } | Measure-Object -Sum).Sum / $totalWeight
    $top = $topCandidates | Sort-Object Weight -Descending | Select-Object -First 5
    return [pscustomobject]@{
        StageId = $Stage.StageId
        StageName = $Stage.Name
        Prefix = $Stage.Prefix
        Depth = $Depth
        EstimatedLevel = Get-EstimatedLevel $Stage $Depth
        Mean = $mean
        P50 = Get-WeightedQuantile -Samples $samples -Quantile 0.50
        P75 = Get-WeightedQuantile -Samples $samples -Quantile 0.75
        P85 = Get-WeightedQuantile -Samples $samples -Quantile 0.85
        TotalWeight = $totalWeight
        TopItems = $top
    }
}

function Get-EnemySpawnSummary($Enemies, $Stage, [int]$Depth) {
    $column = "$($Stage.Prefix)_E$Depth"
    $samples = New-Object System.Collections.Generic.List[object]
    $names = New-Object System.Collections.Generic.List[string]
    foreach ($enemy in $Enemies) {
        $weight = Get-Number (Get-Cell $enemy @($column)) 0.0
        if ($weight -le 0.0) {
            continue
        }
        $hp = [int](Get-Number (Get-Cell $enemy @("HP", "hp")) 1.0)
        $name = Get-Cell $enemy @("名前", "name", "ID", "id")
        $samples.Add([pscustomobject]@{ Value = $hp; Weight = $weight }) | Out-Null
        if ($names.Count -lt 6) {
            $names.Add($name) | Out-Null
        }
    }
    if ($samples.Count -eq 0) {
        return $null
    }
    $totalWeight = ($samples | Measure-Object -Property Weight -Sum).Sum
    $mean = ($samples | ForEach-Object { $_.Value * $_.Weight } | Measure-Object -Sum).Sum / $totalWeight
    return [pscustomobject]@{
        MeanHp = $mean
        P50Hp = Get-WeightedQuantile -Samples $samples -Quantile 0.50
        TotalWeight = $totalWeight
        ExampleEnemies = [string]::Join(" / ", $names)
    }
}

function ConvertTo-CsvText([System.Collections.IEnumerable]$Rows) {
    $items = @($Rows | ForEach-Object { $_ })
    if ($items.Count -eq 0) {
        return ""
    }
    return ($items | ConvertTo-Csv -NoTypeInformation) -join "`r`n"
}

function Escape-MarkdownCell([string]$Text) {
    return ($Text -replace "\|", "\/")
}

$config = Read-KeyValueFile $ConfigPath
if (-not $config.ContainsKey("spreadsheet_id")) {
    throw "spreadsheet_id is missing in $ConfigPath"
}
$spreadsheetId = $config["spreadsheet_id"]
$objectsSheet = if ($config.ContainsKey("objects_sheet")) { $config["objects_sheet"] } else { "Objects" }
$enemiesSheet = if ($config.ContainsKey("enemies_sheet")) { $config["enemies_sheet"] } else { "Enemies" }

Write-Host "[enemy-hp] loading Objects sheet: $objectsSheet"
$objects = @(Get-GoogleSheetRows $spreadsheetId $objectsSheet)
Write-Host "[enemy-hp] loading Enemies sheet: $enemiesSheet"
$enemies = @(Get-GoogleSheetRows $spreadsheetId $enemiesSheet)

$stages = @(Get-StageSpecs)
$attackByKey = @{}
$stageRows = New-Object System.Collections.Generic.List[object]

foreach ($stage in $stages) {
    for ($depth = 1; $depth -le $stage.MaxDepth; ++$depth) {
        $attack = Get-AttackDistribution $objects $stage $depth $ExpectedAttackBonus
        $spawn = Get-EnemySpawnSummary $enemies $stage $depth
        if ($null -eq $attack -or $null -eq $spawn) {
            continue
        }
        $key = "$($stage.Prefix):$depth"
        $attackByKey[$key] = $attack
        $normalHp = Round-NiceHp ($attack.P50 * $DefaultTargetHits)
        $stageRows.Add([pscustomobject]@{
            stage_id = $stage.StageId
            stage = $stage.Name
            depth = $depth
            estimated_player_level = $attack.EstimatedLevel
            damage_p50 = [int]$attack.P50
            damage_mean = [Math]::Round($attack.Mean, 2)
            damage_p75 = [int]$attack.P75
            normal_enemy_hp_for_5_hits = $normalHp
            current_enemy_hp_weighted_avg = [Math]::Round($spawn.MeanHp, 2)
            current_hits_vs_p50 = [Math]::Round($spawn.MeanHp / [Math]::Max(1.0, $attack.P50), 2)
            example_enemies = $spawn.ExampleEnemies
            top_weighted_attack_items = [string]::Join(" / ", @($attack.TopItems | ForEach-Object { "$($_.Name):$($_.Damage)" }))
        }) | Out-Null
    }
}

$enemyRows = New-Object System.Collections.Generic.List[object]
foreach ($enemy in $enemies) {
    $enemyId = Get-Cell $enemy @("ID", "id")
    if ([string]::IsNullOrWhiteSpace($enemyId)) {
        continue
    }

    $currentHp = [int](Get-Number (Get-Cell $enemy @("HP", "hp")) 1.0)
    $weightedDamage = 0.0
    $weightedLevel = 0.0
    $totalWeight = 0.0
    $dominant = $null
    $appearances = New-Object System.Collections.Generic.List[string]

    foreach ($stage in $stages) {
        for ($depth = 1; $depth -le $stage.MaxDepth; ++$depth) {
            $column = "$($stage.Prefix)_E$depth"
            $spawnWeight = Get-Number (Get-Cell $enemy @($column)) 0.0
            $key = "$($stage.Prefix):$depth"
            if ($spawnWeight -le 0.0 -or -not $attackByKey.ContainsKey($key)) {
                continue
            }
            $attack = $attackByKey[$key]
            $weightedDamage += $spawnWeight * $attack.P50
            $weightedLevel += $spawnWeight * $attack.EstimatedLevel
            $totalWeight += $spawnWeight
            $appearances.Add("$($stage.Prefix)$depth") | Out-Null
            if ($null -eq $dominant -or $spawnWeight -gt $dominant.Weight) {
                $dominant = [pscustomobject]@{
                    StageDepth = "$($stage.Prefix)$depth"
                    Stage = $stage.Name
                    Depth = $depth
                    Weight = $spawnWeight
                    DamageP50 = $attack.P50
                    Level = $attack.EstimatedLevel
                }
            }
        }
    }

    if ($totalWeight -le 0.0) {
        continue
    }

    $target = Get-EnemyTargetHits $enemy $DefaultTargetHits
    $baseDamage = $weightedDamage / $totalWeight
    $recommendedHp = Round-NiceHp ($baseDamage * $target.Hits)
    $name = Get-Cell $enemy @("名前", "name", "ID", "id")

    $enemyRows.Add([pscustomobject]@{
        enemy_id = $enemyId
        name = $name
        current_hp = $currentHp
        recommended_hp = $recommendedHp
        hp_ratio = [Math]::Round($recommendedHp / [Math]::Max(1.0, $currentHp), 2)
        current_hits = [Math]::Round($currentHp / [Math]::Max(1.0, $baseDamage), 2)
        target_hits = [Math]::Round($target.Hits, 2)
        base_damage_p50_weighted = [Math]::Round($baseDamage, 2)
        estimated_player_level_weighted = [Math]::Round($weightedLevel / $totalWeight, 1)
        dominant_stage_depth = if ($null -ne $dominant) { $dominant.StageDepth } else { "" }
        dominant_stage = if ($null -ne $dominant) { $dominant.Stage } else { "" }
        appearances = [string]::Join(" ", $appearances)
        role_notes = $target.Notes
        tags = [string]::Join(",", @(Get-EnemyTags $enemy))
    }) | Out-Null
}

$sortedEnemyRows = @($enemyRows | Sort-Object hp_ratio -Descending)
$stageCsvPath = Join-Path $OutputDir "stage_depth_hp_baseline.csv"
$enemyCsvPath = Join-Path $OutputDir "enemy_hp_recommendations.csv"
$reportPath = Join-Path $OutputDir "enemy_hp_balance_report.md"

Write-Utf8BomFile $stageCsvPath (ConvertTo-CsvText $stageRows)
Write-Utf8BomFile $enemyCsvPath (ConvertTo-CsvText $sortedEnemyRows)

$report = New-Object System.Collections.Generic.List[string]
$report.Add("# Enemy HP Balance Report") | Out-Null
$report.Add("") | Out-Null
$report.Add("Generated: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")") | Out-Null
$report.Add("") | Out-Null
$report.Add("Source: Google Sheet Objects=`"$objectsSheet`", Enemies=`"$enemiesSheet`"") | Out-Null
$report.Add("") | Out-Null
$report.Add("Policy: stage/depth item attack uses weighted median of combat-capable loot. Recommended HP is base damage * target hits, rounded to readable HP values. Estimated player level is a tuning label and is not used as a direct damage multiplier.") | Out-Null
$report.Add("") | Out-Null
$report.Add("## Stage Depth Baseline") | Out-Null
$report.Add("") | Out-Null
$report.Add("| Stage | Depth | Lv | Damage p50 | Damage mean | Damage p75 | Normal HP | Current avg HP | Current hits | Example enemies |") | Out-Null
$report.Add("|---|---:|---:|---:|---:|---:|---:|---:|---:|---|") | Out-Null
foreach ($row in $stageRows) {
    $report.Add("| $(Escape-MarkdownCell $row.stage) | $($row.depth) | $($row.estimated_player_level) | $($row.damage_p50) | $($row.damage_mean) | $($row.damage_p75) | $($row.normal_enemy_hp_for_5_hits) | $($row.current_enemy_hp_weighted_avg) | $($row.current_hits_vs_p50) | $(Escape-MarkdownCell $row.example_enemies) |") | Out-Null
}

$report.Add("") | Out-Null
$report.Add("## Largest HP Gaps") | Out-Null
$report.Add("") | Out-Null
$report.Add("| Enemy | Current HP | Recommended HP | Ratio | Current hits | Target hits | Base damage | Dominant | Notes |") | Out-Null
$report.Add("|---|---:|---:|---:|---:|---:|---:|---|---|") | Out-Null
foreach ($row in ($sortedEnemyRows | Select-Object -First $TopCount)) {
    $report.Add("| $(Escape-MarkdownCell $row.name) | $($row.current_hp) | $($row.recommended_hp) | $($row.hp_ratio) | $($row.current_hits) | $($row.target_hits) | $($row.base_damage_p50_weighted) | $($row.dominant_stage_depth) | $(Escape-MarkdownCell $row.role_notes) |") | Out-Null
}

$report.Add("") | Out-Null
$report.Add("## Output Files") | Out-Null
$report.Add("") | Out-Null
$report.Add("- stage_depth_hp_baseline.csv") | Out-Null
$report.Add("- enemy_hp_recommendations.csv") | Out-Null
$report.Add("") | Out-Null
$report.Add("Notes: Boss HP is included for visibility, but boss tuning should still be checked by fight duration and phase design rather than this normal-enemy hit-count formula.") | Out-Null

Write-Utf8BomFile $reportPath ([string]::Join("`r`n", $report))

Write-Host "[enemy-hp] wrote $stageCsvPath"
Write-Host "[enemy-hp] wrote $enemyCsvPath"
Write-Host "[enemy-hp] wrote $reportPath"
