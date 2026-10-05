#ifndef STARDSI_GAME_H
#define STARDSI_GAME_H

#include <nds.h>

enum
{
    FIELD_COLUMNS = 8,
    FIELD_ROWS = 5,
    TILE_SIZE = 28,
    FIELD_LEFT = 16,
    FIELD_TOP = 38,
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
    SAVE_VERSION = 5,
    VIEW_FARM = 0,
    VIEW_TOWN = 1,
    TOWN_STORE = 0,
    TOWN_BOARD = 1,
    TOWN_FARM_GATE = 2
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
    u64 treeMask;
    u32 repairs;
    u32 harvests;
    u32 checksum;
    u8 season;
    u8 tileHighlighter;
    u8 reserved[2];
    FarmTile farm[FIELD_ROWS][FIELD_COLUMNS];
} SaveData;

typedef struct
{
    const char *name;
    const char *seasonName;
    unsigned daysToGrow;
    unsigned season;
    unsigned seedPackPrice;
    unsigned shipPrice;
    u16 color;
} CropInfo;

extern const char *const seasonNames[4];
extern SaveData game;
extern int view;
extern int focus;
extern int townFocus;
extern int selectedSlot;
extern int selectedShopCrop;
extern char message[48];

unsigned gameEnergyLimit(void);
unsigned gameCurrentSeason(void);
bool gameIsRaining(void);
void gameNew(void);
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
void gameMoveTownFocus(int direction);
void gameInteractTown(void);
unsigned gameInventoryCount(unsigned itemId);
void gameEnsureSeasonalShopOffer(void);

#endif
