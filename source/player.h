#ifndef STARDSI_PLAYER_H
#define STARDSI_PLAYER_H

#include <nds.h>

void playerInitialize(void);
void playerUpdateSprite(void);
void playerStartToolAnimation(unsigned toolIndex);
bool playerMove(u32 heldKeys);
// True if walking changed the status message (for example a closed exit).
bool playerTakeStatusDirty(void);
// True once when the farmer walked into the house door.
bool playerTakeEnterHouse(void);
// The shop whose door the farmer just walked into (TOWN_STORE or TOWN_BOARD), else -1.
int playerTakeEnterShop(void);
// Stands the farmer in front of a shop's door in town.
void playerPlaceAtShopDoor(int shop);
void playerSetHidden(bool hidden);
void playerPlaceAtHouseDoor(void);

#endif
