#ifndef STARDSI_MAPS_H
#define STARDSI_MAPS_H

#include <nds.h>

enum { TOWN_BUILDING_COUNT = 3 };

void mapsInitialize(u16 *frameBuffer);
void mapsDraw(int mapId);
// True if the screen pixel lies on a fence of the crossroads map.
bool mapsRoadsSolid(int x, int y);
// Town collision: the building a feet box overlaps (or -1), the shop its door
// opens (TOWN_STORE, TOWN_BOARD, or -1 for a home), its door's centre x, the y
// just below the buildings, and whether the box hits a tree trunk.
int mapsTownBuildingAt(int left, int top, int right, int bottom);
int mapsTownBuildingShop(int building);
int mapsTownBuildingDoorX(int building);
int mapsTownDoorFrontY(void);
bool mapsTownTreeSolid(int left, int top, int right, int bottom);
void mapsDrawTitle(void);
u16 *mapsBitmap(void);
// Forces the next farm draw to repaint the whole bitmap.
void mapsInvalidate(void);
void mapsFillBlack(void);

#endif
