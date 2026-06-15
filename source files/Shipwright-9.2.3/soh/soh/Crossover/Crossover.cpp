#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <libultraship/libultraship.h>
#include <nlohmann/json.hpp>
#include <ship/window/Window.h>
#include <spdlog/spdlog.h>

#include "soh/OTRGlobals.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/randomizer_entrance.h"
#include "soh/Notification/Notification.h"
#include "soh/Crossover/Crossover.h"
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "global.h"
#include "functions.h"
#include "message_data_static.h"
#include "z64.h"
#include "macros.h"
void Save_LoadFile(void);
extern PlayState* gPlayState;
}

namespace SohCrossover {

constexpr const char* kStateFileName = "crossover_state.json";
constexpr const char* kHandoffFileName = "crossover_handoff.json";
constexpr const char* kSeedFileName = "crossover_seed.json";
constexpr const char* kInventoryFileName = "crossover_inventory.json";
constexpr const char* kPlacementFileName = "crossover_placements.json";
constexpr const char* kPlacementPreviewFileName = "crossover_placements_preview.json";
constexpr const char* kDungeonRouteFileName = "crossover_dungeon_entrances.json";
constexpr const char* kLastDungeonTransitionFileName = "crossover_last_dungeon_transition.json";
constexpr const char* kTargetGame = "Oot";
constexpr const char* kTargetEntrance = "oot:happy_mask_shop_return";
constexpr const char* kTitleScreenEntrance = "oot:title_screen";
constexpr const char* kPendingFileSelectReturnEntrance = "oot:happy_mask_shop_return_after_file_select";
constexpr const char* kMmTargetEntrance = "mm:clock_tower_interior_exit";

bool sBootHandoffConsumed = false;
bool sSwitchRequested = false;
bool sPendingFileSelectReturnApplied = false;

std::unordered_map<std::string, SharedPlacement> sSharedPlacementsByNativeCheck;
bool sSharedPlacementsLoaded = false;
bool sSharedPlacementsAppliedToNativeRando = false;
bool sPendingOotGrantsApplied = false;
std::optional<std::string> sPendingPickupTextboxMessage;
bool sSuppressNextNativeRewardForCrossGameMmItem = false;
int sForcePlacementApplyFrames = 0;
int sPlacementHeartbeatFrames = 0;
bool sMmO2rArchiveLoadAttempted = false;
bool sMmO2rArchiveLoaded = false;
std::shared_ptr<Ship::Archive> sMmO2rArchive = nullptr;
std::string sDungeonReturnEntrance;
bool sComboDungeonNativeEntrancesReset = false;
bool sComboDungeonTransitionHandled = false;

struct GossipHintPool {
    std::vector<std::string> direct;
    std::vector<std::string> wayOfTheHero;
    std::vector<std::string> foolish;
    std::unordered_map<std::string, std::string> assignedByStone;
    std::unordered_set<std::string> usedHints;
};

GossipHintPool sGossipHints;
bool sGossipHintsLoaded = false;

void SaveCurrentGameBeforeSwitch();
int GetCurrentSaveSlot();
std::optional<std::filesystem::path> FindCrossoverRoot();
std::string StripItemPrefix(const std::string& item);
std::string SourceGameLabel(const std::string& sourceGame);
std::string HumanizeItemName(const SharedPlacement& placement);
std::string HumanizeItemNameWithSource(const SharedPlacement& placement);
bool ShouldGiveNativeItemForMmItem(const std::string& rawItem);
void MarkOotMaskOwnedForCrossoverGrant(u8 itemId);
uint32_t StableHashString(const std::string& value);
void WriteJsonAtomic(const std::filesystem::path& path, const nlohmann::json& json);
std::string TaggedEntrance(const char* game, int32_t entrance);

struct ComboDungeonEndpoint {
    const char* game;
    const char* key;
    int32_t entrance;
    int32_t exitEntrance;
    int32_t returnEntrance;
    bool finalBoss;
};

struct ComboDungeonRoute {
    ComboDungeonEndpoint origin;
    ComboDungeonEndpoint target;
};

std::vector<ComboDungeonRoute> sComboDungeonRoutes;
std::string sComboDungeonRouteSeed;
int32_t sComboDungeonRouteSetting = -1;

nlohmann::json ComboDungeonEndpointToJson(const ComboDungeonEndpoint& endpoint) {
    return {
        { "game", endpoint.game },
        { "key", endpoint.key },
        { "entrance", endpoint.entrance },
        { "exitEntrance", endpoint.exitEntrance },
        { "returnEntrance", endpoint.returnEntrance },
        { "finalBoss", endpoint.finalBoss },
    };
}

void InvalidateComboDungeonRoutes() {
    sComboDungeonRoutes.clear();
    sComboDungeonRouteSeed.clear();
    sComboDungeonRouteSetting = -1;
}

void WriteComboDungeonRoutesDebug() {
    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return;
    }

    nlohmann::json routes = nlohmann::json::array();
    for (const auto& route : sComboDungeonRoutes) {
        routes.push_back({
            { "origin", ComboDungeonEndpointToJson(route.origin) },
            { "target", ComboDungeonEndpointToJson(route.target) },
        });
    }

    nlohmann::json payload = {
        { "seed", sComboDungeonRouteSeed },
        { "setting", sComboDungeonRouteSetting },
        { "builtBy", "oot" },
        { "routes", routes },
    };

    try {
        WriteJsonAtomic(*root / kDungeonRouteFileName, payload);
    } catch (...) {
    }
}

void WriteComboDungeonTransitionDebug(const ComboDungeonEndpoint& origin, const ComboDungeonEndpoint& target,
                                      int32_t observedEntrance) {
    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return;
    }

    nlohmann::json payload = {
        { "game", "oot" },
        { "observedEntrance", observedEntrance },
        { "currentEntrance", gSaveContext.entranceIndex },
        { "sceneNum", gPlayState != nullptr ? gPlayState->sceneNum : -1 },
        { "origin", ComboDungeonEndpointToJson(origin) },
        { "target", ComboDungeonEndpointToJson(target) },
        { "returnEntrance", TaggedEntrance("oot", origin.returnEntrance) },
    };
    if (gPlayState != nullptr) {
        Player* player = GET_PLAYER(gPlayState);
        if (player != nullptr) {
            payload["playerPos"] = {
                { "x", player->actor.world.pos.x },
                { "y", player->actor.world.pos.y },
                { "z", player->actor.world.pos.z },
            };
        }
    }

    try {
        WriteJsonAtomic(*root / kLastDungeonTransitionFileName, payload);
    } catch (...) {
    }
}

std::vector<std::filesystem::path> GetMmO2rCandidatePaths() {
    std::vector<std::filesystem::path> candidates;
    const auto cwd = std::filesystem::current_path();

    candidates.push_back(cwd / "mm.o2r");
    candidates.push_back(cwd / ".." / "mm.o2r");
    candidates.push_back(cwd / "2ship" / "mm.o2r");
    candidates.push_back(cwd / ".." / "2ship" / "mm.o2r");

    if (const auto root = FindCrossoverRoot(); root.has_value()) {
        candidates.push_back(*root / "2ship" / "mm.o2r");
        candidates.push_back(*root / "extracted_win" / "2ship" / "mm.o2r");
    }

    candidates.push_back(cwd / "x64" / "Release" / "mm.o2r");
    candidates.push_back(cwd / "x64" / "Debug" / "mm.o2r");
    candidates.push_back(cwd / "build" / "x64" / "mm.o2r");

    return candidates;
}

bool IsSamePath(const std::filesystem::path& left, const std::filesystem::path& right) {
    std::error_code ec;
    const auto leftCanonical = std::filesystem::weakly_canonical(left, ec);
    if (ec) {
        return left.lexically_normal() == right.lexically_normal();
    }

    const auto rightCanonical = std::filesystem::weakly_canonical(right, ec);
    if (ec) {
        return left.lexically_normal() == right.lexically_normal();
    }

    return leftCanonical == rightCanonical;
}

void KeepMmO2rAtLowestResourcePriority(std::shared_ptr<Ship::Archive> mmArchive) {
    if (mmArchive == nullptr) {
        return;
    }

    auto archiveManager = Ship::Context::GetInstance()->GetResourceManager()->GetArchiveManager();
    if (archiveManager == nullptr) {
        return;
    }

    const auto archives = archiveManager->GetArchives();
    if (archives == nullptr) {
        return;
    }

    auto reordered = std::make_shared<std::vector<std::shared_ptr<Ship::Archive>>>();
    reordered->push_back(mmArchive);
    for (const auto& archive : *archives) {
        if (archive == nullptr || IsSamePath(archive->GetPath(), mmArchive->GetPath())) {
            continue;
        }

        reordered->push_back(archive);
    }

    archiveManager->SetArchivesPreserveLoaded(reordered);
}

bool EnsureMmO2rArchiveLoaded() {
    if (sMmO2rArchiveLoaded) {
        return true;
    }
    if (sMmO2rArchiveLoadAttempted) {
        return false;
    }

    sMmO2rArchiveLoadAttempted = true;
    auto archiveManager = Ship::Context::GetInstance()->GetResourceManager()->GetArchiveManager();
    if (archiveManager == nullptr) {
        SPDLOG_WARN("[OoTxMM] Unable to load mm.o2r: archive manager is unavailable");
        return false;
    }

    std::optional<std::filesystem::path> mmO2rPath;
    for (const auto& candidate : GetMmO2rCandidatePaths()) {
        std::error_code ec;
        const auto normalized = std::filesystem::weakly_canonical(candidate, ec);
        const auto path = ec ? candidate.lexically_normal() : normalized;
        if (std::filesystem::exists(path)) {
            mmO2rPath = path;
            break;
        }
    }

    if (!mmO2rPath.has_value()) {
        SPDLOG_WARN("[OoTxMM] mm.o2r was not found; foreign MM asset models will use fallbacks");
        return false;
    }

    const auto archives = archiveManager->GetArchives();
    if (archives != nullptr) {
        for (const auto& archive : *archives) {
            if (archive != nullptr && IsSamePath(archive->GetPath(), *mmO2rPath)) {
                KeepMmO2rAtLowestResourcePriority(archive);
                sMmO2rArchive = archive;
                sMmO2rArchiveLoaded = true;
                SPDLOG_INFO("[OoTxMM] mm.o2r already mounted: {}", mmO2rPath->string());
                return true;
            }
        }
    }

    const auto mmArchive = archiveManager->AddArchive(mmO2rPath->string());
    if (mmArchive == nullptr) {
        SPDLOG_WARN("[OoTxMM] Failed to mount mm.o2r at {}", mmO2rPath->string());
        return false;
    }

    sMmO2rArchiveLoaded = true;
    sMmO2rArchive = mmArchive;
    KeepMmO2rAtLowestResourcePriority(mmArchive);
    SPDLOG_INFO("[OoTxMM] Loaded mm.o2r for explicit foreign asset lookup: {}", mmO2rPath->string());
    return true;
}

std::shared_ptr<Ship::Archive> GetMmO2rArchive() {
    if (!EnsureMmO2rArchiveLoaded()) {
        return nullptr;
    }

    return sMmO2rArchive;
}

std::optional<std::filesystem::path> FindCrossoverRoot() {
    std::filesystem::path path = std::filesystem::current_path();

    while (!path.empty()) {
        if (std::filesystem::exists(path / "crossover_launcher.json") ||
            (std::filesystem::exists(path / "soh") && std::filesystem::exists(path / "2ship")) ||
            (std::filesystem::exists(path / "extracted_win") && std::filesystem::exists(path / "source files"))) {
            return path;
        }

        if (path == path.parent_path()) {
            break;
        }
        path = path.parent_path();
    }

    return std::nullopt;
}

nlohmann::json ReadJson(const std::filesystem::path& path) {
    std::ifstream stream(path);
    nlohmann::json json;
    stream >> json;
    return json;
}

nlohmann::json ReadJsonOrObject(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return nlohmann::json::object();
    }

    try {
        return ReadJson(path);
    } catch (...) {
        return nlohmann::json::object();
    }
}

void WriteJsonAtomic(const std::filesystem::path& path, const nlohmann::json& json) {
    const auto tempPath = path.string() + ".tmp";
    {
        std::ofstream stream(tempPath);
        stream << std::setw(4) << json << std::endl;
    }
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
    std::filesystem::rename(tempPath, path);
}

nlohmann::json ReadConfigBlock(const std::filesystem::path& configPath, const std::string& blockName) {
    const auto config = ReadJsonOrObject(configPath);
    if (!config.contains("CVars") || !config["CVars"].is_object()) {
        return nlohmann::json::object();
    }

    const auto& cvars = config["CVars"];
    if (!cvars.contains(blockName) || !cvars[blockName].is_object()) {
        return nlohmann::json::object();
    }

    return cvars[blockName];
}

int JsonInt(const nlohmann::json& json, const std::string& key, int fallback = 0) {
    if (!json.contains(key)) {
        return fallback;
    }

    try {
        return json[key].get<int>();
    } catch (...) {
        return fallback;
    }
}

std::optional<nlohmann::json> ReadPlacementManifest(const std::filesystem::path& root) {
    const auto placementsPath = root / "crossover_placements.json";
    if (std::filesystem::exists(placementsPath)) {
        const auto manifest = ReadJsonOrObject(placementsPath);
        if (manifest.is_object() && manifest.value("format", "") == "ootxmm-shared-placement") {
            return std::optional<nlohmann::json>(manifest);
        }
    }

    const auto seedManifest = ReadJsonOrObject(root / kSeedFileName);
    if (seedManifest.contains("placementManifest") && seedManifest["placementManifest"].is_object()) {
        return std::optional<nlohmann::json>(seedManifest["placementManifest"]);
    }

    return std::nullopt;
}

uint32_t StableHashString(const std::string& value) {
    uint32_t hash = 2166136261u;
    for (const unsigned char character : value) {
        hash ^= character;
        hash *= 16777619u;
    }
    return hash;
}

