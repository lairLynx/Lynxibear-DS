#include "menu.h"

#include "audio.h"
#include "game.h"
#include "maps.h"
#include "save.h"
#include "ui.h"

enum
{
    MENU_CONTINUE,
    MENU_NEW_GAME
};

bool menuRun(bool saveAvailable)
{
    SaveData saved;
    bool hasSave = saveAvailable && savePeek(&saved);
    int selected = hasSave ? MENU_CONTINUE : MENU_NEW_GAME;
    bool confirming = false;
    bool redraw = true;

    mapsDrawTitle();
    uiDrawTitleText(mapsBitmap());

    while (1)
    {
        if (redraw)
        {
            uiRenderMenu(selected, hasSave, confirming,
                         hasSave ? &saved : NULL, storageMessage);
            redraw = false;
        }

        swiWaitForVBlank();
        audioUpdate();
        scanKeys();
        u32 pressed = keysDown();
        bool activate = (pressed & KEY_A) != 0;

        if (pressed & KEY_TOUCH)
        {
            touchPosition touch;
            touchRead(&touch);
            int button = uiMenuButtonAt(touch.px, touch.py);
            if (button >= 0 && (confirming || hasSave || button == MENU_NEW_GAME))
            {
                selected = button;
                activate = true;
                redraw = true;
            }
        }

        if (!confirming && hasSave && (pressed & (KEY_UP | KEY_DOWN)))
        {
            selected = selected == MENU_CONTINUE ? MENU_NEW_GAME : MENU_CONTINUE;
            audioPlaySound(SOUND_HOVER);
            redraw = true;
        }
        else if (confirming && (pressed & (KEY_UP | KEY_DOWN)))
        {
            selected = selected == 0 ? 1 : 0;
            audioPlaySound(SOUND_HOVER);
            redraw = true;
        }

        if (confirming && (pressed & KEY_B))
        {
            audioPlaySound(SOUND_BACK);
            confirming = false;
            selected = MENU_NEW_GAME;
            redraw = true;
        }

        if (!activate)
            continue;

        audioPlaySound(SOUND_CONFIRM);

        if (confirming)
        {
            if (selected == 0)
                return false;
            confirming = false;
            selected = MENU_NEW_GAME;
        }
        else if (selected == MENU_CONTINUE)
            return true;
        else if (hasSave)
        {
            confirming = true;
            selected = 1;
        }
        else
            return false;
        redraw = true;
    }
}