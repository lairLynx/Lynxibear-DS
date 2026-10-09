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
static int drawnScreen;

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
    const FarmTile *farmTile = &CURRENT_FARM[tile / FIELD_COLUMNS]
                                         [tile % FIELD_COLUMNS];
    TileLook look = { farmTile->tilled, farmTile->watered, farmTile->crop,
                      0, (u8)((CURRENT_TREES >> tile) & 1) };

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
    const FarmTile *farmTile = &CURRENT_FARM[row][column];
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
    if ((CURRENT_TREES & ((u64)1 << tile)) == 0)
        return;
    drawTree(FIELD_LEFT + (tile % FIELD_COLUMNS) * TILE_SIZE - 2,
             FIELD_TOP + (tile / FIELD_COLUMNS) * TILE_SIZE - 4);
}

// Dirt path with a few darker flecks; flecks are placed from the absolute
// coordinates so partial and full redraws match.
static void drawPathArea(int x, int y, int width, int height)
{
    static const u16 pathBase = COLOR(24, 19, 13);
    static const u16 pathFleck = COLOR(21, 15, 11);
    fillRect(x, y, width, height, pathBase);
    for (int row = y; row < y + height; row += 4)
    {
        for (int column = x; column < x + width; column += 4)
        {
            unsigned hash = (unsigned)(column * 73856093) ^ (unsigned)(row * 19349663);
            if (((hash >> 5) % 5) == 0)
                fillRect(column + (int)((hash >> 9) & 1), row + (int)((hash >> 11) & 1),
                         2, 1, pathFleck);
        }
    }
}

// The farmhouse sits over the top of the home field. Its 80x80 sprite (Mini
// Farm house, stage 1) is bottom-aligned with the lower edge of the house
// tiles and centred on them.
static void drawHouse(void)
{
    if (game.screen != HOME_SCREEN)
        return;

    int x = FIELD_LEFT + (HOUSE_FIRST_COLUMN + HOUSE_COLUMNS / 2) * TILE_SIZE - 40;
    int y = FIELD_TOP + HOUSE_ROWS * TILE_SIZE - 80;
    if (!intersectsClip(x, y, 80, 80))
        return;

    for (int row = 0; row < 80; row++)
    {
        for (int column = 0; column < 80; column++)
        {
            int tile = (row / 8) * 10 + column / 8;
            int pixel = tile * 64 + (row % 8) * 8 + column % 8;
            u8 packed = sproutHouseSprite[pixel / 2];
            unsigned index = pixel & 1 ? packed >> 4 : packed & 0x0F;
            if (index != 0)
                putPixel(x + column, y + row, sproutHousePalette[index]);
        }
    }
}

// The shipping bin: a wooden crate (CC0, Adventure Awaits by Ishtar Pixels)
// beside the farmhouse.
static void drawBin(void)
{
    if (game.screen != HOME_SCREEN)
        return;

    int x = FIELD_LEFT + BIN_COLUMN * TILE_SIZE;
    int y = FIELD_TOP + BIN_ROW * TILE_SIZE;
    if (!intersectsClip(x, y, TILE_SIZE, TILE_SIZE))
        return;

    drawPackedTile(sproutBinSprite, sproutBinPalette, x + 6, y + 8, 5);
}

// The road out of the home field: a path through the right-hand fence.
static void drawHomeExit(void)
{
    if (game.screen != HOME_SCREEN)
        return;

    // The path starts at the farmhouse door and runs east to the road out.
    int x = FIELD_LEFT + HOUSE_FIRST_COLUMN * TILE_SIZE;
    int y = FIELD_TOP + HOME_EXIT_ROW * TILE_SIZE;
    drawPathArea(x, y, SCREEN_WIDTH - x, TILE_SIZE - 1);
    for (int fenceY = 40; fenceY <= 72; fenceY += 16)
        drawPackedTile(sproutFenceTiles[3], sproutFencePalette, 240, fenceY, 5);
    for (int fenceY = 128; fenceY <= 160; fenceY += 16)
        drawPackedTile(sproutFenceTiles[3], sproutFencePalette, 240, fenceY, 5);
}

// Repaints everything inside the current clip rectangle.
static void drawFarmClipped(void)
{
    drawGrassMeadow();
    for (int tile = 0; tile < FARM_TILE_COUNT; tile++)
        drawFarmTile(tile);
    for (int tile = 0; tile < FARM_TILE_COUNT; tile++)
        drawFarmTree(tile);
    drawBin();
    drawHouse();
    drawHomeExit();
}

