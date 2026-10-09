#include "audio.h"
#include "game.h"
#include "items.h"
#include "maps.h"
#include "menu.h"
#include "player.h"
#include "render.h"
#include "save.h"
#include "ui.h"

#include <stdio.h>

static void saveProgress(void)
{
    saveGame();
}

static void waitFrames(int frames)
{
    for (int i = 0; i < frames; i++)
    {
        swiWaitForVBlank();
        audioUpdate();
    }
}

// Fades both screens to black (or back up from black).
static void fadeScreens(bool toBlack)
{
    for (int step = 0; step <= 16; step++)
    {
        setBrightness(3, toBlack ? -step : step - 16);
        waitFrames(2);
    }
}

// Going to bed: fade to a night card, sleep (the sleep sound plays), save, then
// wake the farmer on the next day in front of the house.
static void windDownDay(void)
{
    fadeScreens(true);
    playerSetHidden(true);
    playerUpdateSprite();
    uiRenderNightCard(mapsBitmap());
    fadeScreens(false);

    gameSleepUntilMorning();
    view = VIEW_FARM;
    game.screen = HOME_SCREEN;
    saveProgress();
    waitFrames(200);

    fadeScreens(true);
    playerPlaceAtHouseDoor();
    mapsInvalidate();
    renderScene();
    uiRenderStatus();
    playerSetHidden(false);
    playerUpdateSprite();
    fadeScreens(false);
}

// Leaves a shop menu and stands the farmer in front of its door in town.
static void leaveShop(void)
{
    audioPlaySound(SOUND_BACK);
    view = VIEW_TOWN;
    playerPlaceAtShopDoor(townFocus);
    snprintf(message, sizeof(message), "Back in Lynxibear Valley.");
}

int main(void)
{
    int shownScreen = 0;
    int shownView = VIEW_FARM;
    renderInitialize();
    gameNew();
    bool saveReady = saveInitialize();
    uiInitialize();
    audioInitialize();

    if (menuRun(saveReady))
    {
        if (!saveLoadGame())
            gameNew();
    }
    else if (saveReady)
    {
        gameNew();
        saveProgress();
    }

    shownScreen = game.screen;
    shownView = view;
    playerInitialize();
    audioPlay(MUSIC_GAME);
    renderScene();
    uiRenderStatus();

    while (1)
    {
        scanKeys();
        u32 pressed = keysDown();
        u32 held = keysHeld();
        bool changed = false;
        bool sceneChanged = false;
        bool leftShop = false;  // B/START that leaves a shop must not also use a tool
        u32 repeated = keysDownRepeat();

        if (view == VIEW_SHOP && (pressed & (KEY_START | KEY_B)))
        {
            leaveShop();
            leftShop = true;
            changed = true;
            sceneChanged = true;
        }

        if (uiHandleTouch(pressed, held))
            changed = true;

        if (!uiInventoryOpen() && !leftShop && gameViewWalkable())
        {
            playerMove(held);
            if (playerTakeEnterHouse() || (pressed & KEY_Y))
            {
                windDownDay();
                shownScreen = game.screen;
                shownView = view;
                continue;
            }
            if (game.screen != shownScreen || view != shownView)
            {
                changed = true;
                sceneChanged = true;
            }
            if (playerTakeStatusDirty())
                changed = true;

            int shop = playerTakeEnterShop();
            if (shop >= 0)
            {
                view = VIEW_SHOP;
                townFocus = shop;
                audioPlaySound(SOUND_SHOP_BELL);
                snprintf(message, sizeof(message), shop == TOWN_STORE ?
                         "General store: left/right picks, A buys." :
                         "Carpenter: A invests, B leaves.");
                changed = true;
                sceneChanged = true;
            }

            if (pressed & KEY_A)
            {
                gameInteractFarm();
                changed = true;
                sceneChanged = true;
            }
            else if (pressed & KEY_B)
            {
                gameUseSelectedTool();
                if (selectedSlot >= 0 && selectedSlot < INVENTORY_SLOTS &&
                    itemIsTool(game.inventory[selectedSlot].item))
                    playerStartToolAnimation(itemToolIndex(
                        game.inventory[selectedSlot].item));
                changed = true;
                sceneChanged = true;
            }
            else if (pressed & KEY_L)
            {
                audioPlaySound(SOUND_CLICK);
                gameCycleInventory(-1);
                changed = true;
            }
            else if (pressed & KEY_R)
            {
                audioPlaySound(SOUND_CLICK);
                gameCycleInventory(1);
                changed = true;
            }
        }
        else if (!uiInventoryOpen() && !leftShop && view == VIEW_SHOP)
        {
            if (pressed & KEY_A)
            {
                gameInteractTown();
                changed = true;
                sceneChanged = true;
            }
            else if (townFocus == TOWN_STORE &&
                     (repeated & (KEY_L | KEY_R | KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN)))
            {
                audioPlaySound(SOUND_CLICK);
                gameCycleShopCrop((repeated & (KEY_R | KEY_RIGHT | KEY_DOWN)) ? 1 : -1);
                changed = true;
                sceneChanged = true;
            }
        }

        if (changed)
        {
            if (uiTakeInventoryDirty())
                saveProgress();
            if (uiTakeSettingsDirty())
            {
                saveProgress();
                sceneChanged = true;
            }
            if (gameViewWalkable() &&
                ((pressed & (KEY_A | KEY_B | KEY_Y)) != 0))
                saveProgress();
            else if (view == VIEW_SHOP && (pressed & KEY_A))
                saveProgress();

            if (view != shownView || game.screen != shownScreen)
                sceneChanged = true;
            if (sceneChanged)
            {
                shownScreen = game.screen;
                shownView = view;
                renderScene();
            }
            uiRenderStatus();
        }
        swiWaitForVBlank();
        audioUpdate();
        playerUpdateSprite();
    }

    return 0;
}
