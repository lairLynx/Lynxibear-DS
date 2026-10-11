#include "save.h"

#include "game.h"
#include "items.h"

#include <fat.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define SAVE_PATH "/lynxibear.sav"
#define SAVE_TEMP_PATH "/lynxibear.tmp"
#define SAVE_BACKUP_PATH "/lynxibear.bak"

// Saves before version 7 had five rows of plots per screen.
#define OLD_FIELD_ROWS 5
#define OLD_TILE_COUNT (OLD_FIELD_ROWS * FIELD_COLUMNS)

char storageMessage[44] = "SD save not loaded.";

typedef struct
{
    u32 magic;
    u32 version;
    u32 day;
    u32 gold;
    u32 energy;
    u8 seeds[4];
    u8 produce[4];
    u32 repairs;
    u32 harvests;
    u32 checksum;
    u8 season;
    u8 reserved[3];
    FarmTile farm[OLD_FIELD_ROWS][FIELD_COLUMNS];
} SaveDataV1;

typedef struct
{
    u32 magic;
    u32 version;
    u32 day;
    u32 gold;
    u32 energy;
    InventorySlot inventory[5];
    u32 repairs;
    u32 harvests;
    u32 checksum;
    u8 season;
    u8 reserved[3];
    FarmTile farm[OLD_FIELD_ROWS][FIELD_COLUMNS];
} SaveDataV2;

typedef struct
{
    u32 magic;
    u32 version;
    u32 day;
    u32 gold;
    u32 energy;
    InventorySlot inventory[INVENTORY_SLOTS];
    u32 repairs;
    u32 harvests;
    u32 checksum;
    u8 season;
    u8 reserved[3];
    FarmTile farm[OLD_FIELD_ROWS][FIELD_COLUMNS];
} SaveDataV3;

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
    u8 reserved[3];
    FarmTile farm[OLD_FIELD_ROWS][FIELD_COLUMNS];
} SaveDataV4;

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
    FarmTile farm[OLD_FIELD_ROWS][FIELD_COLUMNS];
} SaveDataV5;

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
    FarmTile farm[WORLD_SCREENS][OLD_FIELD_ROWS][FIELD_COLUMNS];
} SaveDataV6;

static u32 checksumBytes(const void *data, size_t size, size_t checksumOffset)
{
    const u8 *bytes = data;
    u32 hash = 2166136261u;

    for (size_t i = 0; i < size; i++)
    {
        if (i >= checksumOffset && i < checksumOffset + sizeof(u32))
            continue;
        hash = (hash ^ bytes[i]) * 16777619u;
    }

    return hash;
}

static bool validFarmTiles(const FarmTile *farm, int count)
{
    for (int i = 0; i < count; i++)
    {
        const FarmTile *tile = &farm[i];
        if (tile->crop > CROP_COUNT || tile->growth > 7 ||
            tile->tilled > 1 || tile->watered > 1)
            return false;
    }

    return true;
}

static bool validWorld(const SaveData *data)
{
    for (int screen = 0; screen < WORLD_SCREENS; screen++)
    {
        if ((data->treeMask[screen] >> FARM_TILE_COUNT) != 0 ||
            !validFarmTiles(&data->farm[screen][0][0], FARM_TILE_COUNT))
            return false;
    }
    return true;
}

// Version 7 files are the current layout without the trailing shipping bin.
#define SAVE_V7_SIZE offsetof(SaveData, bin)

_Static_assert(offsetof(SaveData, bin) == 1912, "SaveData v7 prefix size changed");
_Static_assert(sizeof(SaveData) == 1928, "SaveData size changed");

// Why the last validation or read failed, for the status line (a save that
// "is found but unreadable" is otherwise impossible to diagnose on hardware).
static const char *failReason = "";
static size_t failBytes;

#define FAIL(reason) do { failReason = (reason); return false; } while (0)