int32_t ComboDungeonSetting() {
    return CVarGetInteger("gRandomizer.Combo.Logic.DungeonEntrances", 0);
}

std::string ComboDungeonSeed() {
    if (const auto root = FindCrossoverRoot(); root.has_value()) {
        const auto seedManifest = ReadJsonOrObject(*root / kSeedFileName);
        if (seedManifest.contains("seedString") && seedManifest["seedString"].is_string()) {
            return seedManifest["seedString"].get<std::string>();
        }
    }
    return std::to_string(CVarGetInteger("gRandomizer.Seed", 0));
}

std::vector<ComboDungeonEndpoint> ComboDungeonEndpoints() {
    std::vector<ComboDungeonEndpoint> endpoints = {
        { "oot", "oot_deku", ENTR_DEKU_TREE_ENTRANCE, ENTR_KOKIRI_FOREST_OUTSIDE_DEKU_TREE,
          ENTR_KOKIRI_FOREST_OUTSIDE_DEKU_TREE, false },
        { "oot", "oot_dodongo", ENTR_DODONGOS_CAVERN_ENTRANCE, ENTR_DEATH_MOUNTAIN_TRAIL_OUTSIDE_DODONGOS_CAVERN,
          ENTR_DEATH_MOUNTAIN_TRAIL_OUTSIDE_DODONGOS_CAVERN, false },
        { "oot", "oot_jabu", ENTR_JABU_JABU_ENTRANCE, ENTR_ZORAS_FOUNTAIN_OUTSIDE_JABU_JABU,
          ENTR_ZORAS_FOUNTAIN_OUTSIDE_JABU_JABU, false },
        { "oot", "oot_forest", ENTR_FOREST_TEMPLE_ENTRANCE, ENTR_SACRED_FOREST_MEADOW_OUTSIDE_TEMPLE,
          ENTR_SACRED_FOREST_MEADOW_OUTSIDE_TEMPLE, false },
        { "oot", "oot_fire", ENTR_FIRE_TEMPLE_ENTRANCE, ENTR_DEATH_MOUNTAIN_CRATER_OUTSIDE_TEMPLE,
          ENTR_DEATH_MOUNTAIN_CRATER_OUTSIDE_TEMPLE, false },
        { "oot", "oot_water", ENTR_WATER_TEMPLE_ENTRANCE, ENTR_LAKE_HYLIA_OUTSIDE_TEMPLE,
          ENTR_LAKE_HYLIA_OUTSIDE_TEMPLE, false },
        { "oot", "oot_shadow", ENTR_SHADOW_TEMPLE_ENTRANCE, ENTR_GRAVEYARD_OUTSIDE_TEMPLE,
          ENTR_GRAVEYARD_OUTSIDE_TEMPLE, false },
        { "oot", "oot_spirit", ENTR_SPIRIT_TEMPLE_ENTRANCE, ENTR_DESERT_COLOSSUS_OUTSIDE_TEMPLE,
          ENTR_DESERT_COLOSSUS_OUTSIDE_TEMPLE, false },
        { "oot", "oot_well", ENTR_BOTTOM_OF_THE_WELL_ENTRANCE, ENTR_KAKARIKO_VILLAGE_OUTSIDE_BOTTOM_OF_THE_WELL,
          ENTR_KAKARIKO_VILLAGE_OUTSIDE_BOTTOM_OF_THE_WELL, false },
        { "oot", "oot_ice_cavern", ENTR_ICE_CAVERN_ENTRANCE, ENTR_ZORAS_FOUNTAIN_OUTSIDE_ICE_CAVERN,
          ENTR_ZORAS_FOUNTAIN_OUTSIDE_ICE_CAVERN, false },
        { "oot", "oot_gtg", ENTR_GERUDO_TRAINING_GROUND_ENTRANCE,
          ENTR_GERUDOS_FORTRESS_OUTSIDE_GERUDO_TRAINING_GROUND,
          ENTR_GERUDOS_FORTRESS_OUTSIDE_GERUDO_TRAINING_GROUND, false },
        { "mm", "mm_woodfall", 0x3000, 0x2E01, 0x2E01, false },
        { "mm", "mm_snowhead", 0x3C00, 0x1801, 0x1801, false },
        { "mm", "mm_great_bay", 0x8C00, 0x7407, 0x7407, false },
        { "mm", "mm_stone_tower", 0x2600, 0xAA20, 0xAA20, false },
        { "mm", "mm_stone_tower_inverted", 0x2A00, 0xAC00, 0xAC00, false },
    };

    if (ComboDungeonSetting() >= 2) {
        endpoints.push_back({ "oot", "oot_ganon", ENTR_INSIDE_GANONS_CASTLE_ENTRANCE, ENTR_OUTSIDE_GANONS_CASTLE_0_2,
                              ENTR_OUTSIDE_GANONS_CASTLE_0_2, true });
        endpoints.push_back({ "mm", "mm_majora", 0x0200, 0xC800, 0xC800, true });
    }

    return endpoints;
}

std::optional<ComboDungeonEndpoint> FindComboDungeonEndpointByEntrance(const char* game, int32_t entrance) {
    if (ComboDungeonSetting() == 0) {
        return std::nullopt;
    }

    for (const auto& endpoint : ComboDungeonEndpoints()) {
        if (std::string(endpoint.game) == game && endpoint.entrance == entrance) {
            return endpoint;
        }
    }

    return std::nullopt;
}

std::optional<ComboDungeonEndpoint> FindOotComboDungeonEndpointByExteriorScene(int16_t sceneNum, int32_t observedEntrance) {
    const char* key = nullptr;
    switch (sceneNum) {
        case SCENE_KOKIRI_FOREST:
            key = "oot_deku";
            break;
        case SCENE_DEATH_MOUNTAIN_TRAIL:
            key = "oot_dodongo";
            break;
        case SCENE_ZORAS_FOUNTAIN: {
            if (observedEntrance == ENTR_JABU_JABU_ENTRANCE) {
                key = "oot_jabu";
            } else if (observedEntrance == ENTR_ICE_CAVERN_ENTRANCE) {
                key = "oot_ice_cavern";
            } else if (gSaveContext.entranceIndex == ENTR_ZORAS_FOUNTAIN_OUTSIDE_JABU_JABU) {
                key = "oot_jabu";
            } else if (gSaveContext.entranceIndex == ENTR_ZORAS_FOUNTAIN_OUTSIDE_ICE_CAVERN) {
                key = "oot_ice_cavern";
            } else {
                Player* player = gPlayState != nullptr ? GET_PLAYER(gPlayState) : nullptr;
                key = (player != nullptr && player->actor.world.pos.z < -900.0f) ? "oot_ice_cavern" : "oot_jabu";
            }
            break;
        }
        case SCENE_SACRED_FOREST_MEADOW:
            key = "oot_forest";
            break;
        case SCENE_DEATH_MOUNTAIN_CRATER:
            key = "oot_fire";
            break;
        case SCENE_LAKE_HYLIA:
            key = "oot_water";
            break;
        case SCENE_GRAVEYARD:
            key = "oot_shadow";
            break;
        case SCENE_DESERT_COLOSSUS:
            key = "oot_spirit";
            break;
        case SCENE_KAKARIKO_VILLAGE:
            key = "oot_well";
            break;
        case SCENE_GERUDOS_FORTRESS:
            key = "oot_gtg";
            break;
        default:
            break;
    }

    if (key == nullptr) {
        return std::nullopt;
    }

    for (const auto& endpoint : ComboDungeonEndpoints()) {
        if (std::string(endpoint.game) == "oot" && std::string(endpoint.key) == key) {
            return endpoint;
        }
    }

    return std::nullopt;
}

std::optional<ComboDungeonEndpoint> ResolveComboDungeonTarget(const ComboDungeonEndpoint& origin) {
    const int32_t setting = ComboDungeonSetting();
    if (setting == 0) {
        return std::nullopt;
    }

    const std::string seed = ComboDungeonSeed();
    if (sComboDungeonRouteSetting != setting || sComboDungeonRouteSeed != seed || sComboDungeonRoutes.empty()) {
        auto endpoints = ComboDungeonEndpoints();
        auto targets = endpoints;

        std::sort(targets.begin(), targets.end(),
                  [&seed](const ComboDungeonEndpoint& left, const ComboDungeonEndpoint& right) {
                      const std::string leftKey = std::string(left.game) + ":" + left.key;
                      const std::string rightKey = std::string(right.game) + ":" + right.key;
                      const uint32_t leftHash = StableHashString(seed + "|combo-dungeon-er-target|" + leftKey);
                      const uint32_t rightHash = StableHashString(seed + "|combo-dungeon-er-target|" + rightKey);
                      if (leftHash != rightHash) {
                          return leftHash < rightHash;
                      }
                      return leftKey < rightKey;
                  });

        sComboDungeonRoutes.clear();
        for (size_t i = 0; i < endpoints.size() && i < targets.size(); i++) {
            sComboDungeonRoutes.push_back({ endpoints[i], targets[i] });
        }
        sComboDungeonRouteSetting = setting;
        sComboDungeonRouteSeed = seed;
        WriteComboDungeonRoutesDebug();
    }

    for (const auto& route : sComboDungeonRoutes) {
        if (std::string(route.origin.game) == origin.game && std::string(route.origin.key) == origin.key) {
            return route.target;
        }
    }

    return std::nullopt;
}

std::optional<ComboDungeonEndpoint> ResolveComboDungeonOriginForTarget(const char* targetGame, int32_t targetEntrance) {
    for (const auto& origin : ComboDungeonEndpoints()) {
        if (const auto target = ResolveComboDungeonTarget(origin); target.has_value() &&
            std::string(target->game) == targetGame && target->entrance == targetEntrance) {
            return origin;
        }
    }
    return std::nullopt;
}

bool IsComboDungeonReturnExit(const char* game, int32_t entrance) {
    for (const auto& endpoint : ComboDungeonEndpoints()) {
        if (std::string(endpoint.game) == game &&
            (entrance == endpoint.exitEntrance || entrance == endpoint.returnEntrance)) {
            return true;
        }
    }

    if (std::string(game) != "oot") {
        return false;
    }

    switch (entrance) {
        case ENTR_KOKIRI_FOREST_DEKU_TREE_BLUE_WARP:
        case ENTR_DEATH_MOUNTAIN_TRAIL_DODONGO_BLUE_WARP:
        case ENTR_ZORAS_FOUNTAIN_JABU_JABU_BLUE_WARP:
        case ENTR_SACRED_FOREST_MEADOW_FOREST_TEMPLE_BLUE_WARP:
        case ENTR_DEATH_MOUNTAIN_CRATER_FIRE_TEMPLE_BLUE_WARP:
        case ENTR_LAKE_HYLIA_WATER_TEMPLE_BLUE_WARP:
        case ENTR_DESERT_COLOSSUS_SPIRIT_TEMPLE_BLUE_WARP:
        case ENTR_GRAVEYARD_SHADOW_TEMPLE_BLUE_WARP:
            return true;
        default:
            return false;
    }
}

std::optional<int32_t> ParseTaggedEntrance(const std::string& value, const std::string& prefix) {
    if (value.rfind(prefix, 0) != 0) {
        return std::nullopt;
    }

    try {
        return std::stoi(value.substr(prefix.size()), nullptr, 0);
    } catch (...) {
        return std::nullopt;
    }
}

std::string TaggedEntrance(const char* game, int32_t entrance) {
    return std::string(game) + ":entrance:" + std::to_string(entrance);
}

std::string HumanizeHintValue(std::string value, const std::string& sourceGame = "") {
    const auto separator = value.find(':');
    if (separator != std::string::npos) {
        value = value.substr(separator + 1);
    }
    if (value.rfind("RI_", 0) == 0 || value.rfind("RG_", 0) == 0 || value.rfind("RC_", 0) == 0) {
        value = value.substr(3);
    }

    std::string label;
    bool newWord = true;
    for (const char character : value) {
        if (character == '_' || character == '-') {
            if (!label.empty() && label.back() != ' ') {
                label.push_back(' ');
            }
            newWord = true;
            continue;
        }

        if (newWord) {
            label.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
            newWord = false;
        } else {
            label.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
        }
    }

    if (label.empty()) {
        label = "Unknown";
    }
    if (label == "Song" || label == "Songs") {
        label = "Ocarina Songs";
    } else if (label == "Lh") {
        label = "Lake Hylia";
    } else if (label == "Kf") {
        label = "Kokiri Forest";
    } else if (label == "Lw") {
        label = "Lost Woods";
    } else if (label == "Sfm") {
        label = "Sacred Forest Meadow";
    } else if (label == "Hf") {
        label = "Hyrule Field";
    } else if (label == "Dmt") {
        label = "Death Mountain Trail";
    } else if (label == "Dmc") {
        label = "Death Mountain Crater";
    } else if (label == "Kak") {
        label = "Kakariko";
    } else if (label == "Llr") {
        label = "Lon Lon Ranch";
    } else if (label == "Gv") {
        label = "Gerudo Valley";
    } else if (label == "Gf") {
        label = "Gerudo Fortress";
    } else if (label == "Zf") {
        label = "Zora's Fountain";
    } else if (label == "Zd") {
        label = "Zora's Domain";
    } else if (label == "Zr") {
        label = "Zora's River";
    } else if (label.rfind("Zf ", 0) == 0) {
        label = "Zora's Fountain " + label.substr(3);
    } else if (label.rfind("Zd ", 0) == 0) {
        label = "Zora's Domain " + label.substr(3);
    } else if (label.rfind("Zr ", 0) == 0) {
        label = "Zora's River " + label.substr(3);
    }
    if (!sourceGame.empty()) {
        label += " (" + SourceGameLabel(sourceGame) + ")";
    }
    return label;
}

