#include "maps.h"

#include "game.h"
#include "items.h"
#include "sprout_assets.h"

#include <string.h>

static u16 *frameBuffer;

// All drawing is clipped to this rectangle so a partial redraw can repaint
// a few tiles without touching the rest of the bitmap.
static int clipLeft = 0;
static int clipTop = 0;
static int clipRight = SCREEN_WIDTH;
static int clipBottom = SCREEN_HEIGHT;

typedef struct
{
    u8 tilled;
    u8 watered;
    u8 crop;
    u8 stage;
    u8 tree;
} TileLook;

static TileLook drawnLook[FARM_TILE_COUNT];
static bool farmDrawn;

static void setClip(int left, int top, int right, int bottom)
{
    clipLeft = left < 0 ? 0 : left;
    clipTop = top < 0 ? 0 : top;
    clipRight = right > SCREEN_WIDTH ? SCREEN_WIDTH : right;
    clipBottom = bottom > SCREEN_HEIGHT ? SCREEN_HEIGHT : bottom;
}

static void resetClip(void)
{
    setClip(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

static void putPixel(int x, int y, u16 color)
{
    if (x >= clipLeft && x < clipRight && y >= clipTop && y < clipBottom)
        frameBuffer[y * SCREEN_WIDTH + x] = color;
}

static bool intersectsClip(int x, int y, int width, int height)
{
    return x < clipRight && x + width > clipLeft &&
           y < clipBottom && y + height > clipTop;
}

static const u16 grassBaseColor = COLOR(15, 23, 10);
static const u16 soilBaseColor = COLOR(17, 11, 6);

static void fillRect(int x, int y, int width, int height, u16 color)
{
    int top = y > clipTop ? y : clipTop;
    int bottom = y + height < clipBottom ? y + height : clipBottom;
    int left = x > clipLeft ? x : clipLeft;
    int right = x + width < clipRight ? x + width : clipRight;

    for (int row = top; row < bottom; row++)
    {
        for (int column = left; column < right; column++)
            frameBuffer[row * SCREEN_WIDTH + column] = color;
    }
}

static void drawRect(int x, int y, int width, int height, u16 color)
{
    fillRect(x, y, width, 1, color);
    fillRect(x, y + height - 1, width, 1, color);
    fillRect(x, y, 1, height, color);
    fillRect(x + width - 1, y, 1, height, color);
}

static unsigned packedTilePixel(const u8 *sprite, int x, int y)
{
    int tile = (y / 8) * 2 + x / 8;
    int pixel = tile * 64 + (y % 8) * 8 + x % 8;
    u8 packed = sprite[pixel / 2];
    return pixel & 1 ? packed >> 4 : packed & 0x0F;
}

static u16 shadeColor(u16 color, unsigned numerator, unsigned denominator)
{
    unsigned red = color & 31;
    unsigned green = (color >> 5) & 31;
    unsigned blue = (color >> 10) & 31;
    return COLOR(red * numerator / denominator,
                 green * numerator / denominator,
                 blue * numerator / denominator);
}

static void drawPackedTile(const u8 *sprite, const u16 *palette,
                           int x, int y, unsigned shade)
{
    if (!intersectsClip(x, y, 16, 16))
        return;

    for (int row = 0; row < 16; row++)
    {
        for (int column = 0; column < 16; column++)
        {
            unsigned index = packedTilePixel(sprite, column, row);
            if (index == 0)
                continue;

            u16 color = palette[index];
            if (shade < 5)
                color = shadeColor(color, shade, 5);
            putPixel(x + column, y + row, color);
        }
    }
}

static void drawTexturedArea(const u8 *sprite, const u16 *palette,
                             int x, int y, int width, int height,
                             unsigned shade)
{
    fillRect(x, y, width, height, soilBaseColor);
    // The 16 px texture tiles overhang the area; keep them inside it.
    int savedLeft = clipLeft, savedTop = clipTop;
    int savedRight = clipRight, savedBottom = clipBottom;
    setClip(x > clipLeft ? x : clipLeft, y > clipTop ? y : clipTop,
            x + width < clipRight ? x + width : clipRight,
            y + height < clipBottom ? y + height : clipBottom);
    for (int row = 0; row < height; row += 16)
    {
        for (int column = 0; column < width; column += 16)
            drawPackedTile(sprite, palette, x + column, y + row, shade);
    }
    setClip(savedLeft, savedTop, savedRight, savedBottom);
}

static void drawGrassMeadow(void)
{
    fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, grassBaseColor);
    for (int row = 0; row < SCREEN_HEIGHT; row += 16)
    {
        for (int column = 0; column < SCREEN_WIDTH; column += 16)
            drawPackedTile(sproutGrassTiles[0], sproutGrassPalette,
                           column, row, 5);
    }

    for (int row = 0; row < 6; row++)
    {
        for (int column = 0; column < 8; column++)
        {
            if ((row * 5 + column * 3) % 3 == 0)
                continue;

            int x = column * 32 + ((row * 7 + column * 5) % 16);
            int y = row * 32 + ((row * 11 + column * 3) % 16);
            unsigned texture = 2 + (unsigned)((row + column) & 1);
            drawPackedTile(sproutGrassTiles[texture], sproutGrassPalette,
                           x, y, 5);
        }
    }
}

static void drawCrop(const FarmTile *tile, int x, int y)
{
    const CropInfo *crop = &crops[tile->crop - 1];
    unsigned stage = tile->growth * 4 / crop->daysToGrow;
    if (stage > 3)
        stage = 3;

    drawPackedTile(sproutGrowthStages[stage], sproutGrowthPalette,
                   x + 6, y + 5, 5);
    if (tile->growth >= crop->daysToGrow)
    {
        unsigned cropIndex = tile->crop - 1;
        for (int row = 0; row < 12; row++)
        {
            for (int column = 0; column < 12; column++)
            {
                unsigned index = packedTilePixel(
                    sproutCropIcons[cropIndex], column, row);
                if (index != 0)
                    putPixel(x + column + 8, y + row + 2,
                             sproutCropPalettes[cropIndex][index]);
            }
        }
    }
}

static unsigned packedSpritePixel(const u8 *sprite, int x, int y)
{
    int tile = (y / 8) * 4 + x / 8;
    int pixel = tile * 64 + (y % 8) * 8 + x % 8;
    u8 packed = sprite[pixel / 2];
    return pixel & 1 ? packed >> 4 : packed & 0x0F;
}

static void drawTree(int x, int y)
{
    if (!intersectsClip(x, y, 32, 32))
        return;

    for (int row = 0; row < 32; row++)
    {
        for (int column = 0; column < 32; column++)
        {
            unsigned paletteIndex = packedSpritePixel(sproutTreeSprite,
                                                       column, row);
            if (paletteIndex != 0)
                putPixel(x + column, y + row,
                         sproutTreePalette[paletteIndex]);
        }
    }
}

static TileLook tileLook(int tile)
{
    const FarmTile *farmTile = &game.farm[tile / FIELD_COLUMNS]
                                         [tile % FIELD_COLUMNS];
    TileLook look = { farmTile->tilled, farmTile->watered, farmTile->crop,
                      0, (u8)((game.treeMask >> tile) & 1) };

    if (farmTile->crop != CROP_EMPTY)
    {
        const CropInfo *crop = &crops[farmTile->crop - 1];
        unsigned stage = farmTile->growth * 4 / crop->daysToGrow;
        if (stage > 3)
            stage = 3;
        look.stage = (u8)stage;
        if (farmTile->growth >= crop->daysToGrow)
            look.stage |= 4;
    }
    return look;
}

static void drawFarmTile(int tile)
{
    int row = tile / FIELD_COLUMNS;
    int column = tile % FIELD_COLUMNS;
    const FarmTile *farmTile = &game.farm[row][column];
    int x = FIELD_LEFT + column * TILE_SIZE;
    int y = FIELD_TOP + row * TILE_SIZE;

    if (!intersectsClip(x, y, TILE_SIZE, TILE_SIZE))
        return;

    if (farmTile->tilled)
    {
        unsigned texture = (unsigned)(row * 2 + column) % 3;
        drawTexturedArea(sproutSoilTiles[texture], sproutSoilPalette,
                         x, y, TILE_SIZE - 1, TILE_SIZE - 1,
                         farmTile->watered ? 3 : 4);
    }

    if (farmTile->crop != CROP_EMPTY)
        drawCrop(farmTile, x, y);
}

static void drawFarmTree(int tile)
{
    if ((game.treeMask & ((u64)1 << tile)) == 0)
        return;
    drawTree(FIELD_LEFT + (tile % FIELD_COLUMNS) * TILE_SIZE - 2,
             FIELD_TOP + (tile / FIELD_COLUMNS) * TILE_SIZE - 4);
}

// Repaints everything inside the current clip rectangle.
static void drawFarmClipped(void)
{
    drawGrassMeadow();
    for (int tile = 0; tile < FARM_TILE_COUNT; tile++)
        drawFarmTile(tile);
    for (int tile = 0; tile < FARM_TILE_COUNT; tile++)
        drawFarmTree(tile);
}

static void drawFarm(void)
{
    if (!farmDrawn)
    {
        resetClip();
        drawFarmClipped();
        for (int tile = 0; tile < FARM_TILE_COUNT; tile++)
            drawnLook[tile] = tileLook(tile);
        farmDrawn = true;
        return;
    }

    // Repaint only tiles whose appearance changed. A tree sprite overhangs
    // its tile (2 px sideways, 4 px upward), so the clip covers that too.
    for (int tile = 0; tile < FARM_TILE_COUNT; tile++)
    {
        TileLook now = tileLook(tile);
        if (memcmp(&now, &drawnLook[tile], sizeof(now)) == 0)
            continue;

        int x = FIELD_LEFT + (tile % FIELD_COLUMNS) * TILE_SIZE;
        int y = FIELD_TOP + (tile / FIELD_COLUMNS) * TILE_SIZE;
        setClip(x - 2, y - 4, x + TILE_SIZE + 2, y + TILE_SIZE);
        drawFarmClipped();
        drawnLook[tile] = now;
    }
    resetClip();
}
static void drawTown(void)
{
    fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR(15, 23, 30));
    fillRect(0, 37, SCREEN_WIDTH, 139, COLOR(8, 18, 10));
    fillRect(0, 91, SCREEN_WIDTH, 31, COLOR(21, 16, 10));
    fillRect(0, 104, SCREEN_WIDTH, 5, COLOR(25, 19, 11));
    fillRect(0, 178, SCREEN_WIDTH, 14, COLOR(6, 15, 8));

    fillRect(17, 55, 64, 31, COLOR(18, 12, 9));
    fillRect(23, 44, 52, 13, COLOR(25, 8, 7));
    fillRect(28, 61, 13, 25, COLOR(25, 20, 12));
    fillRect(51, 62, 22, 13, COLOR(8, 20, 23));
    fillRect(95, 52, 65, 34, COLOR(17, 13, 9));
    fillRect(91, 42, 73, 12, COLOR(24, 19, 11));
    fillRect(105, 64, 8, 22, COLOR(12, 24, 14));
    fillRect(125, 63, 27, 14, COLOR(8, 18, 20));
    fillRect(181, 59, 53, 27, COLOR(14, 11, 9));
    fillRect(176, 50, 63, 10, COLOR(23, 18, 11));
    fillRect(188, 64, 12, 17, COLOR(27, 24, 16));
    fillRect(207, 66, 19, 15, COLOR(9, 18, 22));

    u16 outline = COLOR(31, 27, 13);
    if (townFocus == TOWN_STORE)
        drawRect(14, 41, 69, 48, outline);
    else if (townFocus == TOWN_BOARD)
        drawRect(88, 39, 80, 52, outline);
    else
        drawRect(173, 46, 69, 46, outline);

    fillRect(102, 129, 52, 35, COLOR(21, 15, 9));
    fillRect(110, 121, 35, 10, COLOR(26, 20, 11));
    fillRect(124, 142, 10, 22, COLOR(9, 20, 12));
    fillRect(27, 96, 2, 2, COLOR(31, 26, 17));
    fillRect(221, 97, 2, 2, COLOR(31, 26, 17));
    fillRect(0, 188, SCREEN_WIDTH, 4, COLOR(20, 25, 17));
}

void mapsInitialize(u16 *bitmap)
{
    frameBuffer = bitmap;
}

u16 *mapsBitmap(void)
{
    return frameBuffer;
}

static void drawScaledSprite(const u8 *sprite, const u16 *palette,
                             int x, int y, int scale)
{
    for (int row = 0; row < 32; row++)
    {
        for (int column = 0; column < 32; column++)
        {
            unsigned index = packedSpritePixel(sprite, column, row);
            if (index != 0)
                fillRect(x + column * scale, y + row * scale, scale, scale,
                         palette[index]);
        }
    }
}

// Title scenery for the main menu; the caller adds the lettering.
void mapsDrawTitle(void)
{
    resetClip();
    farmDrawn = false;
    drawGrassMeadow();
    drawScaledSprite(sproutTreeSprite, sproutTreePalette, 14, 112, 2);
    drawScaledSprite(sproutTreeSprite, sproutTreePalette, 178, 112, 2);
    drawScaledSprite(sproutPlayerFrames[0], sproutPlayerPalette, 96, 112, 2);
}

void mapsDraw(int mapId)
{
    resetClip();
    if (mapId == VIEW_TOWN)
    {
        drawTown();
        farmDrawn = false;
    }
    else
    {
        drawFarm();
    }
}
