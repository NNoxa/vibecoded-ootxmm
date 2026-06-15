#pragma once

#include <optional>
#include <string>
#include <memory>

#include "2s2h/Rando/Types.h"
#include <libultraship/libultraship.h>

namespace TwoShipCrossover {

struct SharedPlacement {
    std::string check;
    std::string nativeCheck;
    std::string item;
    std::string itemSourceGame;
    std::string itemSourceCheck;
    std::string model;
    bool isCrossGame = false;
};

std::optional<std::string> GetOoTxMmSeedString();
std::optional<SharedPlacement> GetSharedPlacementByNativeCheck(const std::string& nativeCheck);
std::optional<std::string> GetSharedPlacementDisplayNameByNativeCheck(const std::string& nativeCheck);
std::optional<std::string> GetModelIdByNativeCheck(const std::string& nativeCheck);
std::optional<std::string> GetModelIdByCheckId(RandoCheckId randoCheckId);
std::optional<std::string> GetOoTxMmGossipHintMessage(const std::string& stoneKey);
bool IsForeignOotExclusiveModelId(const std::string& modelId);
bool EnsureOotO2rArchiveLoaded();
std::shared_ptr<Ship::Archive> GetOotO2rArchive();
void ApplySharedPlacementsToNativeRando();
void ApplyPendingMmGrants();
bool HandleQueuedSharedCheck(RandoCheckId randoCheckId);
bool RequestResetToOotTitle();

}
