param(
    [string]$Root = "",
    [switch]$OnlyMissing
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Root)) {
    $RootPath = Resolve-Path (Join-Path $PSScriptRoot "..")
} else {
    $RootPath = Resolve-Path $Root
}

$AudioRoot = Join-Path $RootPath "assets\audio"
$BgmRoot = Join-Path $AudioRoot "bgm"
$SeRoot = Join-Path $AudioRoot "se"
New-Item -ItemType Directory -Force -Path $BgmRoot, $SeRoot | Out-Null

$SampleRate = 44100
$Channels = 1
$TwoPi = [Math]::PI * 2.0

function Clamp-Sample([double]$Value) {
    if ($Value -gt 0.95) { return 0.95 }
    if ($Value -lt -0.95) { return -0.95 }
    return $Value
}

function Decay([double]$Time, [double]$Duration, [double]$Power) {
    if ($Duration -le 0.0) { return 0.0 }
    $x = 1.0 - ($Time / $Duration)
    if ($x -lt 0.0) { return 0.0 }
    return [Math]::Pow($x, $Power)
}

function Note-Envelope([double]$Time, [double]$Length) {
    if ($Time -lt 0.0 -or $Time -gt $Length) { return 0.0 }
    $attack = 0.025
    $release = 0.12
    if ($Time -lt $attack) {
        return $Time / $attack
    }
    if ($Time -gt $Length - $release) {
        return [Math]::Max(0.0, ($Length - $Time) / $release)
    }
    return 1.0
}

