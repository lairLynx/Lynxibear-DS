#include "ui.h"

#include "game.h"
#include "items.h"
#include "save.h"
#include "sprout_assets.h"

#include <stdio.h>
#include <string.h>

enum
{
    UI_ACTIVITY,
    UI_INFO,
    UI_CONTROLS,
    UI_INVENTORY,
    UI_SETTINGS,
    UI_SLOT_X = 12,
    UI_SLOT_STEP = 26,
    UI_SLOT_SIZE = 24,
    UI_SLOT_Y = 130,
    UI_STORAGE_SLOT_Y = 42,
    UI_STORAGE_ROW_STEP = 24
};

static int uiPage = UI_ACTIVITY;
static int uiBackground;
static u16 *uiBitmap;
static bool dragActive;
static bool dragSourceStorage;
static unsigned dragSourceSlot;
static int lastTouchX;
static int lastTouchY;
static bool inventoryDirty;
static bool settingsDirty;

static const u8 fontRows[36][7] = {
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30},
    {14, 17, 16, 16, 16, 17, 14}, {30, 17, 17, 17, 17, 17, 30},
    {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17},
    {14, 4, 4, 4, 4, 4, 14}, {7, 2, 2, 2, 2, 18, 12},
    {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17},
    {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 16, 16, 16},
    {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30}, {31, 4, 4, 4, 4, 4, 4},
    {17, 17, 17, 17, 17, 17, 14}, {17, 17, 17, 17, 17, 10, 4},
    {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4}, {31, 1, 2, 4, 8, 16, 31},
    {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},
    {14, 17, 1, 2, 4, 8, 31}, {30, 1, 1, 14, 1, 1, 30},
    {2, 6, 10, 18, 31, 2, 2}, {31, 16, 16, 30, 1, 1, 30},
    {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
    {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14},
};

static const u16 paletteInk = COLOR(12, 8, 4);
static const u16 paletteMuted = COLOR(17, 12, 7);
static const u16 paletteWood = COLOR(19, 12, 6);
static const u16 paletteWoodLight = COLOR(27, 20, 12);
static const u16 palettePaper = COLOR(31, 27, 19);
static const u16 palettePaperLight = COLOR(31, 29, 23);
static const u16 paletteGreen = COLOR(12, 19, 9);
static const u16 paletteGold = COLOR(28, 20, 8);

