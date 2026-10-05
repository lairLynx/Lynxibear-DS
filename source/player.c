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
static int displayedFrame = -1;
static bool movedThisFrame;
static int walkTicks;
static u16 *actionGfx;
static u16 *highlightGfx;
static int toolAnimationTicks;
static int toolAnimationTool;
static int toolAnimationDirection;
static int displayedAction = -1;

enum { WALK_FRAME_TICKS = 6, TOOL_ANIMATION_TICKS = 16 };

static void setHighlightPixel(int x, int y)
{
    int tile = (y / 8) * 8 + x / 8;
    int pixel = tile * 64 + (y % 8) * 8 + x % 8;
    u8 *packed = (u8 *)highlightGfx;
    packed[pixel / 2] |= pixel & 1 ? 0x10 : 0x01;
}

// Outline of the 28x28 tile, placed 18 px inside the 64x64 sprite.
static void initializeHighlightSprite(void)
{
    memset(highlightGfx, 0, 64 * 64 / 2);
    for (int x = 18; x < 46; x++)
    {
        setHighlightPixel(x, 18);
        setHighlightPixel(x, 19);
        setHighlightPixel(x, 44);
        setHighlightPixel(x, 45);
    }
    for (int y = 18; y < 46; y++)
    {
        setHighlightPixel(18, y);
        setHighlightPixel(19, y);
        setHighlightPixel(44, y);
        setHighlightPixel(45, y);
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
    actionGfx = oamAllocateGfx(&oamMain, SpriteSize_64x64,
                               SpriteColorFormat_16Color);
    highlightGfx = oamAllocateGfx(&oamMain, SpriteSize_64x64,
                                  SpriteColorFormat_16Color);
    if (playerGfx == NULL || actionGfx == NULL || highlightGfx == NULL)
    {
        snprintf(message, sizeof(message), "Could not allocate game sprites.");
        return;
    }

    memcpy(SPRITE_PALETTE, sproutPlayerPalette, sizeof(sproutPlayerPalette));
    SPRITE_PALETTE[32] = 0;
    SPRITE_PALETTE[33] = COLOR(31, 31, 0);
    memcpy(SPRITE_PALETTE + 48, sproutToolActionPalette,
           sizeof(sproutToolActionPalette));
    initializeHighlightSprite();
    updateFocusedPlot();
}

// Sheet rows are down, up, left, right.
static int facingDirection(void)
{
    return facingY < 0 ? 1 : facingY > 0 ? 0 : facingX < 0 ? 2 : 3;
}

void playerStartToolAnimation(unsigned toolIndex)
{
    if (toolIndex > 2)
        return;
    toolAnimationTool = (int)toolIndex;
    toolAnimationDirection = facingDirection();
    toolAnimationTicks = TOOL_ANIMATION_TICKS;
}

void playerUpdateSprite(void)
{
    if (playerGfx == NULL || actionGfx == NULL || highlightGfx == NULL)
        return;

    walkTicks = movedThisFrame ? walkTicks + 1 : 0;
    movedThisFrame = false;
    int frame = facingDirection() * 4 + (walkTicks / WALK_FRAME_TICKS) % 4;
    if (frame != displayedFrame)
    {
        memcpy(playerGfx, sproutPlayerFrames[frame],
               sizeof(sproutPlayerFrames[frame]));
        displayedFrame = frame;
    }

    bool swinging = toolAnimationTicks > 0 && view == VIEW_FARM;
    oamSet(&oamMain, 0, playerX - 4, playerY - 8, 0, 0,
           SpriteSize_32x32, SpriteColorFormat_16Color,
           playerGfx, -1, false, view != VIEW_FARM || swinging,
           false, false, false);
    if (swinging)
    {
        // Wind-up frame first, then the strike frame.
        int step = toolAnimationTicks > TOOL_ANIMATION_TICKS / 2 ? 0 : 1;
        int action = (toolAnimationTool * 4 + toolAnimationDirection) * 2 + step;
        if (action != displayedAction)
        {
            memcpy(actionGfx, sproutToolActions[action],
                   sizeof(sproutToolActions[action]));
            displayedAction = action;
        }
        // The body sits 18 px in from the corner of the 64x64 action frame.
        oamSet(&oamMain, 1, playerX - 18, playerY - 22, 0, 3,
               SpriteSize_64x64, SpriteColorFormat_16Color,
               actionGfx, -1, false, false, false, false, false);
        toolAnimationTicks--;
    }
    else
    {
        toolAnimationTicks = 0;
        oamSet(&oamMain, 1, 0, 0, 0, 3, SpriteSize_64x64,
               SpriteColorFormat_16Color, actionGfx, -1, false,
               true, false, false, false);
    }
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
        {
            movedThisFrame = true;
            return focus != previousFocus;
        }
    }

    return false;
}
