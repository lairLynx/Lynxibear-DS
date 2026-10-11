#ifndef STARDSI_RENDER_H
#define STARDSI_RENDER_H

#include <stdbool.h>

void renderInitialize(void);
void renderScene(void);
// Fades both screens to black or back in, one brightness level per frame
// (about 0.27 s). The music stream keeps being fed.
void renderFade(bool toBlack);

#endif
