#include "player.h"

#include "game.h"
#include "items.h"
#include "sprout_assets.h"

#include <stdio.h>
#include <string.h>

static int playerX = 120;
static int playerY = 150;
static int facingX;
static int facingY = -1;
static u16 *playerGfx;
static int displayedFacing = -1;
static u16 *toolGfx;
static u16 *highlightGfx;
static int toolAnimationTicks;
static int toolAnimationPose;

static void setHighlightPixel(int x, int y)
{
    int tile = (y / 8) * 8 + x / 8;
    int pixel = tile * 64 + (y % 8) * 8 + x % 8;
    u8 *packed = (u8 *)highlightGfx;
    packed[pixel / 2] |= pixel & 1 ? 0x10 : 0x01;
}

static void initializeHighlightSprite(void)
{
    memset(highlightGfx, 0, 64 * 64 / 2);
    for (int offset = 14; offset < 50; offset++)
    {
        setHighlightPixel(offset, 14);
        setHighlightPixel(offset, 15);
        setHighlightPixel(offset, 48);
        setHighlightPixel(offset, 49);
        setHighlightPixel(14, offset);
        setHighlightPixel(15, offset);
        setHighlightPixel(48, offset);
        setHighlightPixel(49, offset);
    }
}

static void updateFocusedPlot(void)
{
    int playerColumn = (playerX + 8 - FIELD_LEFT) / TILE_SIZE;
    int playerRow = (playerY + 8 - FIELD_TOP) / TILE_SIZE;
    int column = playerColumn + facingX;
    int row = playerRow + facingY;

    if (column < 0 || row < 0 ||
        column >= FIELD_COLUMNS || row >= FIELD_ROWS)
        focus = -1;
    else
        focus = row * FIELD_COLUMNS + column;
}

static bool canMovePlayerTo(int x, int y)
{
    if (x < FIELD_LEFT + 4 ||
        x > FIELD_LEFT + FIELD_COLUMNS * TILE_SIZE - 20 ||
        y < FIELD_TOP + 8 ||
        y > FIELD_TOP + FIELD_ROWS * TILE_SIZE - 16)
        return false;

    int centerX = x + 8;
    int centerY = y + 8;
    int column = (centerX - FIELD_LEFT) / TILE_SIZE;
    int row = (centerY - FIELD_TOP) / TILE_SIZE;
    if (column >= 0 && column < FIELD_COLUMNS &&
        row >= 0 && row < FIELD_ROWS)
    {
        unsigned tile = (unsigned)(row * FIELD_COLUMNS + column);
        if ((game.treeMask & ((u64)1 << tile)) != 0)
            return false;
    }

    return true;
}

static bool movePlayerBy(int dx, int dy)
{
    int nextX = playerX + dx;
    int nextY = playerY + dy;
    if (!canMovePlayerTo(nextX, nextY))
        return false;

    playerX = nextX;
    playerY = nextY;
    if (dx != 0)
    {
        facingX = dx < 0 ? -1 : 1;
        facingY = 0;
    }
    else if (dy != 0)
    {
        facingX = 0;
        facingY = dy < 0 ? -1 : 1;
    }

    updateFocusedPlot();
    return true;
}

void playerInitialize(void)
{
    oamInit(&oamMain, SpriteMapping_1D_32, false);
    playerGfx = oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_16Color);
    toolGfx = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_16Color);
    highlightGfx = oamAllocateGfx(&oamMain, SpriteSize_64x64,
                                  SpriteColorFormat_16Color);
    if (playerGfx == NULL || toolGfx == NULL || highlightGfx == NULL)
    {
        snprintf(message, sizeof(message), "Could not allocate game sprites.");
        return;
    }

    memcpy(SPRITE_PALETTE, sproutPlayerPalette, sizeof(sproutPlayerPalette));
    memcpy(SPRITE_PALETTE + 16, sproutToolSwingPalette,
           sizeof(sproutToolSwingPalette));
    SPRITE_PALETTE[32] = 0;
    SPRITE_PALETTE[33] = COLOR(31, 31, 0);
    initializeHighlightSprite();
    updateFocusedPlot();
}

void playerStartToolAnimation(void)
{
    if (facingY < 0)
        toolAnimationPose = facingX < 0 ? 1 : 2;
    else if (facingY > 0)
        toolAnimationPose = facingX < 0 ? 4 : 5;
    else
        toolAnimationPose = facingX < 0 ? 0 : 3;
    toolAnimationTicks = 12;
}

void playerUpdateSprite(void)
{
    if (playerGfx == NULL || toolGfx == NULL || highlightGfx == NULL)
        return;

    int facing = facingY < 0 ? 1 : 0;
    if (facing != displayedFacing)
    {
        const u8 *frame = facing ? sproutPlayerBack : sproutPlayerFront;
        memcpy(playerGfx, frame, sizeof(sproutPlayerFront));
        displayedFacing = facing;
    }

    oamSet(&oamMain, 0, playerX - 4, playerY - 8, 0, 0,
           SpriteSize_32x32, SpriteColorFormat_16Color,
           playerGfx, -1, false, view != VIEW_FARM,
           facingX < 0, false, false);
    bool hideTool = toolAnimationTicks == 0 || view != VIEW_FARM;
    if (!hideTool)
    {
        int animationFrame = toolAnimationPose +
            ((toolAnimationTicks / 3) & 1 ? 6 : 0);
        memcpy(toolGfx, sproutToolSwing[animationFrame],
               sizeof(sproutToolSwing[animationFrame]));
        oamSet(&oamMain, 1, playerX - 4 + facingX * 9 - 6,
               playerY - 8 + facingY * 9 - 6, 0, 1,
               SpriteSize_16x16, SpriteColorFormat_16Color,
               toolGfx, -1, false, false, facingX < 0, false, false);
        toolAnimationTicks--;
    }
    else
        oamSet(&oamMain, 1, 0, 0, 0, 1, SpriteSize_16x16,
               SpriteColorFormat_16Color, toolGfx, -1, false,
               true, false, false, false);

    bool hideHighlight = view != VIEW_FARM || focus < 0 ||
                         game.tileHighlighter == 0;
    int highlightX = 0;
    int highlightY = 0;
    if (!hideHighlight)
    {
        highlightX = FIELD_LEFT + (focus % FIELD_COLUMNS) * TILE_SIZE - 18;
        highlightY = FIELD_TOP + (focus / FIELD_COLUMNS) * TILE_SIZE - 18;
    }
    oamSet(&oamMain, 2, highlightX, highlightY, 0, 2,
           SpriteSize_64x64, SpriteColorFormat_16Color,
           highlightGfx, -1, false, hideHighlight,
           false, false, false);
    oamUpdate(&oamMain);
}

bool playerMove(u32 heldKeys)
{
    int dx = 0;
    int dy = 0;

    if (heldKeys & KEY_LEFT)
        dx = -1;
    else if (heldKeys & KEY_RIGHT)
        dx = 1;
    if (heldKeys & KEY_UP)
        dy = -1;
    else if (heldKeys & KEY_DOWN)
        dy = 1;

    if (dx != 0 || dy != 0)
    {
        int previousFocus = focus;
        if (movePlayerBy(dx, dy))
            return focus != previousFocus;
    }

    return false;
}
