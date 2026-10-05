#ifndef STARDSI_ITEMS_H
#define STARDSI_ITEMS_H

#include "game.h"

extern const CropInfo crops[CROP_COUNT];

enum
{
    ITEM_NONE,
    ITEM_SEED_FIRST = 1,
    ITEM_PRODUCE_FIRST = ITEM_SEED_FIRST + CROP_COUNT,
    ITEM_PRODUCE_LAST = ITEM_PRODUCE_FIRST + CROP_COUNT - 1,
    ITEM_AXE,
    ITEM_HOE,
    ITEM_WATERING_CAN,
    ITEM_WOOD,
    ITEM_LAST = ITEM_WOOD
};

unsigned itemSeedId(unsigned cropIndex);
unsigned itemProduceId(unsigned cropIndex);
bool itemIsSeed(unsigned itemId);
bool itemIsProduce(unsigned itemId);
bool itemIsTool(unsigned itemId);
unsigned itemToolIndex(unsigned itemId);
unsigned itemCropIndex(unsigned itemId);
const char *itemName(unsigned itemId);
void itemShortName(unsigned itemId, char shortName[3]);

#endif