bool uiInitialize(void)
{
    uiBackground = bgInitSub(2, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    uiBitmap = (u16 *)bgGetGfxPtr(uiBackground);
    if (uiBitmap == NULL)
    {
        snprintf(message, sizeof(message), "Bottom screen graphics failed.");
        return false;
    }
    return true;
}

bool uiInventoryOpen(void)
{
    return uiPage == UI_INVENTORY;
}

bool uiTakeInventoryDirty(void)
{
    bool dirty = inventoryDirty;
    inventoryDirty = false;
    return dirty;
}

bool uiTakeSettingsDirty(void)
{
    bool dirty = settingsDirty;
    settingsDirty = false;
    return dirty;
}

static void fillRect(int x, int y, int width, int height, u16 color)
{
    for (int row = y; row < y + height; row++)
    {
        u16 *pixel = uiBitmap + row * 256 + x;
        for (int column = 0; column < width; column++)
            pixel[column] = color;
    }
}

static void drawFrame(int x, int y, int width, int height)
{
    fillRect(x, y, width, height, paletteWood);
    fillRect(x + 2, y + 2, width - 4, height - 4, paletteWoodLight);
    fillRect(x + 4, y + 4, width - 8, height - 8, palettePaper);
}

static const u8 *getGlyph(char character, u8 punctuation[7])
{
    if (character >= 'a' && character <= 'z')
        character -= 'a' - 'A';
    if (character >= 'A' && character <= 'Z')
        return fontRows[character - 'A'];
    if (character >= '0' && character <= '9')
        return fontRows[26 + character - '0'];

    memset(punctuation, 0, 7);
    switch (character)
    {
    case '-': punctuation[3] = 14; break;
    case ':': punctuation[2] = 4; punctuation[5] = 4; break;
    case '.': punctuation[6] = 4; break;
    case ',': punctuation[5] = 4; punctuation[6] = 8; break;
    case '/': punctuation[0] = 1; punctuation[1] = 2; punctuation[2] = 2;
              punctuation[3] = 4; punctuation[4] = 8; punctuation[5] = 8;
              punctuation[6] = 16; break;
    case '+': punctuation[2] = 4; punctuation[3] = 14;
              punctuation[4] = 4; break;
    case '\'': punctuation[0] = 4; punctuation[1] = 4; break;
    case '(': punctuation[1] = 2; punctuation[2] = 4; punctuation[3] = 4;
              punctuation[4] = 4; punctuation[5] = 2; break;
    case ')': punctuation[1] = 8; punctuation[2] = 4; punctuation[3] = 4;
              punctuation[4] = 4; punctuation[5] = 8; break;
    case '!': punctuation[0] = 4; punctuation[1] = 4; punctuation[2] = 4;
              punctuation[3] = 4; punctuation[5] = 4; break;
    case '?': punctuation[0] = 14; punctuation[1] = 17; punctuation[2] = 1;
              punctuation[3] = 2; punctuation[4] = 4; punctuation[6] = 4; break;
    default: break;
    }
    return punctuation;
}

static void drawText(int x, int y, const char *text, u16 color)
{
    while (*text != '\0')
    {
        u8 punctuation[7];
        const u8 *glyph = getGlyph(*text++, punctuation);
        for (int row = 0; row < 7; row++)
        {
            for (int column = 0; column < 5; column++)
            {
                if ((glyph[row] & (1 << (4 - column))) != 0)
                    uiBitmap[(y + row) * 256 + x + column] = color;
            }
        }
        x += 6;
        if (x >= SCREEN_WIDTH - 5)
            break;
    }
}

static unsigned packedPixel(const u8 *sprite, int sourceSize, int x, int y)
{
    int tilesWide = sourceSize / 8;
    int tile = (y / 8) * tilesWide + x / 8;
    int pixel = tile * 64 + (y % 8) * 8 + x % 8;
    u8 packed = sprite[pixel / 2];
    return pixel & 1 ? packed >> 4 : packed & 0x0F;
}

static void drawPackedSprite(const u8 *sprite, int sourceSize,
                             const u16 *palette, int x, int y,
                             int drawWidth, int drawHeight)
{
    for (int row = 0; row < drawHeight; row++)
    {
        int sourceY = row * sourceSize / drawHeight;
        for (int column = 0; column < drawWidth; column++)
        {
            int sourceX = column * sourceSize / drawWidth;
            unsigned index = packedPixel(sprite, sourceSize, sourceX, sourceY);
            if (index != 0)
                uiBitmap[(y + row) * 256 + x + column] =
                    palette[index] | BIT(15);
        }
    }
}

static void drawWrappedMessage(int x, int y, const char *text, u16 color)
{
    int line = 0;
    int width = 38;
    while (*text != '\0' && line < 2)
    {
        char buffer[39];
        int length = 0;
        while (text[length] != '\0' && length < width)
            length++;
        if (text[length] != '\0')
        {
            int split = length;
            while (split > 0 && text[split] != ' ')
                split--;
            if (split > 0)
                length = split;
        }
        memcpy(buffer, text, length);
        buffer[length] = '\0';
        drawText(x, y + line * 10, buffer, color);
        text += length;
        while (*text == ' ')
            text++;
        line++;
    }
}

static void drawHeader(void)
{
    char line[48];
    drawText(12, 9, "LYNXIBEAR FARM", paletteGreen);
    snprintf(line, sizeof(line), "%s  DAY %02lu/%u  %s",
             seasonNames[gameCurrentSeason()], (unsigned long)game.day,
             SEASON_LENGTH, gameIsRaining() ? "RAIN" : "CLEAR");
    drawText(12, 21, line, paletteInk);
    snprintf(line, sizeof(line), "G %lu E %lu/%u",
             (unsigned long)game.gold, (unsigned long)game.energy,
             gameEnergyLimit());
    drawText(112, 9, line, paletteInk);
}

static void drawActivityPage(void)
{
    char line[48];

    if (view == VIEW_FARM)
    {
        drawText(12, 39, "FARM ACTIVITY", paletteGreen);
        drawText(12, 52, "A INTERACTS WITH TARGET TILE", paletteInk);
        drawText(12, 64, "B USES THE SELECTED TOOL", paletteMuted);
    }
    else
    {
        drawText(12, 39, "LYNXIBEAR VALLEY", paletteGreen);
        drawText(12, 52, townFocus == TOWN_STORE ? "GENERAL STORE" :
                 townFocus == TOWN_BOARD ? "VALLEY BOARD" : "FARM GATE",
                 paletteInk);
        if (townFocus == TOWN_STORE)
        {
            gameEnsureSeasonalShopOffer();
            const CropInfo *crop = &crops[selectedShopCrop];
            snprintf(line, sizeof(line), "%s STOCK: %s",
                     seasonNames[gameCurrentSeason()], crop->name);
            drawText(12, 64, line, paletteInk);
            snprintf(line, sizeof(line), "3 SEEDS - %u GOLD",
                     crop->seedPackPrice);
            drawText(12, 76, line, paletteInk);
            drawText(12, 88, "L/R CHANGES SEASONAL STOCK", paletteMuted);
        }
        else if (townFocus == TOWN_BOARD)
        {
            snprintf(line, sizeof(line), "UPGRADE %lu OF 2",
                     (unsigned long)(game.repairs + 1));
            drawText(12, 64, line, paletteInk);
            drawText(12, 76, game.repairs == 0 ?
                     "INVESTMENT: 250 GOLD" : "INVESTMENT: 650 GOLD",
                     paletteInk);
        }
        else
            drawText(12, 64, "PRESS A TO RETURN TO THE FARM", paletteInk);
    }

    drawText(12, 82, "LATEST", paletteGreen);
    drawWrappedMessage(12, 93, message, paletteMuted);
}

static void drawInfoPage(void)
{
    char line[48];
    drawText(12, 39, "FARM JOURNAL", paletteGreen);
    snprintf(line, sizeof(line), "REPAIRS %lu/2  SHIPPED %lu",
             (unsigned long)game.repairs, (unsigned long)game.harvests);
    drawText(12, 52, line, paletteInk);
    drawText(12, 64, "IN-SEASON SEEDS", paletteGreen);

    int y = 76;
    for (int i = 0; i < CROP_COUNT && y <= 100; i++)
    {
        if (crops[i].season != gameCurrentSeason())
            continue;
        snprintf(line, sizeof(line), "%s  %u DAYS  SEEDS %lu",
                 crops[i].name, crops[i].daysToGrow,
                 (unsigned long)gameInventoryCount(itemSeedId(i)));
        drawText(12, y, line, paletteInk);
        y += 10;
    }
}

static void drawControlsPage(void)
{
    drawText(12, 39, "CONTROLS", paletteGreen);
    if (view == VIEW_FARM)
    {
        drawText(12, 51, "D-PAD  WALK  |  START  TOWN", paletteInk);
        drawText(12, 61, "A  INTERACT: PLANT OR HARVEST", paletteInk);
        drawText(12, 71, "B  USE SELECTED TOOL", paletteInk);
        drawText(12, 81, "Y  SLEEP AND SHIP PRODUCE", paletteInk);
        drawText(12, 91, "L/R  SELECT INVENTORY SLOT", paletteInk);
        drawText(12, 101, "BAG STORAGE  |  SETTINGS HIGHLIGHT", paletteInk);
    }
    else
    {
        drawText(12, 51, "D-PAD  CHOOSE A LOCATION", paletteInk);
        drawText(12, 61, "A  VISIT OR USE LOCATION", paletteInk);
        drawText(12, 71, "B  RETURN TO THE FARM", paletteInk);
        drawText(12, 81, "L/R  CHOOSE SEASONAL STOCK", paletteInk);
        drawText(12, 91, "START  SWITCH BACK TO FARM", paletteInk);
    }
}

static void drawSettingsPage(void)
{
    drawText(12, 39, "SETTINGS", paletteGreen);
    drawText(12, 56, "TILE HIGHLIGHTER", paletteInk);

    fillRect(184, 51, 58, 22, paletteWood);
    fillRect(186, 53, 54, 18,
             game.tileHighlighter ? paletteGold : palettePaperLight);
    const char *state = game.tileHighlighter ? "ON" : "OFF";
    int textWidth = (int)strlen(state) * 6;
    drawText(213 - textWidth / 2, 58, state, paletteInk);
    drawText(12, 79, "TOUCH THE SWITCH TO TOGGLE", paletteMuted);
}

static void drawItemIcon(const InventorySlot *slot, int x, int y, int size)
{
    if (slot->item == ITEM_NONE)
        return;

    if (itemIsTool(slot->item))
    {
        unsigned tool = itemToolIndex(slot->item);
        drawPackedSprite(sproutUtilityIcons[tool], 16, sproutUtilityPalette,
                         x, y, size, size);
    }
    else if (slot->item == ITEM_WOOD)
        drawPackedSprite(sproutUtilityIcons[3], 16, sproutUtilityPalette,
                         x, y, size, size);
    else if (itemIsSeed(slot->item))
        drawPackedSprite(sproutSeedIcon, 16, sproutSeedPalette,
                         x, y, size, size);
    else
    {
        unsigned crop = itemCropIndex(slot->item);
        if (crop < CROP_COUNT)
            drawPackedSprite(sproutCropIcons[crop], 16,
                             sproutCropPalettes[crop], x, y, size, size);
    }
}

static void drawSlotContents(const InventorySlot *slot, int x, int y,
                             bool selected, int width)
{
    u16 border = selected ? paletteGold : paletteWood;
    fillRect(x, y, width, width, border);
    fillRect(x + 1, y + 1, width - 2, width - 2, paletteWoodLight);
    fillRect(x + 2, y + 2, width - 4, width - 4, palettePaperLight);

    bool tool = itemIsTool(slot->item);
    int iconSize = tool ? 16 : 12;
    int iconY = tool ? y + 5 : y + 1;
    drawItemIcon(slot, x + (width - iconSize) / 2, iconY, iconSize);
    if (slot->item != ITEM_NONE && !itemIsTool(slot->item))
    {
        char count[11];
        snprintf(count, sizeof(count), "%02u", slot->count);
        drawText(x + (width - 12) / 2, y + width - 9, count, paletteInk);
    }
}

static void drawInventoryBar(void)
{
    char line[48];

    int labelY = uiPage == UI_INVENTORY ? 121 : 115;
    drawText(12, labelY, "BAR", paletteGreen);
    InventorySlot *selected = &game.inventory[selectedSlot];
    if (selected->item == ITEM_NONE)
        snprintf(line, sizeof(line), "SLOT %d: EMPTY", selectedSlot + 1);
    else
        snprintf(line, sizeof(line), "SLOT %d: %s X%u", selectedSlot + 1,
                 itemName(selected->item), selected->count);
    drawText(55, labelY, line, paletteInk);

    for (int slot = 0; slot < INVENTORY_SLOTS; slot++)
    {
        int x = UI_SLOT_X + slot * UI_SLOT_STEP;
        drawSlotContents(&game.inventory[slot], x, UI_SLOT_Y,
                         slot == selectedSlot, UI_SLOT_SIZE);
    }

    snprintf(line, sizeof(line), "%s", storageMessage);
    drawText(12, 159, line, paletteMuted);
}

static void drawStoragePage(void)
{
    drawText(12, 37, "STORAGE - DRAG ITEMS TO MOVE", paletteGreen);
    for (int row = 0; row < STORAGE_ROWS; row++)
    {
        for (int column = 0; column < STORAGE_COLUMNS; column++)
        {
            int slot = row * STORAGE_COLUMNS + column;
            int x = UI_SLOT_X + column * UI_SLOT_STEP;
            int y = UI_STORAGE_SLOT_Y + row * UI_STORAGE_ROW_STEP;
            drawSlotContents(&game.storage[slot], x, y, false, UI_SLOT_SIZE);
        }
    }
}

static void drawButton(int x, const char *label, bool active,
                       bool settingsIcon)
{
    u16 base = active ? paletteGold : palettePaperLight;
    int textWidth = (int)strlen(label) * 6;
    fillRect(x, 171, 58, 18, paletteWood);
    fillRect(x + 2, 173, 54, 14, base);
    if (settingsIcon)
    {
        drawPackedSprite(sproutSettingsIcon, 16, sproutSettingsPalette,
                         x + 3, 174, 12, 12);
        drawText(x + 19, 176, "SET", paletteInk);
    }
    else
        drawText(x + (58 - textWidth) / 2, 176, label, paletteInk);
}

void uiRenderStatus(void)
{
    if (uiBitmap == NULL)
        return;

    fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, paletteWoodLight);
    drawFrame(4, 4, 248, uiPage == UI_INVENTORY ? 165 : 106);
    fillRect(8, 8, 240, 24, palettePaperLight);
    fillRect(8, 33, 240, 1, paletteWood);
    drawHeader();

    if (uiPage == UI_INVENTORY)
        drawStoragePage();
    else if (uiPage == UI_INFO)
        drawInfoPage();
    else if (uiPage == UI_CONTROLS)
        drawControlsPage();
    else if (uiPage == UI_SETTINGS)
        drawSettingsPage();
    else
        drawActivityPage();

    drawInventoryBar();
    drawButton(4, "INFO", uiPage == UI_INFO, false);
    drawButton(66, "BAG", uiPage == UI_INVENTORY, false);
    drawButton(128, "CONTROLS", uiPage == UI_CONTROLS, false);
    drawButton(190, "SETTINGS", uiPage == UI_SETTINGS, true);
}

