#ifndef STARDSI_PLAYER_H
#define STARDSI_PLAYER_H

#include <nds.h>

void playerInitialize(void);
void playerUpdateSprite(void);
void playerStartToolAnimation(unsigned toolIndex);
bool playerMove(u32 heldKeys);

#endif
