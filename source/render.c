#include "render.h"

#include "game.h"
#include "maps.h"

static int farmBackground;

void renderInitialize(void)
{
    videoSetMode(MODE_5_2D | DISPLAY_BG2_ACTIVE |
                 DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D_LAYOUT);
    videoSetModeSub(MODE_5_2D | DISPLAY_BG2_ACTIVE);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE);
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankD(VRAM_D_LCD);
    vramSetBankI(VRAM_I_SUB_SPRITE);

    farmBackground = bgInit(2, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    mapsInitialize((u16 *)bgGetGfxPtr(farmBackground));
}

void renderScene(void)
{
    mapsDraw(view);
}