static bool uiHandleButtonPress(int x, int y)
{
    if (y < 168)
        return false;

    int selectedPage;
    if (x >= 4 && x < 62)
        selectedPage = UI_INFO;
    else if (x >= 66 && x < 124)
        selectedPage = UI_INVENTORY;
    else if (x >= 128 && x < 186)
        selectedPage = UI_CONTROLS;
    else if (x >= 190 && x < 248)
        selectedPage = UI_SETTINGS;
    else
        return false;

    uiPage = selectedPage == uiPage ? UI_ACTIVITY : selectedPage;
    return true;
}

static bool touchSlotAt(int x, int y, bool *storage, unsigned *slot)
{
    if (uiPage == UI_INVENTORY && y >= UI_STORAGE_SLOT_Y &&
        y < UI_STORAGE_SLOT_Y +
                (STORAGE_ROWS - 1) * UI_STORAGE_ROW_STEP + UI_SLOT_SIZE)
    {
        int row = (y - UI_STORAGE_SLOT_Y) / UI_STORAGE_ROW_STEP;
        int column = (x - UI_SLOT_X) / UI_SLOT_STEP;
        if (x < UI_SLOT_X || column < 0 ||
            column >= STORAGE_COLUMNS ||
            x >= UI_SLOT_X + STORAGE_COLUMNS * UI_SLOT_STEP)
            return false;
        if (x >= UI_SLOT_X + column * UI_SLOT_STEP + UI_SLOT_SIZE ||
            y >= UI_STORAGE_SLOT_Y + row * UI_STORAGE_ROW_STEP +
                     UI_SLOT_SIZE)
            return false;
        *storage = true;
        *slot = (unsigned)(row * STORAGE_COLUMNS + column);
        return true;
    }

    if (y >= UI_SLOT_Y && y < UI_SLOT_Y + UI_SLOT_SIZE)
    {
        int column = (x - UI_SLOT_X) / UI_SLOT_STEP;
        if (x < UI_SLOT_X || column < 0 || column >= INVENTORY_SLOTS ||
            x >= UI_SLOT_X + INVENTORY_SLOTS * UI_SLOT_STEP ||
            x >= UI_SLOT_X + column * UI_SLOT_STEP + UI_SLOT_SIZE)
            return false;
        *storage = false;
        *slot = (unsigned)column;
        return true;
    }

    return false;
}