void LoadCrossoverGossipHints() {
    if (sGossipHintsLoaded) {
        return;
    }

    sGossipHintsLoaded = true;
    sGossipHints = {};

    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return;
    }

    const auto manifest = ReadPlacementManifest(*root);
    if (!manifest.has_value() || !manifest->contains("placements") || !(*manifest)["placements"].is_array()) {
        return;
    }

    std::unordered_map<std::string, bool> areasHaveMajor;
    for (const auto& placement : (*manifest)["placements"]) {
        if (!placement.is_object()) {
            continue;
        }

        const std::string itemName = placement.value(
            "hintItemName", HumanizeHintValue(placement.value("item", std::string{}),
                                              placement.value("itemSourceGame", std::string{})));
        const std::string placeName = placement.value(
            "hintPlaceName", HumanizeHintValue(placement.value("nativeCheck", std::string{}),
                                               placement.value("checkGame", std::string{})));
        const std::string areaName = placement.value("hintAreaName", placeName);
        const bool isMajor = placement.value("hintIsMajor", false);

        if (isMajor && !itemName.empty() && !placeName.empty()) {
            sGossipHints.direct.push_back("They say %g" + itemName + "%w is found in %y" + placeName + "%w.");
        }
        if (!areaName.empty()) {
            const auto existing = areasHaveMajor.find(areaName);
            areasHaveMajor[areaName] = isMajor || (existing != areasHaveMajor.end() && existing->second);
        }
    }

    for (const auto& [areaName, hasMajor] : areasHaveMajor) {
        if (hasMajor) {
            sGossipHints.wayOfTheHero.push_back("They say %y" + areaName + "%w is %gthe way of the hero%w.");
        } else {
            sGossipHints.foolish.push_back("They say %y" + areaName + "%w is %rfoolish%w.");
        }
    }
}

std::optional<std::string> SelectGossipHintFromBucket(const std::vector<std::string>& bucket,
                                                      const std::string& stoneKey,
                                                      const std::string& bucketName) {
    if (bucket.empty()) {
        return std::nullopt;
    }

    const std::string assignmentKey = bucketName + "|" + stoneKey;
    if (const auto existing = sGossipHints.assignedByStone.find(assignmentKey);
        existing != sGossipHints.assignedByStone.end()) {
        return existing->second;
    }

    const size_t index = StableHashString(stoneKey + "|" + bucketName) % bucket.size();
    for (size_t offset = 0; offset < bucket.size(); offset++) {
        const auto& hint = bucket[(index + offset) % bucket.size()];
        if (sGossipHints.usedHints.find(hint) == sGossipHints.usedHints.end()) {
            sGossipHints.assignedByStone[assignmentKey] = hint;
            sGossipHints.usedHints.insert(hint);
            return hint;
        }
    }

    return bucket[index];
}

std::optional<std::string> GetOoTxMmGossipHintMessage(const std::string& stoneKey) {
    LoadCrossoverGossipHints();

    if (sGossipHints.direct.empty() && sGossipHints.wayOfTheHero.empty() && sGossipHints.foolish.empty()) {
        return std::nullopt;
    }

    const uint32_t bucket = StableHashString(stoneKey + "|type") % 4;
    if (bucket < 2) {
        if (const auto hint = SelectGossipHintFromBucket(sGossipHints.direct, stoneKey, "direct")) {
            return hint;
        }
    } else if (bucket == 2) {
        if (const auto hint = SelectGossipHintFromBucket(sGossipHints.wayOfTheHero, stoneKey, "woth")) {
            return hint;
        }
    } else if (const auto hint = SelectGossipHintFromBucket(sGossipHints.foolish, stoneKey, "foolish")) {
        return hint;
    }

    if (const auto hint = SelectGossipHintFromBucket(sGossipHints.direct, stoneKey, "direct")) {
        return hint;
    }
    if (const auto hint = SelectGossipHintFromBucket(sGossipHints.wayOfTheHero, stoneKey, "woth")) {
        return hint;
    }
    return SelectGossipHintFromBucket(sGossipHints.foolish, stoneKey, "foolish");
}

void LoadSharedPlacements() {
    if (sSharedPlacementsLoaded) {
        return;
    }

    sSharedPlacementsLoaded = true;
    sSharedPlacementsByNativeCheck.clear();

    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        SPDLOG_INFO("[OoTxMM] SoH placement manifest skipped: crossover root not found");
        return;
    }

    const auto manifest = ReadPlacementManifest(*root);
    if (!manifest.has_value() || !manifest->contains("placements") || !(*manifest)["placements"].is_array()) {
        SPDLOG_INFO("[OoTxMM] SoH placement manifest skipped: no placements found");
        return;
    }

    size_t total = 0;
    size_t local = 0;
    size_t crossGame = 0;
    for (const auto& placement : (*manifest)["placements"]) {
        if (!placement.is_object() || placement.value("checkGame", "") != "oot") {
            continue;
        }

        SharedPlacement sharedPlacement = {
            placement.value("check", ""),
            placement.value("nativeCheck", ""),
            placement.value("item", ""),
            placement.value("itemSourceGame", ""),
            placement.value("itemSourceCheck", ""),
            placement.value("model", ""),
            placement.value("isCrossGame", false),
        };

        if (sharedPlacement.nativeCheck.empty()) {
            continue;
        }

        sSharedPlacementsByNativeCheck[sharedPlacement.nativeCheck] = sharedPlacement;
        total++;
        if (sharedPlacement.isCrossGame) {
            crossGame++;
        } else {
            local++;
        }
    }

    SPDLOG_INFO("[OoTxMM] SoH loaded {} shared placements ({} local, {} cross-game)", total, local, crossGame);
}

std::optional<SharedPlacement> GetSharedPlacementByNativeCheck(const std::string& nativeCheck) {
    LoadSharedPlacements();

    const auto placement = sSharedPlacementsByNativeCheck.find(nativeCheck);
    if (placement == sSharedPlacementsByNativeCheck.end()) {
        return std::nullopt;
    }

    return placement->second;
}

std::optional<std::string> GetModelIdByNativeCheck(const std::string& nativeCheck) {
    const auto placement = GetSharedPlacementByNativeCheck(nativeCheck);
    if (!placement.has_value() || placement->model.empty()) {
        return std::nullopt;
    }

    return placement->model;
}

std::optional<std::string> GetSharedPlacementDisplayNameByNativeCheck(const std::string& nativeCheck) {
    const auto placement = GetSharedPlacementByNativeCheck(nativeCheck);
    if (!placement.has_value()) {
        return std::nullopt;
    }

    return HumanizeItemNameWithSource(*placement);
}

bool IsCrossGameMmCheck(RandomizerCheck rc) {
    const auto location = Rando::StaticData::GetLocation(rc);
    if (location == nullptr) {
        return false;
    }

    const auto placement = GetSharedPlacementByNativeCheck(location->GetName());
    return placement.has_value() && placement->itemSourceGame == "mm";
}

bool ShouldGiveNativeItemForCrossGameMmCheck(RandomizerCheck rc) {
    const auto location = Rando::StaticData::GetLocation(rc);
    if (location == nullptr) {
        return false;
    }

    const auto placement = GetSharedPlacementByNativeCheck(location->GetName());
    return placement.has_value() && placement->itemSourceGame == "mm" && ShouldGiveNativeItemForMmItem(placement->item);
}

bool ShouldUseCrossoverVisualForCrossGameMmCheck(RandomizerCheck rc) {
    const auto location = Rando::StaticData::GetLocation(rc);
    if (location == nullptr) {
        return false;
    }

    const auto placement = GetSharedPlacementByNativeCheck(location->GetName());
    if (!placement.has_value() || placement->itemSourceGame != "mm") {
        return false;
    }

    const std::string item = StripItemPrefix(placement->item);
    return item == "RI_MASK_DEKU" || item == "RI_MASK_FIERCE_DEITY";
}

bool TryGiveNativeItemForCrossGameMmCheck(RandomizerCheck rc, PlayState* play) {
    const auto location = Rando::StaticData::GetLocation(rc);
    if (location == nullptr) {
        return false;
    }

    const auto placement = GetSharedPlacementByNativeCheck(location->GetName());
    if (!placement.has_value() || placement->itemSourceGame != "mm") {
        return false;
    }

    const std::string item = StripItemPrefix(placement->item);
    if (item == "RI_MASK_DEKU") {
        Item_Give(play, ITEM_MASK_DEKU);
        MarkOotMaskOwnedForCrossoverGrant(ITEM_MASK_DEKU);
        return true;
    }
    if (item == "RI_MASK_FIERCE_DEITY") {
        Item_Give(play, ITEM_MASK_FIERCE_DEITY);
        MarkOotMaskOwnedForCrossoverGrant(ITEM_MASK_FIERCE_DEITY);
        return true;
    }

    return false;
}

std::optional<GetItemCategory> GetCrossGameMmItemCategory(RandomizerCheck rc) {
    const auto location = Rando::StaticData::GetLocation(rc);
    if (location == nullptr) {
        return std::nullopt;
    }

    const auto placement = GetSharedPlacementByNativeCheck(location->GetName());
    if (!placement.has_value() || placement->itemSourceGame != "mm") {
        return std::nullopt;
    }

    const std::string item = StripItemPrefix(placement->item);

    if (item == "RI_HEART_PIECE" || item == "RI_HEART_CONTAINER") {
        return ITEM_CATEGORY_HEALTH;
    }

    if (item.find("BOSS_KEY") != std::string::npos) {
        return ITEM_CATEGORY_BOSS_KEY;
    }

    if (item.find("SMALL_KEY") != std::string::npos) {
        return ITEM_CATEGORY_SMALL_KEY;
    }

    if (item.find("GS_TOKEN") != std::string::npos) {
        return ITEM_CATEGORY_SKULLTULA_TOKEN;
    }

    if (item.find("RUPEE_SILVER") != std::string::npos) {
        return ITEM_CATEGORY_MAJOR;
    }

    if (item.find("HUGE_RUPEE") != std::string::npos || item == "RI_RUPEE_HUGE") {
        return ITEM_CATEGORY_LESSER;
    }

    if (item.find("RUPEE") != std::string::npos || item.find("ARROWS") != std::string::npos ||
        item.find("BOMBS") != std::string::npos || item.find("BOMBCHU") != std::string::npos ||
        item.find("REFILL") != std::string::npos || item.find("DEKU_NUT") != std::string::npos ||
        item.find("DEKU_STICK") != std::string::npos || item.find("MAGIC_JAR") != std::string::npos ||
        item == "RI_RECOVERY_HEART" || item == "RI_JUNK" || item == "RI_NONE") {
        return ITEM_CATEGORY_JUNK;
    }

    if (item.find("STRAY_FAIRY") != std::string::npos) {
        return ITEM_CATEGORY_LESSER;
    }

    if (item.find("_MAP") != std::string::npos || item.find("_COMPASS") != std::string::npos ||
        item.find("DEED") != std::string::npos ||
        item.find("LETTER") != std::string::npos || item.find("MOONS_TEAR") != std::string::npos ||
        item.find("ROOM_KEY") != std::string::npos || item.find("NOTEBOOK") != std::string::npos ||
        item.find("PICTOGRAPH") != std::string::npos || item.find("GOLD_DUST") != std::string::npos) {
        return ITEM_CATEGORY_LESSER;
    }

    return ITEM_CATEGORY_MAJOR;
}

std::optional<std::string> GetCrossGameMmPickupMessage(RandomizerCheck rc) {
    const auto location = Rando::StaticData::GetLocation(rc);
    if (location == nullptr) {
        return std::nullopt;
    }

    const auto placement = GetSharedPlacementByNativeCheck(location->GetName());
    if (!placement.has_value() || placement->itemSourceGame != "mm") {
        return std::nullopt;
    }

    return "You found " + HumanizeItemNameWithSource(*placement);
}

void QueueCrossGameMmPickupTextbox(RandomizerCheck rc) {
    sPendingPickupTextboxMessage = GetCrossGameMmPickupMessage(rc);
    sSuppressNextNativeRewardForCrossGameMmItem = sPendingPickupTextboxMessage.has_value();
}

std::optional<std::string> ConsumePendingCrossGameMmPickupTextbox() {
    if (!sPendingPickupTextboxMessage.has_value()) {
        return std::nullopt;
    }

    auto message = sPendingPickupTextboxMessage;
    sPendingPickupTextboxMessage = std::nullopt;
    return message;
}

bool ConsumePendingCrossGameMmNativeRewardSuppression(GetItemEntry getItemEntry) {
    if (!sSuppressNextNativeRewardForCrossGameMmItem || getItemEntry.modIndex != MOD_RANDOMIZER) {
        return false;
    }

    sSuppressNextNativeRewardForCrossGameMmItem = false;
    return true;
}