// Checks everything except the bin; `size` is how many bytes the checksum covers.
static bool validSaveBody(const SaveData *data, u32 version, size_t size)
{
    if (data->magic != SAVE_MAGIC)
        FAIL("magic");
    if (data->version != version)
        FAIL("version");
    if (data->day == 0 || data->day > SEASON_LENGTH)
        FAIL("day");
    if (data->gold > MAX_GOLD)
        FAIL("gold");
    if (data->energy > BASE_ENERGY + 40)
        FAIL("energy");
    if (data->repairs > 2)
        FAIL("repairs");
    if (data->season >= 4)
        FAIL("season");
    if (data->tileHighlighter > 1)
        FAIL("highlighter");
    if (data->screen >= WORLD_SCREENS)
        FAIL("screen");
    if (!validWorld(data))
        FAIL("farm or trees");
    if (data->checksum != checksumBytes(data, size, offsetof(SaveData, checksum)))
        FAIL("checksum");

    const InventorySlot *containers[] = {data->inventory, data->storage};
    const unsigned slotCounts[] = {INVENTORY_SLOTS, STORAGE_SLOTS};
    for (unsigned container = 0; container < 2; container++)
    {
        for (unsigned i = 0; i < slotCounts[container]; i++)
        {
            const InventorySlot *slot = &containers[container][i];
            if ((slot->item == ITEM_NONE && slot->count != 0) ||
                (slot->item != ITEM_NONE &&
                 (slot->item > ITEM_LAST || slot->count == 0 ||
                  slot->count > MAX_STACK)) ||
                (itemIsTool(slot->item) && slot->count != 1))
                FAIL("inventory");
        }
    }

    return true;
}
static bool validCurrentSave(const SaveData *data)
{
    if (!validSaveBody(data, SAVE_VERSION, sizeof(*data)))
        return false;

    for (unsigned crop = 0; crop < CROP_COUNT; crop++)
    {
        if (data->bin[crop] > MAX_STACK)
            FAIL("bin");
    }
    return true;
}

static bool validWorldV6(const SaveDataV6 *data)
{
    for (int screen = 0; screen < WORLD_SCREENS; screen++)
    {
        if ((data->treeMask[screen] >> OLD_TILE_COUNT) != 0 ||
            !validFarmTiles(&data->farm[screen][0][0], OLD_TILE_COUNT))
            return false;
    }
    return true;
}

static bool validV6Save(const SaveDataV6 *data)
{
    if (data->magic != SAVE_MAGIC || data->version != 6 ||
        data->day == 0 || data->day > SEASON_LENGTH ||
        data->gold > MAX_GOLD || data->energy > BASE_ENERGY + 40 ||
        data->repairs > 2 || data->season >= 4 ||
        data->tileHighlighter > 1 || data->screen >= WORLD_SCREENS ||
        !validWorldV6(data) ||
        data->checksum != checksumBytes(data, sizeof(*data),
                                        offsetof(SaveDataV6, checksum)))
        return false;

    const InventorySlot *containers[] = {data->inventory, data->storage};
    const unsigned slotCounts[] = {INVENTORY_SLOTS, STORAGE_SLOTS};
    for (unsigned container = 0; container < 2; container++)
    {
        for (unsigned i = 0; i < slotCounts[container]; i++)
        {
            const InventorySlot *slot = &containers[container][i];
            if ((slot->item == ITEM_NONE && slot->count != 0) ||
                (slot->item != ITEM_NONE &&
                 (slot->item > ITEM_LAST || slot->count == 0 ||
                  slot->count > MAX_STACK)) ||
                (itemIsTool(slot->item) && slot->count != 1))
                return false;
        }
    }

    return true;
}

static bool validV5Save(const SaveDataV5 *data)
{
    if (data->magic != SAVE_MAGIC || data->version != 5 ||
        data->day == 0 || data->day > SEASON_LENGTH ||
        data->gold > MAX_GOLD || data->energy > BASE_ENERGY + 40 ||
        data->repairs > 2 || data->season >= 4 || data->tileHighlighter > 1 ||
        (data->treeMask >> OLD_TILE_COUNT) != 0 ||
        !validFarmTiles(&data->farm[0][0], OLD_TILE_COUNT) ||
        data->checksum != checksumBytes(data, sizeof(*data),
                                        offsetof(SaveDataV5, checksum)))
        return false;

    const InventorySlot *containers[] = {data->inventory, data->storage};
    const unsigned slotCounts[] = {INVENTORY_SLOTS, STORAGE_SLOTS};
    for (unsigned container = 0; container < 2; container++)
    {
        for (unsigned i = 0; i < slotCounts[container]; i++)
        {
            const InventorySlot *slot = &containers[container][i];
            if ((slot->item == ITEM_NONE && slot->count != 0) ||
                (slot->item != ITEM_NONE &&
                 (slot->item > ITEM_LAST || slot->count == 0 ||
                  slot->count > MAX_STACK)) ||
                (itemIsTool(slot->item) && slot->count != 1))
                return false;
        }
    }

    return true;
}

