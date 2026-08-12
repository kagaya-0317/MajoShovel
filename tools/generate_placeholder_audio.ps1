param(
    [string]$Root = "",
    [switch]$OnlyMissing,
    [switch]$OnlySe,
    [switch]$HighQualitySe
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
        "se.facility.forge_upgrade" {
            $env = Decay $Time $Duration 1.15
            $hammer = [Math]::Max(0.0, 1.0 - $Time / 0.13)
            $chime = [Math]::Max(0.0, 1.0 - [Math]::Max(0.0, $Time - 0.14) / 0.48)
            return $hammer * (0.34 * [Math]::Sin($TwoPi * 118.0 * $Time) + 0.24 * [Math]::Sin($TwoPi * 472.0 * $Time)) +
                $env * ($Random.NextDouble() * 2.0 - 1.0) * 0.08 +
                $chime * (0.18 * [Math]::Sin($TwoPi * 784.0 * $Time) + 0.12 * [Math]::Sin($TwoPi * 1176.0 * $Time))
        }
        "se.facility.workbench_upgrade" {
            $env = Decay $Time $Duration 1.35
            $tick = 0.45 + 0.55 * [Math]::Sin($TwoPi * 22.0 * $Time)
            return $env * (0.22 * [Math]::Sin($TwoPi * (420.0 + 520.0 * $Time / $Duration) * $Time) +
                0.18 * $tick * [Math]::Sin($TwoPi * 1260.0 * $Time) +
                0.07 * ($Random.NextDouble() * 2.0 - 1.0))
        }
        "se.facility.workbench_repair" {
            $env = Decay $Time $Duration 1.15
            $ratchet = [Math]::Max(0.0, [Math]::Sin($TwoPi * 15.0 * $Time))
            return $env * (0.18 * $ratchet * [Math]::Sin($TwoPi * 680.0 * $Time) +
                0.13 * [Math]::Sin($TwoPi * 330.0 * $Time) +
                0.12 * [Math]::Sin($TwoPi * 660.0 * $Time))
        }
        "se.merchant.transaction" {
            $env = Decay $Time $Duration 1.55
            $second = if ($Time -ge 0.09) { Decay ($Time - 0.09) ($Duration - 0.09) 1.8 } else { 0.0 }
            return $env * (0.23 * [Math]::Sin($TwoPi * 1320.0 * $Time) + 0.16 * [Math]::Sin($TwoPi * 1980.0 * $Time)) +
                $second * (0.20 * [Math]::Sin($TwoPi * 1568.0 * $Time) + 0.13 * [Math]::Sin($TwoPi * 2352.0 * $Time))
        }
        "se.dialogue.advance" {
            $env = Decay $Time $Duration 1.85
            $freq = 520.0 + 180.0 * ($Time / $Duration)
            return $env * (
                0.30 * [Math]::Sin($TwoPi * $freq * $Time) +
                0.14 * [Math]::Sin($TwoPi * 1040.0 * $Time))
        }
        "se.level_up.jingle" {
            $notes = @(523.25, 659.25, 783.99, 1046.50)
            $stepLength = 0.18
            $step = [Math]::Min($notes.Length - 1, [int][Math]::Floor($Time / $stepLength))
            $noteTime = $Time - ($step * $stepLength)
            $env = Note-Envelope $noteTime 0.26
            $tail = Decay $Time $Duration 0.70
            $spark = 0.5 + 0.5 * [Math]::Sin($TwoPi * 19.0 * $Time)
            return 0.34 * $env * [Math]::Sin($TwoPi * $notes[$step] * $Time) +
                0.18 * $tail * [Math]::Sin($TwoPi * 1568.0 * $Time) +
                0.12 * $spark * $tail * [Math]::Sin($TwoPi * 2093.0 * $Time)
        }
        "se.game_over.jingle" {
            $notes = @(392.00, 329.63, 261.63, 196.00)
            $stepLength = 0.24
            $step = [Math]::Min($notes.Length - 1, [int][Math]::Floor($Time / $stepLength))
            $noteTime = $Time - ($step * $stepLength)
            $env = Note-Envelope $noteTime 0.34
            $tail = Decay $Time $Duration 0.58
            $rumble = ($Random.NextDouble() * 2.0 - 1.0) * 0.025
            return 0.30 * $env * [Math]::Sin($TwoPi * $notes[$step] * $Time) +
                0.16 * $tail * [Math]::Sin($TwoPi * 98.0 * $Time) +
                0.08 * $tail * [Math]::Sin($TwoPi * 147.0 * $Time) +
                $tail * $rumble
        }
        "se.item.new.jingle" {
            $notes = @(659.25, 880.00, 1174.66)
            $stepLength = 0.16
            $step = [Math]::Min($notes.Length - 1, [int][Math]::Floor($Time / $stepLength))
            $noteTime = $Time - ($step * $stepLength)
            $env = Note-Envelope $noteTime 0.24
            $tail = Decay $Time $Duration 0.74
            $spark = 0.5 + 0.5 * [Math]::Sin($TwoPi * 23.0 * $Time)
            return 0.30 * $env * [Math]::Sin($TwoPi * $notes[$step] * $Time) +
                0.16 * $tail * [Math]::Sin($TwoPi * 1760.0 * $Time) +
                0.10 * $spark * $tail * [Math]::Sin($TwoPi * 2349.32 * $Time)
        }
        "se.transition" {
            $env = Decay $Time $Duration 1.2
            $freq = 180.0 + 420.0 * ($Time / $Duration)
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.035
            return $env * (0.34 * [Math]::Sin($TwoPi * $freq * $Time) + $noise)
        }
        "se.footstep.base_outdoor" {
            $env = Decay $Time $Duration 2.2
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.080
            $scrape = 0.5 + 0.5 * [Math]::Sin($TwoPi * 19.0 * $Time)
            return $env * (
                0.18 * [Math]::Sin($TwoPi * 92.0 * $Time) +
                0.10 * [Math]::Sin($TwoPi * 184.0 * $Time) +
                $scrape * $noise)
        }
        "se.footstep.home" {
            $env = Decay $Time $Duration 2.0
            $wood = 0.28 * [Math]::Sin($TwoPi * 245.0 * $Time) +
                0.12 * [Math]::Sin($TwoPi * 490.0 * $Time)
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.035
            return $env * ($wood + $noise)
        }
        "se.footstep.dungeon" {
            $env = Decay $Time $Duration 2.45
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.125
            $grit = 0.5 + 0.5 * [Math]::Sin($TwoPi * 28.0 * $Time)
            return $env * (
                0.16 * [Math]::Sin($TwoPi * 118.0 * $Time) +
                0.09 * [Math]::Sin($TwoPi * 310.0 * $Time) +
                $grit * $noise)
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
        "se.impact.dirt.scoop" {
            $env = Decay $Time $Duration 2.1
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.20
            $scrape = 0.45 + 0.55 * [Math]::Sin($TwoPi * 32.0 * $Time)
            return $env * (0.22 * [Math]::Sin($TwoPi * 88.0 * $Time) + 0.10 * [Math]::Sin($TwoPi * 176.0 * $Time) + $scrape * $noise)
        }
        "se.impact.dirt.blade" {
            $env = Decay $Time $Duration 2.4
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.16
            $slice = [Math]::Min(1.0, $Time / [Math]::Max(0.001, $Duration * 0.35))
            return $env * (0.18 * [Math]::Sin($TwoPi * 118.0 * $Time) + 0.10 * [Math]::Sin($TwoPi * 236.0 * $Time) + $slice * $noise)
        }
        "se.impact.dirt.pointed" {
            $env = Decay $Time $Duration 3.0
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.12
            return $env * (0.30 * [Math]::Sin($TwoPi * 132.0 * $Time) + 0.12 * [Math]::Sin($TwoPi * 360.0 * $Time) + $noise)
        }
        "se.impact.dirt.generic" {
            $env = Decay $Time $Duration 2.2
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.18
            return $env * (0.24 * [Math]::Sin($TwoPi * 96.0 * $Time) + $noise)
        }
        "se.impact.rock.metal" {
            $env = Decay $Time $Duration 1.75
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.10
            return $env * (0.28 * [Math]::Sin($TwoPi * 360.0 * $Time) + 0.22 * [Math]::Sin($TwoPi * 720.0 * $Time) + 0.10 * [Math]::Sin($TwoPi * 1080.0 * $Time) + $noise)
        }
        "se.impact.rock.pointed" {
            $env = Decay $Time $Duration 1.9
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.13
            return $env * (0.30 * [Math]::Sin($TwoPi * 230.0 * $Time) + 0.18 * [Math]::Sin($TwoPi * 690.0 * $Time) + $noise)
        }
        "se.impact.rock.stone" {
            $env = Decay $Time $Duration 1.65
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.18
            return $env * (0.26 * [Math]::Sin($TwoPi * 118.0 * $Time) + 0.18 * [Math]::Sin($TwoPi * 236.0 * $Time) + $noise)
        }
        "se.impact.slime.slime" {
            $env = Decay $Time $Duration 1.3
            $wobble = 1.0 + 0.12 * [Math]::Sin($TwoPi * 18.0 * $Time)
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.025
            return $env * (0.34 * [Math]::Sin($TwoPi * 118.0 * $wobble * $Time) + 0.18 * [Math]::Sin($TwoPi * 178.0 * $wobble * $Time) + $noise)
        }
        "se.impact.soft.soft" {
            $env = Decay $Time $Duration 1.45
            $wobble = 1.0 + 0.08 * [Math]::Sin($TwoPi * 14.0 * $Time)
            return $env * (0.32 * [Math]::Sin($TwoPi * 105.0 * $wobble * $Time) + 0.12 * [Math]::Sin($TwoPi * 210.0 * $Time))
        }
        "se.impact.hard.soft" {
            $env = Decay $Time $Duration 1.55
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.08
            $wobble = 1.0 + 0.10 * [Math]::Sin($TwoPi * 11.0 * $Time)
            return $env * (0.30 * [Math]::Sin($TwoPi * 82.0 * $wobble * $Time) + 0.16 * [Math]::Sin($TwoPi * 164.0 * $Time) + $noise)
        }
        "se.impact.crisp" {
            $env = Decay $Time $Duration 2.8
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.18
            return $env * (0.20 * [Math]::Sin($TwoPi * 520.0 * $Time) + 0.12 * [Math]::Sin($TwoPi * 1040.0 * $Time) + $noise)
        }
        "se.impact.paper.book" {
            $env = Decay $Time $Duration 1.2
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.14
            $flutter = 0.55 + 0.45 * [Math]::Sin($TwoPi * 28.0 * $Time)
            return $env * (0.08 * [Math]::Sin($TwoPi * 180.0 * $Time) + $flutter * $noise)
        }
        "se.impact.metal.trinket" {
            $env = Decay $Time $Duration 1.25
            return $env * (0.24 * [Math]::Sin($TwoPi * 880.0 * $Time) + 0.20 * [Math]::Sin($TwoPi * 1320.0 * $Time) + 0.12 * [Math]::Sin($TwoPi * 1760.0 * $Time))
        }
        "se.impact.resonant.gong" {
            $env = Decay $Time $Duration 0.85
            return $env * (0.32 * [Math]::Sin($TwoPi * 196.0 * $Time) + 0.20 * [Math]::Sin($TwoPi * 294.0 * $Time) + 0.14 * [Math]::Sin($TwoPi * 392.0 * $Time))
        }
        "se.impact.resonant.chime" {
            $env = Decay $Time $Duration 0.95
            return $env * (0.24 * [Math]::Sin($TwoPi * 1046.5 * $Time) + 0.18 * [Math]::Sin($TwoPi * 1568.0 * $Time) + 0.12 * [Math]::Sin($TwoPi * 2093.0 * $Time))
        }
        "se.impact.cloth.fluffy" {
            $env = Decay $Time $Duration 1.1
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.12
            return $env * (0.09 * [Math]::Sin($TwoPi * 74.0 * $Time) + $noise)
        }
        "se.impact.fiber.rustle" {
            $env = Decay $Time $Duration 1.45
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.15
            $bristle = 0.5 + 0.5 * [Math]::Sin($TwoPi * 36.0 * $Time)
            return $env * (0.06 * [Math]::Sin($TwoPi * 220.0 * $Time) + $bristle * $noise)
        }
        "se.impact.glass" {
            $env = Decay $Time $Duration 1.35
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.04
            return $env * (0.26 * [Math]::Sin($TwoPi * 1480.0 * $Time) + 0.20 * [Math]::Sin($TwoPi * 2220.0 * $Time) + $noise)
        }
        "se.impact.metal.metal" {
            $env = Decay $Time $Duration 1.25
            return $env * (0.28 * [Math]::Sin($TwoPi * 620.0 * $Time) + 0.22 * [Math]::Sin($TwoPi * 930.0 * $Time) + 0.12 * [Math]::Sin($TwoPi * 1240.0 * $Time))
        }
        "se.impact.stone.metal" {
            $env = Decay $Time $Duration 1.5
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.11
            return $env * (0.26 * [Math]::Sin($TwoPi * 260.0 * $Time) + 0.20 * [Math]::Sin($TwoPi * 520.0 * $Time) + $noise)
        }
        "se.impact.wood" {
            $env = Decay $Time $Duration 1.8
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.08
            return $env * (0.28 * [Math]::Sin($TwoPi * 180.0 * $Time) + 0.14 * [Math]::Sin($TwoPi * 360.0 * $Time) + $noise)
        }
        "se.impact.generic" {
            $env = Decay $Time $Duration 2.0
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.13
            return $env * (0.24 * [Math]::Sin($TwoPi * 160.0 * $Time) + $noise)
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
        "se.player.pinch" {
            $env = Decay $Time $Duration 0.95
            $pulse = 0.68 + 0.32 * [Math]::Sin($TwoPi * 8.0 * $Time)
            $fall = 740.0 - 250.0 * ($Time / $Duration)
            return $env * $pulse * (
                0.28 * [Math]::Sin($TwoPi * $fall * $Time) +
                0.16 * [Math]::Sin($TwoPi * ($fall * 0.5) * $Time) +
                0.07 * [Math]::Sin($TwoPi * 96.0 * $Time))
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
        "se.enemy.rat_steal" {
            $env = Decay $Time $Duration 1.45
            $u = $Time / $Duration
            $squeak = 1450.0 + 1050.0 * [Math]::Sin([Math]::PI * [Math]::Min(1.0, $u))
            $chirp = if ($Time -ge 0.10) { Decay ($Time - 0.10) ($Duration - 0.10) 1.8 } else { 0.0 }
            return $env * (0.34 * [Math]::Sin($TwoPi * $squeak * $Time) + 0.14 * [Math]::Sin($TwoPi * ($squeak * 1.92) * $Time)) +
                $chirp * 0.18 * [Math]::Sin($TwoPi * 1880.0 * ($Time - 0.10))
        }
        "se.enemy.mimic_bite" {
            $env = Decay $Time $Duration 1.25
            $u = $Time / $Duration
            $sweep = 520.0 - 330.0 * $u
            $snap = [Math]::Exp(-[Math]::Pow(($Time - 0.070) / 0.026, 2.0))
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.14
            return $env * (0.26 * [Math]::Sin($TwoPi * $sweep * $Time) + 0.18 * [Math]::Sin($TwoPi * 96.0 * $Time) + $noise) +
                $snap * (0.22 * [Math]::Sin($TwoPi * 740.0 * $Time) + 0.12 * $noise)
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
        "se.enemy.transform" {
            $env = Decay $Time $Duration 1.2
            $u = $Time / $Duration
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.13
            return $env * (0.26 * [Math]::Sin($TwoPi * (150.0 + 120.0 * $u) * $Time) + 0.18 * [Math]::Sin($TwoPi * 74.0 * $Time) + $noise)
        }
        "se.projectile.impact" {
            $env = Decay $Time $Duration 2.2
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.18
            return $env * (0.30 * [Math]::Sin($TwoPi * 210.0 * $Time) + $noise)
        }
        "se.projectile.pebble.launch" {
            $env = Decay $Time $Duration 2.1
            $sweep = 980.0 + 520.0 * ($Time / $Duration)
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.035
            return $env * (0.34 * [Math]::Sin($TwoPi * $sweep * $Time) + 0.10 * [Math]::Sin($TwoPi * 1960.0 * $Time) + $noise)
        }
        "se.projectile.heavy.launch" {
            $env = Decay $Time $Duration 1.35
            $sweep = 310.0 - 180.0 * ($Time / $Duration)
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.13
            return $env * (0.42 * [Math]::Sin($TwoPi * $sweep * $Time) + 0.16 * [Math]::Sin($TwoPi * 86.0 * $Time) + $noise)
        }
        "se.projectile.frog.launch" {
            $env = Decay $Time $Duration 1.45
            $u = $Time / $Duration
            $wobble = 1.0 + 0.14 * [Math]::Sin($TwoPi * 18.0 * $Time)
            $freq = (185.0 - 52.0 * $u) * $wobble
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.08
            return $env * (0.38 * [Math]::Sin($TwoPi * $freq * $Time) + 0.22 * [Math]::Sin($TwoPi * ($freq * 0.52) * $Time) + $noise)
        }
        "se.projectile.quick.launch" {
            $env = Decay $Time $Duration 2.4
            $sweep = 1260.0 + 640.0 * ($Time / $Duration)
            return $env * (0.30 * [Math]::Sin($TwoPi * $sweep * $Time) + 0.16 * [Math]::Sin($TwoPi * 2520.0 * $Time))
        }
        "se.projectile.water_pip.launch" {
            $env = Decay $Time $Duration 1.9
            $sweep = 720.0 + 180.0 * ($Time / $Duration)
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.05
            return $env * (0.28 * [Math]::Sin($TwoPi * $sweep * $Time) + 0.17 * [Math]::Sin($TwoPi * 1440.0 * $Time) + $noise)
        }
        "se.projectile.bubble.launch" {
            $env = Decay $Time $Duration 1.25
            $u = $Time / $Duration
            $pop = if ($Time -lt 0.055) { [Math]::Pow(1.0 - ($Time / 0.055), 1.8) } else { 0.0 }
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.09
            return $env * (0.34 * [Math]::Sin($TwoPi * (238.0 - 78.0 * $u) * $Time) + 0.18 * $pop * [Math]::Sin($TwoPi * 760.0 * $Time) + $noise)
        }
        "se.projectile.bubble.pop" {
            $env = Decay $Time $Duration 1.8
            $snap = if ($Time -lt 0.030) { [Math]::Pow(1.0 - ($Time / 0.030), 2.1) } else { 0.0 }
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.12
            return $env * (0.30 * [Math]::Sin($TwoPi * 860.0 * $Time) + 0.20 * $snap * [Math]::Sin($TwoPi * 1640.0 * $Time) + $noise)
        }
        "se.projectile.fire_breath.launch" {
            $env = Decay $Time $Duration 1.15
            $u = $Time / $Duration
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.18
            return $env * (0.32 * [Math]::Sin($TwoPi * (96.0 - 24.0 * $u) * $Time) + 0.22 * [Math]::Sin($TwoPi * (182.0 - 64.0 * $u) * $Time) + $noise)
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
        "se.ring.slow_bite" {
            $env = Decay $Time $Duration 1.65
            $u = $Time / $Duration
            $wobble = 1.0 + 0.06 * [Math]::Sin($TwoPi * 5.5 * $Time)
            $freq = (315.0 - 190.0 * $u) * $wobble
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.08
            $snap = 0.0
            if ($Time -lt 0.045) {
                $snap = [Math]::Pow(1.0 - ($Time / 0.045), 2.2) * [Math]::Sin($TwoPi * 920.0 * $Time)
            }
            return $env * (0.34 * [Math]::Sin($TwoPi * $freq * $Time) + 0.24 * [Math]::Sin($TwoPi * ($freq * 0.48) * $Time) + $noise) + 0.20 * $snap
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
        "se.discovery.monster" {
            $env = Decay $Time $Duration 0.95
            $u = $Time / $Duration
            $sweep = 880.0 + 1320.0 * $u
            return $env * (0.28 * [Math]::Sin($TwoPi * $sweep * $Time) + 0.20 * [Math]::Sin($TwoPi * 1760.0 * $Time) + 0.14 * [Math]::Sin($TwoPi * 2349.32 * $Time))
        }
        "se.discovery.effect" {
            $env = Decay $Time $Duration 0.9
            $shimmer = 0.5 + 0.5 * [Math]::Sin($TwoPi * 18.0 * $Time)
            return $env * (0.22 * [Math]::Sin($TwoPi * 1174.66 * $Time) + 0.20 * [Math]::Sin($TwoPi * 2093.0 * $Time) + 0.14 * $shimmer * [Math]::Sin($TwoPi * 3135.96 * $Time))
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
        "se.item.break.ceramic" {
            $env = Decay $Time $Duration 1.45
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.08
            return $env * (0.24 * [Math]::Sin($TwoPi * 520.0 * $Time) + 0.20 * [Math]::Sin($TwoPi * 860.0 * $Time) + 0.12 * [Math]::Sin($TwoPi * 1280.0 * $Time) + $noise)
        }
        "se.item.break.glass" {
            $env = Decay $Time $Duration 1.35
            $noise = ($Random.NextDouble() * 2.0 - 1.0) * 0.05
            return $env * (0.28 * [Math]::Sin($TwoPi * 1480.0 * $Time) + 0.20 * [Math]::Sin($TwoPi * 2220.0 * $Time) + 0.12 * [Math]::Sin($TwoPi * 2960.0 * $Time) + $noise)
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

if ($HighQualitySe) {
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Text;

public static class MajoPlaceholderAudioHQ
{
    private const double TwoPi = Math.PI * 2.0;

    private static double Clamp(double value, double min, double max)
    {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }

    private static double Pos(double t, double d)
    {
        return d <= 0.0 ? 0.0 : Clamp(t / d, 0.0, 1.0);
    }

    private static double S(double freq, double t)
    {
        return Math.Sin(TwoPi * freq * t);
    }

    private static double N(Random rng)
    {
        return rng.NextDouble() * 2.0 - 1.0;
    }

    private static double Decay(double t, double d, double power)
    {
        if (d <= 0.0) return 0.0;
        double x = 1.0 - t / d;
        if (x <= 0.0) return 0.0;
        return Math.Pow(x, power);
    }

    private static double Env(double t, double d, double attack, double power)
    {
        if (t < 0.0 || t > d) return 0.0;
        double a = attack <= 0.0 ? 1.0 : Clamp(t / attack, 0.0, 1.0);
        return a * Decay(t, d, power);
    }

    private static double Burst(double t, double start, double length, double power)
    {
        double local = t - start;
        if (local < 0.0 || local > length) return 0.0;
        return Env(local, length, Math.Min(0.004, length * 0.25), power);
    }

    private static double Sweep(double t, double d, double startFreq, double endFreq)
    {
        double u = Pos(t, d);
        double freq = startFreq + (endFreq - startFreq) * u;
        return S(freq, t);
    }

    private static double Ring(double t, double d, double freq, double amp, double power)
    {
        double e = Env(t, d, 0.002, power);
        return amp * e * (S(freq, t) + 0.45 * S(freq * 2.01, t) + 0.18 * S(freq * 2.98, t));
    }

    private static double Sparkle(double t, double d, double baseFreq, double amp)
    {
        double e = Env(t, d, 0.006, 1.25);
        double shimmer = 0.65 + 0.35 * S(17.0, t);
        return amp * e * (S(baseFreq, t) + 0.55 * S(baseFreq * 1.5, t) + 0.32 * shimmer * S(baseFreq * 2.0, t));
    }

    private static double Dust(double t, double d, Random rng, double bodyFreq, double noiseAmp, double gritFreq)
    {
        double e = Env(t, d, 0.004, 2.25);
        double grit = 0.55 + 0.45 * S(gritFreq, t);
        return e * (0.24 * S(bodyFreq, t) + 0.11 * S(bodyFreq * 2.05, t) + noiseAmp * grit * N(rng));
    }

    private static double Stone(double t, double d, Random rng, double bodyFreq, double metal)
    {
        double e = Env(t, d, 0.002, 1.7);
        double crunch = (0.55 + 0.45 * S(43.0, t)) * N(rng);
        return e * (
            0.25 * S(bodyFreq, t) +
            0.18 * S(bodyFreq * 2.02, t) +
            0.10 * S(bodyFreq * 3.11, t) +
            0.15 * crunch) +
            metal * Ring(t, d, 720.0, 0.16, 1.5);
    }

    private static double Soft(double t, double d, Random rng, double freq, double wobbleDepth, double noiseAmp)
    {
        double e = Env(t, d, 0.005, 1.35);
        double wobble = 1.0 + wobbleDepth * S(13.0, t);
        return e * (0.34 * S(freq * wobble, t) + 0.18 * S(freq * 1.55 * wobble, t) + noiseAmp * N(rng));
    }

    private static double Whoosh(double t, double d, Random rng, double startFreq, double endFreq, double noiseAmp)
    {
        double e = Env(t, d, 0.012, 1.15);
        double gate = 0.50 + 0.50 * S(18.0 + 14.0 * Pos(t, d), t);
        return e * (0.24 * Sweep(t, d, startFreq, endFreq) + noiseAmp * gate * N(rng));
    }

    private static double Page(double t, double d, Random rng, double amp)
    {
        double e = Env(t, d, 0.006, 1.05);
        double flutter = 0.55 + 0.45 * S(26.0, t);
        return e * (0.07 * S(180.0, t) + amp * flutter * N(rng));
    }

    private static double Sample(string kind, double t, double d, Random rng)
    {
        double u = Pos(t, d);
        switch (kind)
        {
            case "se.ui.confirm":
                return Env(t, d, 0.004, 1.75) * (0.34 * Sweep(t, d, 700.0, 1180.0) + 0.20 * S(1568.0, t)) + 0.18 * Burst(t, 0.055, 0.07, 2.2) * S(2093.0, t);
            case "se.ui.cancel":
                return Env(t, d, 0.004, 1.65) * (0.36 * Sweep(t, d, 580.0, 300.0) + 0.15 * S(220.0, t));
            case "se.ui.menu_open":
                return Whoosh(t, d, rng, 250.0, 620.0, 0.045) + Sparkle(t, d, 660.0, 0.12);
            case "se.ui.tab_switch":
                return Ring(t, d, 720.0, 0.22, 2.0) + Ring(t, d, 1080.0, 0.12, 2.2);
            case "se.ui.book_open":
                return Page(t, d, rng, 0.18) + 0.08 * Burst(t, 0.055, 0.10, 1.5) * S(260.0, t);
            case "se.ui.item_move":
                return Ring(t, d, 430.0, 0.18, 2.0) + Ring(t, d, 690.0, 0.11, 2.1);
            case "se.ui.item_use":
                return Sparkle(t, d, 880.0, 0.24) + 0.08 * Whoosh(t, d, rng, 320.0, 960.0, 0.025);
            case "se.ui.ring_place":
                return Ring(t, d, 520.0, 0.20, 1.55) + Ring(t, d, 780.0, 0.16, 1.7) + Sparkle(t, d, 1040.0, 0.08);
            case "se.ui.upgrade_select":
                return Sparkle(t, d, 660.0, 0.18) + Sparkle(t, d, 990.0, 0.14) + Ring(t, d, 1320.0, 0.10, 1.2);
            case "se.facility.forge_upgrade":
                return 0.82 * Stone(t, Math.Min(d, 0.19), rng, 112.0, 0.9) +
                    0.20 * Whoosh(t, d, rng, 180.0, 720.0, 0.07) +
                    Ring(t - 0.13, d * 0.80, 783.99, 0.18, 1.05) +
                    Ring(t - 0.17, d * 0.72, 1174.66, 0.12, 1.15);
            case "se.facility.workbench_upgrade":
                return 0.36 * Ring(t, d, 520.0, 0.20, 1.55) +
                    0.32 * Ring(t - 0.075, d * 0.82, 780.0, 0.18, 1.65) +
                    Sparkle(t - 0.12, d * 0.72, 1040.0, 0.16);
            case "se.facility.workbench_repair":
                return 0.22 * Burst(t, 0.00, 0.08, 2.2) * S(620.0, t) +
                    0.20 * Burst(t, 0.09, 0.08, 2.2) * S(700.0, t) +
                    0.18 * Burst(t, 0.18, 0.08, 2.2) * S(780.0, t) +
                    Ring(t - 0.20, d * 0.62, 523.25, 0.17, 1.05) +
                    Ring(t - 0.22, d * 0.58, 783.99, 0.10, 1.15);
            case "se.merchant.transaction":
                return Ring(t, d, 1320.0, 0.24, 1.55) +
                    Ring(t, d, 1980.0, 0.13, 1.75) +
                    Ring(t - 0.085, d * 0.72, 1568.0, 0.22, 1.55) +
                    Ring(t - 0.085, d * 0.72, 2352.0, 0.11, 1.75);
            case "se.dialogue.advance":
                return Env(t, d, 0.003, 1.8) * (0.26 * Sweep(t, d, 540.0, 760.0) + 0.16 * Ring(t, d, 1180.0, 0.10, 2.0));
            case "se.level_up.jingle":
                return Sparkle(t, d, 523.25, 0.14) +
                    Sparkle(t - 0.16, d * 0.86, 659.25, 0.16) +
                    Sparkle(t - 0.32, d * 0.72, 783.99, 0.16) +
                    Sparkle(t - 0.50, d * 0.56, 1046.50, 0.18) +
                    0.10 * Ring(t, d, 1568.0, 0.12, 0.82);
            case "se.game_over.jingle":
                return Ring(t, d, 392.0, 0.18, 0.72) +
                    Ring(t - 0.22, d * 0.82, 329.63, 0.16, 0.82) +
                    Ring(t - 0.44, d * 0.66, 261.63, 0.16, 0.92) +
                    Ring(t - 0.68, d * 0.50, 196.0, 0.20, 1.05) +
                    0.10 * Env(t, d, 0.025, 0.72) * S(73.42, t);
            case "se.item.new.jingle":
                return Sparkle(t, d, 659.25, 0.15) +
                    Sparkle(t - 0.14, d * 0.82, 880.0, 0.16) +
                    Sparkle(t - 0.30, d * 0.64, 1174.66, 0.18) +
                    0.12 * Ring(t, d, 1760.0, 0.10, 0.78);
            case "se.transition":
                return Whoosh(t, d, rng, 160.0, 720.0, 0.075) + 0.10 * S(96.0, t) * Env(t, d, 0.02, 1.4);

            case "se.footstep.base_outdoor":
                return Dust(t, d, rng, 88.0, 0.18, 31.0) + 0.10 * Burst(t, 0.020, 0.045, 2.4) * N(rng);
            case "se.footstep.home":
                return Env(t, d, 0.002, 2.0) * (0.26 * S(185.0, t) + 0.15 * S(370.0, t) + 0.04 * N(rng));
            case "se.footstep.dungeon":
                return Dust(t, d, rng, 112.0, 0.24, 38.0) + 0.08 * Stone(t, d * 0.75, rng, 170.0, 0.2);

            case "se.dig.hit":
                return Dust(t, d, rng, 126.0, 0.22, 35.0) + 0.14 * Burst(t, 0.012, 0.035, 2.6) * S(320.0, t);
            case "se.dig.break":
                return Dust(t, d, rng, 92.0, 0.28, 28.0) + 0.12 * Stone(t, d, rng, 118.0, 0.0);
            case "se.dig.ore_break":
                return Stone(t, d, rng, 132.0, 0.85) + 0.12 * Ring(t, d, 980.0, 0.12, 1.6);
            case "se.attack.hit":
                return Env(t, d, 0.002, 1.9) * (0.36 * S(170.0, t) + 0.20 * S(340.0, t) + 0.18 * N(rng)) + 0.10 * Ring(t, d, 520.0, 0.10, 2.0);

            case "se.impact.dirt.scoop":
                return Dust(t, d, rng, 82.0, 0.30, 27.0) + 0.10 * Burst(t, 0.045, 0.05, 1.8) * N(rng);
            case "se.impact.dirt.blade":
                return Dust(t, d, rng, 112.0, 0.18, 42.0) + 0.12 * Burst(t, 0.010, 0.035, 2.8) * S(520.0, t);
            case "se.impact.dirt.pointed":
                return Dust(t, d, rng, 135.0, 0.16, 34.0) + 0.18 * Burst(t, 0.000, 0.028, 3.0) * S(410.0, t);
            case "se.impact.dirt.generic":
                return Dust(t, d, rng, 96.0, 0.22, 30.0);
            case "se.impact.rock.metal":
                return Stone(t, d, rng, 180.0, 1.0) + Ring(t, d, 960.0, 0.16, 1.55);
            case "se.impact.rock.pointed":
                return Stone(t, d, rng, 220.0, 0.65) + 0.16 * Burst(t, 0.000, 0.030, 2.8) * S(1320.0, t);
            case "se.impact.rock.stone":
                return Stone(t, d, rng, 120.0, 0.15) + 0.08 * Dust(t, d, rng, 80.0, 0.12, 21.0);
            case "se.impact.slime.slime":
                return Soft(t, d, rng, 118.0, 0.15, 0.025) + 0.12 * Soft(t - 0.030, d * 0.75, rng, 155.0, 0.10, 0.02);
            case "se.impact.soft.soft":
                return Soft(t, d, rng, 96.0, 0.10, 0.018);
            case "se.impact.hard.soft":
                return Soft(t, d, rng, 82.0, 0.12, 0.06) + 0.12 * Burst(t, 0.000, 0.04, 2.2) * S(220.0, t);
            case "se.impact.crisp":
                return Env(t, d, 0.001, 2.5) * (0.15 * S(760.0, t) + 0.12 * S(1520.0, t) + 0.22 * N(rng) * (0.55 + 0.45 * S(72.0, t)));
            case "se.impact.paper.book":
                return Page(t, d, rng, 0.20) + 0.05 * S(220.0, t) * Env(t, d, 0.012, 1.2);
            case "se.impact.metal.trinket":
                return Ring(t, d, 880.0, 0.24, 1.2) + Ring(t, d, 1320.0, 0.17, 1.35) + Ring(t, d, 1760.0, 0.09, 1.55);
            case "se.impact.resonant.gong":
                return Ring(t, d, 196.0, 0.34, 0.85) + Ring(t, d, 294.0, 0.22, 1.05) + Ring(t, d, 392.0, 0.14, 1.25);
            case "se.impact.resonant.chime":
                return Ring(t, d, 1046.5, 0.25, 0.95) + Ring(t, d, 1568.0, 0.18, 1.1) + Ring(t, d, 2093.0, 0.10, 1.35);
            case "se.impact.cloth.fluffy":
                return Env(t, d, 0.010, 1.15) * (0.08 * S(72.0, t) + 0.18 * N(rng) * (0.5 + 0.5 * S(18.0, t)));
            case "se.impact.fiber.rustle":
                return Env(t, d, 0.004, 1.35) * (0.06 * S(210.0, t) + 0.22 * N(rng) * (0.5 + 0.5 * S(42.0, t)));
            case "se.impact.glass":
                return Ring(t, d, 1480.0, 0.26, 1.05) + Ring(t, d, 2220.0, 0.19, 1.25) + 0.05 * N(rng) * Env(t, d, 0.001, 2.0);
            case "se.impact.metal.metal":
                return Ring(t, d, 620.0, 0.28, 1.1) + Ring(t, d, 930.0, 0.20, 1.25) + Ring(t, d, 1240.0, 0.12, 1.45);
            case "se.impact.stone.metal":
                return Stone(t, d, rng, 260.0, 0.75) + 0.10 * Ring(t, d, 520.0, 0.12, 1.5);
            case "se.impact.wood":
                return Env(t, d, 0.002, 1.8) * (0.30 * S(180.0, t) + 0.16 * S(360.0, t) + 0.10 * N(rng));
            case "se.impact.generic":
                return Env(t, d, 0.003, 2.0) * (0.24 * S(160.0, t) + 0.13 * N(rng));

            case "se.pickup":
                return Sparkle(t, d, 784.0, 0.22) + 0.10 * Sparkle(t - 0.060, d * 0.75, 1174.66, 0.16);
            case "se.boss.spawn":
                return Env(t, d, 0.030, 0.65) * (0.34 * Sweep(t, d, 62.0, 180.0) + 0.20 * S(36.0, t) + 0.08 * N(rng)) + 0.12 * Ring(t, d, 294.0, 0.15, 1.1);
            case "se.boss.defeat":
                return Env(t, d, 0.006, 0.85) * (0.28 * Sweep(t, d, 360.0, 95.0) + 0.20 * S(196.0, t) + 0.10 * N(rng)) + Ring(t, d, 392.0, 0.12, 1.0);
            case "se.player.damage":
                return Env(t, d, 0.002, 1.7) * (0.34 * S(140.0, t) + 0.18 * S(70.0, t) + 0.16 * N(rng));
            case "se.player.pinch":
                return Ring(t, d, 740.0, 0.22, 0.95) + 0.18 * Ring(t - 0.120, d * 0.72, 554.0, 0.90, 1.0) + 0.08 * S(96.0, t) * Env(t, d, 0.012, 0.8);
            case "se.ring.throw":
                return Whoosh(t, d, rng, 260.0, 980.0, 0.055) + 0.10 * Ring(t, d, 1180.0, 0.12, 1.2);
            case "se.enemy.defeat":
                return Env(t, d, 0.006, 0.95) * (0.24 * Sweep(t, d, 360.0, 95.0) + 0.18 * N(rng) * (1.0 - u));
            case "se.enemy.spawn":
                return Whoosh(t, d, rng, 90.0, 260.0, 0.10) + 0.08 * S(180.0, t) * Env(t, d, 0.02, 1.1);
            case "se.enemy.alert":
                return Ring(t, d, 880.0, 0.26, 1.25) + Ring(t, d, 1320.0, 0.18, 1.35);
            case "se.enemy.attack":
                return Whoosh(t, d, rng, 520.0, 150.0, 0.12) + 0.16 * Burst(t, 0.040, 0.05, 2.1) * S(190.0, t);
            case "se.enemy.rat_steal":
                return Env(t, d, 0.003, 1.45) * (0.34 * Sweep(t, d, 1450.0, 2480.0) + 0.14 * Sweep(t, d, 2780.0, 4100.0)) +
                    0.18 * Env(t - 0.10, d - 0.10, 0.002, 1.8) * Sweep(t - 0.10, d - 0.10, 2050.0, 1540.0);
            case "se.enemy.mimic_bite":
                return Whoosh(t, d, rng, 620.0, 170.0, 0.13) + 0.22 * Burst(t, 0.070, 0.045, 2.2) * S(760.0, t) + 0.14 * Env(t, d, 0.002, 1.05) * S(88.0, t);
            case "se.enemy.shoot":
                return Env(t, d, 0.002, 1.8) * (0.28 * Sweep(t, d, 720.0, 360.0) + 0.13 * S(1440.0, t));
            case "se.enemy.heal":
                return Sparkle(t, d, 520.0, 0.16) + Sparkle(t, d, 780.0, 0.14) + Sparkle(t, d, 1040.0, 0.10);
            case "se.enemy.transform":
                return Whoosh(t, d, rng, 150.0, 270.0, 0.16) + 0.18 * Env(t, d, 0.002, 1.1) * S(74.0, t) + 0.08 * N(rng) * Env(t, d, 0.001, 1.4);
            case "se.projectile.impact":
                return Env(t, d, 0.002, 2.0) * (0.30 * S(220.0, t) + 0.18 * N(rng)) + 0.08 * Ring(t, d, 660.0, 0.10, 1.8);
            case "se.projectile.pebble.launch":
                return Env(t, d, 0.001, 2.1) * (0.34 * Sweep(t, d, 980.0, 1500.0) + 0.10 * S(1960.0, t) + 0.035 * N(rng));
            case "se.projectile.heavy.launch":
                return Whoosh(t, d, rng, 310.0, 130.0, 0.11) + 0.18 * Env(t, d, 0.002, 1.2) * S(86.0, t);
            case "se.projectile.frog.launch":
                return Soft(t, d, rng, 168.0, 0.28, 0.08) + 0.12 * Env(t, d, 0.006, 1.2) * Sweep(t, d, 238.0, 132.0);
            case "se.projectile.quick.launch":
                return Env(t, d, 0.001, 2.4) * (0.30 * Sweep(t, d, 1260.0, 1900.0) + 0.16 * S(2520.0, t));
            case "se.projectile.water_pip.launch":
                return Env(t, d, 0.001, 1.9) * (0.28 * Sweep(t, d, 720.0, 900.0) + 0.17 * S(1440.0, t) + 0.05 * N(rng));
            case "se.projectile.bubble.launch":
                return Soft(t, d, rng, 210.0, 0.34, 0.06) + 0.18 * Burst(t, 0.000, 0.055, 1.8) * S(760.0, t);
            case "se.projectile.bubble.pop":
                return Env(t, d, 0.001, 1.8) * (0.30 * S(860.0, t) + 0.16 * N(rng)) + 0.20 * Burst(t, 0.000, 0.030, 2.1) * S(1640.0, t);
            case "se.projectile.fire_breath.launch":
                return Whoosh(t, d, rng, 96.0, 182.0, 0.18) + 0.18 * Env(t, d, 0.004, 1.05) * S(72.0, t);
            case "se.ring.guard":
                return Ring(t, d, 360.0, 0.24, 1.25) + Ring(t, d, 720.0, 0.24, 1.4);
            case "se.ring.reflect":
                return Ring(t, d, 560.0, 0.20, 1.1) + 0.24 * Env(t, d, 0.003, 1.2) * Sweep(t, d, 760.0, 1480.0);
            case "se.ring.slow_bite":
                return Env(t, d, 0.012, 1.55) * (
                    0.30 * Sweep(t, d, 315.0, 125.0) +
                    0.20 * S(92.0, t) +
                    0.10 * N(rng) * (0.65 + 0.35 * S(22.0, t))) +
                    0.16 * Burst(t, 0.000, 0.045, 2.2) * S(920.0, t);
            case "se.magic.cast":
                return Sparkle(t, d, 660.0, 0.18) + 0.15 * Whoosh(t, d, rng, 320.0, 980.0, 0.04);
            case "se.magic.impact":
                return Ring(t, d, 240.0, 0.22, 1.4) + Ring(t, d, 720.0, 0.16, 1.55) + 0.13 * Env(t, d, 0.002, 1.8) * N(rng);
            case "se.capture.throw":
                return Whoosh(t, d, rng, 420.0, 900.0, 0.050) + 0.09 * Ring(t, d, 1180.0, 0.11, 1.2);
            case "se.capture.success":
                return Sparkle(t, d, 660.0, 0.16) + Sparkle(t, d, 990.0, 0.16) + Sparkle(t, d, 1320.0, 0.14);
            case "se.capture.fail":
                return Env(t, d, 0.004, 1.55) * (0.30 * Sweep(t, d, 520.0, 220.0) + 0.14 * S(210.0, t));
            case "se.discovery":
                return Sparkle(t, d, 784.0, 0.18) + Sparkle(t, d, 1174.66, 0.15) + Sparkle(t, d, 1568.0, 0.10);
            case "se.discovery.monster":
                return Sparkle(t, d, 880.0, 0.18) + Sparkle(t, d, 1318.51, 0.18) + Sparkle(t, d, 1760.0, 0.16) + 0.12 * Ring(t, d, 420.0, 0.18, 1.1);
            case "se.discovery.effect":
                return Sparkle(t, d, 1174.66, 0.16) + Sparkle(t, d, 1567.98, 0.16) + Sparkle(t, d, 2093.0, 0.14) + 0.09 * Ring(t, d, 3135.96, 0.10, 0.85);
            case "se.discovery.warp":
                return Ring(t, d, 392.0, 0.18, 0.9) + Sparkle(t, d, 784.0, 0.15) + 0.10 * S(1174.66, t) * Env(t, d, 0.01, 0.9);
            case "se.chest.open":
                return Ring(t, d, 180.0, 0.16, 1.35) + Ring(t, d, 430.0, 0.12, 1.55) + 0.09 * Burst(t, 0.060, 0.12, 1.5) * N(rng);
            case "se.crate.break":
                return Stone(t, d, rng, 120.0, 0.0) + 0.22 * Env(t, d, 0.002, 1.7) * N(rng);
            case "se.item.break":
                return Ring(t, d, 260.0, 0.18, 1.45) + Ring(t, d, 620.0, 0.15, 1.6) + 0.10 * N(rng) * Env(t, d, 0.002, 1.8);
            case "se.item.break.ceramic":
                return Ring(t, d, 520.0, 0.20, 1.25) + Ring(t, d, 860.0, 0.17, 1.42) + 0.10 * Env(t, d, 0.001, 1.8) * N(rng);
            case "se.item.break.glass":
                return Ring(t, d, 1480.0, 0.26, 1.05) + Ring(t, d, 2220.0, 0.19, 1.25) + Ring(t, d, 2960.0, 0.10, 1.45) + 0.04 * N(rng) * Env(t, d, 0.001, 2.0);
            case "se.explosion":
                return Env(t, d, 0.001, 1.05) * (0.34 * S(72.0, t) + 0.20 * S(144.0, t) + 0.34 * N(rng) * (1.0 - 0.65 * u));
        }
        return Env(t, d, 0.004, 1.8) * (0.24 * S(220.0, t) + 0.08 * N(rng));
    }

    private static double SoftClip(double value)
    {
        return Math.Tanh(value * 1.15) / Math.Tanh(1.15);
    }

    public static void WriteWav(string path, string kind, double durationSeconds, int seed, int sampleRate)
    {
        int sampleCount = (int)Math.Round(durationSeconds * sampleRate);
        double[] samples = new double[sampleCount];
        Random rng = new Random(seed);
        double peak = 0.0;
        for (int i = 0; i < sampleCount; ++i)
        {
            double t = (double)i / sampleRate;
            double fadeIn = Math.Min(1.0, i / Math.Max(1.0, sampleRate * 0.002));
            double fadeOut = Math.Min(1.0, (sampleCount - i - 1) / Math.Max(1.0, sampleRate * 0.006));
            double sample = SoftClip(Sample(kind, t, durationSeconds, rng)) * Math.Min(fadeIn, fadeOut);
            samples[i] = sample;
            double abs = Math.Abs(sample);
            if (abs > peak) peak = abs;
        }

        double gain = peak > 0.000001 ? Math.Min(3.0, 0.78 / peak) : 1.0;
        Directory.CreateDirectory(Path.GetDirectoryName(path));
        using (FileStream stream = File.Open(path, FileMode.Create, FileAccess.Write))
        using (BinaryWriter writer = new BinaryWriter(stream, Encoding.ASCII))
        {
            short channels = 1;
            short bitsPerSample = 16;
            short blockAlign = (short)(channels * (bitsPerSample / 8));
            int byteRate = sampleRate * blockAlign;
            int dataSize = sampleCount * blockAlign;
            writer.Write(Encoding.ASCII.GetBytes("RIFF"));
            writer.Write(36 + dataSize);
            writer.Write(Encoding.ASCII.GetBytes("WAVE"));
            writer.Write(Encoding.ASCII.GetBytes("fmt "));
            writer.Write(16);
            writer.Write((short)1);
            writer.Write(channels);
            writer.Write(sampleRate);
            writer.Write(byteRate);
            writer.Write(blockAlign);
            writer.Write(bitsPerSample);
            writer.Write(Encoding.ASCII.GetBytes("data"));
            writer.Write(dataSize);
            for (int i = 0; i < sampleCount; ++i)
            {
                int pcm = (int)Math.Round(Clamp(samples[i] * gain, -0.95, 0.95) * 32767.0);
                if (pcm > 32767) pcm = 32767;
                if (pcm < -32768) pcm = -32768;
                writer.Write((short)pcm);
            }
        }
    }
}
'@
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
    @{ Path = Join-Path $SeRoot "facility_forge_upgrade.wav"; Kind = "se.facility.forge_upgrade"; Duration = 0.62; Seed = 2120 },
    @{ Path = Join-Path $SeRoot "facility_workbench_upgrade.wav"; Kind = "se.facility.workbench_upgrade"; Duration = 0.48; Seed = 2121 },
    @{ Path = Join-Path $SeRoot "facility_workbench_repair.wav"; Kind = "se.facility.workbench_repair"; Duration = 0.55; Seed = 2122 },
    @{ Path = Join-Path $SeRoot "merchant_transaction.wav"; Kind = "se.merchant.transaction"; Duration = 0.36; Seed = 2123 },
    @{ Path = Join-Path $SeRoot "zz_tmp_dialogue_advance_placeholder.wav"; Kind = "se.dialogue.advance"; Duration = 0.12; Seed = 2010 },
    @{ Path = Join-Path $SeRoot "level_up_jingle_placeholder.wav"; Kind = "se.level_up.jingle"; Duration = 1.16; Seed = 2072 },
    @{ Path = Join-Path $SeRoot "game_over_jingle_placeholder.wav"; Kind = "se.game_over.jingle"; Duration = 1.34; Seed = 2110 },
    @{ Path = Join-Path $SeRoot "item_new_jingle_placeholder.wav"; Kind = "se.item.new.jingle"; Duration = 0.88; Seed = 2111 },
    @{ Path = Join-Path $SeRoot "transition_placeholder.wav"; Kind = "se.transition"; Duration = 0.42; Seed = 2010 },
    @{ Path = Join-Path $SeRoot "footstep_base_outdoor_placeholder.wav"; Kind = "se.footstep.base_outdoor"; Duration = 0.16; Seed = 2040 },
    @{ Path = Join-Path $SeRoot "footstep_home_placeholder.wav"; Kind = "se.footstep.home"; Duration = 0.14; Seed = 2041 },
    @{ Path = Join-Path $SeRoot "footstep_dungeon_placeholder.wav"; Kind = "se.footstep.dungeon"; Duration = 0.17; Seed = 2042 },
    @{ Path = Join-Path $SeRoot "dig_hit_placeholder.wav"; Kind = "se.dig.hit"; Duration = 0.12; Seed = 2011 },
    @{ Path = Join-Path $SeRoot "dig_break_placeholder.wav"; Kind = "se.dig.break"; Duration = 0.22; Seed = 2012 },
    @{ Path = Join-Path $SeRoot "attack_hit_placeholder.wav"; Kind = "se.attack.hit"; Duration = 0.15; Seed = 2013 },
    @{ Path = Join-Path $SeRoot "impact_dirt_scoop_placeholder.wav"; Kind = "se.impact.dirt.scoop"; Duration = 0.14; Seed = 2050 },
    @{ Path = Join-Path $SeRoot "impact_dirt_blade_placeholder.wav"; Kind = "se.impact.dirt.blade"; Duration = 0.13; Seed = 2051 },
    @{ Path = Join-Path $SeRoot "impact_dirt_pointed_placeholder.wav"; Kind = "se.impact.dirt.pointed"; Duration = 0.12; Seed = 2052 },
    @{ Path = Join-Path $SeRoot "impact_dirt_generic_placeholder.wav"; Kind = "se.impact.dirt.generic"; Duration = 0.13; Seed = 2053 },
    @{ Path = Join-Path $SeRoot "impact_rock_metal_placeholder.wav"; Kind = "se.impact.rock.metal"; Duration = 0.16; Seed = 2054 },
    @{ Path = Join-Path $SeRoot "impact_rock_pointed_placeholder.wav"; Kind = "se.impact.rock.pointed"; Duration = 0.15; Seed = 2055 },
    @{ Path = Join-Path $SeRoot "impact_rock_stone_placeholder.wav"; Kind = "se.impact.rock.stone"; Duration = 0.15; Seed = 2056 },
    @{ Path = Join-Path $SeRoot "impact_slime_slime_placeholder.wav"; Kind = "se.impact.slime.slime"; Duration = 0.18; Seed = 2057 },
    @{ Path = Join-Path $SeRoot "impact_soft_soft_placeholder.wav"; Kind = "se.impact.soft.soft"; Duration = 0.16; Seed = 2058 },
    @{ Path = Join-Path $SeRoot "impact_hard_soft_placeholder.wav"; Kind = "se.impact.hard.soft"; Duration = 0.16; Seed = 2059 },
    @{ Path = Join-Path $SeRoot "impact_crisp_placeholder.wav"; Kind = "se.impact.crisp"; Duration = 0.12; Seed = 2060 },
    @{ Path = Join-Path $SeRoot "impact_paper_book_placeholder.wav"; Kind = "se.impact.paper.book"; Duration = 0.18; Seed = 2061 },
    @{ Path = Join-Path $SeRoot "impact_metal_trinket_placeholder.wav"; Kind = "se.impact.metal.trinket"; Duration = 0.20; Seed = 2062 },
    @{ Path = Join-Path $SeRoot "impact_resonant_gong_placeholder.wav"; Kind = "se.impact.resonant.gong"; Duration = 0.60; Seed = 2063 },
    @{ Path = Join-Path $SeRoot "impact_resonant_chime_placeholder.wav"; Kind = "se.impact.resonant.chime"; Duration = 0.42; Seed = 2064 },
    @{ Path = Join-Path $SeRoot "impact_cloth_fluffy_placeholder.wav"; Kind = "se.impact.cloth.fluffy"; Duration = 0.18; Seed = 2065 },
    @{ Path = Join-Path $SeRoot "impact_fiber_rustle_placeholder.wav"; Kind = "se.impact.fiber.rustle"; Duration = 0.18; Seed = 2066 },
    @{ Path = Join-Path $SeRoot "impact_glass_placeholder.wav"; Kind = "se.impact.glass"; Duration = 0.18; Seed = 2067 },
    @{ Path = Join-Path $SeRoot "impact_metal_metal_placeholder.wav"; Kind = "se.impact.metal.metal"; Duration = 0.18; Seed = 2068 },
    @{ Path = Join-Path $SeRoot "impact_stone_metal_placeholder.wav"; Kind = "se.impact.stone.metal"; Duration = 0.17; Seed = 2069 },
    @{ Path = Join-Path $SeRoot "impact_wood_placeholder.wav"; Kind = "se.impact.wood"; Duration = 0.14; Seed = 2070 },
    @{ Path = Join-Path $SeRoot "impact_generic_placeholder.wav"; Kind = "se.impact.generic"; Duration = 0.13; Seed = 2071 },
    @{ Path = Join-Path $SeRoot "pickup_placeholder.wav"; Kind = "se.pickup"; Duration = 0.24; Seed = 2014 },
    @{ Path = Join-Path $SeRoot "boss_spawn_placeholder.wav"; Kind = "se.boss.spawn"; Duration = 0.78; Seed = 2015 },
    @{ Path = Join-Path $SeRoot "boss_defeat_placeholder.wav"; Kind = "se.boss.defeat"; Duration = 1.05; Seed = 2016 },
    @{ Path = Join-Path $SeRoot "dig_ore_break_placeholder.wav"; Kind = "se.dig.ore_break"; Duration = 0.24; Seed = 2017 },
    @{ Path = Join-Path $SeRoot "player_damage_placeholder.wav"; Kind = "se.player.damage"; Duration = 0.22; Seed = 2018 },
    @{ Path = Join-Path $SeRoot "player_pinch_placeholder.wav"; Kind = "se.player.pinch"; Duration = 0.46; Seed = 2072 },
    @{ Path = Join-Path $SeRoot "ring_throw_placeholder.wav"; Kind = "se.ring.throw"; Duration = 0.24; Seed = 2019 },
    @{ Path = Join-Path $SeRoot "enemy_defeat_placeholder.wav"; Kind = "se.enemy.defeat"; Duration = 0.28; Seed = 2020 },
    @{ Path = Join-Path $SeRoot "enemy_spawn_placeholder.wav"; Kind = "se.enemy.spawn"; Duration = 0.32; Seed = 2021 },
    @{ Path = Join-Path $SeRoot "enemy_alert_placeholder.wav"; Kind = "se.enemy.alert"; Duration = 0.18; Seed = 2022 },
    @{ Path = Join-Path $SeRoot "enemy_attack_placeholder.wav"; Kind = "se.enemy.attack"; Duration = 0.18; Seed = 2023 },
    @{ Path = Join-Path $SeRoot "enemy_rat_steal.wav"; Kind = "se.enemy.rat_steal"; Duration = 0.30; Seed = 2124 },
    @{ Path = Join-Path $SeRoot "enemy_mimic_bite_placeholder.wav"; Kind = "se.enemy.mimic_bite"; Duration = 0.28; Seed = 2110 },
    @{ Path = Join-Path $SeRoot "enemy_shoot_placeholder.wav"; Kind = "se.enemy.shoot"; Duration = 0.14; Seed = 2024 },
    @{ Path = Join-Path $SeRoot "enemy_heal_placeholder.wav"; Kind = "se.enemy.heal"; Duration = 0.26; Seed = 2025 },
    @{ Path = Join-Path $SeRoot "enemy_transform_placeholder.wav"; Kind = "se.enemy.transform"; Duration = 0.30; Seed = 2107 },
    @{ Path = Join-Path $SeRoot "projectile_pebble_launch_placeholder.wav"; Kind = "se.projectile.pebble.launch"; Duration = 0.10; Seed = 2101 },
    @{ Path = Join-Path $SeRoot "projectile_heavy_launch_placeholder.wav"; Kind = "se.projectile.heavy.launch"; Duration = 0.16; Seed = 2102 },
    @{ Path = Join-Path $SeRoot "projectile_frog_launch_placeholder.wav"; Kind = "se.projectile.frog.launch"; Duration = 0.18; Seed = 2103 },
    @{ Path = Join-Path $SeRoot "projectile_quick_launch_placeholder.wav"; Kind = "se.projectile.quick.launch"; Duration = 0.085; Seed = 2104 },
    @{ Path = Join-Path $SeRoot "projectile_water_pip_launch_placeholder.wav"; Kind = "se.projectile.water_pip.launch"; Duration = 0.095; Seed = 2105 },
    @{ Path = Join-Path $SeRoot "projectile_bubble_launch_placeholder.wav"; Kind = "se.projectile.bubble.launch"; Duration = 0.14; Seed = 2106 },
    @{ Path = Join-Path $SeRoot "projectile_bubble_pop_placeholder.wav"; Kind = "se.projectile.bubble.pop"; Duration = 0.12; Seed = 2108 },
    @{ Path = Join-Path $SeRoot "projectile_fire_breath_launch_placeholder.wav"; Kind = "se.projectile.fire_breath.launch"; Duration = 0.20; Seed = 2109 },
    @{ Path = Join-Path $SeRoot "projectile_stone_launch_placeholder.wav"; Kind = "se.projectile.stone.launch"; Duration = 0.11; Seed = 2080 },
    @{ Path = Join-Path $SeRoot "projectile_stone_destroy_placeholder.wav"; Kind = "se.projectile.stone.destroy"; Duration = 0.14; Seed = 2081 },
    @{ Path = Join-Path $SeRoot "projectile_metal_launch_placeholder.wav"; Kind = "se.projectile.metal.launch"; Duration = 0.12; Seed = 2082 },
    @{ Path = Join-Path $SeRoot "projectile_metal_destroy_placeholder.wav"; Kind = "se.projectile.metal.destroy"; Duration = 0.16; Seed = 2083 },
    @{ Path = Join-Path $SeRoot "projectile_liquid_launch_placeholder.wav"; Kind = "se.projectile.liquid.launch"; Duration = 0.12; Seed = 2084 },
    @{ Path = Join-Path $SeRoot "projectile_liquid_destroy_placeholder.wav"; Kind = "se.projectile.liquid.destroy"; Duration = 0.16; Seed = 2085 },
    @{ Path = Join-Path $SeRoot "projectile_magic_launch_placeholder.wav"; Kind = "se.projectile.magic.launch"; Duration = 0.13; Seed = 2086 },
    @{ Path = Join-Path $SeRoot "projectile_magic_destroy_placeholder.wav"; Kind = "se.projectile.magic.destroy"; Duration = 0.18; Seed = 2087 },
    @{ Path = Join-Path $SeRoot "projectile_needle_launch_placeholder.wav"; Kind = "se.projectile.needle.launch"; Duration = 0.09; Seed = 2088 },
    @{ Path = Join-Path $SeRoot "projectile_needle_destroy_placeholder.wav"; Kind = "se.projectile.needle.destroy"; Duration = 0.12; Seed = 2089 },
    @{ Path = Join-Path $SeRoot "projectile_water_launch_placeholder.wav"; Kind = "se.projectile.water.launch"; Duration = 0.12; Seed = 2090 },
    @{ Path = Join-Path $SeRoot "projectile_water_destroy_placeholder.wav"; Kind = "se.projectile.water.destroy"; Duration = 0.16; Seed = 2091 },
    @{ Path = Join-Path $SeRoot "projectile_fire_launch_placeholder.wav"; Kind = "se.projectile.fire.launch"; Duration = 0.12; Seed = 2092 },
    @{ Path = Join-Path $SeRoot "projectile_fire_destroy_placeholder.wav"; Kind = "se.projectile.fire.destroy"; Duration = 0.18; Seed = 2093 },
    @{ Path = Join-Path $SeRoot "projectile_web_launch_placeholder.wav"; Kind = "se.projectile.web.launch"; Duration = 0.12; Seed = 2094 },
    @{ Path = Join-Path $SeRoot "projectile_web_destroy_placeholder.wav"; Kind = "se.projectile.web.destroy"; Duration = 0.16; Seed = 2095 },
    @{ Path = Join-Path $SeRoot "projectile_wind_launch_placeholder.wav"; Kind = "se.projectile.wind.launch"; Duration = 0.12; Seed = 2096 },
    @{ Path = Join-Path $SeRoot "projectile_wind_destroy_placeholder.wav"; Kind = "se.projectile.wind.destroy"; Duration = 0.15; Seed = 2097 },
    @{ Path = Join-Path $SeRoot "projectile_explosion_launch_placeholder.wav"; Kind = "se.projectile.explosion.launch"; Duration = 0.14; Seed = 2098 },
    @{ Path = Join-Path $SeRoot "projectile_explosion_destroy_placeholder.wav"; Kind = "se.projectile.explosion.destroy"; Duration = 0.26; Seed = 2099 },
    @{ Path = Join-Path $SeRoot "projectile_impact_placeholder.wav"; Kind = "se.projectile.impact"; Duration = 0.16; Seed = 2026 },
    @{ Path = Join-Path $SeRoot "ring_guard_placeholder.wav"; Kind = "se.ring.guard"; Duration = 0.18; Seed = 2027 },
    @{ Path = Join-Path $SeRoot "ring_reflect_placeholder.wav"; Kind = "se.ring.reflect"; Duration = 0.22; Seed = 2028 },
    @{ Path = Join-Path $SeRoot "ring_slow_bite_placeholder.wav"; Kind = "se.ring.slow_bite"; Duration = 0.42; Seed = 2100 },
    @{ Path = Join-Path $SeRoot "magic_cast_placeholder.wav"; Kind = "se.magic.cast"; Duration = 0.20; Seed = 2029 },
    @{ Path = Join-Path $SeRoot "magic_impact_placeholder.wav"; Kind = "se.magic.impact"; Duration = 0.22; Seed = 2030 },
    @{ Path = Join-Path $SeRoot "capture_throw_placeholder.wav"; Kind = "se.capture.throw"; Duration = 0.18; Seed = 2031 },
    @{ Path = Join-Path $SeRoot "capture_success_placeholder.wav"; Kind = "se.capture.success"; Duration = 0.32; Seed = 2032 },
    @{ Path = Join-Path $SeRoot "capture_fail_placeholder.wav"; Kind = "se.capture.fail"; Duration = 0.24; Seed = 2033 },
    @{ Path = Join-Path $SeRoot "discovery_placeholder.wav"; Kind = "se.discovery"; Duration = 0.30; Seed = 2034 },
    @{ Path = Join-Path $SeRoot "discovery_monster_placeholder.wav"; Kind = "se.discovery.monster"; Duration = 0.46; Seed = 2111 },
    @{ Path = Join-Path $SeRoot "discovery_effect_placeholder.wav"; Kind = "se.discovery.effect"; Duration = 0.52; Seed = 2112 },
    @{ Path = Join-Path $SeRoot "discovery_warp_placeholder.wav"; Kind = "se.discovery.warp"; Duration = 0.58; Seed = 2035 },
    @{ Path = Join-Path $SeRoot "chest_open_placeholder.wav"; Kind = "se.chest.open"; Duration = 0.32; Seed = 2036 },
    @{ Path = Join-Path $SeRoot "crate_break_placeholder.wav"; Kind = "se.crate.break"; Duration = 0.24; Seed = 2037 },
    @{ Path = Join-Path $SeRoot "item_break_placeholder.wav"; Kind = "se.item.break"; Duration = 0.28; Seed = 2038 },
    @{ Path = Join-Path $SeRoot "item_break_ceramic_placeholder.wav"; Kind = "se.item.break.ceramic"; Duration = 0.24; Seed = 2108 },
    @{ Path = Join-Path $SeRoot "item_break_glass_placeholder.wav"; Kind = "se.item.break.glass"; Duration = 0.22; Seed = 2109 },
    @{ Path = Join-Path $SeRoot "explosion_placeholder.wav"; Kind = "se.explosion"; Duration = 0.36; Seed = 2039 }
)

foreach ($clip in $clips) {
    if ($OnlySe -and -not ([string]$clip.Kind).StartsWith("se.", [System.StringComparison]::Ordinal)) {
        continue
    }
    if ($HighQualitySe -and ([string]$clip.Kind).StartsWith("se.", [System.StringComparison]::Ordinal)) {
        if ($OnlyMissing -and (Test-Path -LiteralPath $clip.Path)) {
            Write-Host "[audio] skip existing $($clip.Path)"
            continue
        }
        [MajoPlaceholderAudioHQ]::WriteWav(
            [string]$clip.Path,
            [string]$clip.Kind,
            [double]$clip.Duration,
            [int]$clip.Seed,
            [int]$SampleRate)
        Write-Host "[audio] wrote $($clip.Path)"
        continue
    }
    Write-PlaceholderWav $clip.Path $clip.Kind $clip.Duration $clip.Seed
}
