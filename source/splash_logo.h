#ifndef STARDSI_SPLASH_LOGO_H
#define STARDSI_SPLASH_LOGO_H

#include <nds.h>

enum
{
    SPLASH_LOGO_WIDTH = 72,
    SPLASH_LOGO_HEIGHT = 104
};

// Coverage (0-255) of the lairLynx Studios logo, row by row.
extern const u8 splashLogo[SPLASH_LOGO_WIDTH * SPLASH_LOGO_HEIGHT];

#endif