nlohmann::json BuildEnabledCheckGroups(const nlohmann::json& ootSettings, const nlohmann::json& mmRando) {
    nlohmann::json groups = nlohmann::json::array();
    const auto mmOptions =
        (mmRando.contains("Options") && mmRando["Options"].is_object()) ? mmRando["Options"] : nlohmann::json::object();

    auto addGroup = [&groups](const char* game, const char* group, const char* setting, int value) {
        if (value == 0) {
            return;
        }

        groups.push_back({
            { "game", game },
            { "group", group },
            { "setting", setting },
            { "value", value },
        });
    };

    addGroup("oot", "grass", "ShuffleGrass", JsonInt(ootSettings, "ShuffleGrass"));
    addGroup("oot", "pots", "ShufflePots", JsonInt(ootSettings, "ShufflePots"));
    addGroup("oot", "crates", "ShuffleCrates", JsonInt(ootSettings, "ShuffleCrates"));
    addGroup("oot", "beehives", "ShuffleBeehives", JsonInt(ootSettings, "ShuffleBeehives"));
    addGroup("oot", "cows", "ShuffleCows", JsonInt(ootSettings, "ShuffleCows"));
    addGroup("oot", "freestanding", "ShuffleFreestanding", JsonInt(ootSettings, "ShuffleFreestanding"));
    addGroup("oot", "roc_feather", "RocsFeather", JsonInt(ootSettings, "RocsFeather"));
    addGroup("oot", "kokiri_sword", "ShuffleKokiriSword", JsonInt(ootSettings, "ShuffleKokiriSword"));
    addGroup("oot", "master_sword", "ShuffleMasterSword", JsonInt(ootSettings, "ShuffleMasterSword"));
    addGroup("oot", "ocarinas", "ShuffleOcarinas", JsonInt(ootSettings, "ShuffleOcarinas"));
    addGroup("oot", "weird_egg", "ShuffleWeirdEgg", JsonInt(ootSettings, "ShuffleWeirdEgg"));
    addGroup("oot", "gerudo_card", "ShuffleGerudoToken", JsonInt(ootSettings, "ShuffleGerudoToken"));
    addGroup("oot", "frog_song_rupees", "ShuffleFrogSongRupees", JsonInt(ootSettings, "ShuffleFrogSongRupees"));
    addGroup("oot", "adult_trade", "ShuffleAdultTrade", JsonInt(ootSettings, "ShuffleAdultTrade"));
    addGroup("oot", "hundred_gs_reward", "Shuffle100GSReward", JsonInt(ootSettings, "Shuffle100GSReward"));
    addGroup("oot", "fountain_fairies", "ShuffleFountainFairies", JsonInt(ootSettings, "ShuffleFountainFairies"));
    addGroup("oot", "gossip_stone_fairies", "ShuffleStoneFairies", JsonInt(ootSettings, "ShuffleStoneFairies"));
    addGroup("oot", "fairy_spots", "ShuffleFairySpots", JsonInt(ootSettings, "ShuffleFairySpots"));
    addGroup("oot", "songs", "ShuffleSongs", JsonInt(ootSettings, "ShuffleSongs"));
    addGroup("oot", "tokens", "ShuffleTokens", JsonInt(ootSettings, "ShuffleTokens"));
    addGroup("oot", "fishsanity", "Fishsanity", JsonInt(ootSettings, "Fishsanity"));
    addGroup("oot", "shopsanity", "Shopsanity", JsonInt(ootSettings, "Shopsanity"));
    addGroup("oot", "scrubs", "ShuffleScrubs", JsonInt(ootSettings, "ShuffleScrubs"));
    addGroup("oot", "merchants", "ShuffleMerchants", JsonInt(ootSettings, "ShuffleMerchants"));
    addGroup("oot", "bombchu_bag", "BombchuBag", JsonInt(ootSettings, "BombchuBag"));
    addGroup("oot", "infinite_upgrades", "InfiniteUpgrades", JsonInt(ootSettings, "InfiniteUpgrades"));
    addGroup("oot", "skeleton_key", "SkeletonKey", JsonInt(ootSettings, "SkeletonKey"));
    addGroup("oot", "maps_compasses", "StartingMapsCompasses",
             JsonInt(ootSettings, "StartingMapsCompasses") >= 2 ? JsonInt(ootSettings, "StartingMapsCompasses") : 0);
    addGroup("oot", "small_keys", "Keysanity", JsonInt(ootSettings, "Keysanity") >= 2 ? JsonInt(ootSettings, "Keysanity") : 0);
    addGroup("oot", "boss_keys", "BossKeysanity",
             JsonInt(ootSettings, "BossKeysanity") >= 2 ? JsonInt(ootSettings, "BossKeysanity") : 0);
    addGroup("oot", "dungeon_rewards", "ShuffleDungeonRewards", JsonInt(ootSettings, "ShuffleDungeonRewards"));
    addGroup("oot", "gerudo_fortress_keys", "GerudoKeys", JsonInt(ootSettings, "GerudoKeys"));
    addGroup("oot", "boss_souls", "ShuffleBossSouls", JsonInt(ootSettings, "ShuffleBossSouls"));
    addGroup("oot", "keyrings", "ShuffleKeyRings", JsonInt(ootSettings, "ShuffleKeyRings"));

    addGroup("mm", "grass", "RO_SHUFFLE_GRASS_DROPS", JsonInt(mmOptions, "RO_SHUFFLE_GRASS_DROPS"));
    addGroup("mm", "pots", "RO_SHUFFLE_POT_DROPS", JsonInt(mmOptions, "RO_SHUFFLE_POT_DROPS"));
    addGroup("mm", "crates", "RO_SHUFFLE_CRATE_DROPS", JsonInt(mmOptions, "RO_SHUFFLE_CRATE_DROPS"));
    addGroup("mm", "barrels", "RO_SHUFFLE_BARREL_DROPS", JsonInt(mmOptions, "RO_SHUFFLE_BARREL_DROPS"));
    addGroup("mm", "owl_statues", "RO_SHUFFLE_OWL_STATUES", JsonInt(mmOptions, "RO_SHUFFLE_OWL_STATUES"));
    addGroup("mm", "cows", "RO_SHUFFLE_COWS", JsonInt(mmOptions, "RO_SHUFFLE_COWS"));
    addGroup("mm", "freestanding", "RO_SHUFFLE_FREESTANDING_ITEMS", JsonInt(mmOptions, "RO_SHUFFLE_FREESTANDING_ITEMS"));
    addGroup("mm", "shops", "RO_SHUFFLE_SHOPS", JsonInt(mmOptions, "RO_SHUFFLE_SHOPS"));
    addGroup("mm", "tingle_maps", "RO_SHUFFLE_TINGLE_SHOPS", JsonInt(mmOptions, "RO_SHUFFLE_TINGLE_SHOPS"));
    addGroup("mm", "tokens", "RO_SHUFFLE_GOLD_SKULLTULAS", JsonInt(mmOptions, "RO_SHUFFLE_GOLD_SKULLTULAS"));
    addGroup("mm", "snowballs", "RO_SHUFFLE_SNOWBALL_DROPS", JsonInt(mmOptions, "RO_SHUFFLE_SNOWBALL_DROPS"));
    addGroup("mm", "boss_remains", "RO_SHUFFLE_BOSS_REMAINS", JsonInt(mmOptions, "RO_SHUFFLE_BOSS_REMAINS"));
    addGroup("mm", "enemy_drops", "RO_SHUFFLE_ENEMY_DROPS", JsonInt(mmOptions, "RO_SHUFFLE_ENEMY_DROPS"));

    return groups;
}

nlohmann::json ReadModManifestsFrom(const std::filesystem::path& modsPath, const std::string& game) {
    nlohmann::json manifests = nlohmann::json::array();
    if (!std::filesystem::exists(modsPath)) {
        return manifests;
    }

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(modsPath)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") {
                continue;
            }

            const auto fileName = entry.path().filename().string();
            if (fileName.find("ootxmm") == std::string::npos) {
                continue;
            }

            auto manifest = ReadJsonOrObject(entry.path());
            if (!manifest.is_object() || manifest.value("format", "") != "ootxmm-mod") {
                continue;
            }

            manifest["sourceGame"] = game;
            manifest["sourcePath"] = entry.path().lexically_relative(modsPath).generic_string();
            manifests.push_back(manifest);
        }
    } catch (...) {
    }

    return manifests;
}

nlohmann::json BuildModCatalog(const std::filesystem::path& root) {
    nlohmann::json manifests = nlohmann::json::array();
    nlohmann::json items = nlohmann::json::array();
    nlohmann::json checks = nlohmann::json::array();

    auto appendManifests = [&manifests](const nlohmann::json& source) {
        if (!source.is_array()) {
            return;
        }

        for (const auto& manifest : source) {
            manifests.push_back(manifest);
        }
    };

    appendManifests(ReadModManifestsFrom(root / "soh" / "mods", "oot"));
    appendManifests(ReadModManifestsFrom(root / "2ship" / "mods", "mm"));
    appendManifests(ReadModManifestsFrom(root / "extracted_win" / "soh" / "mods", "oot"));
    appendManifests(ReadModManifestsFrom(root / "extracted_win" / "2ship" / "mods", "mm"));

    for (const auto& manifest : manifests) {
        const auto modId = manifest.value("modId", "unknown");
        const auto sourceGame = manifest.value("sourceGame", "");

        if (manifest.contains("items") && manifest["items"].is_array()) {
            for (auto item : manifest["items"]) {
                if (!item.is_object()) {
                    continue;
                }

                item["modId"] = modId;
                item["sourceGame"] = sourceGame;
                items.push_back(item);
            }
        }

        if (manifest.contains("checks") && manifest["checks"].is_array()) {
            for (auto check : manifest["checks"]) {
                if (!check.is_object()) {
                    continue;
                }

                check["modId"] = modId;
                check["sourceGame"] = sourceGame;
                checks.push_back(check);
            }
        }
    }

    return {
        { "manifests", manifests },
        { "items", items },
        { "checks", checks },
    };
}

nlohmann::json BuildCrossoverSettingsSnapshot(const std::filesystem::path& root) {
    auto ootSettings = ReadConfigBlock(root / "soh" / "shipofharkinian.json", "gRandoSettings");
    if (!ootSettings.is_object() || ootSettings.empty()) {
        ootSettings = ReadConfigBlock(root / "extracted_win" / "soh" / "shipofharkinian.json", "gRandoSettings");
    }

    auto mmRando = ReadConfigBlock(root / "2ship" / "2ship2harkinian.json", "gRando");
    if (!mmRando.is_object() || mmRando.empty()) {
        mmRando = ReadConfigBlock(root / "extracted_win" / "2ship" / "2ship2harkinian.json", "gRando");
    }

    return {
        { "oot", ootSettings },
        { "mm", mmRando },
        { "enabledCheckGroups", BuildEnabledCheckGroups(ootSettings, mmRando) },
    };
}

char DecodeFileNameChar(u8 value, s32 filenameLanguage) {
    if (filenameLanguage == NAME_LANGUAGE_PAL) {
        if (value >= 0x0A && value <= 0x23) {
            return static_cast<char>('A' + (value - 0x0A));
        }
        if (value >= 0x24 && value <= 0x3D) {
            return static_cast<char>('a' + (value - 0x24));
        }
        if (value == 0x3E) {
            return ' ';
        }
    } else {
        if (value >= 0xAB && value <= 0xC4) {
            return static_cast<char>('A' + (value - 0xAB));
        }
        if (value >= 0xC5 && value <= 0xDE) {
            return static_cast<char>('a' + (value - 0xC5));
        }
        if (value == 0xDF) {
            return ' ';
        }
    }

    return '\0';
}

bool SaveFileNameEqualsDebug(int fileNum) {
    if (fileNum < 0 || fileNum > 2) {
        return false;
    }

    const SaveFileMetaInfo* meta = &SaveManager::Instance->fileMetaInfo[fileNum];
    if (meta == nullptr || !meta->valid) {
        return false;
    }

    std::string name;
    for (u8 rawChar : meta->playerName) {
        const char decoded = DecodeFileNameChar(rawChar, meta->filenameLanguage);
        if (decoded == '\0' || decoded == ' ') {
            break;
        }
        name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(decoded))));
    }

    return name == "debug";
}

bool ShouldUseTestingModelSeedForCurrentGeneration(int saveSlot) {
    return saveSlot == 2 && SaveFileNameEqualsDebug(0);
}

void RebuildPlacementManifest(const std::filesystem::path& root, bool testingModelSeed) {
    const auto scriptPath = root / "tools" / "Build-CrossoverPlacementPreview.ps1";
    if (!std::filesystem::exists(scriptPath)) {
        SPDLOG_WARN("[OoTxMM] Placement builder script not found: {}", scriptPath.string());
        return;
    }

    const std::string command = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" + scriptPath.string() +
                                "\" -Root \"" + root.string() + "\"" +
                                (testingModelSeed ? " -TestingModelSeed" : "");
    const int result = std::system(command.c_str());
    if (result != 0) {
        SPDLOG_WARN("[OoTxMM] Placement builder exited with {}", result);
    }
}

void PublishOoTxMmSeed(const std::string& seedString, uint32_t sohFinalSeed) {
    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return;
    }

    InvalidateComboDungeonRoutes();

    const int saveSlot = GetCurrentSaveSlot();
    const bool testingModelSeed = ShouldUseTestingModelSeedForCurrentGeneration(saveSlot);
    const std::string twoShipSeedString = seedString + "2ship2harkinian";
    const auto existingSeedManifest = ReadJsonOrObject(*root / kSeedFileName);
    const auto modCatalog = BuildModCatalog(*root);
    nlohmann::json seedManifest = {
        { "mode", "OoTxMM" },
        { "saveSlot", saveSlot },
        { "seedString", seedString },
        { "sohSeedString", seedString },
        { "sohFinalSeed", sohFinalSeed },
        { "twoShipSeedString", twoShipSeedString },
        { "formula", "twoShipSeedString = seedString + '2ship2harkinian'; each port applies its native hash" },
        { "testingMode", testingModelSeed },
        { "settings", BuildCrossoverSettingsSnapshot(*root) },
        { "mods", modCatalog },
        { "placementManifest", {
            { "version", 1 },
            { "format", "ootxmm-shared-placement" },
            { "status", "settings-snapshot-only" },
            { "checks", modCatalog["checks"] },
            { "items", modCatalog["items"] },
            { "placements", nlohmann::json::array() },
        } },
    };

    try {
        std::filesystem::remove(*root / kInventoryFileName);
        WriteJsonAtomic(*root / kSeedFileName, seedManifest);
        RebuildPlacementManifest(*root, testingModelSeed);
        sSharedPlacementsLoaded = false;
        sSharedPlacementsAppliedToNativeRando = false;
        sPendingOotGrantsApplied = false;
    } catch (...) {
    }
}

int GetCurrentSaveSlot() {
    if (gSaveContext.fileNum >= 0 && gSaveContext.fileNum <= 2) {
        return gSaveContext.fileNum;
    }

    return 0;
}

std::string StripItemPrefix(const std::string& item) {
    const auto separator = item.find(':');
    if (separator == std::string::npos) {
        return item;
    }

    return item.substr(separator + 1);
}