function Placeholder-Sample([string]$Kind, [double]$Time, [double]$Duration, [System.Random]$Random) {
    switch ($Kind) {
        "bgm.title" {
            $bar = $Time % 4.0
            $notes = @(261.63, 329.63, 392.00, 523.25)
            $step = [int]([Math]::Floor($bar * 2.0) % $notes.Length)
            $noteTime = $bar - ([Math]::Floor($bar * 2.0) / 2.0)
            $env = Note-Envelope $noteTime 0.5
            $pad = 0.075 * [Math]::Sin($TwoPi * 130.815 * $Time) +
                0.055 * [Math]::Sin($TwoPi * 196.00 * $Time) +
                0.045 * [Math]::Sin($TwoPi * 261.63 * $Time)
            $lead = 0.14 * $env * [Math]::Sin($TwoPi * $notes[$step] * $Time)
            return $pad + $lead
        }
        "bgm.base" {
            $sway = 0.72 + 0.28 * [Math]::Sin($TwoPi * 0.5 * $Time)
            return $sway * (
                0.090 * [Math]::Sin($TwoPi * 196.00 * $Time) +
                0.070 * [Math]::Sin($TwoPi * 246.94 * $Time) +
                0.050 * [Math]::Sin($TwoPi * 293.66 * $Time))
        }
        "bgm.dungeon" {
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.010
            $pulse = 0.5 + 0.5 * [Math]::Sin($TwoPi * 0.75 * $Time)
            return 0.105 * [Math]::Sin($TwoPi * 110.0 * $Time) +
                0.050 * [Math]::Sin($TwoPi * 146.83 * $Time) +
                0.045 * $pulse * [Math]::Sin($TwoPi * 55.0 * $Time) +
                $noise
        }
        "bgm.boss" {
            $pulse = 0.35
            if ([Math]::Sin($TwoPi * 4.0 * $Time) -ge 0.0) {
                $pulse = 1.0
            }
            return 0.115 * [Math]::Sin($TwoPi * 82.41 * $Time) +
                0.085 * $pulse * [Math]::Sin($TwoPi * 164.81 * $Time) +
                0.040 * [Math]::Sin($TwoPi * 246.94 * $Time)
        }
        "se.ui.confirm" {
            $env = Decay $Time $Duration 1.8
            $freq = 620.0 + 460.0 * ($Time / $Duration)
            return 0.46 * $env * [Math]::Sin($TwoPi * $freq * $Time)
        }
        "se.ui.cancel" {
            $env = Decay $Time $Duration 1.6
            $freq = 460.0 - 210.0 * ($Time / $Duration)
            return 0.44 * $env * [Math]::Sin($TwoPi * $freq * $Time)
        }
        "se.ui.menu_open" {
            $env = Decay $Time $Duration 1.35
            $sweep = 300.0 + 260.0 * ($Time / $Duration)
            return $env * (
                0.26 * [Math]::Sin($TwoPi * $sweep * $Time) +
                0.20 * [Math]::Sin($TwoPi * 660.0 * $Time))
        }
        "se.ui.tab_switch" {
            $env = Decay $Time $Duration 1.9
            $freq = 560.0 + 110.0 * [Math]::Sin($TwoPi * 8.0 * $Time)
            return 0.36 * $env * [Math]::Sin($TwoPi * $freq * $Time)
        }
        "se.ui.book_open" {
            $env = Decay $Time $Duration 1.25
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.10
            return $env * (
                0.18 * [Math]::Sin($TwoPi * 220.0 * $Time) +
                0.12 * [Math]::Sin($TwoPi * 440.0 * $Time) +
                $noise)
        }
        "se.ui.item_move" {
            $env = Decay $Time $Duration 1.7
            return $env * (
                0.24 * [Math]::Sin($TwoPi * 380.0 * $Time) +
                0.18 * [Math]::Sin($TwoPi * 520.0 * $Time))
        }
        "se.ui.item_use" {
            $env = Decay $Time $Duration 1.5
            $spark = 0.5 + 0.5 * [Math]::Sin($TwoPi * 18.0 * $Time)
            return $env * (
                0.24 * [Math]::Sin($TwoPi * 740.0 * $Time) +
                0.18 * $spark * [Math]::Sin($TwoPi * 1110.0 * $Time))
        }
        "se.ui.ring_place" {
            $env = Decay $Time $Duration 1.45
            return $env * (
                0.25 * [Math]::Sin($TwoPi * 520.0 * $Time) +
                0.20 * [Math]::Sin($TwoPi * 780.0 * $Time) +
                0.12 * [Math]::Sin($TwoPi * 1040.0 * $Time))
        }
        "se.ui.upgrade_select" {
            $env = Decay $Time $Duration 1.25
            return $env * (
                0.24 * [Math]::Sin($TwoPi * 660.0 * $Time) +
                0.20 * [Math]::Sin($TwoPi * 990.0 * $Time) +
                0.15 * [Math]::Sin($TwoPi * 1320.0 * $Time))
        }
        "se.transition" {
            $env = Decay $Time $Duration 1.2
            $freq = 180.0 + 420.0 * ($Time / $Duration)
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.035
            return $env * (0.34 * [Math]::Sin($TwoPi * $freq * $Time) + $noise)
        }
        "se.dig.hit" {
            $env = Decay $Time $Duration 2.8
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.16
            return $env * (0.34 * [Math]::Sin($TwoPi * 140.0 * $Time) + $noise)
        }
        "se.dig.break" {
            $env = Decay $Time $Duration 2.0
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.22
            return $env * (0.26 * [Math]::Sin($TwoPi * 95.0 * $Time) + 0.18 * [Math]::Sin($TwoPi * 240.0 * $Time) + $noise)
        }
        "se.attack.hit" {
            $env = Decay $Time $Duration 2.4
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.20
            return $env * (0.40 * [Math]::Sin($TwoPi * 185.0 * $Time) + $noise)
        }
        "se.pickup" {
            $env = Decay $Time $Duration 1.7
            return $env * (
                0.30 * [Math]::Sin($TwoPi * 784.0 * $Time) +
                0.22 * [Math]::Sin($TwoPi * 1174.66 * $Time) +
                0.14 * [Math]::Sin($TwoPi * 1568.0 * $Time))
        }
        "se.boss.spawn" {
            $env = Decay $Time $Duration 0.55
            $rise = 70.0 + 130.0 * ($Time / $Duration)
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.025
            return $env * (0.42 * [Math]::Sin($TwoPi * $rise * $Time) + 0.16 * [Math]::Sin($TwoPi * 36.0 * $Time) + $noise)
        }
        "se.boss.defeat" {
            $env = Decay $Time $Duration 0.8
            $fall = 260.0 - 150.0 * ($Time / $Duration)
            return $env * (
                0.34 * [Math]::Sin($TwoPi * $fall * $Time) +
                0.20 * [Math]::Sin($TwoPi * 196.0 * $Time) +
                0.12 * [Math]::Sin($TwoPi * 392.0 * $Time))
        }
        "se.dig.ore_break" {
            $env = Decay $Time $Duration 1.7
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.20
            return $env * (0.28 * [Math]::Sin($TwoPi * 125.0 * $Time) + 0.22 * [Math]::Sin($TwoPi * 510.0 * $Time) + $noise)
        }
        "se.player.damage" {
            $env = Decay $Time $Duration 1.6
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.12
            return $env * (0.32 * [Math]::Sin($TwoPi * 170.0 * $Time) + 0.18 * [Math]::Sin($TwoPi * 92.0 * $Time) + $noise)
        }
        "se.ring.throw" {
            $env = Decay $Time $Duration 1.25
            $sweep = 260.0 + 520.0 * ($Time / $Duration)
            return $env * (0.30 * [Math]::Sin($TwoPi * $sweep * $Time) + 0.16 * [Math]::Sin($TwoPi * 940.0 * $Time))
        }
        "se.enemy.defeat" {
            $env = Decay $Time $Duration 1.0
            $fall = 360.0 - 240.0 * ($Time / $Duration)
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.10
            return $env * (0.28 * [Math]::Sin($TwoPi * $fall * $Time) + 0.18 * [Math]::Sin($TwoPi * 120.0 * $Time) + $noise)
        }
        "se.enemy.spawn" {
            $env = Decay $Time $Duration 1.15
            $rise = 90.0 + 160.0 * ($Time / $Duration)
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.12
            return $env * (0.24 * [Math]::Sin($TwoPi * $rise * $Time) + $noise)
        }
        "se.enemy.alert" {
            $env = Decay $Time $Duration 1.35
            return $env * (0.30 * [Math]::Sin($TwoPi * 880.0 * $Time) + 0.22 * [Math]::Sin($TwoPi * 1320.0 * $Time))
        }
        "se.enemy.attack" {
            $env = Decay $Time $Duration 1.7
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.16
            return $env * (0.34 * [Math]::Sin($TwoPi * 155.0 * $Time) + $noise)
        }
        "se.enemy.shoot" {
            $env = Decay $Time $Duration 1.8
            $sweep = 480.0 - 180.0 * ($Time / $Duration)
            return $env * (0.28 * [Math]::Sin($TwoPi * $sweep * $Time) + 0.12 * [Math]::Sin($TwoPi * 960.0 * $Time))
        }
        "se.enemy.heal" {
            $env = Decay $Time $Duration 1.15
            return $env * (0.24 * [Math]::Sin($TwoPi * 520.0 * $Time) + 0.20 * [Math]::Sin($TwoPi * 780.0 * $Time) + 0.14 * [Math]::Sin($TwoPi * 1040.0 * $Time))
        }
        "se.projectile.impact" {
            $env = Decay $Time $Duration 2.2
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.18
            return $env * (0.30 * [Math]::Sin($TwoPi * 210.0 * $Time) + $noise)
        }
        "se.ring.guard" {
            $env = Decay $Time $Duration 1.35
            return $env * (0.24 * [Math]::Sin($TwoPi * 360.0 * $Time) + 0.26 * [Math]::Sin($TwoPi * 720.0 * $Time))
        }
        "se.ring.reflect" {
            $env = Decay $Time $Duration 1.1
            $sweep = 560.0 + 520.0 * ($Time / $Duration)
            return $env * (0.26 * [Math]::Sin($TwoPi * $sweep * $Time) + 0.20 * [Math]::Sin($TwoPi * 1180.0 * $Time))
        }
        "se.magic.cast" {
            $env = Decay $Time $Duration 1.2
            $shimmer = 0.5 + 0.5 * [Math]::Sin($TwoPi * 22.0 * $Time)
            return $env * (0.22 * [Math]::Sin($TwoPi * 660.0 * $Time) + 0.18 * $shimmer * [Math]::Sin($TwoPi * 990.0 * $Time))
        }
        "se.magic.impact" {
            $env = Decay $Time $Duration 1.6
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.12
            return $env * (0.28 * [Math]::Sin($TwoPi * 240.0 * $Time) + 0.20 * [Math]::Sin($TwoPi * 720.0 * $Time) + $noise)
        }
        "se.capture.throw" {
            $env = Decay $Time $Duration 1.3
            $sweep = 420.0 + 340.0 * ($Time / $Duration)
            return $env * (0.28 * [Math]::Sin($TwoPi * $sweep * $Time) + 0.12 * [Math]::Sin($TwoPi * 1180.0 * $Time))
        }
        "se.capture.success" {
            $env = Decay $Time $Duration 1.1
            return $env * (0.24 * [Math]::Sin($TwoPi * 660.0 * $Time) + 0.22 * [Math]::Sin($TwoPi * 990.0 * $Time) + 0.18 * [Math]::Sin($TwoPi * 1320.0 * $Time))
        }
        "se.capture.fail" {
            $env = Decay $Time $Duration 1.45
            $fall = 520.0 - 250.0 * ($Time / $Duration)
            return $env * (0.30 * [Math]::Sin($TwoPi * $fall * $Time) + 0.14 * [Math]::Sin($TwoPi * 210.0 * $Time))
        }
        "se.discovery" {
            $env = Decay $Time $Duration 1.1
            return $env * (0.22 * [Math]::Sin($TwoPi * 784.0 * $Time) + 0.18 * [Math]::Sin($TwoPi * 1174.66 * $Time) + 0.14 * [Math]::Sin($TwoPi * 1568.0 * $Time))
        }
        "se.discovery.warp" {
            $env = Decay $Time $Duration 0.95
            $pulse = 0.5 + 0.5 * [Math]::Sin($TwoPi * 8.0 * $Time)
            return $env * (0.24 * [Math]::Sin($TwoPi * 392.0 * $Time) + 0.18 * $pulse * [Math]::Sin($TwoPi * 784.0 * $Time) + 0.16 * [Math]::Sin($TwoPi * 1174.66 * $Time))
        }
        "se.chest.open" {
            $env = Decay $Time $Duration 1.25
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.09
            return $env * (0.22 * [Math]::Sin($TwoPi * 180.0 * $Time) + 0.16 * [Math]::Sin($TwoPi * 430.0 * $Time) + $noise)
        }
        "se.crate.break" {
            $env = Decay $Time $Duration 1.8
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.24
            return $env * (0.24 * [Math]::Sin($TwoPi * 130.0 * $Time) + $noise)
        }
        "se.item.break" {
            $env = Decay $Time $Duration 1.55
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.12
            return $env * (0.22 * [Math]::Sin($TwoPi * 260.0 * $Time) + 0.18 * [Math]::Sin($TwoPi * 620.0 * $Time) + $noise)
        }
        "se.explosion" {
            $env = Decay $Time $Duration 1.1
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.28
            return $env * (0.32 * [Math]::Sin($TwoPi * 78.0 * $Time) + 0.18 * [Math]::Sin($TwoPi * 156.0 * $Time) + $noise)
        }
    }

    return 0.0
}

