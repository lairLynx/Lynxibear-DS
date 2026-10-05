#ifndef STARDSI_MAPS_H
#define STARDSI_MAPS_H

#include <nds.h>

void mapsInitialize(u16 *frameBuffer);
void mapsDraw(int mapId);
// True if the screen pixel lies on a fence of the crossroads map.
bool mapsRoadsSolid(int x, int y);
void mapsDrawTitle(void);
u16 *mapsBitmap(void);

#endif
