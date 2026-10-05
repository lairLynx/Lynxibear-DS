$ErrorActionPreference = "Stop"

Add-Type -AssemblyName PresentationCore
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root = Split-Path -Parent $PSScriptRoot
$incoming = Join-Path $root "assets\incoming"
$spriteZipPath = Join-Path $incoming "Sprout Lands - Sprites - Basic pack.zip"
$uiZipPath = Join-Path $incoming "Sprout Lands - UI Pack - Basic pack.zip"
$sourceDir = Join-Path $root "source"

function Open-ZipImage {
    param(
        [System.IO.Compression.ZipArchive]$Archive,
        [string]$Suffix
    )

    $entry = $Archive.Entries | Where-Object { $_.FullName.EndsWith($Suffix, [StringComparison]::OrdinalIgnoreCase) } | Select-Object -First 1
    if ($null -eq $entry) {
        throw "Could not find '$Suffix' in $($Archive.Name)."
    }

    $stream = $entry.Open()
    $memory = [System.IO.MemoryStream]::new()
    try {
        $stream.CopyTo($memory)
        $memory.Position = 0
        $decoder = [Windows.Media.Imaging.BitmapDecoder]::Create(
            $memory,
            [Windows.Media.Imaging.BitmapCreateOptions]::PreservePixelFormat,
            [Windows.Media.Imaging.BitmapCacheOption]::OnLoad)
        $converted = [Windows.Media.Imaging.FormatConvertedBitmap]::new(
            $decoder.Frames[0],
            [Windows.Media.PixelFormats]::Bgra32,
            $null,
            0)
        $stride = $converted.PixelWidth * 4
        $pixels = [byte[]]::new($stride * $converted.PixelHeight)
        $converted.CopyPixels($pixels, $stride, 0)

        return [pscustomobject]@{
            Width = $converted.PixelWidth
            Height = $converted.PixelHeight
            Pixels = $pixels
        }
    }
    finally {
        $stream.Dispose()
        $memory.Dispose()
    }
}

function Get-RegionPixels {
    param(
        [pscustomobject]$Image,
        [int]$X,
        [int]$Y,
        [int]$Width,
        [int]$Height,
        [int]$OutputWidth = 16,
        [int]$OutputHeight = 16
    )

    if ($X -lt 0 -or $Y -lt 0 -or $X + $Width -gt $Image.Width -or $Y + $Height -gt $Image.Height) {
        throw "Sprite region ($X, $Y, $Width, $Height) is outside the source image."
    }

    $result = [byte[]]::new($OutputWidth * $OutputHeight * 4)
    for ($yOut = 0; $yOut -lt $OutputHeight; $yOut++) {
        $yIn = $Y + [Math]::Floor($yOut * $Height / $OutputHeight)
        for ($xOut = 0; $xOut -lt $OutputWidth; $xOut++) {
            $xIn = $X + [Math]::Floor($xOut * $Width / $OutputWidth)
            $sourceOffset = ($yIn * $Image.Width + $xIn) * 4
            $targetOffset = ($yOut * $OutputWidth + $xOut) * 4
            [Array]::Copy($Image.Pixels, $sourceOffset, $result, $targetOffset, 4)
        }
    }

    return ,$result
}

