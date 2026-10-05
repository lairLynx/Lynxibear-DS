#include "audio.h"
#include "game.h"
#include "items.h"
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
        u32 repeated = keysDownRepeat();
        bool changed = false;
        bool sceneChanged = false;

        if (pressed & KEY_START)
        {
            if (view == VIEW_TOWN)
            {
                view = townReturnView;
                snprintf(message, sizeof(message), view == VIEW_ROADS ?
                         "Back on the crossroads." : "Back at the farm.");
            }
            else
            {
                townReturnView = view;
                view = VIEW_TOWN;
                snprintf(message, sizeof(message), "Lynxibear Valley: choose a place to visit.");
            }
            changed = true;
            sceneChanged = true;
        }

        if (uiHandleTouch(pressed, held))
            changed = true;

        if (!uiInventoryOpen() && gameViewWalkable())
        {
            playerMove(held);
            if (game.screen != shownScreen || view != shownView)
            {
                changed = true;
                sceneChanged = true;
            }
            if (playerTakeStatusDirty())
                changed = true;

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
            else if (pressed & KEY_Y)
            {
                gameSleepUntilMorning();
                changed = true;
                sceneChanged = true;
            }
            else if (pressed & KEY_L)
            {
                gameCycleInventory(-1);
                changed = true;
            }
            else if (pressed & KEY_R)
            {
                gameCycleInventory(1);
                changed = true;
            }
        }
        else if (!uiInventoryOpen())
        {
            if (repeated & (KEY_LEFT | KEY_UP))
            {
                gameMoveTownFocus(-1);
                changed = true;
                sceneChanged = true;
            }
            else if (repeated & (KEY_RIGHT | KEY_DOWN))
            {
                gameMoveTownFocus(1);
                changed = true;
                sceneChanged = true;
            }

            if (pressed & KEY_A)
            {
                gameInteractTown();
                changed = true;
                sceneChanged = true;
            }
            else if (pressed & KEY_B)
            {
                view = townReturnView;
                snprintf(message, sizeof(message), view == VIEW_ROADS ?
                         "Back on the crossroads." : "Back at the farm.");
                changed = true;
                sceneChanged = true;
            }
            else if (pressed & KEY_R)
            {
                gameCycleShopCrop(1);
                changed = true;
            }
            else if (pressed & KEY_L)
            {
                gameCycleShopCrop(-1);
                changed = true;
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
            else if (view == VIEW_TOWN && (pressed & KEY_A))
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