static bool validV4Save(const SaveDataV4 *data)
{
    if (data->magic != SAVE_MAGIC || data->version != 4 ||
        data->day == 0 || data->day > SEASON_LENGTH ||
        data->gold > MAX_GOLD || data->energy > BASE_ENERGY + 40 ||
        data->repairs > 2 || data->season >= 4 ||
        (data->treeMask >> OLD_TILE_COUNT) != 0 ||
        !validFarmTiles(&data->farm[0][0], OLD_TILE_COUNT) ||
        data->checksum != checksumBytes(data, sizeof(*data),
                                        offsetof(SaveDataV4, checksum)))
        return false;

    const InventorySlot *containers[] = {data->inventory, data->storage};
    const unsigned slotCounts[] = {INVENTORY_SLOTS, STORAGE_SLOTS};
    for (unsigned container = 0; container < 2; container++)
    {
        for (unsigned i = 0; i < slotCounts[container]; i++)
        {
            const InventorySlot *slot = &containers[container][i];
            if ((slot->item == ITEM_NONE && slot->count != 0) ||
                (slot->item != ITEM_NONE &&
                 (slot->item > ITEM_LAST || slot->count == 0 ||
                  slot->count > MAX_STACK)) ||
                (itemIsTool(slot->item) && slot->count != 1))
                return false;
        }
    }

    return true;
}

static bool validV2Save(const SaveDataV2 *data)
{
    if (data->magic != SAVE_MAGIC || data->version != 2 ||
        data->day == 0 || data->day > SEASON_LENGTH ||
        data->gold > MAX_GOLD || data->energy > BASE_ENERGY + 40 ||
        data->repairs > 2 || data->season >= 4 ||
        !validFarmTiles(&data->farm[0][0], OLD_TILE_COUNT) ||
        data->checksum != checksumBytes(data, sizeof(*data),
                                        offsetof(SaveDataV2, checksum)))
        return false;

    for (int i = 0; i < 5; i++)
    {
        const InventorySlot *slot = &data->inventory[i];
        if ((slot->item == ITEM_NONE && slot->count != 0) ||
            (slot->item != ITEM_NONE &&
             (slot->item > ITEM_PRODUCE_LAST || slot->count == 0 ||
              slot->count > MAX_STACK)))
            return false;
    }

    return true;
}

static bool validV3Save(const SaveDataV3 *data)
{
    if (data->magic != SAVE_MAGIC || data->version != 3 ||
        data->day == 0 || data->day > SEASON_LENGTH ||
        data->gold > MAX_GOLD || data->energy > BASE_ENERGY + 40 ||
        data->repairs > 2 || data->season >= 4 ||
        !validFarmTiles(&data->farm[0][0], OLD_TILE_COUNT) ||
        data->checksum != checksumBytes(data, sizeof(*data),
                                        offsetof(SaveDataV3, checksum)))
        return false;

    for (int i = 0; i < INVENTORY_SLOTS; i++)
    {
        const InventorySlot *slot = &data->inventory[i];
        if ((slot->item == ITEM_NONE && slot->count != 0) ||
            (slot->item != ITEM_NONE &&
             (slot->item > ITEM_PRODUCE_LAST || slot->count == 0 ||
              slot->count > MAX_STACK)))
            return false;
    }
    return true;
}

