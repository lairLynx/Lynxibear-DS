#include "player.h"

#include "game.h"
#include "items.h"
#include "maps.h"
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

static bool statusDirty;

static void setStatus(const char *text)
{
    if (strcmp(message, text) != 0)
    {
        snprintf(message, sizeof(message), "%s", text);
        statusDirty = true;
    }
}

bool playerTakeStatusDirty(void)
{
    bool dirty = statusDirty;
    statusDirty = false;
    return dirty;
}

static void updateFocusedPlot(void)
{
    if (view != VIEW_FARM)
    {
        focus = -1;
        return;
    }

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

enum
{
    WALK_MIN_X = FIELD_LEFT + 4,
    WALK_MAX_X = FIELD_LEFT + FIELD_COLUMNS * TILE_SIZE - 20,
    WALK_MIN_Y = FIELD_TOP + 8,
    WALK_MAX_Y = FIELD_TOP + FIELD_ROWS * TILE_SIZE - 16,
    // Sprite y that centres the figure on the horizontal road.
    ROADS_ENTRY_Y = 84
};

static bool treeAt(int screen, int x, int y)
{
    int column = (x + 8 - FIELD_LEFT) / TILE_SIZE;
    int row = (y + 8 - FIELD_TOP) / TILE_SIZE;
    if (column < 0 || column >= FIELD_COLUMNS || row < 0 || row >= FIELD_ROWS)
        return false;
    return (game.treeMask[screen] & ((u64)1 << (row * FIELD_COLUMNS + column))) != 0;
}

static bool canMovePlayerTo(int x, int y)
{
    if (x < WALK_MIN_X || x > WALK_MAX_X || y < WALK_MIN_Y || y > WALK_MAX_Y)
        return false;

    return !treeAt(game.screen, x, y);
}

// Walking off an edge moves to the neighbouring screen of the farm grid,
// arriving on the opposite edge, unless there is no neighbour or a tree is in
// the way. On success the new screen and player position are applied.
static bool crossScreenEdge(int nextX, int nextY)
{
    int column = game.screen % WORLD_COLUMNS;
    int row = game.screen / WORLD_COLUMNS;
    int arriveX = nextX;
    int arriveY = nextY;

    if (game.screen == HOME_SCREEN && nextX > WALK_MAX_X &&
        (nextY + 8 - FIELD_TOP) / TILE_SIZE == HOME_EXIT_ROW)
    {
        // The road out of the home field leads to the crossroads.
        view = VIEW_ROADS;
        playerX = 0;
        playerY = ROADS_ENTRY_Y;
        setStatus("The crossroads. Mines north, beach south.");
        return true;
    }

    if (nextX < WALK_MIN_X && column > 0)
    {
        column--;
        arriveX = WALK_MAX_X;
    }
    else if (nextX > WALK_MAX_X && column < WORLD_COLUMNS - 1)
    {
        column++;
        arriveX = WALK_MIN_X;
    }
    else if (nextY < WALK_MIN_Y && row > 0)
    {
        row--;
        arriveY = WALK_MAX_Y;
    }
    else if (nextY > WALK_MAX_Y && row < WORLD_ROWS - 1)
    {
        row++;
        arriveY = WALK_MIN_Y;
    }
    else
        return false;

    int screen = row * WORLD_COLUMNS + column;
    if (treeAt(screen, arriveX, arriveY))
        return false;

    game.screen = (u8)screen;
    nextX = arriveX;
    nextY = arriveY;
    playerX = nextX;
    playerY = nextY;
    return true;
}

// The feet of the 24 px figure, relative to the sprite position, are what
// collide with fences on the crossroads.
static bool moveOnRoads(int nextX, int nextY)
{
    int left = nextX + 4;
    int right = nextX + 19;
    int top = nextY + 12;
    int bottom = nextY + 19;

    if (left < 0)
    {
        view = VIEW_FARM;
        game.screen = HOME_SCREEN;
        playerX = WALK_MAX_X;
        playerY = FIELD_TOP + HOME_EXIT_ROW * TILE_SIZE + 6;
        setStatus("Back at the farm.");
        return true;
    }
    if (right >= SCREEN_WIDTH)
    {
        townReturnView = VIEW_ROADS;
        view = VIEW_TOWN;
        playerX = SCREEN_WIDTH - 36;
        playerY = ROADS_ENTRY_Y;
        setStatus("Lynxibear Valley: choose a place to visit.");
        return true;
    }
    if (top < 0)
    {
        setStatus("The mines are closed for now.");
        return false;
    }
    if (bottom >= SCREEN_HEIGHT)
    {
        setStatus("The beach is closed for now.");
        return false;
    }
    if (mapsRoadsSolid(left, top) || mapsRoadsSolid(right, top) ||
        mapsRoadsSolid(left, bottom) || mapsRoadsSolid(right, bottom))
        return false;

    playerX = nextX;
    playerY = nextY;
    return true;
}

static bool movePlayerBy(int dx, int dy)
{
    int nextX = playerX + dx;
    int nextY = playerY + dy;
    if (view == VIEW_ROADS)
    {
        if (!moveOnRoads(nextX, nextY))
            return false;
    }
    else if (canMovePlayerTo(nextX, nextY))
    {
        playerX = nextX;
        playerY = nextY;
    }
    else if (!crossScreenEdge(nextX, nextY))
        return false;

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

    bool swinging = toolAnimationTicks > 0 && view != VIEW_TOWN;
    oamSet(&oamMain, 0, playerX - 4, playerY - 8, 0, 0,
           SpriteSize_32x32, SpriteColorFormat_16Color,
           playerGfx, -1, false, view == VIEW_TOWN || swinging,
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
