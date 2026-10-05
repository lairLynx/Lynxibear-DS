#ifndef STARDSI_PLAYER_H
#define STARDSI_PLAYER_H

#include <nds.h>

void playerInitialize(void);
void playerUpdateSprite(void);
void playerStartToolAnimation(unsigned toolIndex);
bool playerMove(u32 heldKeys);
// True if walking changed the status message (for example a closed exit).
bool playerTakeStatusDirty(void);

#endif