static bool finishDrag(int x, int y)
{
    bool targetStorage;
    unsigned targetSlot;
    bool changed = false;

    if (touchSlotAt(x, y, &targetStorage, &targetSlot))
    {
        if (dragSourceStorage == targetStorage &&
            dragSourceSlot == targetSlot)
        {
            if (!dragSourceStorage && selectedSlot != (int)dragSourceSlot)
            {
                selectedSlot = (int)dragSourceSlot;
                changed = true;
            }
        }
        else if (gameMoveInventoryItem(dragSourceStorage, dragSourceSlot,
                                       targetStorage, targetSlot))
        {
            inventoryDirty = true;
            changed = true;
            if (dragSourceStorage && !targetStorage)
                selectedSlot = (int)targetSlot;
        }
    }
    else if (!dragSourceStorage && selectedSlot != (int)dragSourceSlot)
    {
        selectedSlot = (int)dragSourceSlot;
        changed = true;
    }

    return changed;
}

bool uiHandleTouch(u32 pressedKeys, u32 heldKeys)
{
    bool changed = false;
    touchPosition touch;

    if ((pressedKeys & KEY_TOUCH) != 0)
    {
        touchRead(&touch);
        lastTouchX = touch.px;
        lastTouchY = touch.py;
        if (uiHandleButtonPress(lastTouchX, lastTouchY))
            return true;
        if (uiPage == UI_SETTINGS && lastTouchX >= 184 &&
            lastTouchX < 242 && lastTouchY >= 51 && lastTouchY < 73)
        {
            game.tileHighlighter = !game.tileHighlighter;
            settingsDirty = true;
            return true;
        }
        if (touchSlotAt(lastTouchX, lastTouchY,
                        &dragSourceStorage, &dragSourceSlot))
            dragActive = true;
    }

    if (dragActive && (heldKeys & KEY_TOUCH) != 0)
    {
        touchRead(&touch);
        lastTouchX = touch.px;
        lastTouchY = touch.py;
    }
    else if (dragActive)
    {
        changed = finishDrag(lastTouchX, lastTouchY);
        dragActive = false;
    }

    return changed;
}