static bool validV1Save(const SaveDataV1 *data)
{
    if (data->magic != SAVE_MAGIC || data->version != 1 ||
        data->day == 0 || data->day > SEASON_LENGTH ||
        data->gold > MAX_GOLD || data->energy > BASE_ENERGY + 40 ||
        data->repairs > 2 || data->season >= 4 ||
        !validFarmTiles(&data->farm[0][0], OLD_TILE_COUNT) ||
        data->checksum != checksumBytes(data, sizeof(*data),
                                        offsetof(SaveDataV1, checksum)))
        return false;

    for (int i = 0; i < 4; i++)
    {
        if (data->seeds[i] > MAX_STACK || data->produce[i] > MAX_STACK)
            return false;
    }

    return true;
}

static bool readCurrentSave(const char *path, SaveData *data)
{
    failBytes = 0;
    failReason = "open";
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;

    failBytes = fread(data, 1, sizeof(*data), file);
    fclose(file);
    if (failBytes != sizeof(*data))
    {
        failReason = "short read";
        return false;
    }
    return validCurrentSave(data);
}

// Reads a version 7 file into the current layout with an empty shipping bin.
static bool readV7Save(const char *path, SaveData *data)
{
    failBytes = 0;
    failReason = "open";
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;

    memset(data, 0, sizeof(*data));
    failBytes = fread(data, 1, SAVE_V7_SIZE, file);
    fclose(file);
    if (failBytes != SAVE_V7_SIZE)
    {
        failReason = "short read";
        return false;
    }
    if (!validSaveBody(data, 7, SAVE_V7_SIZE))
        return false;

    data->version = SAVE_VERSION;
    return true;
}

static bool readV6Save(const char *path, SaveDataV6 *data)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;

    bool readOk = fread(data, sizeof(*data), 1, file) == 1;
    fclose(file);
    return readOk && validV6Save(data);
}

static bool readV5Save(const char *path, SaveDataV5 *data)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;

    bool readOk = fread(data, sizeof(*data), 1, file) == 1;
    fclose(file);
    return readOk && validV5Save(data);
}

static bool readV4Save(const char *path, SaveDataV4 *data)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;

    bool readOk = fread(data, sizeof(*data), 1, file) == 1;
    fclose(file);
    return readOk && validV4Save(data);
}

static bool readV2Save(const char *path, SaveDataV2 *data)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;

    bool readOk = fread(data, sizeof(*data), 1, file) == 1;
    fclose(file);
    return readOk && validV2Save(data);
}

static bool readV3Save(const char *path, SaveDataV3 *data)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;

    bool readOk = fread(data, sizeof(*data), 1, file) == 1;
    fclose(file);
    return readOk && validV3Save(data);
}

static bool readV1Save(const char *path, SaveDataV1 *data)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;

    bool readOk = fread(data, sizeof(*data), 1, file) == 1;
    fclose(file);
    return readOk && validV1Save(data);
}

static bool addInventory(SaveDataV5 *data, unsigned item, unsigned count)
{
    InventorySlot *containers[] = {data->inventory, data->storage};
    const unsigned slotCounts[] = {INVENTORY_SLOTS, STORAGE_SLOTS};
    InventorySlot *empty = NULL;
    for (unsigned container = 0; container < 2; container++)
    {
        for (unsigned i = 0; i < slotCounts[container]; i++)
        {
            InventorySlot *slot = &containers[container][i];
            if (slot->item == item)
            {
                if (itemIsTool(item) || count > MAX_STACK - slot->count)
                    return false;
                slot->count += count;
                return true;
            }
            if (slot->item == ITEM_NONE && empty == NULL)
                empty = slot;
        }
    }

    if (empty == NULL || count == 0 || count > MAX_STACK)
        return false;
    empty->item = item;
    empty->count = count;
    return true;
}

static bool hasInventoryItem(const SaveDataV5 *data, unsigned item)
{
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (data->inventory[i].item == item)
            return true;
    for (int i = 0; i < STORAGE_SLOTS; i++)
        if (data->storage[i].item == item)
            return true;
    return false;
}

static void addStartingTools(SaveDataV5 *data)
{
    const unsigned tools[] = {ITEM_AXE, ITEM_HOE, ITEM_WATERING_CAN};
    for (unsigned i = 0; i < sizeof(tools) / sizeof(tools[0]); i++)
        if (!hasInventoryItem(data, tools[i]))
            addInventory(data, tools[i], 1);
}

