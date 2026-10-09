#include "items.h"

const CropInfo crops[CROP_COUNT] = {
    {"Parsnip", "Spring", 2, SPRING, 10, 35, COLOR(28, 21, 9)},
    {"Tomato", "Summer", 3, SUMMER, 15, 55, COLOR(29, 9, 7)},
    {"Pumpkin", "Autumn", 4, AUTUMN, 20, 80, COLOR(30, 15, 5)},
    {"Yam", "Winter", 5, WINTER, 25, 110, COLOR(19, 10, 20)},
    {"Potato", "Spring", 3, SPRING, 12, 48, COLOR(24, 20, 13)},
    {"Cauliflower", "Spring", 5, SPRING, 19, 82, COLOR(25, 25, 21)},
    {"Corn", "Summer", 4, SUMMER, 17, 72, COLOR(29, 23, 8)},
    {"Winter Root", "Winter", 4, WINTER, 19, 82, COLOR(23, 12, 8)},
    {"Snow Yam", "Winter", 5, WINTER, 24, 105, COLOR(18, 17, 27)},
};

unsigned itemSeedId(unsigned cropIndex)
{
    return ITEM_SEED_FIRST + cropIndex;
}

unsigned itemProduceId(unsigned cropIndex)
{
    return ITEM_PRODUCE_FIRST + cropIndex;
}

bool itemIsSeed(unsigned itemId)
{
    return itemId >= ITEM_SEED_FIRST && itemId < ITEM_PRODUCE_FIRST;
}

bool itemIsProduce(unsigned itemId)
{
    return itemId >= ITEM_PRODUCE_FIRST && itemId <= ITEM_PRODUCE_LAST;
}

bool itemIsTool(unsigned itemId)
{
    return itemId >= ITEM_AXE && itemId <= ITEM_WATERING_CAN;
}

unsigned itemToolIndex(unsigned itemId)
{
    switch (itemId)
    {
    case ITEM_AXE: return 0;
    case ITEM_HOE: return 1;
    case ITEM_WATERING_CAN: return 2;
    default: return 3;
    }
}

unsigned itemCropIndex(unsigned itemId)
{
    if (itemId >= ITEM_SEED_FIRST && itemId < ITEM_PRODUCE_FIRST)
        return itemId - ITEM_SEED_FIRST;
    if (itemIsProduce(itemId))
        return itemId - ITEM_PRODUCE_FIRST;
    return CROP_COUNT;
}

const char *itemName(unsigned itemId)
{
    unsigned cropIndex = itemCropIndex(itemId);
    if (cropIndex >= CROP_COUNT)
    {
        switch (itemId)
        {
        case ITEM_AXE: return "Axe";
        case ITEM_HOE: return "Hoe";
        case ITEM_WATERING_CAN: return "Watering Can";
        case ITEM_WOOD: return "Wood";
        default: return "Empty";
        }
    }
    return crops[cropIndex].name;
}

void itemShortName(unsigned itemId, char shortName[3])
{
    static const char *const seedCodes[CROP_COUNT] = {
        "PS", "TS", "PU", "YS", "PT", "CF", "CS", "WR", "SY"};
    static const char *const cropCodes[CROP_COUNT] = {
        "PN", "TM", "PK", "YM", "PO", "CF", "CR", "WR", "SY"};
    static const char *const specialCodes[] = {"AX", "HO", "WC", "WD"};

    unsigned cropIndex = itemCropIndex(itemId);
    if (cropIndex >= CROP_COUNT)
    {
        unsigned special = itemId >= ITEM_AXE ? itemId - ITEM_AXE : 4;
        shortName[0] = special < 4 ? specialCodes[special][0] : '-';
        shortName[1] = special < 4 ? specialCodes[special][1] : '-';
    }
    else
    {
        const char *code = itemIsSeed(itemId) ? seedCodes[cropIndex] : cropCodes[cropIndex];
        shortName[0] = code[0];
        shortName[1] = code[1];
    }
    shortName[2] = '\0';
}
