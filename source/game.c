#include "game.h"

#include "items.h"

#include <stdio.h>
#include <string.h>

const char *const seasonNames[4] = {
    "Spring", "Summer", "Autumn", "Winter"};

SaveData game;
int view = VIEW_FARM;
int townReturnView = VIEW_FARM;
int focus = -1;
int townFocus = TOWN_STORE;
int selectedSlot;
int selectedShopCrop;
char message[48] = "Welcome to Lynxibear Valley!";

static const unsigned startingTreeTiles[] = {0, 7, 40};

static InventorySlot *inventoryAt(bool storage, unsigned slot)
{
    if (storage)
        return slot < STORAGE_SLOTS ? &game.storage[slot] : NULL;
    return slot < INVENTORY_SLOTS ? &game.inventory[slot] : NULL;
}

static void spendEnergy(unsigned amount)
{
    game.energy -= amount;
}

static bool inSeason(unsigned crop)
{
    return crops[crop].season == gameCurrentSeason();
}

static int findInventorySlot(unsigned itemId)
{
    for (int slot = 0; slot < INVENTORY_SLOTS; slot++)
    {
        if (game.inventory[slot].item == itemId)
            return slot;
    }
    for (int slot = 0; slot < STORAGE_SLOTS; slot++)
    {
        if (game.storage[slot].item == itemId)
            return INVENTORY_SLOTS + slot;
    }
    return -1;
}

static int findEmptyInventorySlot(void)
{
    for (int slot = 0; slot < INVENTORY_SLOTS; slot++)
    {
        if (game.inventory[slot].item == ITEM_NONE)
            return slot;
    }
    for (int slot = 0; slot < STORAGE_SLOTS; slot++)
    {
        if (game.storage[slot].item == ITEM_NONE)
            return INVENTORY_SLOTS + slot;
    }
    return -1;
}

unsigned gameInventoryCount(unsigned itemId)
{
    int slot = findInventorySlot(itemId);
    if (slot < 0)
        return 0;
    return slot >= INVENTORY_SLOTS
        ? game.storage[slot - INVENTORY_SLOTS].count
        : game.inventory[slot].count;
}

static bool addInventoryItem(unsigned itemId, unsigned count)
{
    int slot = findInventorySlot(itemId);
    if (slot < 0)
        slot = findEmptyInventorySlot();
    if (slot < 0)
        return false;

    InventorySlot *target = inventoryAt(slot >= INVENTORY_SLOTS,
                                        (unsigned)(slot >= INVENTORY_SLOTS
                                            ? slot - INVENTORY_SLOTS : slot));
    if (target == NULL || count > MAX_STACK - target->count)
        return false;

    target->item = itemId;
    target->count += count;
    return true;
}

static bool removeInventoryItem(unsigned slot, unsigned count)
{
    if (slot >= INVENTORY_SLOTS || game.inventory[slot].item == ITEM_NONE ||
        game.inventory[slot].count < count)
        return false;

    game.inventory[slot].count -= count;
    if (game.inventory[slot].count == 0)
        game.inventory[slot].item = ITEM_NONE;
    return true;
}

unsigned gameEnergyLimit(void)
{
    return BASE_ENERGY + game.repairs * 20;
}

unsigned gameCurrentSeason(void)
{
    return game.season;
}

bool gameIsRaining(void)
{
    return ((game.day + game.season * 3) % 6) == 0;
}

// Scatters a few trees over every screen except home. Trees stay off the outer
// ring of tiles so walking onto a new screen never lands on one.
void gameGenerateWildTrees(SaveData *data)
{
    for (int screen = 0; screen < WORLD_SCREENS; screen++)
    {
        if (screen == HOME_SCREEN)
            continue;

        u32 seed = (u32)screen * 2654435761u + 12345u;
        data->treeMask[screen] = 0;
        for (int i = 0; i < 6; i++)
        {
            seed = seed * 1664525u + 1013904223u;
            unsigned row = 1 + (seed >> 16) % (FIELD_ROWS - 2);
            seed = seed * 1664525u + 1013904223u;
            unsigned column = 1 + (seed >> 16) % (FIELD_COLUMNS - 2);
            data->treeMask[screen] |= (u64)1 << (row * FIELD_COLUMNS + column);
        }
    }
}