function Write-PlaceholderWav([string]$Path, [string]$Kind, [double]$DurationSeconds, [int]$Seed) {
    if ($OnlyMissing -and (Test-Path -LiteralPath $Path)) {
        Write-Host "[audio] skip existing $Path"
        return
    }

    $sampleCount = [int][Math]::Round($DurationSeconds * $SampleRate)
    $bitsPerSample = 16
    $bytesPerSample = 2
    $blockAlign = $Channels * $bytesPerSample
    $byteRate = $SampleRate * $blockAlign
    $dataSize = $sampleCount * $blockAlign
    $chunkSize = 36 + $dataSize
    $random = [System.Random]::new($Seed)

    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
    try {
        $writer = [System.IO.BinaryWriter]::new($stream, [System.Text.Encoding]::ASCII, $true)
        try {
            $writer.Write([System.Text.Encoding]::ASCII.GetBytes("RIFF"))
            $writer.Write([int]$chunkSize)
            $writer.Write([System.Text.Encoding]::ASCII.GetBytes("WAVE"))
            $writer.Write([System.Text.Encoding]::ASCII.GetBytes("fmt "))
            $writer.Write([int]16)
            $writer.Write([int16]1)
            $writer.Write([int16]$Channels)
            $writer.Write([int]$SampleRate)
            $writer.Write([int]$byteRate)
            $writer.Write([int16]$blockAlign)
            $writer.Write([int16]$bitsPerSample)
            $writer.Write([System.Text.Encoding]::ASCII.GetBytes("data"))
            $writer.Write([int]$dataSize)

            for ($i = 0; $i -lt $sampleCount; ++$i) {
                $time = [double]$i / [double]$SampleRate
                $sample = Clamp-Sample (Placeholder-Sample $Kind $time $DurationSeconds $random)
                $pcm = [int][Math]::Round($sample * 32767.0)
                if ($pcm -gt 32767) { $pcm = 32767 }
                if ($pcm -lt -32768) { $pcm = -32768 }
                $writer.Write([int16]$pcm)
            }
        } finally {
            $writer.Dispose()
        }
    } finally {
        $stream.Dispose()
    }

    Write-Host "[audio] wrote $Path"
}