std::string SourceGameLabel(const std::string& sourceGame) {
    if (sourceGame == "oot") {
        return "OoT";
    }
    if (sourceGame == "mm") {
        return "MM";
    }

    return sourceGame;
}

std::string TitleCaseToken(std::string token) {
    if (token.empty()) {
        return token;
    }

    for (char& character : token) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }

    token[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(token[0])));
    return token;
}

std::vector<std::string> SplitItemWords(const std::string& itemName) {
    std::vector<std::string> words;
    std::string word;

    for (const char character : itemName) {
        if (character == '_') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word.push_back(character);
        }
    }

    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

std::string NormalizeItemAliasKey(std::string itemName) {
    std::string key;
    for (const char character : itemName) {
        if (std::isalnum(static_cast<unsigned char>(character))) {
            key.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
        } else if (!key.empty() && key.back() != '_') {
            key.push_back('_');
        }
    }

    while (!key.empty() && key.back() == '_') {
        key.pop_back();
    }

    return key;
}

std::optional<std::string> GetItemNameAlias(const std::string& itemName) {
    static const std::unordered_map<std::string, std::string> aliases = {
        { "SHIELD_HERO", "Hero's Shield" },
        { "BOTTLE_OF_EMPTY", "Empty Bottle" },
        { "BOTTLE_EMPTY", "Empty Bottle" },
        { "EMPTY_BOTTLE", "Empty Bottle" },
        { "TRUTH_MASK", "Mask of Truth" },
        { "MASK_TRUTH", "Mask of Truth" },
        { "MASK_OF_TRUTH", "Mask of Truth" },
        { "KAFEI_MASK", "Kafei's Mask" },
        { "MASK_KAFEI", "Kafei's Mask" },
        { "KAFEIS_MASK", "Kafei's Mask" },
        { "MASK_KAFEIS_MASK", "Kafei's Mask" },
        { "LENS", "Lens of Truth" },
        { "LENS_OF_TRUTH", "Lens of Truth" },
        { "POSTMAN_MASK", "Postman's Hat" },
        { "MASK_POSTMAN", "Postman's Hat" },
        { "SONG_EPONA", "Epona's Song" },
        { "SONG_OF_EPONA", "Epona's Song" },
        { "EPONA_SONG", "Epona's Song" },
        { "EPONAS_SONG", "Epona's Song" },
        { "SONG_NOVA", "New Wave Bossa Nova" },
        { "SONG_OF_NOVA", "New Wave Bossa Nova" },
        { "NEW_WAVE_BOSSA_NOVA", "New Wave Bossa Nova" },
        { "SONG_SONATA", "Sonata of Awakening" },
        { "SONG_OF_SONATA", "Sonata of Awakening" },
        { "SONATA_OF_AWAKENING", "Sonata of Awakening" },
        { "SONG_SOARING", "Song of Soaring" },
        { "SONG_OF_SOARING", "Song of Soaring" },
        { "SONG_OF_ELEGY", "Elegy of Emptiness" },
        { "SONG_ELEGY", "Elegy of Emptiness" },
        { "ELEGY_OF_EMPTINESS", "Elegy of Emptiness" },
        { "PROGRESSIVE_LULLABY", "Progressive Goron Lullaby" },
        { "SONG_LULLABY_INTRO", "Goron Lullaby Intro" },
        { "LULLABY_INTRO", "Goron Lullaby Intro" },
        { "SONG_LULLABY", "Goron Lullaby" },
        { "GORON_LULLABY", "Goron Lullaby" },
        { "SHIELD_MIRROR", "Mirror Shield" },
        { "MIRROR_SHIELD", "Mirror Shield" },
        { "SONG_OF_OATH", "Oath to Order" },
        { "SONG_OATH", "Oath to Order" },
        { "OATH_TO_ORDER", "Oath to Order" },
        { "DEED_LAND", "Land Deed" },
        { "LAND_DEED", "Land Deed" },
        { "DEED_SWAMP", "Swamp Deed" },
        { "SWAMP_DEED", "Swamp Deed" },
        { "DEED_MOUNTAIN", "Mountain Deed" },
        { "MOUNTAIN_DEED", "Mountain Deed" },
        { "DEED_OCEAN", "Ocean Deed" },
        { "OCEAN_DEED", "Ocean Deed" },
        { "TIME_DAY_1", "Day 1" },
        { "TIME_DAY_2", "Day 2" },
        { "TIME_DAY_3", "Day 3" },
        { "TIME_NIGHT_1", "Night 1" },
        { "TIME_NIGHT_2", "Night 2" },
        { "TIME_NIGHT_3", "Night 3" },
        { "TIME_PROGRESSIVE", "Progressive Time" },
        { "FROG_BLUE", "Blue Frog" },
        { "BLUE_FROG", "Blue Frog" },
        { "FROG_CYAN", "Cyan Frog" },
        { "CYAN_FROG", "Cyan Frog" },
        { "FROG_PINK", "Pink Frog" },
        { "PINK_FROG", "Pink Frog" },
        { "FROG_WHITE", "White Frog" },
        { "WHITE_FROG", "White Frog" },
        { "NONE", "Literally Nothing" },
        { "RI_NONE", "Literally Nothing" },
    };

    const auto alias = aliases.find(NormalizeItemAliasKey(itemName));
    if (alias != aliases.end()) {
        return alias->second;
    }

    return std::nullopt;
}

std::string JoinTitleWords(const std::vector<std::string>& words, size_t startIndex = 0) {
    std::string label;
    for (size_t index = startIndex; index < words.size(); index++) {
        if (!label.empty()) {
            label += " ";
        }
        label += TitleCaseToken(words[index]);
    }

    return label;
}

std::string HumanizeItemName(const SharedPlacement& placement) {
    std::string itemName = StripItemPrefix(placement.item);
    if (itemName.rfind("RI_", 0) == 0 || itemName.rfind("RG_", 0) == 0) {
        itemName = itemName.substr(3);
    }

    if (const auto alias = GetItemNameAlias(itemName)) {
        return *alias;
    }

    if (itemName.find('_') == std::string::npos) {
        return itemName;
    }

    const auto words = SplitItemWords(itemName);
    if (words.empty()) {
        return itemName;
    }

    if (words[0] == "MASK" && words.size() > 1) {
        if (words[1] == "COUPLE") {
            return "Couple's Mask";
        }
        return JoinTitleWords(words, 1) + " Mask";
    }
    if (words[0] == "ARROW" && words.size() > 1) {
        return JoinTitleWords(words, 1) + " Arrow";
    }
    if (words[0] == "SONG" && words.size() > 1) {
        return "Song of " + JoinTitleWords(words, 1);
    }
    if (words[0] == "BOTTLE" && words.size() > 1) {
        return "Bottle of " + JoinTitleWords(words, 1);
    }
    if (words[0] == "PROGRESSIVE" && words.size() > 1) {
        return "Progressive " + JoinTitleWords(words, 1);
    }

    return JoinTitleWords(words);
}

bool IsOcarinaButtonItem(const SharedPlacement& placement) {
    const std::string key = NormalizeItemAliasKey(StripItemPrefix(placement.item));
    return key.find("OCARINA_BUTTON") != std::string::npos || key == "BUTTON_A" ||
           key == "BUTTON_C_UP" || key == "BUTTON_C_LEFT" || key == "BUTTON_C_DOWN" ||
           key == "BUTTON_C_RIGHT";
}

bool IsIceTrapItem(const std::string& rawItem) {
    const std::string key = NormalizeItemAliasKey(StripItemPrefix(rawItem));
    return key == "RI_TRAP" || key == "TRAP" || key == "RG_ICE_TRAP" || key == "ICE_TRAP" ||
           key == "KNOCKOFF_ITEM";
}

bool IsIceTrapPlacement(const SharedPlacement& placement) {
    return IsIceTrapItem(placement.item) || placement.model == "xoot.ice_trap" || placement.model == "xmm.trap";
}

bool IsLiteralNothingItemKey(const std::string& key) {
    return key == "NONE" || key == "RI_NONE";
}

bool IsSilverRupeeItemKey(const std::string& key) {
    return key == "RUPEE_SILVER" || key == "SILVER_RUPEE" || key.find("SILVER_RUPEE") != std::string::npos;
}

bool IsHugeRupeeItemKey(const std::string& key) {
    return key == "RUPEE_HUGE" || key == "HUGE_RUPEE" || key.find("HUGE_RUPEE") != std::string::npos;
}

bool IsJunkLabeledItemKey(const std::string& key) {
    if (IsLiteralNothingItemKey(key) || IsSilverRupeeItemKey(key) || IsHugeRupeeItemKey(key)) {
        return false;
    }

    return key == "JUNK" || key == "RI_JUNK" || key == "RECOVERY_HEART" || key == "GREEN_RUPEE" ||
           key == "RUPEE_GREEN" || key == "BLUE_RUPEE" || key == "RUPEE_BLUE" || key == "RED_RUPEE" ||
           key == "RUPEE_RED" || key == "PURPLE_RUPEE" || key == "RUPEE_PURPLE" ||
           key.find("ARROWS_") != std::string::npos || key.find("BOMBS_") != std::string::npos ||
           key.find("BOMBCHU") != std::string::npos || key.find("DEKU_NUT") != std::string::npos ||
           key.find("DEKU_STICK") != std::string::npos || key.find("DEKU_SEED") != std::string::npos ||
           key.find("MAGIC_JAR") != std::string::npos || key.find("REFILL") != std::string::npos ||
           key.find("FISH") != std::string::npos || key.find("MILK") != std::string::npos;
}

std::string HumanizeItemNameWithSource(const SharedPlacement& placement) {
    const std::string key = NormalizeItemAliasKey(StripItemPrefix(placement.item));
    if (IsLiteralNothingItemKey(key)) {
        return "Literally Nothing";
    }

    if (IsOcarinaButtonItem(placement)) {
        return HumanizeItemName(placement);
    }

    if (IsJunkLabeledItemKey(key)) {
        return "Junk (" + SourceGameLabel(placement.itemSourceGame) + ")";
    }

    return HumanizeItemName(placement) + " (" + SourceGameLabel(placement.itemSourceGame) + ")";
}

void ShowSharedPickupMessage(const SharedPlacement& placement) {
    const std::string suffix = HumanizeItemNameWithSource(placement);

    try {
        Notification::Emit({
            .message = "You found ",
            .suffix = suffix,
        });
        return;
    } catch (...) {
    }

    const std::string message = "You found " + suffix;

    try {
        auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
        if (gui != nullptr) {
            gui->GetGameOverlay()->TextDrawNotification(5.0f, true, "%s", message.c_str());
        }
    } catch (...) {
    }
}

bool IsShopNativeCheck(const std::string& nativeCheck) {
    return nativeCheck.find(" Shop Item ") != std::string::npos;
}

std::optional<RandomizerGet> ResolveOotVisualPlaceholderForMmItem(const std::string& rawItem) {
    const std::string item = StripItemPrefix(rawItem);

    if (item == "RI_HEART_PIECE") {
        return RG_PIECE_OF_HEART;
    }
    if (item == "RI_HEART_CONTAINER") {
        return RG_HEART_CONTAINER;
    }
    if (IsIceTrapItem(item)) {
        return RG_ICE_TRAP;
    }
    if (item.find("STRAY_FAIRY") != std::string::npos) {
        return RG_BOTTLE_WITH_FAIRY;
    }
    if (item.find("BOSS_KEY") != std::string::npos) {
        return RG_FOREST_TEMPLE_BOSS_KEY;
    }
    if (item.find("SMALL_KEY") != std::string::npos) {
        return RG_FOREST_TEMPLE_SMALL_KEY;
    }
    if (item.find("LETTER") != std::string::npos || item.find("EXPRESS_MAIL") != std::string::npos) {
        return RG_ZELDAS_LETTER;
    }
    if (item.find("MOON") != std::string::npos || item.find("TEAR") != std::string::npos) {
        return RG_STONE_OF_AGONY;
    }
    if (item.find("PENDANT") != std::string::npos || item.find("TITLE_DEED") != std::string::npos ||
        item.find("ROOM_KEY") != std::string::npos || item.find("BOMBERS_NOTEBOOK") != std::string::npos ||
        item.find("SUN_MASK") != std::string::npos) {
        return RG_CLAIM_CHECK;
    }
    if (item == "RI_JUNK" || item == "RI_RECOVERY_HEART") {
        return RG_BOTTLE_WITH_RED_POTION;
    }
    if (item == "RI_RUPEE_GREEN") {
        return RG_GREEN_RUPEE;
    }
    if (item == "RI_RUPEE_BLUE") {
        return RG_BLUE_RUPEE;
    }
    if (item == "RI_RUPEE_RED") {
        return RG_RED_RUPEE;
    }
    if (item == "RI_RUPEE_PURPLE") {
        return RG_PURPLE_RUPEE;
    }
    if (item == "RI_RUPEE_SILVER") {
        return RG_HUGE_RUPEE;
    }
    if (item == "RI_ARROW_FIRE" || item == "RI_FIRE_ARROW" || item == "RI_FIRE_ARROWS") {
        return RG_FIRE_ARROWS;
    }
    if (item == "RI_ARROW_ICE" || item == "RI_ICE_ARROW" || item == "RI_ICE_ARROWS") {
        return RG_ICE_ARROWS;
    }
    if (item == "RI_ARROW_LIGHT" || item == "RI_LIGHT_ARROW" || item == "RI_LIGHT_ARROWS") {
        return RG_LIGHT_ARROWS;
    }
    if (item == "RI_ARROWS_10") {
        return RG_ARROWS_10;
    }
    if (item == "RI_ARROWS_30") {
        return RG_ARROWS_30;
    }
    if (item == "RI_ARROWS_50") {
        return RG_BUY_ARROWS_50;
    }
    if (item == "RI_BOMBS_5") {
        return RG_BOMBS_5;
    }
    if (item == "RI_BOMBS_10") {
        return RG_BOMBS_10;
    }
    if (item == "RI_BOMBCHU" || item == "RI_BOMBCHU_5") {
        return RG_BOMBCHU_5;
    }
    if (item == "RI_BOMBCHU_10") {
        return RG_BOMBCHU_10;
    }
    if (item == "RI_DEKU_NUT") {
        return RG_DEKU_NUTS_5;
    }
    if (item == "RI_DEKU_NUTS_10") {
        return RG_DEKU_NUTS_10;
    }
    if (item == "RI_DEKU_STICK") {
        return RG_DEKU_STICK_1;
    }
    if (item == "RI_BOTTLE_EMPTY") {
        return RG_EMPTY_BOTTLE;
    }
    if (item == "RI_BOTTLE_MILK" || item == "RI_MILK_REFILL") {
        return RG_BOTTLE_WITH_RED_POTION;
    }
    if (item == "RI_BOTTLE_RED_POTION" || item == "RI_RED_POTION_REFILL") {
        return RG_BOTTLE_WITH_RED_POTION;
    }
    if (item == "RI_BLUE_POTION_REFILL") {
        return RG_BOTTLE_WITH_RED_POTION;
    }
    if (item == "RI_FAIRY_REFILL") {
        return RG_BOTTLE_WITH_RED_POTION;
    }
    if (item == "RI_MAGIC_BEAN") {
        return RG_MAGIC_BEAN;
    }
    if (item == "RI_PROGRESSIVE_BOMB_BAG") {
        return RG_PROGRESSIVE_BOMB_BAG;
    }
    if (item == "RI_MASK_GORON") {
        return RG_GORON_MASK;
    }
    if (item == "RI_MASK_ZORA") {
        return RG_ZORA_MASK;
    }
    if (item == "RI_MASK_KEATON") {
        return RG_KEATON_MASK;
    }
    if (item == "RI_MASK_TRUTH") {
        return RG_MASK_OF_TRUTH;
    }
    if (item == "RI_MASK_BUNNY") {
        return RG_BUNNY_HOOD;
    }
    if (item == "RI_SONG_TIME") {
        return RG_SONG_OF_TIME;
    }
    if (item == "RI_SONG_EPONA") {
        return RG_EPONAS_SONG;
    }
    if (item == "RI_SONG_SARIA") {
        return RG_SARIAS_SONG;
    }
    if (item == "RI_SONG_SUN") {
        return RG_SUNS_SONG;
    }
    if (item == "RI_SONG_STORMS") {
        return RG_SONG_OF_STORMS;
    }

    return std::nullopt;
}