void gameNew(void)
{
    memset(&game, 0, sizeof(game));
    game.day = 1;
    game.gold = 100;
    game.energy = BASE_ENERGY;
    game.tileHighlighter = 1;
    game.inventory[0] = (InventorySlot){ITEM_AXE, 1};
    game.inventory[1] = (InventorySlot){ITEM_HOE, 1};
    game.inventory[2] = (InventorySlot){ITEM_WATERING_CAN, 1};
    game.inventory[3] = (InventorySlot){itemSeedId(0), STARTING_SEEDS};
    for (unsigned i = 0; i < sizeof(startingTreeTiles) / sizeof(startingTreeTiles[0]); i++)
        game.treeMask[HOME_SCREEN] |= (u64)1 << startingTreeTiles[i];
    gameGenerateWildTrees(&game);
    game.screen = HOME_SCREEN;
    view = VIEW_FARM;
    townReturnView = VIEW_FARM;
    focus = -1;
    townFocus = TOWN_STORE;
    selectedSlot = 1;
    selectedShopCrop = 0;
    snprintf(message, sizeof(message), "Welcome to Lynxibear Valley!");
}

FarmTile *gameFocusedTile(void)
{
    if (focus < 0 || focus >= FIELD_COLUMNS * FIELD_ROWS)
        return NULL;
    if (game.screen == HOME_SCREEN && focus == HOME_EXIT_TILE)
        return NULL;

    return &CURRENT_FARM[focus / FIELD_COLUMNS][focus % FIELD_COLUMNS];
}

bool gameMoveInventoryItem(bool fromStorage, unsigned fromSlot,
                           bool toStorage, unsigned toSlot)
{
    InventorySlot *source = inventoryAt(fromStorage, fromSlot);
    InventorySlot *target = inventoryAt(toStorage, toSlot);
    if (source == NULL || target == NULL || source == target ||
        source->item == ITEM_NONE)
        return false;

    if (source->item == target->item)
    {
        if (itemIsTool(source->item) ||
            source->count > MAX_STACK - target->count)
            return false;
        target->count += source->count;
        source->item = ITEM_NONE;
        source->count = 0;
        return true;
    }

    InventorySlot swap = *source;
    *source = *target;
    *target = swap;
    return true;
}

void gameHoeTile(void)
{
    FarmTile *tile = gameFocusedTile();

    if (focus >= 0 && (CURRENT_TREES & ((u64)1 << focus)) != 0)
        snprintf(message, sizeof(message), "A tree blocks this patch. Use the axe.");
    else if (tile == NULL)
        snprintf(message, sizeof(message), "Face a plot to use your hoe.");
    else if (tile->tilled)
        snprintf(message, sizeof(message), "This tile is already hoed.");
    else if (game.energy < 4)
        snprintf(message, sizeof(message), "Too tired. Sleep to recover.");
    else
    {
        tile->tilled = true;
        spendEnergy(4);
        snprintf(message, sizeof(message), "Soil turned. Choose seeds with L/R.");
    }
}

void gameInteractFarm(void)
{
    FarmTile *tile = gameFocusedTile();
    if (focus >= 0 && (CURRENT_TREES & ((u64)1 << focus)) != 0)
    {
        snprintf(message, sizeof(message), "A tree stands here. Use the axe.");
        return;
    }
    if (tile == NULL)
    {
        snprintf(message, sizeof(message), "Face a farm plot first.");
        return;
    }

    if (tile->crop != CROP_EMPTY &&
        tile->growth >= crops[tile->crop - 1].daysToGrow)
    {
        unsigned harvestedCrop = tile->crop - 1;
        if (!addInventoryItem(itemProduceId(harvestedCrop), 1))
        {
            snprintf(message, sizeof(message), "Inventory full. Ship items or clear a slot.");
            return;
        }

        game.harvests++;
        tile->crop = CROP_EMPTY;
        tile->growth = 0;
        tile->watered = false;
        snprintf(message, sizeof(message), "Harvested %s! Ships tonight.",
                 crops[harvestedCrop].name);
        return;
    }

    if (tile->crop != CROP_EMPTY)
        snprintf(message, sizeof(message), "Crop growing: water it and sleep.");
    else if (!tile->tilled)
        snprintf(message, sizeof(message), "Hoe the grass before planting.");
    else if (selectedSlot < 0 || selectedSlot >= INVENTORY_SLOTS ||
             !itemIsSeed(game.inventory[selectedSlot].item))
        snprintf(message, sizeof(message), "Select a seed in your inventory.");
    else if (!inSeason(itemCropIndex(game.inventory[selectedSlot].item)))
        snprintf(message, sizeof(message), "%s grows in %s.",
                 itemName(game.inventory[selectedSlot].item),
                 crops[itemCropIndex(game.inventory[selectedSlot].item)].seasonName);
    else if (game.inventory[selectedSlot].count == 0)
        snprintf(message, sizeof(message), "That seed stack is empty.");
    else if (game.energy < 3)
        snprintf(message, sizeof(message), "Too tired. Sleep to recover.");
    else
    {
        unsigned cropIndex = itemCropIndex(game.inventory[selectedSlot].item);
        const CropInfo *crop = &crops[cropIndex];
        tile->crop = cropIndex + 1;
        tile->growth = 0;
        tile->watered = false;
        removeInventoryItem((unsigned)selectedSlot, 1);
        spendEnergy(3);
        snprintf(message, sizeof(message), "Planted %s. Water it today!", crop->name);
    }
}