$clips = @(
    @{ Path = Join-Path $BgmRoot "title_placeholder.wav"; Kind = "bgm.title"; Duration = 4.0; Seed = 1001 },
    @{ Path = Join-Path $BgmRoot "base_placeholder.wav"; Kind = "bgm.base"; Duration = 4.0; Seed = 1002 },
    @{ Path = Join-Path $BgmRoot "dungeon_placeholder.wav"; Kind = "bgm.dungeon"; Duration = 4.0; Seed = 1003 },
    @{ Path = Join-Path $BgmRoot "boss_placeholder.wav"; Kind = "bgm.boss"; Duration = 4.0; Seed = 1004 },
    @{ Path = Join-Path $SeRoot "ui_confirm_placeholder.wav"; Kind = "se.ui.confirm"; Duration = 0.16; Seed = 2001 },
    @{ Path = Join-Path $SeRoot "ui_cancel_placeholder.wav"; Kind = "se.ui.cancel"; Duration = 0.16; Seed = 2002 },
    @{ Path = Join-Path $SeRoot "ui_menu_open_placeholder.wav"; Kind = "se.ui.menu_open"; Duration = 0.18; Seed = 2003 },
    @{ Path = Join-Path $SeRoot "ui_tab_switch_placeholder.wav"; Kind = "se.ui.tab_switch"; Duration = 0.12; Seed = 2004 },
    @{ Path = Join-Path $SeRoot "ui_book_open_placeholder.wav"; Kind = "se.ui.book_open"; Duration = 0.28; Seed = 2005 },
    @{ Path = Join-Path $SeRoot "ui_item_move_placeholder.wav"; Kind = "se.ui.item_move"; Duration = 0.14; Seed = 2006 },
    @{ Path = Join-Path $SeRoot "ui_item_use_placeholder.wav"; Kind = "se.ui.item_use"; Duration = 0.20; Seed = 2007 },
    @{ Path = Join-Path $SeRoot "ui_ring_place_placeholder.wav"; Kind = "se.ui.ring_place"; Duration = 0.22; Seed = 2008 },
    @{ Path = Join-Path $SeRoot "ui_upgrade_select_placeholder.wav"; Kind = "se.ui.upgrade_select"; Duration = 0.30; Seed = 2009 },
    @{ Path = Join-Path $SeRoot "transition_placeholder.wav"; Kind = "se.transition"; Duration = 0.42; Seed = 2010 },
    @{ Path = Join-Path $SeRoot "dig_hit_placeholder.wav"; Kind = "se.dig.hit"; Duration = 0.12; Seed = 2011 },
    @{ Path = Join-Path $SeRoot "dig_break_placeholder.wav"; Kind = "se.dig.break"; Duration = 0.22; Seed = 2012 },
    @{ Path = Join-Path $SeRoot "attack_hit_placeholder.wav"; Kind = "se.attack.hit"; Duration = 0.15; Seed = 2013 },
    @{ Path = Join-Path $SeRoot "pickup_placeholder.wav"; Kind = "se.pickup"; Duration = 0.24; Seed = 2014 },
    @{ Path = Join-Path $SeRoot "boss_spawn_placeholder.wav"; Kind = "se.boss.spawn"; Duration = 0.78; Seed = 2015 },
    @{ Path = Join-Path $SeRoot "boss_defeat_placeholder.wav"; Kind = "se.boss.defeat"; Duration = 1.05; Seed = 2016 },
    @{ Path = Join-Path $SeRoot "dig_ore_break_placeholder.wav"; Kind = "se.dig.ore_break"; Duration = 0.24; Seed = 2017 },
    @{ Path = Join-Path $SeRoot "player_damage_placeholder.wav"; Kind = "se.player.damage"; Duration = 0.22; Seed = 2018 },
    @{ Path = Join-Path $SeRoot "ring_throw_placeholder.wav"; Kind = "se.ring.throw"; Duration = 0.24; Seed = 2019 },
    @{ Path = Join-Path $SeRoot "enemy_defeat_placeholder.wav"; Kind = "se.enemy.defeat"; Duration = 0.28; Seed = 2020 },
    @{ Path = Join-Path $SeRoot "enemy_spawn_placeholder.wav"; Kind = "se.enemy.spawn"; Duration = 0.32; Seed = 2021 },
    @{ Path = Join-Path $SeRoot "enemy_alert_placeholder.wav"; Kind = "se.enemy.alert"; Duration = 0.18; Seed = 2022 },
    @{ Path = Join-Path $SeRoot "enemy_attack_placeholder.wav"; Kind = "se.enemy.attack"; Duration = 0.18; Seed = 2023 },
    @{ Path = Join-Path $SeRoot "enemy_shoot_placeholder.wav"; Kind = "se.enemy.shoot"; Duration = 0.14; Seed = 2024 },
    @{ Path = Join-Path $SeRoot "enemy_heal_placeholder.wav"; Kind = "se.enemy.heal"; Duration = 0.26; Seed = 2025 },
    @{ Path = Join-Path $SeRoot "projectile_impact_placeholder.wav"; Kind = "se.projectile.impact"; Duration = 0.16; Seed = 2026 },
    @{ Path = Join-Path $SeRoot "ring_guard_placeholder.wav"; Kind = "se.ring.guard"; Duration = 0.18; Seed = 2027 },
    @{ Path = Join-Path $SeRoot "ring_reflect_placeholder.wav"; Kind = "se.ring.reflect"; Duration = 0.22; Seed = 2028 },
    @{ Path = Join-Path $SeRoot "magic_cast_placeholder.wav"; Kind = "se.magic.cast"; Duration = 0.20; Seed = 2029 },
    @{ Path = Join-Path $SeRoot "magic_impact_placeholder.wav"; Kind = "se.magic.impact"; Duration = 0.22; Seed = 2030 },
    @{ Path = Join-Path $SeRoot "capture_throw_placeholder.wav"; Kind = "se.capture.throw"; Duration = 0.18; Seed = 2031 },
    @{ Path = Join-Path $SeRoot "capture_success_placeholder.wav"; Kind = "se.capture.success"; Duration = 0.32; Seed = 2032 },
    @{ Path = Join-Path $SeRoot "capture_fail_placeholder.wav"; Kind = "se.capture.fail"; Duration = 0.24; Seed = 2033 },
    @{ Path = Join-Path $SeRoot "discovery_placeholder.wav"; Kind = "se.discovery"; Duration = 0.30; Seed = 2034 },
    @{ Path = Join-Path $SeRoot "discovery_warp_placeholder.wav"; Kind = "se.discovery.warp"; Duration = 0.58; Seed = 2035 },
    @{ Path = Join-Path $SeRoot "chest_open_placeholder.wav"; Kind = "se.chest.open"; Duration = 0.32; Seed = 2036 },
    @{ Path = Join-Path $SeRoot "crate_break_placeholder.wav"; Kind = "se.crate.break"; Duration = 0.24; Seed = 2037 },
    @{ Path = Join-Path $SeRoot "item_break_placeholder.wav"; Kind = "se.item.break"; Duration = 0.28; Seed = 2038 },
    @{ Path = Join-Path $SeRoot "explosion_placeholder.wav"; Kind = "se.explosion"; Duration = 0.36; Seed = 2039 }
)

foreach ($clip in $clips) {
    Write-PlaceholderWav $clip.Path $clip.Kind $clip.Duration $clip.Seed
}