static void initializeMigratedTrees(SaveDataV5 *data)
{
    const unsigned treeTiles[] = {0, 7, 32};
    for (unsigned i = 0; i < sizeof(treeTiles) / sizeof(treeTiles[0]); i++)
    {
        unsigned index = treeTiles[i];
        const FarmTile *tile = &data->farm[index / FIELD_COLUMNS]
                                           [index % FIELD_COLUMNS];
        if (tile->crop == CROP_EMPTY && !tile->tilled && !tile->watered)
            data->treeMask |= (u64)1 << index;
    }
}

static void migrateV1(const SaveDataV1 *old, SaveDataV5 *data)
{
    memset(data, 0, sizeof(*data));
    data->magic = SAVE_MAGIC;
    data->version = 5;
    data->day = old->day;
    data->gold = old->gold;
    data->energy = old->energy;
    data->repairs = old->repairs;
    data->harvests = old->harvests;
    data->season = old->season;
    data->tileHighlighter = 1;
    memcpy(data->farm, old->farm, sizeof(data->farm));

    for (int crop = 0; crop < 4; crop++)
    {
        if (old->seeds[crop] != 0)
            addInventory(data, itemSeedId(crop), old->seeds[crop]);
    }

    for (int crop = 0; crop < 4; crop++)
    {
        if (old->produce[crop] == 0)
            continue;

        if (!addInventory(data, itemProduceId(crop), old->produce[crop]))
        {
            unsigned earned = old->produce[crop] * crops[crop].shipPrice;
            data->gold = earned > MAX_GOLD - data->gold ? MAX_GOLD : data->gold + earned;
        }
    }

    addStartingTools(data);
    initializeMigratedTrees(data);
}

static void migrateV3(const SaveDataV3 *old, SaveDataV5 *data)
{
    memset(data, 0, sizeof(*data));
    data->magic = SAVE_MAGIC;
    data->version = 5;
    data->day = old->day;
    data->gold = old->gold;
    data->energy = old->energy;
    memcpy(data->inventory, old->inventory, sizeof(old->inventory));
    data->repairs = old->repairs;
    data->harvests = old->harvests;
    data->season = old->season;
    data->tileHighlighter = 1;
    memcpy(data->farm, old->farm, sizeof(data->farm));
    addStartingTools(data);
    initializeMigratedTrees(data);
}

static void migrateV4(const SaveDataV4 *old, SaveDataV5 *data)
{
    memset(data, 0, sizeof(*data));
    data->magic = SAVE_MAGIC;
    data->version = 5;
    data->day = old->day;
    data->gold = old->gold;
    data->energy = old->energy;
    memcpy(data->inventory, old->inventory, sizeof(old->inventory));
    memcpy(data->storage, old->storage, sizeof(old->storage));
    data->treeMask = old->treeMask;
    data->repairs = old->repairs;
    data->harvests = old->harvests;
    data->season = old->season;
    data->tileHighlighter = 1;
    memcpy(data->farm, old->farm, sizeof(data->farm));
}

static void migrateV2(const SaveDataV2 *old, SaveDataV5 *data)
{
    memset(data, 0, sizeof(*data));
    data->magic = SAVE_MAGIC;
    data->version = 5;
    data->day = old->day;
    data->gold = old->gold;
    data->energy = old->energy;
    memcpy(data->inventory, old->inventory, sizeof(old->inventory));
    data->repairs = old->repairs;
    data->harvests = old->harvests;
    data->season = old->season;
    data->tileHighlighter = 1;
    memcpy(data->farm, old->farm, sizeof(data->farm));
    addStartingTools(data);
    initializeMigratedTrees(data);
}

// Version 7 added a sixth row of plots at the top of every screen, so older
// rows move down by one (the same place on screen) and trees shift with them.
static void clearHomeExit(SaveData *data)
{
    data->farm[HOME_SCREEN][HOME_EXIT_ROW][FIELD_COLUMNS - 1] = (FarmTile){0};
    data->treeMask[HOME_SCREEN] &= ~((u64)1 << HOME_EXIT_TILE);
}