function Convert-FramesTo4Bpp {
    param([byte[][]]$Frames)

    $histogram = @{}
    foreach ($frame in $Frames) {
        for ($offset = 0; $offset -lt $frame.Length; $offset += 4) {
            if ($frame[$offset + 3] -eq 0) { continue }
            $key = "{0:X2}{1:X2}{2:X2}" -f $frame[$offset + 2], $frame[$offset + 1], $frame[$offset]
            if ($histogram.ContainsKey($key)) { $histogram[$key]++ }
            else { $histogram[$key] = 1 }
        }
    }

    $colors = @($histogram.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 15)
    $palette = [int[]]::new(16)
    $colorIndexes = @{}
    for ($i = 0; $i -lt $colors.Count; $i++) {
        $rgb = [Convert]::ToInt32($colors[$i].Key, 16)
        $red = ($rgb -shr 16) -band 0xFF
        $green = ($rgb -shr 8) -band 0xFF
        $blue = $rgb -band 0xFF
        $palette[$i + 1] = 0x8000 -bor (($red -shr 3) -bor (($green -shr 3) -shl 5) -bor (($blue -shr 3) -shl 10))
        $colorIndexes[$colors[$i].Key] = $i + 1
    }

    $packedFrames = [System.Collections.Generic.List[byte[]]]::new()
    foreach ($frame in $Frames) {
        $pixelCount = [int]($frame.Length / 4)
        $width = [int][Math]::Sqrt($pixelCount)
        if ($width * $width -ne $pixelCount -or ($width % 8) -ne 0) {
            throw "4bpp sprite frames must be square and divisible into 8x8 tiles."
        }

        $packed = [byte[]]::new([int][Math]::Ceiling($pixelCount / 2))
        $packedPixel = 0
        $tilesPerSide = [int]($width / 8)
        for ($tileY = 0; $tileY -lt $tilesPerSide; $tileY++) {
            for ($tileX = 0; $tileX -lt $tilesPerSide; $tileX++) {
                for ($pixelY = 0; $pixelY -lt 8; $pixelY++) {
                    for ($pixelX = 0; $pixelX -lt 8; $pixelX++) {
                        $x = $tileX * 8 + $pixelX
                        $y = $tileY * 8 + $pixelY
                        $offset = ($y * $width + $x) * 4
                        $index = 0
                        if ($frame[$offset + 3] -ne 0) {
                            $key = "{0:X2}{1:X2}{2:X2}" -f $frame[$offset + 2], $frame[$offset + 1], $frame[$offset]
                            if ($colorIndexes.ContainsKey($key)) {
                                $index = $colorIndexes[$key]
                            }
                            else {
                                $red = [int]$frame[$offset + 2]
                                $green = [int]$frame[$offset + 1]
                                $blue = [int]$frame[$offset]
                                $bestDistance = [int]::MaxValue
                                for ($colorIndex = 1; $colorIndex -le $colors.Count; $colorIndex++) {
                                    $candidate = [Convert]::ToInt32($colors[$colorIndex - 1].Key, 16)
                                    $dr = $red - (($candidate -shr 16) -band 0xFF)
                                    $dg = $green - (($candidate -shr 8) -band 0xFF)
                                    $db = $blue - ($candidate -band 0xFF)
                                    $distance = $dr * $dr + $dg * $dg + $db * $db
                                    if ($distance -lt $bestDistance) {
                                        $bestDistance = $distance
                                        $index = $colorIndex
                                    }
                                }
                            }
                        }

                        $packedOffset = $packedPixel -shr 1
                        if (($packedPixel -band 1) -eq 0) {
                            $packed[$packedOffset] = [byte]$index
                        }
                        else {
                            $packed[$packedOffset] = $packed[$packedOffset] -bor [byte]($index -shl 4)
                        }
                        $packedPixel++
                    }
                }
            }
        }
        $packedFrames.Add($packed)
    }

    return [pscustomobject]@{ Frames = $packedFrames.ToArray(); Palette = $palette }
}

function Get-PaddedPlayerFrame {
    param([pscustomobject]$Image, [int]$X, [int]$Y)

    $scaled = Get-RegionPixels $Image $X $Y 16 16 24 24
    $canvas = [byte[]]::new(32 * 32 * 4)
    for ($row = 0; $row -lt 24; $row++) {
        [Array]::Copy($scaled, $row * 24 * 4, $canvas, (($row + 4) * 32 + 4) * 4, 24 * 4)
    }
    return ,$canvas
}

function Write-ByteArray {
    param([System.Text.StringBuilder]$Builder, [string]$Name, [byte[]]$Values)
    [void]$Builder.AppendLine("const u8 $Name[$($Values.Length)] = {")
    for ($i = 0; $i -lt $Values.Length; $i += 16) {
        $end = [Math]::Min($i + 15, $Values.Length - 1)
        $row = for ($j = $i; $j -le $end; $j++) { "0x{0:X2}" -f $Values[$j] }
        [void]$Builder.AppendLine("    " + ($row -join ", ") + ",")
    }
    [void]$Builder.AppendLine("};")
    [void]$Builder.AppendLine()
}

function Write-Palette {
    param([System.Text.StringBuilder]$Builder, [string]$Name, [int[]]$Values)
    [void]$Builder.AppendLine("const u16 $Name[16] = {")
    for ($i = 0; $i -lt 16; $i += 8) {
        $end = [Math]::Min($i + 7, 15)
        $row = for ($j = $i; $j -le $end; $j++) { "0x{0:X4}" -f $Values[$j] }
        [void]$Builder.AppendLine("    " + ($row -join ", ") + ",")
    }
    [void]$Builder.AppendLine("};")
    [void]$Builder.AppendLine()
}

