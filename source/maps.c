#include "maps.h"

#include "game.h"
#include "items.h"
#include "sprout_assets.h"

static u16 *frameBuffer;

static const u16 grassBaseColor = COLOR(15, 23, 10);
static const u16 soilBaseColor = COLOR(17, 11, 6);

static void fillRect(int x, int y, int width, int height, u16 color)
{
    for (int row = y; row < y + height; row++)
    {
        if (row < 0 || row >= SCREEN_HEIGHT)
            continue;

        for (int column = x; column < x + width; column++)
        {
            if (column >= 0 && column < SCREEN_WIDTH)
                frameBuffer[row * SCREEN_WIDTH + column] = color;
        }
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
            int screenX = x + column;
            int screenY = y + row;
            if (screenX >= 0 && screenX < SCREEN_WIDTH &&
                screenY >= 0 && screenY < SCREEN_HEIGHT)
                frameBuffer[screenY * SCREEN_WIDTH + screenX] = color;
        }
    }
}

static void drawTexturedArea(const u8 *sprite, const u16 *palette,
                             int x, int y, int width, int height,
                             unsigned shade)
{
    fillRect(x, y, width, height, soilBaseColor);
    for (int row = 0; row < height; row += 16)
    {
        for (int column = 0; column < width; column += 16)
            drawPackedTile(sprite, palette, x + column, y + row, shade);
    }
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
                    frameBuffer[(y + row + 2) * SCREEN_WIDTH +
                                x + column + 8] =
                        sproutCropPalettes[cropIndex][index];
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
    for (int row = 0; row < 32; row++)
    {
        for (int column = 0; column < 32; column++)
        {
            unsigned paletteIndex = packedSpritePixel(sproutTreeSprite,
                                                       column, row);
            if (paletteIndex != 0)
                frameBuffer[(y + row) * SCREEN_WIDTH + x + column] =
                    sproutTreePalette[paletteIndex];
        }
    }
}

static void drawFarm(void)
{
    drawGrassMeadow();

    for (int row = 0; row < FIELD_ROWS; row++)
    {
        for (int column = 0; column < FIELD_COLUMNS; column++)
        {
            const FarmTile *tile = &game.farm[row][column];
            int x = FIELD_LEFT + column * TILE_SIZE;
            int y = FIELD_TOP + row * TILE_SIZE;
            if (tile->tilled)
            {
                unsigned texture = (unsigned)(row * 2 + column) % 3;
                drawTexturedArea(sproutSoilTiles[texture], sproutSoilPalette,
                                 x, y, TILE_SIZE - 1, TILE_SIZE - 1,
                                 tile->watered ? 3 : 4);
            }

            if (tile->crop != CROP_EMPTY)
                drawCrop(tile, x, y);
        }
    }

    for (int tile = 0; tile < FARM_TILE_COUNT; tile++)
    {
        if ((game.treeMask & ((u64)1 << tile)) == 0)
            continue;
        int x = FIELD_LEFT + (tile % FIELD_COLUMNS) * TILE_SIZE - 2;
        int y = FIELD_TOP + (tile / FIELD_COLUMNS) * TILE_SIZE - 4;
        drawTree(x, y);
    }

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

void mapsDraw(int mapId)
{
    if (mapId == VIEW_TOWN)
        drawTown();
    else
        drawFarm();
}