static void migrateV5(const SaveDataV5 *old, SaveData *data)
{
    memset(data, 0, sizeof(*data));
    data->magic = SAVE_MAGIC;
    data->version = SAVE_VERSION;
    data->day = old->day;
    data->gold = old->gold;
    data->energy = old->energy;
    memcpy(data->inventory, old->inventory, sizeof(old->inventory));
    memcpy(data->storage, old->storage, sizeof(old->storage));
    data->repairs = old->repairs;
    data->harvests = old->harvests;
    data->season = old->season;
    data->tileHighlighter = old->tileHighlighter;
    data->screen = HOME_SCREEN;
    memcpy(&data->farm[HOME_SCREEN][1][0], old->farm, sizeof(old->farm));
    data->treeMask[HOME_SCREEN] = old->treeMask << FIELD_COLUMNS;
    gameGenerateWildTrees(data);
    clearHomeExit(data);
}

static void migrateV6(const SaveDataV6 *old, SaveData *data)
{
    memset(data, 0, sizeof(*data));
    data->magic = SAVE_MAGIC;
    data->version = SAVE_VERSION;
    data->day = old->day;
    data->gold = old->gold;
    data->energy = old->energy;
    memcpy(data->inventory, old->inventory, sizeof(old->inventory));
    memcpy(data->storage, old->storage, sizeof(old->storage));
    data->repairs = old->repairs;
    data->harvests = old->harvests;
    data->season = old->season;
    data->tileHighlighter = old->tileHighlighter;
    data->screen = old->screen;
    for (int screen = 0; screen < WORLD_SCREENS; screen++)
    {
        memcpy(&data->farm[screen][1][0], old->farm[screen], sizeof(old->farm[screen]));
        data->treeMask[screen] = old->treeMask[screen] << FIELD_COLUMNS;
    }
    clearHomeExit(data);
}

// The first reason a file that exists could not be read, e.g. "sav 1928B:
// checksum", kept for the status line.
static char failureNote[44];

static void noteFailure(const char *path)
{
    if (failureNote[0] != '\0' || failBytes == 0)
        return;
    const char *extension = strrchr(path, '.');
    snprintf(failureNote, sizeof(failureNote), "%s %uB: %s",
             extension != NULL ? extension + 1 : "?", (unsigned)failBytes, failReason);
}

static bool readSave(const char *path, SaveData *data, bool *migrated)
{
    if (migrated != NULL)
        *migrated = false;
    if (readCurrentSave(path, data))
        return true;
    // A version 7 file is shorter than the current layout; that is not the
    // interesting failure, the version 7 attempt below is.
    if (strcmp(failReason, "short read") != 0)
        noteFailure(path);

    if (readV7Save(path, data))
    {
        if (migrated != NULL)
            *migrated = true;
        return true;
    }

    noteFailure(path);

    SaveDataV6 previous;
    if (readV6Save(path, &previous))
    {
        migrateV6(&previous, data);
        if (migrated != NULL)
            *migrated = true;
        return true;
    }

    SaveDataV5 upgraded;
    if (readV5Save(path, &upgraded))
    {
        migrateV5(&upgraded, data);
        if (migrated != NULL)
            *migrated = true;
        return true;
    }

    SaveDataV4 oldV4;
    if (readV4Save(path, &oldV4))
    {
        migrateV4(&oldV4, &upgraded);
        migrateV5(&upgraded, data);
        if (migrated != NULL)
            *migrated = true;
        return true;
    }

    SaveDataV3 oldV3;
    if (readV3Save(path, &oldV3))
    {
        migrateV3(&oldV3, &upgraded);
        migrateV5(&upgraded, data);
        if (migrated != NULL)
            *migrated = true;
        return true;
    }

    SaveDataV2 oldV2;
    if (readV2Save(path, &oldV2))
    {
        migrateV2(&oldV2, &upgraded);
        migrateV5(&upgraded, data);
        if (migrated != NULL)
            *migrated = true;
        return true;
    }

    SaveDataV1 old;
    if (!readV1Save(path, &old))
        return false;

    migrateV1(&old, &upgraded);
    migrateV5(&upgraded, data);
    if (migrated != NULL)
        *migrated = true;
    return true;
}

