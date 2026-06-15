#include <stdint.h>
#include "item_category_adj.h"
#include "soh/Crossover/Crossover.h"
#include "z64item.h"
#include "variables.h"
#include "macros.h"

static bool IsForcedMajorNativeItem(const GetItemEntry& item) {
    if (item.modIndex != MOD_NONE) {
        return false;
    }

    const uint32_t id = item.itemId;
    return
        (id >= ITEM_BOW && id <= ITEM_NAYRUS_LOVE) ||
        (id >= ITEM_BOTTLE && id <= ITEM_POE) ||
        (id >= ITEM_MASK_KEATON && id <= ITEM_MASK_TRUTH) ||
        (id >= ITEM_SWORD_KOKIRI && id <= ITEM_FISHING_POLE) ||
        (id >= ITEM_SONG_MINUET && id <= ITEM_SONG_STORMS) ||
        (id >= ITEM_MEDALLION_FOREST && id <= ITEM_GERUDO_CARD) ||
        id == ITEM_MASK_DEKU || id == ITEM_MASK_FIERCE_DEITY;
}

static bool IsForcedMajorRandomizerItem(const GetItemEntry& item) {
    if (item.modIndex != MOD_RANDOMIZER) {
        return false;
    }

    const uint32_t id = item.getItemId;
    return
        (id >= RG_PROGRESSIVE_HOOKSHOT && id <= RG_PROGRESSIVE_GORONSWORD) ||
        (id >= RG_OCARINA_A_BUTTON && id <= RG_OCARINA_C_RIGHT_BUTTON) ||
        (id >= RG_GOHMA_SOUL && id <= RG_GANON_SOUL) ||
        id == RG_FISHING_POLE ||
        id == RG_SKELETON_KEY ||
        id == RG_ROCS_FEATHER ||
        id == RG_MASK_OF_TRUTH ||
        id == RG_CHILD_WALLET ||
        id == RG_TYCOON_WALLET ||
        id == RG_MAGIC_DOUBLE;
}

GetItemCategory Randomizer_AdjustItemCategoryForCheck(GetItemEntry item, RandomizerCheck rc) {
    const auto crossoverCategory = SohCrossover::GetCrossGameMmItemCategory(rc);
    if (crossoverCategory.has_value()) {
        return *crossoverCategory;
    }

    GetItemCategory category = item.getItemCategory;

    // Downgrade bombchus to lesser if the player already has bombchus
    if (INV_CONTENT(ITEM_BOMBCHU) == ITEM_BOMBCHU &&
        ((item.modIndex == MOD_RANDOMIZER && item.getItemId == RG_PROGRESSIVE_BOMBCHU_BAG) ||
         (item.modIndex == MOD_NONE &&
          (item.getItemId == GI_BOMBCHUS_5 || item.getItemId == GI_BOMBCHUS_10 || item.getItemId == GI_BOMBCHUS_20)))) {
        category = ITEM_CATEGORY_LESSER;
    }

    // Downgrade bottles to lesser if the player already has a bottle
    if ((item.modIndex == MOD_RANDOMIZER && item.getItemId >= RG_BOTTLE_WITH_RED_POTION &&
         item.getItemId <= RG_BOTTLE_WITH_POE) ||
        (item.modIndex == MOD_NONE && (item.getItemId == GI_BOTTLE || item.getItemId == GI_MILK_BOTTLE))) {
        if (gSaveContext.inventory.items[SLOT_BOTTLE_1] != ITEM_NONE) {
            category = ITEM_CATEGORY_LESSER;
        }
    }

    if (IsForcedMajorNativeItem(item) || IsForcedMajorRandomizerItem(item)) {
        category = ITEM_CATEGORY_MAJOR;
    }

    return category;
}

GetItemCategory Randomizer_AdjustItemCategory(GetItemEntry item) {
    return Randomizer_AdjustItemCategoryForCheck(item, RC_UNKNOWN_CHECK);
}