std::optional<RandomizerGet> ResolveOotVisualPlaceholderForModel(const std::string& modelId) {
    if (modelId == "xmm.stray_fairy") {
        return RG_BOTTLE_WITH_FAIRY;
    }
    if (modelId == "xmm.key.boss") {
        return RG_FOREST_TEMPLE_BOSS_KEY;
    }
    if (modelId == "xmm.key.small") {
        return RG_FOREST_TEMPLE_SMALL_KEY;
    }
    if (modelId.rfind("xmm.quest.", 0) == 0) {
        if (modelId.find("letter") != std::string::npos || modelId.find("mail") != std::string::npos) {
            return RG_ZELDAS_LETTER;
        }
        if (modelId.find("moon") != std::string::npos || modelId.find("tear") != std::string::npos) {
            return RG_STONE_OF_AGONY;
        }
        return RG_CLAIM_CHECK;
    }
    if (modelId == "xshared.heart_piece") {
        return RG_PIECE_OF_HEART;
    }
    if (modelId == "xshared.heart_container" || modelId == "xshared.double_defense") {
        return RG_HEART_CONTAINER;
    }
    if (modelId == "xshared.recovery_heart" || modelId == "xmm.junk" ||
        modelId.find("_refill") != std::string::npos) {
        return RG_BOTTLE_WITH_RED_POTION;
    }
    if (modelId == "xmm.trap") {
        return RG_ICE_TRAP;
    }
    if (modelId == "xshared.rupee.silver") {
        return std::nullopt;
    }
    if (modelId == "xshared.rupee.huge") {
        return RG_HUGE_RUPEE;
    }
    if (modelId == "xshared.rupee.purple") {
        return RG_PURPLE_RUPEE;
    }
    if (modelId == "xshared.rupee.red") {
        return RG_RED_RUPEE;
    }
    if (modelId == "xshared.rupee.blue") {
        return RG_BLUE_RUPEE;
    }
    if (modelId == "xshared.rupee.green" || modelId == "xshared.rupee") {
        return RG_GREEN_RUPEE;
    }
    if (modelId == "xshared.arrows") {
        return RG_ARROWS_30;
    }
    if (modelId == "xshared.arrows.fire") {
        return RG_FIRE_ARROWS;
    }
    if (modelId == "xshared.arrows.ice") {
        return RG_ICE_ARROWS;
    }
    if (modelId == "xshared.arrows.light") {
        return RG_LIGHT_ARROWS;
    }
    if (modelId == "xshared.bombs") {
        return RG_BOMBS_10;
    }
    if (modelId == "xshared.bombchu") {
        return RG_BOMBCHU_10;
    }
    if (modelId == "xshared.magic_jar_small") {
        return RG_MAGIC_SINGLE;
    }
    if (modelId == "xshared.magic_jar_large") {
        return RG_MAGIC_DOUBLE;
    }
    if (modelId == "xshared.deku_seeds") {
        return RG_DEKU_SEEDS_30;
    }
    if (modelId == "xshared.deku_nuts") {
        return RG_DEKU_NUTS_5;
    }
    if (modelId == "xshared.deku_sticks") {
        return RG_DEKU_STICK_1;
    }
    if (modelId == "xshared.bomb_bag") {
        return RG_PROGRESSIVE_BOMB_BAG;
    }
    if (modelId == "xshared.magic_beans") {
        return RG_MAGIC_BEAN;
    }
    if (modelId == "xshared.lens") {
        return RG_LENS_OF_TRUTH;
    }
    if (modelId == "xshared.wallet") {
        return RG_PROGRESSIVE_WALLET;
    }
    if (modelId == "xshared.bottle.empty") {
        return RG_EMPTY_BOTTLE;
    }
    if (modelId == "xshared.bottle.fairy") {
        return RG_BOTTLE_WITH_RED_POTION;
    }
    if (modelId == "xshared.bottle.fish") {
        return RG_BOTTLE_WITH_RED_POTION;
    }
    if (modelId == "xshared.bottle.bug") {
        return RG_BOTTLE_WITH_RED_POTION;
    }
    if (modelId == "xshared.bottle.milk" || modelId == "xshared.bottle.half_milk") {
        return RG_BOTTLE_WITH_RED_POTION;
    }
    if (modelId == "xshared.bottle.red_potion") {
        return RG_BOTTLE_WITH_RED_POTION;
    }
    if (modelId == "xshared.bottle.blue_potion") {
        return RG_BOTTLE_WITH_RED_POTION;
    }
    if (modelId == "xshared.bottle.small_poe") {
        return RG_BOTTLE_WITH_RED_POTION;
    }
    if (modelId == "xshared.bottle.big_poe") {
        return RG_BOTTLE_WITH_RED_POTION;
    }
    if (modelId == "xshared.mask.goron") {
        return RG_GORON_MASK;
    }
    if (modelId == "xshared.mask.zora") {
        return RG_ZORA_MASK;
    }
    if (modelId == "xshared.mask.keaton") {
        return RG_KEATON_MASK;
    }
    if (modelId == "xshared.mask.truth") {
        return RG_MASK_OF_TRUTH;
    }
    if (modelId == "xshared.mask.bunny") {
        return RG_BUNNY_HOOD;
    }
    if (modelId == "xshared.ocarina_of_time") {
        return RG_OCARINA_OF_TIME;
    }
    if (modelId == "xshared.song.time") {
        return RG_SONG_OF_TIME;
    }
    if (modelId == "xshared.song.sun") {
        return RG_SUNS_SONG;
    }
    if (modelId == "xshared.song.saria") {
        return RG_SARIAS_SONG;
    }
    if (modelId == "xshared.song.epona") {
        return RG_EPONAS_SONG;
    }
    if (modelId == "xshared.song.storms") {
        return RG_SONG_OF_STORMS;
    }

    return std::nullopt;
}

std::optional<RandomizerGet> ResolveOotVisualPlaceholderForPlacement(const SharedPlacement& placement) {
    const auto modelPlaceholder = ResolveOotVisualPlaceholderForModel(placement.model);
    if (modelPlaceholder.has_value()) {
        return modelPlaceholder;
    }

    return ResolveOotVisualPlaceholderForMmItem(placement.item);
}

bool ShouldGiveNativeItemForMmItem(const std::string& rawItem) {
    const std::string item = StripItemPrefix(rawItem);
    return item == "RI_MASK_DEKU" || item == "RI_MASK_GORON" || item == "RI_MASK_ZORA" ||
           item == "RI_MASK_FIERCE_DEITY" || item == "RI_MASK_BUNNY" ||
           item == "RI_MASK_KEATON" || item == "RI_MASK_TRUTH" ||
           item == "RI_SONG_TIME" || item == "RI_SONG_SUN" || item == "RI_SONG_SARIA" ||
           item == "RI_SONG_STORMS";
}

std::optional<RandomizerGet> ResolveOotItemForGrant(const std::string& rawItem) {
    const std::string item = StripItemPrefix(rawItem);

    if (item == "RI_OCARINA_BUTTON_A") {
        return RG_OCARINA_A_BUTTON;
    }
    if (item == "RI_OCARINA_BUTTON_C_UP") {
        return RG_OCARINA_C_UP_BUTTON;
    }
    if (item == "RI_OCARINA_BUTTON_C_DOWN") {
        return RG_OCARINA_C_DOWN_BUTTON;
    }
    if (item == "RI_OCARINA_BUTTON_C_LEFT") {
        return RG_OCARINA_C_LEFT_BUTTON;
    }
    if (item == "RI_OCARINA_BUTTON_C_RIGHT") {
        return RG_OCARINA_C_RIGHT_BUTTON;
    }

    const auto direct = Rando::StaticData::itemNameToEnum.find(item);
    if (direct != Rando::StaticData::itemNameToEnum.end()) {
        return direct->second;
    }

    for (const auto& entry : Rando::StaticData::GetItemTable()) {
        if (entry.GetName().GetEnglish() == item) {
            return entry.GetRandomizerGet();
        }
    }

    return std::nullopt;
}

std::optional<RandomizerInf> ResolveOotOcarinaButtonFlagForMmItem(const std::string& rawItem) {
    const std::string item = StripItemPrefix(rawItem);
    if (item == "RI_OCARINA_BUTTON_A" || item == "RG_OCARINA_A_BUTTON") {
        return RAND_INF_HAS_OCARINA_A;
    }
    if (item == "RI_OCARINA_BUTTON_C_UP" || item == "RG_OCARINA_C_UP_BUTTON") {
        return RAND_INF_HAS_OCARINA_C_UP;
    }
    if (item == "RI_OCARINA_BUTTON_C_DOWN" || item == "RG_OCARINA_C_DOWN_BUTTON") {
        return RAND_INF_HAS_OCARINA_C_DOWN;
    }
    if (item == "RI_OCARINA_BUTTON_C_LEFT" || item == "RG_OCARINA_C_LEFT_BUTTON") {
        return RAND_INF_HAS_OCARINA_C_LEFT;
    }
    if (item == "RI_OCARINA_BUTTON_C_RIGHT" || item == "RG_OCARINA_C_RIGHT_BUTTON") {
        return RAND_INF_HAS_OCARINA_C_RIGHT;
    }

    return std::nullopt;
}

std::optional<u8> ResolveOotMaskItemForMmItem(const std::string& rawItem) {
    const std::string item = StripItemPrefix(rawItem);
    if (item == "RI_MASK_DEKU" || item == "RG_DEKU_MASK" || item == "DEKU_MASK") {
        return ITEM_MASK_DEKU;
    }
    if (item == "RI_MASK_GORON" || item == "RG_GORON_MASK" || item == "GORON_MASK") {
        return ITEM_MASK_GORON;
    }
    if (item == "RI_MASK_ZORA" || item == "RG_ZORA_MASK" || item == "ZORA_MASK") {
        return ITEM_MASK_ZORA;
    }
    if (item == "RI_MASK_FIERCE_DEITY" || item == "RG_FIERCE_DEITY_MASK" || item == "FIERCE_DEITY_MASK") {
        return ITEM_MASK_FIERCE_DEITY;
    }
    if (item == "RI_MASK_BUNNY" || item == "RG_BUNNY_HOOD" || item == "BUNNY_HOOD") {
        return ITEM_MASK_BUNNY;
    }
    if (item == "RI_MASK_KEATON" || item == "RG_KEATON_MASK" || item == "KEATON_MASK") {
        return ITEM_MASK_KEATON;
    }
    if (item == "RI_MASK_SKULL" || item == "RG_SKULL_MASK" || item == "SKULL_MASK") {
        return ITEM_MASK_SKULL;
    }
    if (item == "RI_MASK_SPOOKY" || item == "RG_SPOOKY_MASK" || item == "SPOOKY_MASK") {
        return ITEM_MASK_SPOOKY;
    }
    if (item == "RI_MASK_GERUDO" || item == "RG_GERUDO_MASK" || item == "GERUDO_MASK") {
        return ITEM_MASK_GERUDO;
    }
    if (item == "RI_MASK_TRUTH" || item == "RG_MASK_OF_TRUTH" || item == "MASK_OF_TRUTH" || item == "TRUTH_MASK") {
        return ITEM_MASK_TRUTH;
    }
    return std::nullopt;
}

