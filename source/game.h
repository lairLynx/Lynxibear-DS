#ifndef STARDSI_GAME_H
#define STARDSI_GAME_H

#include <nds.h>

enum
{
    FIELD_COLUMNS = 8,
    FIELD_ROWS = 6,
    TILE_SIZE = 28,
    FIELD_LEFT = 16,
    FIELD_TOP = 12,
    SEASON_LENGTH = 28,
    CROP_COUNT = 9,
    INVENTORY_SLOTS = 9,
    STORAGE_ROWS = 3,
    STORAGE_COLUMNS = 9,
    STORAGE_SLOTS = STORAGE_ROWS * STORAGE_COLUMNS,
    FARM_TILE_COUNT = FIELD_COLUMNS * FIELD_ROWS,
    STARTING_SEEDS = 10,
    MAX_STACK = 99,
    BASE_ENERGY = 100,
    MAX_GOLD = 9999,
    SAVE_VERSION = 8,
    WORLD_COLUMNS = 3,
    WORLD_ROWS = 3,
    WORLD_SCREENS = WORLD_COLUMNS * WORLD_ROWS,
    HOME_SCREEN = WORLD_COLUMNS - 1,
    VIEW_FARM = 0,
    VIEW_TOWN = 1,
    VIEW_ROADS = 2,
    VIEW_SHOP = 3,
    // Farm tile (row 2, last column) of the home screen that is the road out.
    HOME_EXIT_ROW = 3,
    // The farmhouse covers these tiles at the top of the home field.
    HOUSE_FIRST_COLUMN = 3,
    HOUSE_COLUMNS = 2,
    HOUSE_ROWS = 3,
    // The shipping bin stands beside the house; it is solid and not farmable.
    BIN_COLUMN = 5,
    BIN_ROW = 2,
    BIN_TILE = BIN_ROW * FIELD_COLUMNS + BIN_COLUMN,
    HOME_EXIT_TILE = HOME_EXIT_ROW * FIELD_COLUMNS + FIELD_COLUMNS - 1,
    TOWN_STORE = 0,
    TOWN_BOARD = 1
};

enum
{
    CROP_EMPTY,
    CROP_PARSNIP,
    CROP_TOMATO,
    CROP_PUMPKIN,
    CROP_YAM
};

enum
{
    SPRING,
    SUMMER,
    AUTUMN,
    WINTER
};

#define COLOR(r, g, b) ((u16)(RGB15((r), (g), (b)) | BIT(15)))
#define SAVE_MAGIC 0x53544453

typedef struct
{
    u8 crop;
    u8 growth;
    u8 tilled;
    u8 watered;
} FarmTile;

typedef struct
{
    u8 item;
    u8 count;
} InventorySlot;

typedef struct
{
    u32 magic;
    u32 version;
    u32 day;
    u32 gold;
    u32 energy;
    InventorySlot inventory[INVENTORY_SLOTS];
    InventorySlot storage[STORAGE_SLOTS];
    u64 treeMask[WORLD_SCREENS];
    u32 repairs;
    u32 harvests;
    u32 checksum;
    u8 season;
    u8 tileHighlighter;
    u8 screen;
    u8 reserved;
    FarmTile farm[WORLD_SCREENS][FIELD_ROWS][FIELD_COLUMNS];
    // Crops waiting in the shipping bin, per crop; sold when the player sleeps.
    // Added in save version 8 and kept at the end so version 7 files still load.
    u8 bin[16];
} SaveData;

typedef struct
{
    const char *name;
    const char *seasonName;
    unsigned daysToGrow;
    unsigned season;
    unsigned seedPrice;
    unsigned shipPrice;
    u16 color;
} CropInfo;

extern const char *const seasonNames[4];
extern SaveData game;

// The farm is a grid of screens; these are the tiles and trees of the screen
// the player is standing on.
#define CURRENT_FARM (game.farm[game.screen])
#define CURRENT_TREES (game.treeMask[game.screen])
extern int view;
extern int focus;
extern int townFocus;
extern int selectedSlot;
extern int selectedShopCrop;
extern char message[48];

unsigned gameEnergyLimit(void);
unsigned gameCurrentSeason(void);
bool gameIsRaining(void);
static inline bool gameIsHouseTile(int screen, int column, int row)
{
    return screen == HOME_SCREEN && row >= 0 && row < HOUSE_ROWS &&
           column >= HOUSE_FIRST_COLUMN &&
           column < HOUSE_FIRST_COLUMN + HOUSE_COLUMNS;
}

/* The house plus a one-tile border around it; not farmable. */
static inline bool gameIsHouseSurroundTile(int screen, int column, int row)
{
    return screen == HOME_SCREEN && row >= 0 && row <= HOUSE_ROWS &&
           column >= HOUSE_FIRST_COLUMN - 1 &&
           column <= HOUSE_FIRST_COLUMN + HOUSE_COLUMNS;
}

/* The dirt path from the farmhouse door east to the road out; not farmable. */
static inline bool gameIsPathTile(int screen, int column, int row)
{
    return screen == HOME_SCREEN && row == HOME_EXIT_ROW &&
           column >= HOUSE_FIRST_COLUMN;
}

static inline bool gameIsBinTile(int screen, int column, int row)
{
    return screen == HOME_SCREEN && column == BIN_COLUMN && row == BIN_ROW;
}

static inline bool gameViewWalkable(void)
{
    return view == VIEW_FARM || view == VIEW_ROADS || view == VIEW_TOWN;
}

void gameNew(void);
void gameGenerateWildTrees(SaveData *data);
FarmTile *gameFocusedTile(void);
bool gameMoveInventoryItem(bool fromStorage, unsigned fromSlot,
                           bool toStorage, unsigned toSlot);
void gameUseSelectedTool(void);
void gameInteractFarm(void);
void gameHoeTile(void);
void gameWaterTile(void);
void gameSleepUntilMorning(void);
void gameCycleInventory(int direction);
void gameCycleShopCrop(int direction);
void gameInteractTown(void);
unsigned gameBinCount(void);
unsigned gameInventoryCount(unsigned itemId);
void gameEnsureSeasonalShopOffer(void);

#endif
