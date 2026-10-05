#ifndef STARDSI_PLAYER_H
#define STARDSI_PLAYER_H

#include <nds.h>

void playerInitialize(void);
void playerUpdateSprite(void);
void playerStartToolAnimation(void);
bool playerMove(u32 heldKeys);

#endif
