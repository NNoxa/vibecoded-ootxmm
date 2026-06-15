#pragma once

#ifndef ITEM_CATEGORY_ADJ_H
#define ITEM_CATEGORY_ADJ_H

#include "../item-tables/ItemTableTypes.h"
#include "randomizerTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

GetItemCategory Randomizer_AdjustItemCategory(GetItemEntry item);
GetItemCategory Randomizer_AdjustItemCategoryForCheck(GetItemEntry item, RandomizerCheck rc);

#ifdef __cplusplus
}
#endif

#endif