void MarkOotMaskOwnedForCrossoverGrant(u8 itemId) {
    switch (itemId) {
        case ITEM_MASK_KEATON:
            Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_KEATON);
            break;
        case ITEM_MASK_SKULL:
            Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_SKULL);
            break;
        case ITEM_MASK_SPOOKY:
            Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_SPOOKY);
            break;
        case ITEM_MASK_BUNNY:
            Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_BUNNY);
            break;
        case ITEM_MASK_GORON:
            Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_GORON);
            break;
        case ITEM_MASK_ZORA:
            Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_ZORA);
            break;
        case ITEM_MASK_GERUDO:
            Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_GERUDO);
            break;
        case ITEM_MASK_TRUTH:
            Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_TRUTH);
            break;
        case ITEM_MASK_DEKU:
            Flags_SetRandomizerInf(RAND_INF_OOTXMM_HAS_MASK_DEKU);
            break;
        case ITEM_MASK_FIERCE_DEITY:
            Flags_SetRandomizerInf(RAND_INF_OOTXMM_HAS_MASK_FIERCE_DEITY);
            break;
        default:
            break;
    }

    if (itemId >= ITEM_MASK_KEATON && itemId <= ITEM_MASK_TRUTH) {
        const u8 currentTradeItem = INV_CONTENT(ITEM_TRADE_CHILD);
        if (currentTradeItem == ITEM_NONE || (currentTradeItem >= ITEM_MASK_KEATON && currentTradeItem <= ITEM_MASK_TRUTH) ||
            currentTradeItem == ITEM_MASK_DEKU || currentTradeItem == ITEM_MASK_FIERCE_DEITY) {
            INV_CONTENT(ITEM_TRADE_CHILD) = itemId;
        }
        return;
    }

}

std::optional<std::filesystem::path> GetInventoryPath() {
    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return std::nullopt;
    }

    return *root / kInventoryFileName;
}

nlohmann::json ReadInventory() {
    const auto inventoryPath = GetInventoryPath();
    if (!inventoryPath.has_value()) {
        return nlohmann::json::object({ { "version", 1 }, { "grants", nlohmann::json::array() } });
    }

    auto inventory = ReadJsonOrObject(*inventoryPath);
    if (!inventory.is_object()) {
        inventory = nlohmann::json::object();
    }
    if (!inventory.contains("version")) {
        inventory["version"] = 1;
    }
    if (!inventory.contains("grants") || !inventory["grants"].is_array()) {
        inventory["grants"] = nlohmann::json::array();
    }

    return inventory;
}

void WriteInventory(const nlohmann::json& inventory) {
    const auto inventoryPath = GetInventoryPath();
    if (!inventoryPath.has_value()) {
        return;
    }

    WriteJsonAtomic(*inventoryPath, inventory);
}

void QueueForeignGrant(const SharedPlacement& placement, const std::string& targetGame) {
    auto inventory = ReadInventory();
    const int saveSlot = GetCurrentSaveSlot();

    for (const auto& grant : inventory["grants"]) {
        if (!grant.is_object()) {
            continue;
        }

        if (grant.value("saveSlot", -1) == saveSlot && grant.value("targetGame", "") == targetGame &&
            grant.value("item", "") == placement.item && grant.value("sourceCheck", "") == placement.check) {
            return;
        }
    }

    inventory["grants"].push_back({
        { "saveSlot", saveSlot },
        { "targetGame", targetGame },
        { "item", placement.item },
        { "sourceGame", "oot" },
        { "sourceCheck", placement.check },
        { "applied", false },
    });
    WriteInventory(inventory);

    SPDLOG_INFO("[OoTxMM] Queued {} grant from {}: {}", targetGame, placement.check, placement.item);
}

int CountMmProgressiveSwordGrantsForCurrentSave() {
    const auto inventory = ReadInventory();
    const int saveSlot = GetCurrentSaveSlot();
    int count = 0;

    if (!inventory.contains("grants") || !inventory["grants"].is_array()) {
        return 0;
    }

    for (const auto& grant : inventory["grants"]) {
        if (!grant.is_object() || grant.value("saveSlot", -1) != saveSlot ||
            grant.value("targetGame", "") != "mm") {
            continue;
        }

        const std::string item = grant.value("item", "");
        if (item.find("PROGRESSIVE_SWORD") != std::string::npos) {
            count++;
        }
    }

    return count;
}

void ApplySharedPlacementsToNativeRando(bool force = false, bool logResult = true) {
    if ((sSharedPlacementsAppliedToNativeRando && !force) || !IS_RANDO) {
        return;
    }

    sSharedPlacementsAppliedToNativeRando = true;
    LoadSharedPlacements();

    size_t local = 0;
    size_t crossGame = 0;
    size_t unknown = 0;

    auto applyToContext = [&](const std::shared_ptr<Rando::Context>& ctx, bool countStats) {
        if (ctx == nullptr) {
            return;
        }

        for (const auto& [nativeCheck, placement] : sSharedPlacementsByNativeCheck) {
            const auto locIt = Rando::StaticData::locationNameToEnum.find(nativeCheck);
            if (locIt == Rando::StaticData::locationNameToEnum.end()) {
                if (countStats) {
                    unknown++;
                }
                continue;
            }

            const auto randomizerCheck = locIt->second;
            if (placement.itemSourceGame == "oot") {
                const auto itemIt = Rando::StaticData::itemNameToEnum.find(StripItemPrefix(placement.item));
                if (itemIt == Rando::StaticData::itemNameToEnum.end()) {
                    if (countStats) {
                        unknown++;
                    }
                    continue;
                }

                ctx->GetItemLocation(randomizerCheck)->SetPlacedItem(itemIt->second);
                if (countStats) {
                    local++;
                }
            } else if (placement.itemSourceGame == "mm") {
                const auto placeholder = ResolveOotVisualPlaceholderForPlacement(placement);
                ctx->GetItemLocation(randomizerCheck)
                    ->SetPlacedItem(placeholder.value_or(IsShopNativeCheck(nativeCheck) ? RG_BUY_HEART
                                                                                         : RG_RECOVERY_HEART));
                if (countStats) {
                    crossGame++;
                }
            }
        }

        ctx->CreateItemOverrides();
    };

    auto singletonCtx = Rando::Context::GetInstance();
    applyToContext(singletonCtx, true);

    auto globals = OTRGlobals::Instance;
    if (globals != nullptr && globals->gRandoContext != nullptr && globals->gRandoContext != singletonCtx) {
        applyToContext(globals->gRandoContext, false);
    }

    if (logResult) {
        SPDLOG_INFO("[OoTxMM] SoH applied shared placements to native rando ({} local, {} cross-game, {} unknown)",
                    local, crossGame, unknown);
    }
}

void ScheduleReturnStabilization() {
    sForcePlacementApplyFrames = 900;
    sPlacementHeartbeatFrames = 0;
}

void MarkPendingOotGrantApplied(const std::string& item, const std::string& sourceCheck) {
    auto inventory = ReadInventory();
    bool changed = false;
    const int saveSlot = GetCurrentSaveSlot();

    for (auto& grant : inventory["grants"]) {
        if (!grant.is_object() || grant.value("applied", false) || grant.value("targetGame", "") != "oot" ||
            grant.value("saveSlot", -1) != saveSlot || grant.value("item", "") != item ||
            grant.value("sourceCheck", "") != sourceCheck) {
            continue;
        }

        grant["applied"] = true;
        changed = true;
        break;
    }

    if (changed) {
        WriteInventory(inventory);
        SaveCurrentGameBeforeSwitch();
        SPDLOG_INFO("[OoTxMM] Confirmed pending OoT grant {}", item);
    }
}

void ApplyPendingOotGrants() {
    if (!IS_RANDO) {
        return;
    }

    auto inventory = ReadInventory();
    bool changed = false;
    size_t applied = 0;
    const int saveSlot = GetCurrentSaveSlot();

    if (inventory.value("ootGrantApplyVersion", 0) < 5) {
        for (auto& grant : inventory["grants"]) {
            if (grant.is_object() && grant.value("targetGame", "") == "oot") {
                grant["applied"] = false;
            }
        }
        inventory["ootGrantApplyVersion"] = 5;
        changed = true;
    }

    for (auto& grant : inventory["grants"]) {
        if (!grant.is_object() || grant.value("applied", false) || grant.value("targetGame", "") != "oot" ||
            grant.value("saveSlot", -1) != saveSlot) {
            continue;
        }

        const auto item = grant.value("item", "");
        const auto sourceCheck = grant.value("sourceCheck", "");
        if (IsIceTrapItem(item)) {
            grant["applied"] = true;
            changed = true;
            SPDLOG_INFO("[OoTxMM] Discarded pending OoT ice trap grant {}", item);
            continue;
        }

        const std::string itemKey = StripItemPrefix(item);
        if (itemKey == "RI_MASK_DEKU" || itemKey == "RI_MASK_FIERCE_DEITY") {
            const u8 maskItem = itemKey == "RI_MASK_DEKU" ? ITEM_MASK_DEKU : ITEM_MASK_FIERCE_DEITY;
            Item_Give(nullptr, maskItem);
            MarkOotMaskOwnedForCrossoverGrant(maskItem);
            grant["applied"] = true;
            changed = true;
            applied++;
            SPDLOG_INFO("[OoTxMM] Silently applied pending OoT transform mask grant {}", item);
            continue;
        }

        const auto itemId = ResolveOotItemForGrant(item);
        if (!itemId.has_value()) {
            SPDLOG_WARN("[OoTxMM] Could not apply unknown OoT grant item {}", item);
            continue;
        }

        const auto getItemEntry = Rando::StaticData::RetrieveItem(*itemId).GetGIEntry_Copy();
        if (getItemEntry.modIndex == MOD_RANDOMIZER) {
            Randomizer_Item_Give(nullptr, getItemEntry);
            if (const auto ocarinaButtonFlag = ResolveOotOcarinaButtonFlagForMmItem(item); ocarinaButtonFlag.has_value()) {
                Flags_SetRandomizerInf(*ocarinaButtonFlag);
            }
            if (const auto maskItem = ResolveOotMaskItemForMmItem(item); maskItem.has_value()) {
                MarkOotMaskOwnedForCrossoverGrant(*maskItem);
            }
        } else if (getItemEntry.modIndex == MOD_NONE) {
            if (getItemEntry.getItemId == GI_SWORD_BGS) {
                gSaveContext.bgsFlag = true;
            }
            Item_Give(nullptr, static_cast<uint8_t>(getItemEntry.itemId));
            if (const auto maskItem = ResolveOotMaskItemForMmItem(item); maskItem.has_value()) {
                MarkOotMaskOwnedForCrossoverGrant(*maskItem);
            }
        } else {
            SPDLOG_WARN("[OoTxMM] Could not apply OoT grant with unsupported mod index {} for item {}",
                        getItemEntry.modIndex, item);
            continue;
        }

        grant["applied"] = true;
        changed = true;
        applied++;
        SPDLOG_INFO("[OoTxMM] Silently applied pending OoT grant {}", item);
    }

    if (changed) {
        WriteInventory(inventory);
        SaveCurrentGameBeforeSwitch();
    }

    if (applied > 0) {
        SPDLOG_INFO("[OoTxMM] Silently applied {} pending OoT grants", applied);
    }
}

void SaveCurrentGameBeforeSwitch() {
    if (gPlayState == nullptr || gSaveContext.fileNum < 0 || gSaveContext.fileNum > 2) {
        SaveManager::Instance->ThreadPoolWait();
        return;
    }

    SaveManager::Instance->SaveFile(gSaveContext.fileNum);
    SaveManager::Instance->ThreadPoolWait();
}

bool IsHappyMaskShopEntrance(int16_t entranceIndex) {
    switch (entranceIndex) {
        case ENTR_HAPPY_MASK_SHOP_0:
        case ENTR_HAPPY_MASK_SHOP_0_1:
        case ENTR_HAPPY_MASK_SHOP_0_2:
        case ENTR_HAPPY_MASK_SHOP_0_3:
            return true;
        default:
            return false;
    }
}

bool IsMisroutedHappyMaskDoorTransition(PlayState* play) {
    if (play == nullptr || play->nextEntranceIndex != ENTR_DEKU_TREE_ENTRANCE) {
        return false;
    }

    Player* player = GET_PLAYER(play);
    if (player == nullptr || player->doorActor == nullptr) {
        return false;
    }

    Actor* doorActor = player->doorActor;
    const s32 doorType = (doorActor->params >> 7) & 7;

    return (gSaveContext.entranceIndex == ENTR_MARKET_DAY_OUTSIDE_HAPPY_MASK_SHOP &&
            (play->sceneNum == SCENE_MARKET_DAY || play->sceneNum == SCENE_MARKET_NIGHT) &&
            doorActor->category == ACTORCAT_DOOR && doorType == 3);
}

void RequestSwitchToMm(const std::string& requestedEntrance = kMmTargetEntrance,
                       const std::string& returnEntrance = std::string()) {
    if (sSwitchRequested) {
        return;
    }

    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return;
    }

    const auto statePath = *root / kStateFileName;
    nlohmann::json state = nlohmann::json::object();
    if (std::filesystem::exists(statePath)) {
        try {
            state = ReadJson(statePath);
        } catch (...) {
            state = nlohmann::json::object();
        }
    }

    const auto nonce = state.value("CommandNonce", state.value("commandNonce", 0LL)) + 1;
    state["ActiveGame"] = kTargetGame;
    state["RequestedGame"] = "Mm";
    state["RequestedEntrance"] = requestedEntrance;
    state["ReturnEntrance"] = returnEntrance;
    if (returnEntrance.empty()) {
        state["OriginEntrance"] = nullptr;
    } else {
        state["OriginEntrance"] = TaggedEntrance("oot", gSaveContext.entranceIndex);
    }
    state["SaveSlot"] = GetCurrentSaveSlot();
    state["CommandNonce"] = nonce;
    state["Status"] = "requested";
    state["LastError"] = nullptr;

    try {
        SaveCurrentGameBeforeSwitch();
        WriteJsonAtomic(statePath, state);
        sSwitchRequested = true;
        Ship::Context::GetInstance()->GetWindow()->Close();
    } catch (...) {
    }
}

