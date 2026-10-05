#ifndef STARDSI_SAVE_H
#define STARDSI_SAVE_H

#include "game.h"

#include <nds.h>

extern char storageMessage[44];

bool saveInitialize(void);
bool saveLoadGame(void);
bool saveGame(void);
bool savePeek(SaveData *out);

#endif