void gameWaterTile(void)
{
    FarmTile *tile = gameFocusedTile();

    if (focus >= 0 && (CURRENT_TREES & ((u64)1 << focus)) != 0)
        snprintf(message, sizeof(message), "A tree stands here; crops need water.");
    else if (tile == NULL)
        snprintf(message, sizeof(message), "Face a growing crop to water it.");
    else if (tile->crop == CROP_EMPTY ||
             tile->growth >= crops[tile->crop - 1].daysToGrow)
        snprintf(message, sizeof(message), "There is no growing crop here.");
    else if (gameIsRaining())
        snprintf(message, sizeof(message), "Rain is watering the whole farm today!");
    else if (tile->watered)
        snprintf(message, sizeof(message), "This crop is watered for today.");
    else if (game.energy < 2)
        snprintf(message, sizeof(message), "Too tired. Sleep to recover.");
    else
    {
        tile->watered = true;
        spendEnergy(2);
        snprintf(message, sizeof(message), "Watered! It grows overnight.");
    }
}

void gameSleepUntilMorning(void)
{
    unsigned shipped = 0;
    unsigned earned = 0;

    InventorySlot *containers[] = {game.inventory, game.storage};
    const unsigned slotCounts[] = {INVENTORY_SLOTS, STORAGE_SLOTS};
    for (unsigned container = 0; container < 2; container++)
    {
        for (unsigned slot = 0; slot < slotCounts[container]; slot++)
        {
            InventorySlot *item = &containers[container][slot];
            if (!itemIsProduce(item->item))
                continue;

            unsigned crop = itemCropIndex(item->item);
            unsigned price = crops[crop].shipPrice;
            if (game.repairs >= 2)
                price = price * 11 / 10;
            shipped += item->count;
            earned += item->count * price;
            item->item = ITEM_NONE;
            item->count = 0;
        }
    }

    if (earned > MAX_GOLD - game.gold)
        game.gold = MAX_GOLD;
    else
        game.gold += earned;

    bool wasRaining = gameIsRaining();
    for (int screen = 0; screen < WORLD_SCREENS; screen++)
    {
        for (int row = 0; row < FIELD_ROWS; row++)
        {
            for (int column = 0; column < FIELD_COLUMNS; column++)
            {
                FarmTile *tile = &game.farm[screen][row][column];
                if (tile->crop != CROP_EMPTY)
                {
                    if (crops[tile->crop - 1].season != gameCurrentSeason())
                    {
                        tile->crop = CROP_EMPTY;
                        tile->growth = 0;
                        tile->watered = false;
                        continue;
                    }

                    if (wasRaining || tile->watered)
                    {
                        if (tile->growth < crops[tile->crop - 1].daysToGrow)
                            tile->growth++;
                    }
                }

                tile->watered = false;
            }
        }
    }

    game.day++;
    if (game.day > SEASON_LENGTH)
    {
        game.day = 1;
        game.season = (game.season + 1) % 4;
        for (int screen = 0; screen < WORLD_SCREENS; screen++)
        {
            for (int row = 0; row < FIELD_ROWS; row++)
            {
                for (int column = 0; column < FIELD_COLUMNS; column++)
                {
                    FarmTile *tile = &game.farm[screen][row][column];
                    if (tile->crop != CROP_EMPTY &&
                        crops[tile->crop - 1].season != gameCurrentSeason())
                    {
                        tile->crop = CROP_EMPTY;
                        tile->growth = 0;
                    }
                }
            }
        }
        snprintf(message, sizeof(message), "%s begins. Out-of-season crops wither.",
                 seasonNames[gameCurrentSeason()]);
    }
    else if (shipped > 0)
        snprintf(message, sizeof(message), "Shipped %u crops for %u gold.", shipped, earned);
    else if (wasRaining)
        snprintf(message, sizeof(message), "Rain watered the valley. A new day begins!");
    else
        snprintf(message, sizeof(message), "Morning! Water crops to help them grow.");

    game.energy = gameEnergyLimit();
}