if (-not (Test-Path -LiteralPath $spriteZipPath) -or -not (Test-Path -LiteralPath $uiZipPath)) {
    throw "Place both free Sprout Lands Basic ZIPs in assets\incoming before running this script."
}

$spriteArchive = [System.IO.Compression.ZipFile]::OpenRead($spriteZipPath)
$uiArchive = [System.IO.Compression.ZipFile]::OpenRead($uiZipPath)
try {
    $playerSheet = Open-ZipImage $spriteArchive "Characters/Basic Charakter Spritesheet.png"
    $actionSheet = Open-ZipImage $spriteArchive "Characters/Basic Charakter Actions.png"
    $plantSheet = Open-ZipImage $spriteArchive "Objects/Basic Plants.png"
    $utilitySheet = Open-ZipImage $spriteArchive "Objects/Basic tools and meterials.png"
    $environmentSheet = Open-ZipImage $spriteArchive "Objects/Basic Grass Biom things 1.png"
    $grassTilesSheet = Open-ZipImage $spriteArchive "Tilesets/Grass.png"
    $soilTilesSheet = Open-ZipImage $spriteArchive "Tilesets/Tilled Dirt.png"
    $emojiSheet = Open-ZipImage $uiArchive "emojis-free/Emoji_Spritesheet_Free.png"
    $settingsIconSheet = Open-ZipImage $uiArchive "Sprite sheets/Icons/All Icons.png"
    $slotSheet = Open-ZipImage $uiArchive "emojis-free/emoji style ui/Inventory_Blocks_Spritesheet.png"
    $buttonSheet = Open-ZipImage $uiArchive "Sprite sheets/UI Big Play Button.png"
    Write-Output "Sprite sheets: player $($playerSheet.Width)x$($playerSheet.Height), actions $($actionSheet.Width)x$($actionSheet.Height), plants $($plantSheet.Width)x$($plantSheet.Height), utility $($utilitySheet.Width)x$($utilitySheet.Height), environment $($environmentSheet.Width)x$($environmentSheet.Height), icons $($emojiSheet.Width)x$($emojiSheet.Height), slots $($slotSheet.Width)x$($slotSheet.Height)"

    # Sheet rows are down, up, left, right; each has 4 walk frames in 48x48 cells.
    $playerFrames = [byte[][]]::new(16)
    for ($direction = 0; $direction -lt 4; $direction++) {
        for ($step = 0; $step -lt 4; $step++) {
            $playerFrames[$direction * 4 + $step] = Get-PaddedPlayerFrame $playerSheet ($step * 48 + 16) ($direction * 48 + 16)
        }
    }
    $playerAssets = Convert-FramesTo4Bpp $playerFrames

    $utilityRegions = @(
        @(16, 0), # Axe
        @(32, 0), # Hoe
        @(0, 0),  # Watering can
        @(32, 16) # Wood
    )
    $utilityFrames = [byte[][]]::new($utilityRegions.Count)
    for ($i = 0; $i -lt $utilityRegions.Count; $i++) {
        $utilityFrames[$i] = Get-RegionPixels $utilitySheet $utilityRegions[$i][0] $utilityRegions[$i][1] 16 16
    }
    $utilityAssets = Convert-FramesTo4Bpp $utilityFrames

    # Actions sheet: 2 columns (wind-up, strike) of 48x48 cells; every 4 rows are
    # one tool (axe, hoe, watering can) in down, up, left, right order. A 42x42
    # crop keeps the tool in view and is scaled 1.5x (same as the walk frames)
    # into a 64x64 sprite, so the body sits 18 px in from the sprite corner.
    $actionFrames = [byte[][]]::new(24)
    for ($row = 0; $row -lt 12; $row++) {
        for ($column = 0; $column -lt 2; $column++) {
            $scaled = Get-RegionPixels $actionSheet ($column * 48 + 4) ($row * 48 + 4) 42 42 63 63
            $canvas = [byte[]]::new(64 * 64 * 4)
            for ($line = 0; $line -lt 63; $line++) {
                [Array]::Copy($scaled, $line * 63 * 4, $canvas, $line * 64 * 4, 63 * 4)
            }
            $actionFrames[$row * 2 + $column] = $canvas
        }
    }
    $actionAssets = Convert-FramesTo4Bpp $actionFrames

    # Blank 96x32 menu button (normal, pressed), cut into three 32x32 pieces each.
    $buttonFrames = [byte[][]]::new(6)
    for ($state = 0; $state -lt 2; $state++) {
        for ($piece = 0; $piece -lt 3; $piece++) {
            $buttonFrames[$state * 3 + $piece] = Get-RegionPixels $buttonSheet ($state * 96 + $piece * 32) 0 32 32 32 32
        }
    }
    $buttonAssets = Convert-FramesTo4Bpp $buttonFrames

    $treeFrames = [byte[][]]::new(1)
    $treeFrames[0] = Get-RegionPixels $environmentSheet 16 0 32 32 32 32
    $treeAssets = Convert-FramesTo4Bpp $treeFrames

    $seedFrame = Get-RegionPixels $plantSheet 0 0 16 16
    $singleFrame = [byte[][]]::new(1)
    $singleFrame[0] = $seedFrame
    $seedAssets = Convert-FramesTo4Bpp -Frames $singleFrame

    $cropRegions = @(
        @(128, 416), # Parsnip: carrot
        @(64, 416),  # Tomato
        @(0, 416),   # Pumpkin: fruit
        @(96, 384),  # Yam: earthy mushroom
        @(0, 384),   # Potato: sprout
        @(32, 384),  # Cauliflower: flower
        @(96, 416),  # Corn
        @(128, 416), # Winter Root: carrot
        @(64, 384)   # Snow Yam: clover
    )
    $cropAssets = [System.Collections.Generic.List[object]]::new()
    foreach ($region in $cropRegions) {
        $frame = Get-RegionPixels $emojiSheet $region[0] $region[1] 32 32
        $singleFrame[0] = $frame
        $cropAssets.Add((Convert-FramesTo4Bpp -Frames $singleFrame))
    }

    if ($seedAssets.Frames[0].Length -ne 128 -or
        @($cropAssets | Where-Object { $_.Frames[0].Length -ne 128 }).Count -ne 0) {
        throw "Expected every 16x16 item sprite to pack to exactly 128 bytes."
    }
    foreach ($asset in $cropAssets) {
        $hasVisibleColor = $false
        foreach ($color in $asset.Palette) {
            if ($color -ne 0) {
                $hasVisibleColor = $true
                break
            }
        }
        if (-not $hasVisibleColor) {
            throw "A crop icon source region was blank; check cropRegions before generating."
        }
    }

    $slotFrames = [byte[][]]::new(2)
    $slotFrames[0] = Get-RegionPixels $slotSheet 0 0 48 48 32 32
    $slotFrames[1] = Get-RegionPixels $slotSheet 48 0 48 48 32 32
    $slotAssets = Convert-FramesTo4Bpp $slotFrames
    $settingsFrame = Get-RegionPixels $settingsIconSheet 48 0 16 16
    $singleFrame[0] = $settingsFrame
    $settingsIconAssets = Convert-FramesTo4Bpp -Frames $singleFrame

    $grassFrames = [byte[][]]::new(4)
    $grassRegions = @(@(16, 16), @(0, 80), @(48, 80), @(64, 80))
    for ($i = 0; $i -lt $grassRegions.Count; $i++) {
        $grassFrames[$i] = Get-RegionPixels $grassTilesSheet $grassRegions[$i][0] $grassRegions[$i][1] 16 16
    }
    $grassAssets = Convert-FramesTo4Bpp $grassFrames

    $soilFrames = [byte[][]]::new(3)
    $soilRegions = @(@(0, 0), @(16, 0), @(32, 0))
    for ($i = 0; $i -lt $soilRegions.Count; $i++) {
        $soilFrames[$i] = Get-RegionPixels $soilTilesSheet $soilRegions[$i][0] $soilRegions[$i][1] 16 16
    }
    $soilAssets = Convert-FramesTo4Bpp $soilFrames

    $growthFrames = [byte[][]]::new(4)
    $growthRegions = @(@(16, 0), @(32, 0), @(48, 0), @(64, 0))
    for ($i = 0; $i -lt $growthRegions.Count; $i++) {
        $growthFrames[$i] = Get-RegionPixels $plantSheet $growthRegions[$i][0] $growthRegions[$i][1] 16 16
    }
    $growthAssets = Convert-FramesTo4Bpp $growthFrames

    if ($playerAssets.Frames[0].Length -ne 512 -or
        @($utilityAssets.Frames | Where-Object { $_.Length -ne 128 }).Count -ne 0 -or
        @($actionAssets.Frames | Where-Object { $_.Length -ne 2048 }).Count -ne 0 -or
        $treeAssets.Frames[0].Length -ne 512 -or
        @($buttonAssets.Frames | Where-Object { $_.Length -ne 512 }).Count -ne 0 -or
        $seedAssets.Frames[0].Length -ne 128 -or
        $settingsIconAssets.Frames[0].Length -ne 128 -or
        @($grassAssets.Frames | Where-Object { $_.Length -ne 128 }).Count -ne 0 -or
        @($soilAssets.Frames | Where-Object { $_.Length -ne 128 }).Count -ne 0 -or
        @($growthAssets.Frames | Where-Object { $_.Length -ne 128 }).Count -ne 0 -or
        $slotAssets.Frames[0].Length -ne 512 -or
        @($cropAssets | Where-Object { $_.Frames[0].Length -ne 128 }).Count -ne 0) {
        throw "Converted sprite dimensions do not match the expected OAM tile sizes."
    }

    $header = @'
#ifndef STARDSI_SPROUT_ASSETS_H
#define STARDSI_SPROUT_ASSETS_H

#include <nds.h>

extern const u8 sproutPlayerFrames[16][512];
extern const u16 sproutPlayerPalette[16];
extern const u8 sproutUtilityIcons[4][128];
extern const u16 sproutUtilityPalette[16];
extern const u8 sproutToolActions[24][2048];
extern const u16 sproutToolActionPalette[16];
extern const u8 sproutMenuButton[6][512];
extern const u16 sproutMenuButtonPalette[16];
extern const u8 sproutTreeSprite[512];
extern const u16 sproutTreePalette[16];
extern const u8 sproutSeedIcon[128];
extern const u16 sproutSeedPalette[16];
extern const u8 sproutCropIcons[9][128];
extern const u16 sproutCropPalettes[9][16];
extern const u8 sproutSlotNormal[512];
extern const u8 sproutSlotSelected[512];
extern const u16 sproutSlotPalette[16];
extern const u8 sproutSettingsIcon[128];
extern const u16 sproutSettingsPalette[16];
extern const u8 sproutGrassTiles[4][128];
extern const u16 sproutGrassPalette[16];
extern const u8 sproutSoilTiles[3][128];
extern const u16 sproutSoilPalette[16];
extern const u8 sproutGrowthStages[4][128];
extern const u16 sproutGrowthPalette[16];

#endif
'@
    [System.IO.File]::WriteAllText((Join-Path $sourceDir "sprout_assets.h"), $header, [System.Text.Encoding]::ASCII)

    $builder = [System.Text.StringBuilder]::new()
    [void]$builder.AppendLine('#include "sprout_assets.h"')
    [void]$builder.AppendLine()
    [void]$builder.AppendLine("/* Sprout Lands Basic assets by Cup Nooble; see README for terms. */")
    [void]$builder.AppendLine()
    [void]$builder.AppendLine("const u8 sproutPlayerFrames[16][512] = {")
    foreach ($frame in $playerAssets.Frames) {
        $row = for ($i = 0; $i -lt $frame.Length; $i++) { "0x{0:X2}" -f $frame[$i] }
        [void]$builder.AppendLine("    { " + ($row -join ", ") + " },")
    }
    [void]$builder.AppendLine("};")
    [void]$builder.AppendLine()
    Write-Palette $builder "sproutPlayerPalette" $playerAssets.Palette
    [void]$builder.AppendLine("const u8 sproutUtilityIcons[4][128] = {")
    foreach ($frame in $utilityAssets.Frames) {
        $row = for ($i = 0; $i -lt $frame.Length; $i++) { "0x{0:X2}" -f $frame[$i] }
        [void]$builder.AppendLine("    { " + ($row -join ", ") + " },")
    }
    [void]$builder.AppendLine("};")
    [void]$builder.AppendLine()
    Write-Palette $builder "sproutUtilityPalette" $utilityAssets.Palette
    [void]$builder.AppendLine("const u8 sproutToolActions[24][2048] = {")
    foreach ($frame in $actionAssets.Frames) {
        $row = for ($i = 0; $i -lt $frame.Length; $i++) { "0x{0:X2}" -f $frame[$i] }
        [void]$builder.AppendLine("    { " + ($row -join ", ") + " },")
    }
    [void]$builder.AppendLine("};")
    [void]$builder.AppendLine()
    Write-Palette $builder "sproutToolActionPalette" $actionAssets.Palette
    [void]$builder.AppendLine("const u8 sproutMenuButton[6][512] = {")
    foreach ($frame in $buttonAssets.Frames) {
        $row = for ($i = 0; $i -lt $frame.Length; $i++) { "0x{0:X2}" -f $frame[$i] }
        [void]$builder.AppendLine("    { " + ($row -join ", ") + " },")
    }
    [void]$builder.AppendLine("};")
    [void]$builder.AppendLine()
    Write-Palette $builder "sproutMenuButtonPalette" $buttonAssets.Palette
    Write-ByteArray $builder "sproutTreeSprite" $treeAssets.Frames[0]
    Write-Palette $builder "sproutTreePalette" $treeAssets.Palette
    Write-ByteArray $builder "sproutSeedIcon" $seedAssets.Frames[0]
    Write-Palette $builder "sproutSeedPalette" $seedAssets.Palette

    [void]$builder.AppendLine("const u8 sproutCropIcons[9][128] = {")
    foreach ($asset in $cropAssets) {
        $values = $asset.Frames[0]
        $row = for ($i = 0; $i -lt $values.Length; $i++) { "0x{0:X2}" -f $values[$i] }
        [void]$builder.AppendLine("    { " + ($row -join ", ") + " },")
    }
    [void]$builder.AppendLine("};")
    [void]$builder.AppendLine()

    [void]$builder.AppendLine("const u16 sproutCropPalettes[9][16] = {")
    foreach ($asset in $cropAssets) {
        $values = $asset.Palette
        $row = for ($i = 0; $i -lt $values.Length; $i++) { "0x{0:X4}" -f $values[$i] }
        [void]$builder.AppendLine("    { " + ($row -join ", ") + " },")
    }
    [void]$builder.AppendLine("};")
    [void]$builder.AppendLine()

    Write-ByteArray $builder "sproutSlotNormal" $slotAssets.Frames[0]
    Write-ByteArray $builder "sproutSlotSelected" $slotAssets.Frames[1]
    Write-Palette $builder "sproutSlotPalette" $slotAssets.Palette
    Write-ByteArray $builder "sproutSettingsIcon" $settingsIconAssets.Frames[0]
    Write-Palette $builder "sproutSettingsPalette" $settingsIconAssets.Palette
    [void]$builder.AppendLine("const u8 sproutGrassTiles[4][128] = {")
    foreach ($frame in $grassAssets.Frames) {
        $row = for ($i = 0; $i -lt $frame.Length; $i++) { "0x{0:X2}" -f $frame[$i] }
        [void]$builder.AppendLine("    { " + ($row -join ", ") + " },")
    }
    [void]$builder.AppendLine("};")
    [void]$builder.AppendLine()
    Write-Palette $builder "sproutGrassPalette" $grassAssets.Palette
    [void]$builder.AppendLine("const u8 sproutSoilTiles[3][128] = {")
    foreach ($frame in $soilAssets.Frames) {
        $row = for ($i = 0; $i -lt $frame.Length; $i++) { "0x{0:X2}" -f $frame[$i] }
        [void]$builder.AppendLine("    { " + ($row -join ", ") + " },")
    }
    [void]$builder.AppendLine("};")
    [void]$builder.AppendLine()
    Write-Palette $builder "sproutSoilPalette" $soilAssets.Palette
    [void]$builder.AppendLine("const u8 sproutGrowthStages[4][128] = {")
    foreach ($frame in $growthAssets.Frames) {
        $row = for ($i = 0; $i -lt $frame.Length; $i++) { "0x{0:X2}" -f $frame[$i] }
        [void]$builder.AppendLine("    { " + ($row -join ", ") + " },")
    }
    [void]$builder.AppendLine("};")
    [void]$builder.AppendLine()
    Write-Palette $builder "sproutGrowthPalette" $growthAssets.Palette
    [System.IO.File]::WriteAllText(
        (Join-Path $sourceDir "sprout_assets.c"),
        $builder.ToString(),
        [System.Text.Encoding]::ASCII)
}
finally {
    $spriteArchive.Dispose()
    $uiArchive.Dispose()
}

Write-Output "Generated source/sprout_assets.c and source/sprout_assets.h."
