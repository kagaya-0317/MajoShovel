param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Prepare", "Finalize", "Stitch")]
    [string]$Operation,

    [int]$Row = 0,
    [int]$Column = 0,
    [string]$GeneratedPath,
    [string]$SourcePath,
    [string]$OutputPath
)

Add-Type -AssemblyName System.Drawing

if ([string]::IsNullOrWhiteSpace($SourcePath)) {
    $SourcePath = Join-Path $PSScriptRoot "..\..\..\assets\kyoten\map.png"
}
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $PSScriptRoot "..\..\..\assets\kyoten\map-ai-1px-16tiles.png"
}

$tileWidth = 320
$tileHeight = 180
$cropWidth = 384
$cropHeight = 216
$horizontalMargin = 32
$verticalMargin = 18
$mapWidth = 1280
$mapHeight = 720

$referenceDir = Join-Path $PSScriptRoot "references"
$generatedDir = Join-Path $PSScriptRoot "generated"
$tileDir = Join-Path $PSScriptRoot "tiles"

function New-NearestNeighborBitmap {
    param(
        [System.Drawing.Image]$Source,
        [System.Drawing.Rectangle]$SourceRect,
        [int]$Width,
        [int]$Height
    )

    $result = [System.Drawing.Bitmap]::new($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $graphics = [System.Drawing.Graphics]::FromImage($result)
    try {
        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
        $graphics.DrawImage(
            $Source,
            [System.Drawing.Rectangle]::new(0, 0, $Width, $Height),
            $SourceRect.X,
            $SourceRect.Y,
            $SourceRect.Width,
            $SourceRect.Height,
            [System.Drawing.GraphicsUnit]::Pixel
        )
    }
    finally {
        $graphics.Dispose()
    }

    return $result
}

function Get-TileSpec {
    param([int]$TileRow, [int]$TileColumn)

    if ($TileRow -lt 1 -or $TileRow -gt 4 -or $TileColumn -lt 1 -or $TileColumn -gt 4) {
        throw "Row and Column must both be in the range 1..4."
    }

    $targetX = ($TileColumn - 1) * $tileWidth
    $targetY = ($TileRow - 1) * $tileHeight
    $cropX = [Math]::Min([Math]::Max($targetX - $horizontalMargin, 0), $mapWidth - $cropWidth)
    $cropY = [Math]::Min([Math]::Max($targetY - $verticalMargin, 0), $mapHeight - $cropHeight)

    return [pscustomobject]@{
        TargetX = $targetX
        TargetY = $targetY
        CropX = $cropX
        CropY = $cropY
        OffsetX = $targetX - $cropX
        OffsetY = $targetY - $cropY
    }
}

function Get-TileName {
    param([int]$TileRow, [int]$TileColumn)
    return "r{0}c{1}" -f $TileRow, $TileColumn
}

switch ($Operation) {
    "Prepare" {
        [System.IO.Directory]::CreateDirectory($referenceDir) | Out-Null
        [System.IO.Directory]::CreateDirectory($generatedDir) | Out-Null
        [System.IO.Directory]::CreateDirectory($tileDir) | Out-Null

        $resolvedSource = (Resolve-Path -LiteralPath $SourcePath).Path
        $source = [System.Drawing.Bitmap]::FromFile($resolvedSource)
        try {
            if ($source.Width -ne $mapWidth -or $source.Height -ne $mapHeight) {
                throw "Expected a 1280x720 source image, got $($source.Width)x$($source.Height)."
            }

            foreach ($tileRow in 1..4) {
                foreach ($tileColumn in 1..4) {
                    $spec = Get-TileSpec -TileRow $tileRow -TileColumn $tileColumn
                    $name = Get-TileName -TileRow $tileRow -TileColumn $tileColumn
                    $cropRect = [System.Drawing.Rectangle]::new($spec.CropX, $spec.CropY, $cropWidth, $cropHeight)
                    $reference = New-NearestNeighborBitmap -Source $source -SourceRect $cropRect -Width 1536 -Height 864
                    try {
                        $reference.Save((Join-Path $referenceDir "$name-reference-1536x864.png"), [System.Drawing.Imaging.ImageFormat]::Png)
                    }
                    finally {
                        $reference.Dispose()
                    }
                }
            }
        }
        finally {
            $source.Dispose()
        }

        Get-ChildItem -LiteralPath $referenceDir -Filter "*-reference-1536x864.png" |
            Sort-Object Name |
            Select-Object -ExpandProperty FullName
    }

    "Finalize" {
        if ([string]::IsNullOrWhiteSpace($GeneratedPath)) {
            throw "GeneratedPath is required for Finalize."
        }

        $spec = Get-TileSpec -TileRow $Row -TileColumn $Column
        $name = Get-TileName -TileRow $Row -TileColumn $Column
        $resolvedGenerated = (Resolve-Path -LiteralPath $GeneratedPath).Path
        [System.IO.Directory]::CreateDirectory($generatedDir) | Out-Null
        [System.IO.Directory]::CreateDirectory($tileDir) | Out-Null

        $generatedCopy = Join-Path $generatedDir "$name-generated-full.png"
        Copy-Item -LiteralPath $resolvedGenerated -Destination $generatedCopy -Force

        $generated = [System.Drawing.Bitmap]::FromFile($resolvedGenerated)
        try {
            $left = [Math]::Round(($spec.OffsetX / $cropWidth) * $generated.Width)
            $top = [Math]::Round(($spec.OffsetY / $cropHeight) * $generated.Height)
            $right = [Math]::Round((($spec.OffsetX + $tileWidth) / $cropWidth) * $generated.Width)
            $bottom = [Math]::Round((($spec.OffsetY + $tileHeight) / $cropHeight) * $generated.Height)
            $generatedRect = [System.Drawing.Rectangle]::new($left, $top, $right - $left, $bottom - $top)
            $tile = New-NearestNeighborBitmap -Source $generated -SourceRect $generatedRect -Width $tileWidth -Height $tileHeight
            try {
                $tilePath = Join-Path $tileDir "$name-320x180.png"
                $tile.Save($tilePath, [System.Drawing.Imaging.ImageFormat]::Png)
            }
            finally {
                $tile.Dispose()
            }
        }
        finally {
            $generated.Dispose()
        }

        Write-Output $tilePath
    }

    "Stitch" {
        $resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
        [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($resolvedOutput)) | Out-Null
        $map = [System.Drawing.Bitmap]::new($mapWidth, $mapHeight, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($map)
            try {
                $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
                foreach ($tileRow in 1..4) {
                    foreach ($tileColumn in 1..4) {
                        $name = Get-TileName -TileRow $tileRow -TileColumn $tileColumn
                        $tilePath = Join-Path $tileDir "$name-320x180.png"
                        if (-not (Test-Path -LiteralPath $tilePath)) {
                            throw "Missing finalized tile: $tilePath"
                        }

                        $tile = [System.Drawing.Bitmap]::FromFile($tilePath)
                        try {
                            $graphics.DrawImageUnscaled($tile, ($tileColumn - 1) * $tileWidth, ($tileRow - 1) * $tileHeight)
                        }
                        finally {
                            $tile.Dispose()
                        }
                    }
                }
            }
            finally {
                $graphics.Dispose()
            }

            $map.Save($resolvedOutput, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $map.Dispose()
        }

        Write-Output $resolvedOutput
    }
}