static void gameChopTree(void)
{
    if (focus < 0 || (CURRENT_TREES & ((u64)1 << focus)) == 0)
    {
        snprintf(message, sizeof(message), "Face a tree to chop it.");
        return;
    }
    if (game.energy < 6)
    {
        snprintf(message, sizeof(message), "Too tired to swing the axe.");
        return;
    }
    if (!addInventoryItem(ITEM_WOOD, 3))
    {
        snprintf(message, sizeof(message), "No room for the wood. Clear a slot.");
        return;
    }

    CURRENT_TREES &= ~((u64)1 << focus);
    spendEnergy(6);
    snprintf(message, sizeof(message), "Chopped the tree. Gathered 3 wood.");
}

void gameUseSelectedTool(void)
{
    if (selectedSlot < 0 || selectedSlot >= INVENTORY_SLOTS)
    {
        snprintf(message, sizeof(message), "Select a tool from the item bar.");
        return;
    }

    switch (game.inventory[selectedSlot].item)
    {
    case ITEM_AXE:
        gameChopTree();
        break;
    case ITEM_HOE:
        gameHoeTile();
        break;
    case ITEM_WATERING_CAN:
        gameWaterTile();
        break;
    default:
        snprintf(message, sizeof(message), "Select the axe, hoe or watering can.");
        break;
    }
}

void gameCycleInventory(int direction)
{
    selectedSlot = (selectedSlot + direction + INVENTORY_SLOTS) % INVENTORY_SLOTS;
}

void gameCycleShopCrop(int direction)
{
    int candidate = selectedShopCrop;
    for (int tries = 0; tries < CROP_COUNT; tries++)
    {
        candidate = (candidate + direction + CROP_COUNT) % CROP_COUNT;
        if (inSeason((unsigned)candidate))
        {
            selectedShopCrop = candidate;
            return;
        }
    }
}

void gameEnsureSeasonalShopOffer(void)
{
    if (!inSeason((unsigned)selectedShopCrop))
    {
        for (int crop = 0; crop < CROP_COUNT; crop++)
        {
            if (inSeason((unsigned)crop))
            {
                selectedShopCrop = crop;
                return;
            }
        }
    }
}

void gameMoveTownFocus(int direction)
{
    townFocus = (townFocus + direction + 3) % 3;
}

void gameInteractTown(void)
{
    if (townFocus == TOWN_FARM_GATE)
    {
        view = townReturnView;
        snprintf(message, sizeof(message), view == VIEW_ROADS ?
                 "Back on the crossroads." : "Back to the farm. What will you grow?");
        return;
    }

    if (townFocus == TOWN_STORE)
    {
        gameEnsureSeasonalShopOffer();
        const CropInfo *crop = &crops[selectedShopCrop];
        unsigned seedItem = itemSeedId((unsigned)selectedShopCrop);
        if (game.gold < crop->seedPackPrice)
            snprintf(message, sizeof(message), "Not enough gold for this seed pack.");
        else if (!addInventoryItem(seedItem, 3))
            snprintf(message, sizeof(message), "Inventory full; make room for seeds.");
        else
        {
            game.gold -= crop->seedPackPrice;
            snprintf(message, sizeof(message), "Bought 3 %s seeds.", crop->name);
        }
        return;
    }

    unsigned cost = game.repairs == 0 ? 250 : 650;
    if (game.repairs >= 2)
        snprintf(message, sizeof(message), "Valley restoration complete!");
    else if (game.gold < cost)
        snprintf(message, sizeof(message), "Save more gold from your harvests.");
    else
    {
        game.gold -= cost;
        game.repairs++;
        snprintf(message, sizeof(message),
                 game.repairs == 1 ? "Board restored! Maximum energy increased."
                                   : "Valley renewed! Shipments now earn 10%% more.");
    }
}