std::optional<nlohmann::json> ConsumeHandoff() {
    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return std::nullopt;
    }

    const auto handoffPath = *root / kHandoffFileName;
    if (!std::filesystem::exists(handoffPath)) {
        return std::nullopt;
    }

    try {
        auto handoff = ReadJson(handoffPath);
        const auto targetEntrance = handoff.value("targetEntrance", "");
        if (handoff.value("targetGame", "") != kTargetGame ||
            (targetEntrance != kTargetEntrance && targetEntrance != kTitleScreenEntrance &&
             !ParseTaggedEntrance(targetEntrance, "oot:entrance:").has_value())) {
            return std::nullopt;
        }

        std::filesystem::remove(handoffPath);
        return handoff;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<nlohmann::json> ConsumePendingFileSelectReturn(int32_t fileNum) {
    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return std::nullopt;
    }

    const auto handoffPath = *root / kHandoffFileName;
    if (!std::filesystem::exists(handoffPath)) {
        return std::nullopt;
    }

    try {
        auto handoff = ReadJson(handoffPath);
        if (handoff.value("targetGame", "") != kTargetGame ||
            handoff.value("targetEntrance", "") != kPendingFileSelectReturnEntrance ||
            handoff.value("saveSlot", 0) != fileNum) {
            return std::nullopt;
        }

        std::filesystem::remove(handoffPath);
        return handoff;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<nlohmann::json> ConsumeImplicitMmReturnFromState(int32_t fileNum) {
    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return std::nullopt;
    }

    const auto statePath = *root / kStateFileName;
    if (!std::filesystem::exists(statePath)) {
        return std::nullopt;
    }

    try {
        auto state = ReadJson(statePath);
        if (state.value("ActiveGame", "") != "Mm" ||
            state.value("RequestedGame", "") != "Mm" ||
            state.value("Status", "") != "running" ||
            state.value("SaveSlot", 0) != fileNum) {
            return std::nullopt;
        }

        const auto nonce = state.value("CommandNonce", state.value("commandNonce", 0LL)) + 1;
        state["ActiveGame"] = kTargetGame;
        state["RequestedGame"] = kTargetGame;
        state["RequestedEntrance"] = kPendingFileSelectReturnEntrance;
        state["CommandNonce"] = nonce;
        state["Status"] = "running";
        state["LastError"] = nullptr;
        WriteJsonAtomic(statePath, state);

        return nlohmann::json{
            { "targetGame", kTargetGame },
            { "targetEntrance", kPendingFileSelectReturnEntrance },
            { "saveSlot", fileNum },
            { "commandNonce", nonce },
        };
    } catch (...) {
        return std::nullopt;
    }
}

void ApplyHappyMaskShopReturn() {
    for (int buttonIndex = 0; buttonIndex < ARRAY_COUNT(gSaveContext.buttonStatus); buttonIndex++) {
        gSaveContext.buttonStatus[buttonIndex] = BTN_ENABLED;
    }

    Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_STOP);
    gSaveContext.entranceIndex = ENTR_MARKET_DAY_OUTSIDE_HAPPY_MASK_SHOP;
    gSaveContext.linkAge = LINK_AGE_CHILD;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.cutsceneTrigger = 0;
    gSaveContext.nextCutsceneIndex = 0;
    gSaveContext.respawnFlag = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = ENTR_LOAD_OPENING;
    gSaveContext.seqId = static_cast<u8>(NA_BGM_DISABLED);
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.showTitleCard = true;
    gWeatherMode = 0;
}

void BootToHappyMaskShopReturn(void* gameState, const nlohmann::json& handoff) {
    TitleContext* titleContext = static_cast<TitleContext*>(gameState);
    const int saveSlot = handoff.value("saveSlot", 0);

    gSaveContext.gameMode = GAMEMODE_NORMAL;
    if (saveSlot >= 0 && saveSlot <= 2) {
        gSaveContext.fileNum = saveSlot;
        Save_LoadFile();
        GameInteractor_ExecuteOnLoadGame(gSaveContext.fileNum);
        ApplySharedPlacementsToNativeRando(true);
        ScheduleReturnStabilization();
    }

    ApplyHappyMaskShopReturn();

    titleContext->state.running = false;
    SET_NEXT_GAMESTATE(&titleContext->state, Play_Init, PlayState);
}

void ApplyGenericEntranceReturn(int32_t entrance) {
    for (int buttonIndex = 0; buttonIndex < ARRAY_COUNT(gSaveContext.buttonStatus); buttonIndex++) {
        gSaveContext.buttonStatus[buttonIndex] = BTN_ENABLED;
    }

    Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_STOP);
    gSaveContext.entranceIndex = static_cast<int16_t>(entrance);
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.cutsceneTrigger = 0;
    gSaveContext.nextCutsceneIndex = 0;
    gSaveContext.respawnFlag = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = ENTR_LOAD_OPENING;
    gSaveContext.seqId = static_cast<u8>(NA_BGM_DISABLED);
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.showTitleCard = true;
    gWeatherMode = 0;
}

void BootToGenericEntrance(void* gameState, const nlohmann::json& handoff, int32_t entrance) {
    TitleContext* titleContext = static_cast<TitleContext*>(gameState);
    const int saveSlot = handoff.value("saveSlot", 0);

    gSaveContext.gameMode = GAMEMODE_NORMAL;
    if (saveSlot >= 0 && saveSlot <= 2) {
        gSaveContext.fileNum = saveSlot;
        Save_LoadFile();
        GameInteractor_ExecuteOnLoadGame(gSaveContext.fileNum);
        ApplySharedPlacementsToNativeRando(true);
        ScheduleReturnStabilization();
    }

    ApplyGenericEntranceReturn(entrance);

    titleContext->state.running = false;
    SET_NEXT_GAMESTATE(&titleContext->state, Play_Init, PlayState);
}

void RegisterHooks() {
    CVarSetInteger(CVAR_RANDOMIZER_SETTING("TriforceHunt"), RO_TRIFORCE_HUNT_OFF);
    CVarSetInteger(CVAR_RANDOMIZER_ENHANCEMENT("TimeSavers.SkipGetItemAnimation"), SGIA_JUNK);

    COND_HOOK(OnZTitleUpdate, !sBootHandoffConsumed, [](void* gameState) {
        auto handoff = ConsumeHandoff();
        if (!handoff.has_value()) {
            return;
        }

        sBootHandoffConsumed = true;
        if (handoff->value("targetEntrance", "") == kTitleScreenEntrance) {
            return;
        }
        if (const auto entrance = ParseTaggedEntrance(handoff->value("targetEntrance", ""), "oot:entrance:");
            entrance.has_value()) {
            sDungeonReturnEntrance = handoff->value("returnEntrance", "");
            if (sDungeonReturnEntrance.empty()) {
                if (const auto origin = ResolveComboDungeonOriginForTarget("oot", *entrance);
                    origin.has_value() && std::string(origin->game) == "mm") {
                    sDungeonReturnEntrance = TaggedEntrance("mm", origin->returnEntrance);
                }
            }
            BootToGenericEntrance(gameState, *handoff, *entrance);
            return;
        }
        BootToHappyMaskShopReturn(gameState, *handoff);
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadFile>([](int32_t fileNum) {
        CVarSetInteger(CVAR_RANDOMIZER_SETTING("TriforceHunt"), RO_TRIFORCE_HUNT_OFF);
        CVarSetInteger(CVAR_RANDOMIZER_ENHANCEMENT("TimeSavers.SkipGetItemAnimation"), SGIA_JUNK);
        LoadSharedPlacements();
        ApplySharedPlacementsToNativeRando();

        if (sPendingFileSelectReturnApplied) {
            return;
        }

        auto handoff = ConsumePendingFileSelectReturn(fileNum);
        if (!handoff.has_value()) {
            handoff = ConsumeImplicitMmReturnFromState(fileNum);
        }
        if (!handoff.has_value()) {
            return;
        }

        sPendingFileSelectReturnApplied = true;
        if (IS_RANDO) {
            Entrance_Init();
            ApplySharedPlacementsToNativeRando(true);
            ScheduleReturnStabilization();
        }
        ApplyHappyMaskShopReturn();
    });

    COND_HOOK(OnGameFrameUpdate, true, []() {
        if (!IS_OOTXMM || sSwitchRequested || gPlayState == nullptr) {
            return;
        }

        CVarSetInteger(CVAR_RANDOMIZER_ENHANCEMENT("TimeSavers.SkipGetItemAnimation"), SGIA_JUNK);
        const bool nativeDungeonEntrancesEnabled =
            CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleDungeonsEntrances"), RO_DUNGEON_ENTRANCE_SHUFFLE_OFF) !=
            RO_DUNGEON_ENTRANCE_SHUFFLE_OFF;
        if (ComboDungeonSetting() != 0 && (!sComboDungeonNativeEntrancesReset || nativeDungeonEntrancesEnabled)) {
            CVarSetInteger(CVAR_RANDOMIZER_SETTING("ShuffleDungeonsEntrances"), RO_DUNGEON_ENTRANCE_SHUFFLE_OFF);
            Entrance_ResetEntranceTable();
            sComboDungeonNativeEntrancesReset = true;
        } else if (ComboDungeonSetting() == 0) {
            sComboDungeonNativeEntrancesReset = false;
        }

        if (sForcePlacementApplyFrames > 0) {
            ApplySharedPlacementsToNativeRando(true, false);
            sForcePlacementApplyFrames--;
        }

        if (sPlacementHeartbeatFrames <= 0) {
            ApplySharedPlacementsToNativeRando(true, false);
            sPlacementHeartbeatFrames = 30;
        } else {
            sPlacementHeartbeatFrames--;
        }

        if (gPlayState->transitionTrigger != TRANS_TRIGGER_START) {
            sComboDungeonTransitionHandled = false;
        }

        if (gPlayState->transitionTrigger == TRANS_TRIGGER_START && !sComboDungeonTransitionHandled) {
            if (!sDungeonReturnEntrance.empty()) {
                if (IsComboDungeonReturnExit("oot", gPlayState->nextEntranceIndex)) {
                    sComboDungeonTransitionHandled = true;
                    const std::string returnEntrance = sDungeonReturnEntrance;
                    sDungeonReturnEntrance.clear();
                    if (returnEntrance.rfind("mm:entrance:", 0) == 0) {
                        gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
                        RequestSwitchToMm(returnEntrance, "");
                        return;
                    }
                    if (const auto entrance = ParseTaggedEntrance(returnEntrance, "oot:entrance:"); entrance.has_value()) {
                        gPlayState->nextEntranceIndex = static_cast<int16_t>(*entrance);
                    }
                    return;
                }
            }

            const int32_t comboEntrance = gPlayState->nextEntranceIndex;

            const bool observedDungeonEntrance =
                FindComboDungeonEndpointByEntrance("oot", comboEntrance).has_value();
            auto origin = observedDungeonEntrance
                              ? FindOotComboDungeonEndpointByExteriorScene(gPlayState->sceneNum, comboEntrance)
                              : std::optional<ComboDungeonEndpoint>(std::nullopt);
            if (!origin.has_value()) {
                origin = FindComboDungeonEndpointByEntrance("oot", comboEntrance);
            }

            if (origin.has_value()) {
                if (const auto target = ResolveComboDungeonTarget(*origin); target.has_value()) {
                    sComboDungeonTransitionHandled = true;
                    WriteComboDungeonTransitionDebug(*origin, *target, comboEntrance);
                    if (std::string(target->game) == "oot") {
                        sDungeonReturnEntrance = TaggedEntrance("oot", origin->returnEntrance);
                        gPlayState->nextEntranceIndex = static_cast<int16_t>(target->entrance);
                    } else {
                        gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
                        RequestSwitchToMm(TaggedEntrance("mm", target->entrance),
                                          TaggedEntrance("oot", origin->returnEntrance));
                        return;
                    }
                }
            }

            if (IsHappyMaskShopEntrance(gPlayState->nextEntranceIndex) || IsMisroutedHappyMaskDoorTransition(gPlayState)) {
                sComboDungeonTransitionHandled = true;
                gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
                RequestSwitchToMm();
            }
        }

        ApplyPendingOotGrants();
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t sceneNum) {
        (void)sceneNum;
        if (!IS_OOTXMM || !IS_RANDO) {
            return;
        }

        ApplySharedPlacementsToNativeRando(true, false);
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnRandoSetCheckStatus>(
        [](RandomizerCheck rc, RandomizerCheckStatus status) {
            if (!IS_OOTXMM || !IS_RANDO || status != RCSHOW_COLLECTED) {
                return;
            }

            const auto location = Rando::StaticData::GetLocation(rc);
            if (location == nullptr) {
                return;
            }

            auto placement = GetSharedPlacementByNativeCheck(location->GetName());
            if (!placement.has_value()) {
                return;
            }

            if (IsIceTrapPlacement(*placement)) {
                SPDLOG_INFO("[OoTxMM] Completed OoT check {} as local OoT ice trap", placement->check);
                return;
            }

            if (placement->itemSourceGame == "mm" || IsOcarinaButtonItem(*placement)) {
                if (placement->itemSourceGame == "mm") {
                    if (const auto ocarinaButtonFlag = ResolveOotOcarinaButtonFlagForMmItem(placement->item);
                        ocarinaButtonFlag.has_value()) {
                        Flags_SetRandomizerInf(*ocarinaButtonFlag);
                    }
                    if (const auto maskItem = ResolveOotMaskItemForMmItem(placement->item); maskItem.has_value()) {
                        MarkOotMaskOwnedForCrossoverGrant(*maskItem);
                    }
                }
                QueueForeignGrant(*placement, "mm");
                SPDLOG_INFO("[OoTxMM] Completed OoT check {} as shared MM grant {}", placement->check,
                            placement->item);
            }
    });

}

} // namespace SohCrossover

static RegisterShipInitFunc initCrossover(SohCrossover::RegisterHooks);
