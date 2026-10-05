#ifndef STARDSI_UI_H
#define STARDSI_UI_H

#include <stdbool.h>
#include <nds.h>

bool uiInitialize(void);
void uiRenderStatus(void);
bool uiHandleTouch(u32 pressedKeys, u32 heldKeys);
bool uiInventoryOpen(void);
bool uiTakeInventoryDirty(void);
bool uiTakeSettingsDirty(void);

#endif