static bool writeSave(const char *path, SaveData *data)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL)
        return false;

    data->checksum = checksumBytes(data, sizeof(*data),
                                   offsetof(SaveData, checksum));
    bool writeOk = fwrite(data, sizeof(*data), 1, file) == 1;
    bool closeOk = fclose(file) == 0;
    return writeOk && closeOk;
}

bool saveInitialize(void)
{
    bool initialized = fatInitDefault();
    snprintf(storageMessage, sizeof(storageMessage),
             initialized ? "SD card ready." : "SD card unavailable.");
    return initialized;
}

static bool fileExists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;
    fclose(file);
    return true;
}

// Tries the save, then its backup, then a finished-but-not-yet-renamed temp
// file (left behind if power was lost between the two renames in saveGame).
// When nothing is readable, the status message says why.
static bool readAnySave(SaveData *data, bool *migrated)
{
    failureNote[0] = '\0';
    if (readSave(SAVE_PATH, data, migrated) ||
        readSave(SAVE_BACKUP_PATH, data, migrated) ||
        readSave(SAVE_TEMP_PATH, data, migrated))
        return true;

    bool found = fileExists(SAVE_PATH) || fileExists(SAVE_BACKUP_PATH) ||
                 fileExists(SAVE_TEMP_PATH);
    if (found && failureNote[0] != '\0')
        snprintf(storageMessage, sizeof(storageMessage), "Unreadable %s", failureNote);
    else
        snprintf(storageMessage, sizeof(storageMessage),
                 found ? "Save file found but unreadable." : "No save file on the SD card.");
    return false;
}

bool saveLoadGame(void)
{
    SaveData saved;
    bool migrated = false;
    if (!readAnySave(&saved, &migrated))
        return false;

    game = saved;
    selectedSlot = 0;
    if (migrated)
    {
        if (saveGame())
            snprintf(storageMessage, sizeof(storageMessage),
                     "Older save upgraded; items preserved.");
    }
    else
        snprintf(storageMessage, sizeof(storageMessage), "Farm loaded from SD card.");
    return true;
}

bool savePeek(SaveData *out)
{
    bool migrated;
    return readAnySave(out, &migrated);
}

bool saveGame(void)
{
    static SaveData verify;
    game.version = SAVE_VERSION;

    // Write the new save to a temp file and read it back before it is allowed
    // to replace anything, so a bad write can never cost the existing save.
    if (writeSave(SAVE_TEMP_PATH, &game))
    {
        if (!readCurrentSave(SAVE_TEMP_PATH, &verify))
        {
            remove(SAVE_TEMP_PATH);
            snprintf(storageMessage, sizeof(storageMessage), "Verify %uB: %s",
                     (unsigned)failBytes, failReason);
            return false;
        }
        if (memcmp(&verify, &game, sizeof(game)) != 0)
        {
            unsigned at = 0;
            while (((const u8 *)&verify)[at] == ((const u8 *)&game)[at])
                at++;
            remove(SAVE_TEMP_PATH);
            snprintf(storageMessage, sizeof(storageMessage),
                     "Verify: byte %u differs", at);
            return false;
        }

        bool hadPreviousFile = fileExists(SAVE_PATH);

        remove(SAVE_BACKUP_PATH);
        if (hadPreviousFile && rename(SAVE_PATH, SAVE_BACKUP_PATH) != 0)
        {
            remove(SAVE_TEMP_PATH);
            snprintf(storageMessage, sizeof(storageMessage),
                     "Could not protect the previous SD save.");
            return false;
        }

        if (rename(SAVE_TEMP_PATH, SAVE_PATH) == 0)
        {
            snprintf(storageMessage, sizeof(storageMessage), "Farm saved to SD card.");
            return true;
        }

        if (hadPreviousFile)
            rename(SAVE_BACKUP_PATH, SAVE_PATH);
        snprintf(storageMessage, sizeof(storageMessage),
                 "SD save could not be renamed.");
        remove(SAVE_TEMP_PATH);
        return false;
    }

    remove(SAVE_TEMP_PATH);
    snprintf(storageMessage, sizeof(storageMessage), "SD write failed; keep this system on.");
    return false;
}