static void drawFarm(void)
{
    if (!farmDrawn || drawnScreen != game.screen)
    {
        drawnScreen = game.screen;
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
// Walkable town: a dirt road along the bottom with three PicoVillage buildings
// above it, all standing on the same baseline. The cabin is the general store,
// the grey workshop the carpenter, and the blue fish shop is closed for now.
// Doors face the road.
enum
{
    TOWN_BASELINE = 121,
    TOWN_ROAD_Y = 128,
    TOWN_ROAD_H = 32
};

typedef struct
{
    const u8 *pixels;
    const u16 *palette;
    short x;
    short width;
    short height;
    short doorX;       // door centre, relative to x
    signed char shop;  // TOWN_STORE, TOWN_BOARD, or -1 when it does not open
} TownBuilding;

static const TownBuilding townBuildings[TOWN_BUILDING_COUNT] = {
    {sproutTownBuilding0, sproutTownBuildingPalette0, 14, 64, 76, 32, TOWN_STORE},
    {sproutTownBuilding1, sproutTownBuildingPalette1, 98, 66, 42, 37, TOWN_BOARD},
    {sproutTownBuilding2, sproutTownBuildingPalette2, 180, 72, 73, 25, -1},
};
static const short townTrees[][2] = {{6, 156}, {92, 160}, {160, 158}, {222, 156}};

int mapsTownBuildingAt(int left, int top, int right, int bottom)
{
    for (int i = 0; i < TOWN_BUILDING_COUNT; i++)
    {
        const TownBuilding *b = &townBuildings[i];
        if (right >= b->x && left < b->x + b->width &&
            bottom >= TOWN_BASELINE - b->height && top < TOWN_BASELINE)
            return i;
    }
    return -1;
}

int mapsTownBuildingShop(int building)
{
    return townBuildings[building].shop;
}

int mapsTownBuildingDoorX(int building)
{
    return townBuildings[building].x + townBuildings[building].doorX;
}

int mapsTownDoorFrontY(void)
{
    return TOWN_BASELINE;
}

// Only the trunk of a town tree blocks the way.
bool mapsTownTreeSolid(int left, int top, int right, int bottom)
{
    for (unsigned i = 0; i < sizeof(townTrees) / sizeof(townTrees[0]); i++)
    {
        if (right >= townTrees[i][0] + 10 && left < townTrees[i][0] + 22 &&
            bottom >= townTrees[i][1] + 20 && top < townTrees[i][1] + 30)
            return true;
    }
    return false;
}

// Buildings are 8bpp (index 0 is transparent), stored row by row.
static void drawTownBuilding(const TownBuilding *building)
{
    int x = building->x;
    int y = TOWN_BASELINE - building->height;
    if (!intersectsClip(x, y, building->width, building->height))
        return;

    for (int row = 0; row < building->height; row++)
    {
        for (int column = 0; column < building->width; column++)
        {
            unsigned index = building->pixels[row * building->width + column];
            if (index != 0)
                putPixel(x + column, y + row, building->palette[index]);
        }
    }
}

static void drawTown(void)
{
    drawGrassMeadow();
    drawPathArea(0, TOWN_ROAD_Y, SCREEN_WIDTH, TOWN_ROAD_H);
    for (int i = 0; i < TOWN_BUILDING_COUNT; i++)
    {
        int doorX = townBuildings[i].x + townBuildings[i].doorX;
        drawPathArea(doorX - 8, TOWN_BASELINE, 16, TOWN_ROAD_Y - TOWN_BASELINE);
        drawTownBuilding(&townBuildings[i]);
    }
    for (unsigned i = 0; i < sizeof(townTrees) / sizeof(townTrees[0]); i++)
        drawTree(townTrees[i][0], townTrees[i][1]);
}
// Shop interiors are drawn on the top screen while the shop menu is open.
static void drawCropIcon(unsigned crop, int x, int y)
{
    for (int row = 0; row < 12; row++)
    {
        for (int column = 0; column < 12; column++)
        {
            unsigned index = packedTilePixel(sproutCropIcons[crop], column, row);
            if (index != 0)
                putPixel(x + column, y + row, sproutCropPalettes[crop][index]);
        }
    }
}

static void drawShop(int shop)
{
    static const u16 wall = COLOR(20, 13, 8);
    static const u16 wallLine = COLOR(16, 10, 6);
    static const u16 floorColor = COLOR(25, 19, 13);
    static const u16 floorLine = COLOR(21, 15, 10);
    static const u16 counterTop = COLOR(27, 21, 13);
    static const u16 counterFront = COLOR(16, 10, 6);
    static const u16 selectColor = COLOR(31, 28, 8);

    fillRect(0, 0, SCREEN_WIDTH, 100, wall);
    for (int y = 6; y < 100; y += 12)
        fillRect(0, y, SCREEN_WIDTH, 1, wallLine);
    fillRect(0, 100, SCREEN_WIDTH, SCREEN_HEIGHT - 100, floorColor);
    for (int y = 112; y < SCREEN_HEIGHT; y += 16)
        fillRect(0, y, SCREEN_WIDTH, 1, floorLine);
    fillRect(0, 98, SCREEN_WIDTH, 3, counterFront);

    // Shelves.
    for (int shelf = 0; shelf < 2; shelf++)
    {
        int y = 32 + shelf * 36;
        fillRect(20, y + 17, 216, 3, counterFront);
        fillRect(20, y + 20, 216, 1, wallLine);
    }

    if (shop == TOWN_STORE)
    {
        // Seed stock: the crops in season sit on the top shelf, centred, with
        // the current offer outlined.
        int stocked[CROP_COUNT];
        int count = 0;
        for (int crop = 0; crop < CROP_COUNT; crop++)
        {
            if (crops[crop].season == gameCurrentSeason())
                stocked[count++] = crop;
        }
        for (int i = 0; i < count; i++)
        {
            int x = SCREEN_WIDTH / 2 - 6 + (2 * i - count + 1) * 24;
            if (stocked[i] == selectedShopCrop)
                drawRect(x - 5, 31, 22, 22, selectColor);
            drawCropIcon((unsigned)stocked[i], x, 36);
        }
    }
    else
    {
        // Tools on the top shelf, and a slot per upgrade below.
        for (int tool = 0; tool < 4; tool++)
            drawPackedTile(sproutUtilityIcons[tool], sproutUtilityPalette,
                           72 + tool * 32, 34, 5);
        for (int upgrade = 0; upgrade < 2; upgrade++)
        {
            int x = 96 + upgrade * 40;
            drawRect(x, 68, 24, 24, counterFront);
            fillRect(x + 1, 69, 22, 22, (unsigned)upgrade < game.repairs ?
                     selectColor : COLOR(23, 17, 11));
        }
    }

    // Counter and doormat.
    fillRect(40, 124, 176, 6, counterTop);
    fillRect(40, 130, 176, 22, counterFront);
    fillRect(40, 130, 176, 2, COLOR(12, 7, 4));
    fillRect(108, 168, 40, 12, COLOR(14, 9, 7));
}
// Crossroads: a 16x12 grid of 16 px cells. A horizontal road (rows 5-6) is
// crossed by a vertical road (columns 7-8), both fenced, with trees and
// bushes in the four corners outside the fences.
enum
{
    ROADS_COLUMNS = 16,
    ROADS_ROWS = 12,
    FENCE_H_MID = 0,
    FENCE_H_LEFT_END,
    FENCE_H_RIGHT_END,
    FENCE_V_MID,
    FENCE_V_TOP_END,
    FENCE_V_BOTTOM_END
};

static int roadsFence(int column, int row)
{
    if (row == 4 || row == 7)
    {
        if (column <= 6)
            return column == 6 ? FENCE_H_RIGHT_END : FENCE_H_MID;
        if (column >= 9)
            return column == 9 ? FENCE_H_LEFT_END : FENCE_H_MID;
        return -1;
    }

    if (column == 6 || column == 9)
    {
        if (row <= 3)
            return row == 3 ? FENCE_V_BOTTOM_END : FENCE_V_MID;
        if (row >= 8)
            return row == 8 ? FENCE_V_TOP_END : FENCE_V_MID;
    }
    return -1;
}

bool mapsRoadsSolid(int x, int y)
{
    if (x < 0 || y < 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
        return false;
    return roadsFence(x / 16, y / 16) >= 0;
}

static void drawRoads(void)
{
    static const short trees[][2] = {
        {4, 4}, {54, 20}, {166, 4}, {214, 22},
        {6, 140}, {54, 152}, {166, 134}, {214, 148},
    };
    static const short bushes[][3] = {
        {40, 6, 0}, {8, 44, 1}, {76, 44, 0}, {204, 2, 0}, {170, 44, 1}, {240, 4, 0},
        {36, 140, 1}, {84, 150, 0}, {10, 176, 0}, {204, 134, 0}, {172, 172, 1}, {236, 176, 1},
    };

    drawGrassMeadow();
    drawPathArea(0, 80, SCREEN_WIDTH, 32);
    drawPathArea(112, 0, 32, SCREEN_HEIGHT);

    for (int row = 0; row < ROADS_ROWS; row++)
    {
        for (int column = 0; column < ROADS_COLUMNS; column++)
        {
            int fence = roadsFence(column, row);
            if (fence >= 0)
                drawPackedTile(sproutFenceTiles[fence], sproutFencePalette,
                               column * 16, row * 16, 5);
        }
    }

    for (unsigned i = 0; i < sizeof(bushes) / sizeof(bushes[0]); i++)
        drawPackedTile(sproutBushTiles[bushes[i][2]], sproutBushPalette,
                       bushes[i][0], bushes[i][1], 5);
    for (unsigned i = 0; i < sizeof(trees) / sizeof(trees[0]); i++)
        drawTree(trees[i][0], trees[i][1]);
}

void mapsInitialize(u16 *bitmap)
{
    frameBuffer = bitmap;
}

u16 *mapsBitmap(void)
{
    return frameBuffer;
}

void mapsInvalidate(void)
{
    farmDrawn = false;
}

void mapsFillBlack(void)
{
    resetClip();
    fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR(0, 0, 0));
    farmDrawn = false;
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
    else if (mapId == VIEW_SHOP)
    {
        drawShop(townFocus);
        farmDrawn = false;
    }
    else if (mapId == VIEW_ROADS)
    {
        drawRoads();
        farmDrawn = false;
    }
    else
    {
        drawFarm();
    }
}
