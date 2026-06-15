#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "soh/Enhancements/item-tables/ItemTableTypes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

struct PlayState;

namespace Ship {
class Archive;
}

namespace SohCrossover {

struct SharedPlacement {
    std::string check;
    std::string nativeCheck;
    std::string item;
    std::string itemSourceGame;
    std::string itemSourceCheck;
    std::string model;
    bool isCrossGame = false;
};

void PublishOoTxMmSeed(const std::string& seedString, uint32_t sohFinalSeed);
std::optional<SharedPlacement> GetSharedPlacementByNativeCheck(const std::string& nativeCheck);
std::optional<std::string> GetModelIdByNativeCheck(const std::string& nativeCheck);
std::optional<std::string> GetSharedPlacementDisplayNameByNativeCheck(const std::string& nativeCheck);
std::optional<std::string> GetOoTxMmGossipHintMessage(const std::string& stoneKey);
bool IsCrossGameMmCheck(RandomizerCheck rc);
bool ShouldGiveNativeItemForCrossGameMmCheck(RandomizerCheck rc);
bool ShouldUseCrossoverVisualForCrossGameMmCheck(RandomizerCheck rc);
bool TryGiveNativeItemForCrossGameMmCheck(RandomizerCheck rc, PlayState* play);
std::optional<GetItemCategory> GetCrossGameMmItemCategory(RandomizerCheck rc);
std::optional<std::string> GetCrossGameMmPickupMessage(RandomizerCheck rc);
void QueueCrossGameMmPickupTextbox(RandomizerCheck rc);
std::optional<std::string> ConsumePendingCrossGameMmPickupTextbox();
bool ConsumePendingCrossGameMmNativeRewardSuppression(GetItemEntry getItemEntry);
bool EnsureMmO2rArchiveLoaded();
std::shared_ptr<Ship::Archive> GetMmO2rArchive();
int CountMmProgressiveSwordGrantsForCurrentSave();

}
