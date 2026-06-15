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
#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>
#include <ship/window/Window.h>
#include <spdlog/spdlog.h>

#include "2s2h/BenPort.h"
#include "2s2h/BenGui/Notification.h"
#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/Crossover/Crossover.h"
#include "2s2h/Enhancements/Saving/SavingEnhancements.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/MiscBehavior/Traps.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/StaticData/StaticData.h"
#include "2s2h/SaveManager/SaveManager.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "global.h"
#include "macros.h"
#include "functions.h"
#include "src/overlays/gamestates/ovl_file_choose/z_file_select.h"
#include "overlays/gamestates/ovl_select/z_select.h"
extern PlayState* gPlayState;
extern SaveContext gSaveContext;
extern GameState* gGameState;
void SaveManager_ThreadPoolWait(void);
}

namespace TwoShipCrossover {

constexpr const char* kStateFileName = "crossover_state.json";
constexpr const char* kHandoffFileName = "crossover_handoff.json";
constexpr const char* kSeedFileName = "crossover_seed.json";
constexpr const char* kInventoryFileName = "crossover_inventory.json";
constexpr const char* kDungeonRouteFileName = "crossover_dungeon_entrances.json";
constexpr const char* kDungeonReturnFileName = "crossover_dungeon_return.json";
constexpr const char* kLastDungeonTransitionFileName = "crossover_last_dungeon_transition.json";
constexpr const char* kTargetGame = "Mm";
constexpr const char* kTargetEntrance = "mm:clock_tower_interior_exit";
constexpr const char* kOotTargetEntrance = "oot:happy_mask_shop_return";
constexpr const char* kOotTitleScreenEntrance = "oot:title_screen";
constexpr int32_t kRecentMmDungeonReturnFrames = 600;

bool sBootHandoffConsumed = false;
bool sSwitchRequested = false;

std::unordered_map<std::string, SharedPlacement> sSharedPlacementsByNativeCheck;
std::unordered_map<int, std::string> sModelIdsByCheckId;
bool sSharedPlacementsLoaded = false;
bool sSharedPlacementsAppliedToNativeRando = false;
bool sOotO2rArchiveLoadAttempted = false;
bool sOotO2rArchiveLoaded = false;
std::shared_ptr<Ship::Archive> sOotO2rArchive = nullptr;
std::string sDungeonReturnEntrance;
int32_t sSuppressForeignMmDungeonOpenFlagsFrames = 0;
bool sComboDungeonTransitionHandled = false;
std::string sLastAppliedMmDungeonReturnEntrance;
int32_t sLastAppliedMmDungeonReturnFrames = 0;

struct GossipHintPool {
    std::vector<std::string> direct;
    std::vector<std::string> wayOfTheHero;
    std::vector<std::string> foolish;
    std::unordered_map<std::string, std::string> assignedByStone;
    std::unordered_set<std::string> usedHints;
};

GossipHintPool sGossipHints;
bool sGossipHintsLoaded = false;

void WriteJsonAtomic(const std::filesystem::path& path, const nlohmann::json& json);
nlohmann::json ReadJsonOrObject(const std::filesystem::path& path);
int GetCurrentSaveSlot();
void RebuildPlacementManifest(const std::filesystem::path& root);
void SaveCurrentGameBeforeSwitch();
bool RequestSwitchToOot(bool saveBeforeSwitch, const std::string& requestedEntrance, const std::string& returnEntrance);
std::optional<std::filesystem::path> FindCrossoverRoot();
std::string SourceGameLabel(const std::string& sourceGame);
uint32_t StableHashString(const std::string& value);
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
        { "builtBy", "mm" },
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
        { "game", "mm" },
        { "observedEntrance", observedEntrance },
        { "currentEntrance", gSaveContext.save.entrance },
        { "sceneId", gPlayState != nullptr ? gPlayState->sceneId : -1 },
        { "origin", ComboDungeonEndpointToJson(origin) },
        { "target", ComboDungeonEndpointToJson(target) },
        { "returnEntrance", TaggedEntrance("mm", origin.returnEntrance) },
    };

    try {
        WriteJsonAtomic(*root / kLastDungeonTransitionFileName, payload);
    } catch (...) {
    }
}

void WriteComboDungeonReturnCommitDebug(int32_t observedEntrance, const std::string& returnEntrance,
                                        int32_t appliedEntrance) {
    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return;
    }

    nlohmann::json payload = {
        { "game", "mm" },
        { "observedEntrance", observedEntrance },
        { "appliedEntrance", appliedEntrance },
        { "returnEntrance", returnEntrance },
        { "sceneId", gPlayState != nullptr ? gPlayState->sceneId : -1 },
    };

    try {
        WriteJsonAtomic(*root / "crossover_last_dungeon_return_commit.json", payload);
    } catch (...) {
    }
}

void WriteComboDungeonReturnEventDebug(const char* eventName, int32_t observedEntrance = -1,
                                       int32_t appliedEntrance = -1, const std::string& note = std::string()) {
    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return;
    }

    static int32_t sequence = 0;
    const auto path = *root / "crossover_dungeon_return_events.json";

    nlohmann::json events = nlohmann::json::array();
    try {
        const auto existing = ReadJsonOrObject(path);
        if (existing.contains("events") && existing["events"].is_array()) {
            events = existing["events"];
        }
    } catch (...) {
    }

    events.push_back({
        { "seq", sequence++ },
        { "event", eventName },
        { "note", note },
        { "observedEntrance", observedEntrance },
        { "appliedEntrance", appliedEntrance },
        { "sceneId", gPlayState != nullptr ? gPlayState->sceneId : -1 },
        { "saveEntrance", gSaveContext.save.entrance },
        { "playNextEntrance", gPlayState != nullptr ? gPlayState->nextEntrance : -1 },
        { "transitionTrigger", gPlayState != nullptr ? gPlayState->transitionTrigger : -1 },
        { "transitionMode", gPlayState != nullptr ? gPlayState->transitionMode : -1 },
        { "transitionType", gPlayState != nullptr ? gPlayState->transitionType : -1 },
        { "nextCutsceneIndex", gSaveContext.nextCutsceneIndex },
        { "saveCutsceneIndex", gSaveContext.save.cutsceneIndex },
        { "pendingReturn", sDungeonReturnEntrance },
        { "recentReturn", sLastAppliedMmDungeonReturnEntrance },
        { "recentFrames", sLastAppliedMmDungeonReturnFrames },
        { "handled", sComboDungeonTransitionHandled },
    });

    while (events.size() > 400) {
        events.erase(events.begin());
    }

    try {
        WriteJsonAtomic(path, nlohmann::json{
                                  { "format", "ootxmm-dungeon-return-events" },
                                  { "events", events },
                              });
    } catch (...) {
    }
}

std::vector<std::filesystem::path> GetOotO2rCandidatePaths() {
    std::vector<std::filesystem::path> candidates;
    const auto cwd = std::filesystem::current_path();

    candidates.push_back(cwd / "oot.o2r");
    candidates.push_back(cwd / ".." / "oot.o2r");
    candidates.push_back(cwd / "soh" / "oot.o2r");
    candidates.push_back(cwd / ".." / "soh" / "oot.o2r");

    if (const auto root = FindCrossoverRoot(); root.has_value()) {
        candidates.push_back(*root / "soh" / "oot.o2r");
        candidates.push_back(*root / "extracted_win" / "soh" / "oot.o2r");
        candidates.push_back(*root / "source files" / "Shipwright-9.2.3" / "oot.o2r");
    }

    candidates.push_back(cwd / "x64" / "Release" / "oot.o2r");
    candidates.push_back(cwd / "x64" / "Debug" / "oot.o2r");
    candidates.push_back(cwd / "build" / "x64" / "oot.o2r");

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

void KeepOotO2rAtLowestResourcePriority(std::shared_ptr<Ship::Archive> ootArchive) {
    if (ootArchive == nullptr) {
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
    reordered->push_back(ootArchive);
    for (const auto& archive : *archives) {
        if (archive == nullptr || IsSamePath(archive->GetPath(), ootArchive->GetPath())) {
            continue;
        }

        reordered->push_back(archive);
    }

    archiveManager->SetArchives(reordered);
}

bool EnsureOotO2rArchiveLoaded() {
    if (sOotO2rArchiveLoaded) {
        return true;
    }
    if (sOotO2rArchiveLoadAttempted) {
        return false;
    }

    sOotO2rArchiveLoadAttempted = true;
    auto archiveManager = Ship::Context::GetInstance()->GetResourceManager()->GetArchiveManager();
    if (archiveManager == nullptr) {
        SPDLOG_WARN("[OoTxMM] Unable to load oot.o2r: archive manager is unavailable");
        return false;
    }

    std::optional<std::filesystem::path> ootO2rPath;
    for (const auto& candidate : GetOotO2rCandidatePaths()) {
        std::error_code ec;
        const auto normalized = std::filesystem::weakly_canonical(candidate, ec);
        const auto path = ec ? candidate.lexically_normal() : normalized;
        if (std::filesystem::exists(path)) {
            ootO2rPath = path;
            break;
        }
    }

    if (!ootO2rPath.has_value()) {
        SPDLOG_WARN("[OoTxMM] oot.o2r was not found; foreign OoT asset models will use fallbacks");
        return false;
    }

    const auto archives = archiveManager->GetArchives();
    if (archives != nullptr) {
        for (const auto& archive : *archives) {
            if (archive != nullptr && IsSamePath(archive->GetPath(), *ootO2rPath)) {
                KeepOotO2rAtLowestResourcePriority(archive);
                sOotO2rArchive = archive;
                sOotO2rArchiveLoaded = true;
                SPDLOG_INFO("[OoTxMM] oot.o2r already mounted: {}", ootO2rPath->string());
                return true;
            }
        }
    }

    const auto ootArchive = archiveManager->AddArchive(ootO2rPath->string());
    if (ootArchive == nullptr) {
        SPDLOG_WARN("[OoTxMM] Failed to mount oot.o2r at {}", ootO2rPath->string());
        return false;
    }

    sOotO2rArchiveLoaded = true;
    sOotO2rArchive = ootArchive;
    KeepOotO2rAtLowestResourcePriority(ootArchive);
    SPDLOG_INFO("[OoTxMM] Loaded oot.o2r for explicit foreign asset lookup: {}", ootO2rPath->string());
    return true;
}

std::shared_ptr<Ship::Archive> GetOotO2rArchive() {
    if (!EnsureOotO2rArchiveLoaded()) {
        return nullptr;
    }

    return sOotO2rArchive;
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

void RebuildPlacementManifest(const std::filesystem::path& root) {
    const auto scriptPath = root / "tools" / "Build-CrossoverPlacementPreview.ps1";
    if (!std::filesystem::exists(scriptPath)) {
        SPDLOG_WARN("[OoTxMM] Placement builder script not found: {}", scriptPath.string());
        return;
    }

    const std::string command = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" + scriptPath.string() +
                                "\" -Root \"" + root.string() + "\"";
    const int result = std::system(command.c_str());
    if (result != 0) {
        SPDLOG_WARN("[OoTxMM] Placement builder exited with {}", result);
    } else {
        SPDLOG_INFO("[OoTxMM] Placement builder completed from 2Ship");
    }
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
    return std::to_string(CVarGetInteger("gRando.SpoilerFileIndex", 0));
}

std::vector<ComboDungeonEndpoint> ComboDungeonEndpoints() {
    std::vector<ComboDungeonEndpoint> endpoints = {
        { "oot", "oot_deku", 0, 521, 521, false },
        { "oot", "oot_dodongo", 4, 578, 578, false },
        { "oot", "oot_jabu", 40, 545, 545, false },
        { "oot", "oot_forest", 361, 533, 533, false },
        { "oot", "oot_fire", 357, 586, 586, false },
        { "oot", "oot_water", 16, 541, 541, false },
        { "oot", "oot_shadow", 55, 517, 517, false },
        { "oot", "oot_spirit", 130, 481, 481, false },
        { "oot", "oot_well", 152, 678, 678, false },
        { "oot", "oot_ice_cavern", 88, 571, 571, false },
        { "oot", "oot_gtg", 8, 583, 583, false },
        { "mm", "mm_woodfall", ENTRANCE(WOODFALL_TEMPLE, 0), ENTRANCE(WOODFALL, 1), ENTRANCE(WOODFALL, 1), false },
        { "mm", "mm_snowhead", ENTRANCE(SNOWHEAD_TEMPLE, 0), ENTRANCE(SNOWHEAD, 1), ENTRANCE(SNOWHEAD, 1), false },
        { "mm", "mm_great_bay", ENTRANCE(GREAT_BAY_TEMPLE, 0), ENTRANCE(ZORA_CAPE, 7), ENTRANCE(ZORA_CAPE, 7), false },
        { "mm", "mm_stone_tower", ENTRANCE(STONE_TOWER_TEMPLE, 0), ENTRANCE(STONE_TOWER, 2),
          ENTRANCE(STONE_TOWER, 2), false },
        { "mm", "mm_stone_tower_inverted", ENTRANCE(STONE_TOWER_TEMPLE_INVERTED, 0),
          ENTRANCE(STONE_TOWER, 1), ENTRANCE(STONE_TOWER_INVERTED, 0), false },
    };

    if (ComboDungeonSetting() >= 2) {
        endpoints.push_back({ "oot", "oot_ganon", 1127, 314, 314, true });
        endpoints.push_back({ "mm", "mm_majora", ENTRANCE(MAJORAS_LAIR, 0), ENTRANCE(THE_MOON, 0),
                              ENTRANCE(THE_MOON, 0), true });
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

std::optional<ComboDungeonEndpoint> FindMmComboDungeonEndpointByExteriorScene(s8 sceneId, int32_t observedEntrance) {
    const char* key = nullptr;
    switch (sceneId) {
        case SCENE_21MITURINMAE:
            key = "mm_woodfall";
            break;
        case SCENE_12HAKUGINMAE:
            key = "mm_snowhead";
            break;
        case SCENE_31MISAKI:
            key = "mm_great_bay";
            break;
        case SCENE_F40:
            key = "mm_stone_tower";
            break;
        case SCENE_F41:
            key = "mm_stone_tower_inverted";
            break;
        default:
            break;
    }

    if (key == nullptr) {
        return std::nullopt;
    }

    for (const auto& endpoint : ComboDungeonEndpoints()) {
        if (std::string(endpoint.game) == "mm" && std::string(endpoint.key) == key) {
            return endpoint;
        }
    }

    return FindComboDungeonEndpointByEntrance("mm", observedEntrance);
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

    if (std::string(game) != "mm") {
        return false;
    }

    switch (entrance) {
        case ENTRANCE(WOODFALL_TEMPLE, 1):
        case ENTRANCE(MOUNTAIN_VILLAGE_SPRING, 7):
        case ENTRANCE(ZORA_CAPE, 9):
        case ENTRANCE(IKANA_CANYON, 15):
        case CS_ENTRANCE(STONE_TOWER_TEMPLE, 0, 2):
        case CS_ENTRANCE(STONE_TOWER_TEMPLE_INVERTED, 0, 2):
        case ENTRANCE(STONE_TOWER, 1):
        case ENTRANCE(STONE_TOWER, 2):
        case ENTRANCE(STONE_TOWER_INVERTED, 0):
            return true;
        default:
            return false;
    }
}

bool IsStoneTowerInverterTransition(int32_t entrance) {
    switch (entrance) {
        case ENTRANCE(STONE_TOWER, 1):
        case ENTRANCE(STONE_TOWER_INVERTED, 0):
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

bool IsMmTempleEntrance(int32_t entrance) {
    return FindComboDungeonEndpointByEntrance("mm", entrance).has_value();
}

void SuppressForeignMmDungeonOpenFlags() {
    CLEAR_WEEKEVENTREG(WEEKEVENTREG_ENTERED_WOODFALL_TEMPLE);
    CLEAR_WEEKEVENTREG(WEEKEVENTREG_ENTERED_WOODFALL_TEMPLE_PRISON);
    CLEAR_WEEKEVENTREG(WEEKEVENTREG_ENTERED_SNOWHEAD_TEMPLE);
    CLEAR_WEEKEVENTREG(WEEKEVENTREG_ENTERED_STONE_TOWER_TEMPLE);
    CLEAR_WEEKEVENTREG(WEEKEVENTREG_ENTERED_STONE_TOWER_TEMPLE_INVERTED);
    CLEAR_WEEKEVENTREG(WEEKEVENTREG_20_01);
    CLEAR_WEEKEVENTREG(WEEKEVENTREG_30_01);
    CLEAR_WEEKEVENTREG(WEEKEVENTREG_53_20);
}

void RestoreDungeonReturnEntranceFromState() {
    if (!sDungeonReturnEntrance.empty()) {
        return;
    }

    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return;
    }

    try {
        const auto dungeonReturn = ReadJsonOrObject(*root / kDungeonReturnFileName);
        const auto returnEntrance = dungeonReturn.value("returnEntrance", std::string());
        if (returnEntrance.rfind("oot:entrance:", 0) == 0 || returnEntrance.rfind("mm:entrance:", 0) == 0) {
            sDungeonReturnEntrance = returnEntrance;
            WriteComboDungeonReturnEventDebug("restore-return-dungeon-file", -1, -1, returnEntrance);
            return;
        }
    } catch (...) {
    }

    try {
        const auto state = ReadJsonOrObject(*root / kStateFileName);
        const auto returnEntrance = state.value("ReturnEntrance", state.value("returnEntrance", std::string()));
        if (returnEntrance.rfind("oot:entrance:", 0) == 0 || returnEntrance.rfind("mm:entrance:", 0) == 0) {
            sDungeonReturnEntrance = returnEntrance;
            WriteComboDungeonReturnEventDebug("restore-return-state", -1, -1, returnEntrance);
        }
    } catch (...) {
    }
}

void ClearDungeonReturnEntranceInState() {
    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return;
    }

    const auto statePath = *root / kStateFileName;
    try {
        auto state = ReadJsonOrObject(statePath);
        state["ReturnEntrance"] = "";
        state["returnEntrance"] = "";
        state["OriginEntrance"] = nullptr;
        WriteJsonAtomic(statePath, state);
    } catch (...) {
    }

    try {
        WriteJsonAtomic(*root / kDungeonReturnFileName, nlohmann::json{
                                                          { "returnEntrance", "" },
                                                          { "originEntrance", nullptr },
                                                          { "targetEntrance", nullptr },
                                                      });
    } catch (...) {
    }
}

void StoreDungeonReturnEntranceInState(const std::string& returnEntrance, const std::string& targetEntrance = std::string()) {
    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return;
    }

    const auto statePath = *root / kStateFileName;
    try {
        auto state = ReadJsonOrObject(statePath);
        state["ReturnEntrance"] = returnEntrance;
        state["returnEntrance"] = returnEntrance;
        state["OriginEntrance"] = TaggedEntrance("mm", gSaveContext.save.entrance);
        WriteJsonAtomic(statePath, state);
        WriteJsonAtomic(*root / kDungeonReturnFileName, nlohmann::json{
                                                          { "returnEntrance", returnEntrance },
                                                          { "originEntrance", TaggedEntrance("mm", gSaveContext.save.entrance) },
                                                          { "targetEntrance", targetEntrance.empty() ? nlohmann::json(nullptr)
                                                                                                      : nlohmann::json(targetEntrance) },
                                                      });
        WriteComboDungeonReturnEventDebug("store-return", -1, -1, returnEntrance);
    } catch (...) {
    }
}

bool ApplyPendingDungeonReturn() {
    RestoreDungeonReturnEntranceFromState();
    if (sDungeonReturnEntrance.empty()) {
        WriteComboDungeonReturnEventDebug("apply-return-empty");
        return false;
    }

    const std::string returnEntrance = sDungeonReturnEntrance;
    sDungeonReturnEntrance.clear();
    ClearDungeonReturnEntranceInState();
    WriteComboDungeonReturnEventDebug("apply-return", gPlayState != nullptr ? gPlayState->nextEntrance : -1, -1,
                                      returnEntrance);
    if (returnEntrance.rfind("oot:entrance:", 0) == 0) {
        if (gPlayState != nullptr) {
            gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
        }
        return RequestSwitchToOot(true, returnEntrance, std::string());
    }
    if (const auto entrance = ParseTaggedEntrance(returnEntrance, "mm:entrance:"); entrance.has_value()) {
        if (gPlayState != nullptr) {
            gPlayState->nextEntrance = *entrance;
        }
        WriteComboDungeonReturnEventDebug("apply-return-mm", -1, *entrance, returnEntrance);
        sLastAppliedMmDungeonReturnEntrance = returnEntrance;
        sLastAppliedMmDungeonReturnFrames = kRecentMmDungeonReturnFrames;
        return true;
    }
    return false;
}

bool HandlePendingMmDungeonReturnTransition() {
    if (gPlayState == nullptr || gPlayState->transitionTrigger != TRANS_TRIGGER_START) {
        return false;
    }

    RestoreDungeonReturnEntranceFromState();
    if (sDungeonReturnEntrance.empty() || !IsComboDungeonReturnExit("mm", gPlayState->nextEntrance)) {
        if (IsComboDungeonReturnExit("mm", gPlayState->nextEntrance) || IsStoneTowerInverterTransition(gPlayState->nextEntrance)) {
            WriteComboDungeonReturnEventDebug("handle-transition-miss", gPlayState->nextEntrance);
        }
        return false;
    }

    sComboDungeonTransitionHandled = true;
    WriteComboDungeonReturnEventDebug("handle-transition-hit", gPlayState->nextEntrance);
    ApplyPendingDungeonReturn();
    return true;
}

bool ReapplyRecentMmDungeonReturn() {
    if (sLastAppliedMmDungeonReturnFrames <= 0 || sLastAppliedMmDungeonReturnEntrance.empty() ||
        gPlayState == nullptr || gPlayState->transitionTrigger != TRANS_TRIGGER_START ||
        !IsStoneTowerInverterTransition(gPlayState->nextEntrance)) {
        return false;
    }

    const auto entrance = ParseTaggedEntrance(sLastAppliedMmDungeonReturnEntrance, "mm:entrance:");
    if (!entrance.has_value()) {
        return false;
    }

    WriteComboDungeonReturnEventDebug("reapply-recent", gPlayState->nextEntrance, *entrance,
                                      sLastAppliedMmDungeonReturnEntrance);
    gPlayState->nextEntrance = *entrance;
    gSaveContext.nextCutsceneIndex = 0;
    return true;
}

bool ShouldSuppressStoneTowerInverter() {
    RestoreDungeonReturnEntranceFromState();
    std::string returnEntrance = sDungeonReturnEntrance;
    if (returnEntrance.empty()) {
        returnEntrance = sLastAppliedMmDungeonReturnEntrance;
    }
    if (returnEntrance.empty()) {
        return false;
    }

    const auto entrance = ParseTaggedEntrance(returnEntrance, "mm:entrance:");
    if (!entrance.has_value()) {
        return false;
    }

    return *entrance != ENTRANCE(STONE_TOWER, 2) && *entrance != ENTRANCE(STONE_TOWER_INVERTED, 0);
}

bool StartRecentMmDungeonReturnTransition() {
    if (gPlayState == nullptr || sLastAppliedMmDungeonReturnEntrance.empty()) {
        return false;
    }

    const auto entrance = ParseTaggedEntrance(sLastAppliedMmDungeonReturnEntrance, "mm:entrance:");
    if (!entrance.has_value()) {
        return false;
    }

    gPlayState->nextEntrance = *entrance;
    gSaveContext.nextCutsceneIndex = 0;
    gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
    gPlayState->transitionTrigger = TRANS_TRIGGER_START;
    sComboDungeonTransitionHandled = true;
    return true;
}

extern "C" bool OoTxMm_OverridePendingMmDungeonExit(PlayState* play) {
    if (play == nullptr) {
        return false;
    }

    RestoreDungeonReturnEntranceFromState();
    if (sDungeonReturnEntrance.empty() || !IsComboDungeonReturnExit("mm", play->nextEntrance)) {
        if (IsComboDungeonReturnExit("mm", play->nextEntrance) || IsStoneTowerInverterTransition(play->nextEntrance)) {
            WriteComboDungeonReturnEventDebug("pending-exit-miss", play->nextEntrance);
        }
        return false;
    }

    const std::string returnEntrance = sDungeonReturnEntrance;
    if (const auto entrance = ParseTaggedEntrance(returnEntrance, "mm:entrance:"); entrance.has_value()) {
        sDungeonReturnEntrance.clear();
        ClearDungeonReturnEntranceInState();
        WriteComboDungeonReturnEventDebug("pending-exit-hit", play->nextEntrance, *entrance, returnEntrance);
        play->nextEntrance = *entrance;
        gSaveContext.nextCutsceneIndex = 0;
        sLastAppliedMmDungeonReturnEntrance = returnEntrance;
        sLastAppliedMmDungeonReturnFrames = kRecentMmDungeonReturnFrames;
        sComboDungeonTransitionHandled = true;
        return true;
    }

    return false;
}

extern "C" bool OoTxMm_OverrideRecentMmDungeonCommit(PlayState* play) {
    if (play == nullptr || !IsComboDungeonReturnExit("mm", play->nextEntrance)) {
        if (play != nullptr && IsStoneTowerInverterTransition(play->nextEntrance)) {
            WriteComboDungeonReturnEventDebug("commit-non-return-stone", play->nextEntrance);
        }
        return false;
    }

    RestoreDungeonReturnEntranceFromState();
    std::string returnEntrance = sDungeonReturnEntrance;
    if (returnEntrance.empty()) {
        returnEntrance = sLastAppliedMmDungeonReturnEntrance;
    }
    if (returnEntrance.empty()) {
        WriteComboDungeonReturnEventDebug("commit-miss-empty", play->nextEntrance);
        return false;
    }

    const auto entrance = ParseTaggedEntrance(returnEntrance, "mm:entrance:");
    if (!entrance.has_value()) {
        WriteComboDungeonReturnEventDebug("commit-miss-parse", play->nextEntrance, -1, returnEntrance);
        return false;
    }

    if (!sDungeonReturnEntrance.empty()) {
        sDungeonReturnEntrance.clear();
        ClearDungeonReturnEntranceInState();
    }
    WriteComboDungeonReturnCommitDebug(play->nextEntrance, returnEntrance, *entrance);
    WriteComboDungeonReturnEventDebug("commit-hit", play->nextEntrance, *entrance, returnEntrance);
    play->nextEntrance = *entrance;
    gSaveContext.nextCutsceneIndex = 0;
    sLastAppliedMmDungeonReturnEntrance = returnEntrance;
    sLastAppliedMmDungeonReturnFrames = kRecentMmDungeonReturnFrames;
    sComboDungeonTransitionHandled = true;
    return true;
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
    sModelIdsByCheckId.clear();

    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        SPDLOG_INFO("[OoTxMM] 2Ship placement manifest skipped: crossover root not found");
        return;
    }

    const auto manifest = ReadPlacementManifest(*root);
    if (!manifest.has_value() || !manifest->contains("placements") || !(*manifest)["placements"].is_array()) {
        SPDLOG_INFO("[OoTxMM] 2Ship placement manifest skipped: no placements found");
        return;
    }

    size_t total = 0;
    size_t local = 0;
    size_t crossGame = 0;
    for (const auto& placement : (*manifest)["placements"]) {
        if (!placement.is_object() || placement.value("checkGame", "") != "mm") {
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
        const RandoCheckId checkId = Rando::StaticData::GetCheckIdFromName(sharedPlacement.nativeCheck.c_str());
        if (checkId != RC_UNKNOWN && !sharedPlacement.model.empty()) {
            sModelIdsByCheckId[static_cast<int>(checkId)] = sharedPlacement.model;
        }
        total++;
        if (sharedPlacement.isCrossGame) {
            crossGame++;
        } else {
            local++;
        }
    }

    SPDLOG_INFO("[OoTxMM] 2Ship loaded {} shared placements ({} local, {} cross-game)", total, local, crossGame);
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

std::optional<std::string> GetModelIdByCheckId(RandoCheckId randoCheckId) {
    LoadSharedPlacements();

    const auto model = sModelIdsByCheckId.find(static_cast<int>(randoCheckId));
    if (model == sModelIdsByCheckId.end() || model->second.empty()) {
        return std::nullopt;
    }

    return model->second;
}

std::string HumanizeItemNameWithSource(const SharedPlacement& placement);

std::optional<std::string> GetSharedPlacementDisplayNameByNativeCheck(const std::string& nativeCheck) {
    const auto placement = GetSharedPlacementByNativeCheck(nativeCheck);
    if (!placement.has_value()) {
        return std::nullopt;
    }

    return HumanizeItemNameWithSource(*placement);
}

bool IsForeignOotExclusiveModelId(const std::string& modelId) {
    return modelId.rfind("xoot.", 0) == 0;
}

std::string StripItemPrefix(const std::string& item) {
    const auto separator = item.find(':');
    if (separator == std::string::npos) {
        return item;
    }

    return item.substr(separator + 1);
}

bool ShouldGrantOotForMmSharedItem(const std::string& rawItem) {
    const std::string item = StripItemPrefix(rawItem);
    return item == "RI_MASK_DEKU" || item == "RI_MASK_GORON" || item == "RI_MASK_ZORA" ||
           item == "RI_MASK_FIERCE_DEITY" || item == "RI_MASK_BUNNY" ||
           item == "RI_MASK_KEATON" || item == "RI_MASK_TRUTH" ||
           item == "RI_OCARINA_BUTTON_A" || item == "RI_OCARINA_BUTTON_C_DOWN" ||
           item == "RI_OCARINA_BUTTON_C_LEFT" || item == "RI_OCARINA_BUTTON_C_RIGHT" ||
           item == "RI_OCARINA_BUTTON_C_UP";
}

bool IsIceTrapItem(const std::string& rawItem) {
    std::string key;
    for (const char character : StripItemPrefix(rawItem)) {
        if (std::isalnum(static_cast<unsigned char>(character))) {
            key.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
        } else if (!key.empty() && key.back() != '_') {
            key.push_back('_');
        }
    }
    while (!key.empty() && key.back() == '_') {
        key.pop_back();
    }

    return key == "RI_TRAP" || key == "TRAP" || key == "RG_ICE_TRAP" || key == "ICE_TRAP" ||
           key == "KNOCKOFF_ITEM";
}

bool IsIceTrapPlacement(const SharedPlacement& placement) {
    return IsIceTrapItem(placement.item) || placement.model == "xoot.ice_trap" || placement.model == "xmm.trap";
}

bool IsMajorForeignOotPlacement(const SharedPlacement& placement) {
    if (placement.itemSourceGame != "oot") {
        return false;
    }

    auto normalizeKey = [](std::string value) {
        std::string key;
        for (const char character : value) {
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
    };

    const std::string item = normalizeKey(StripItemPrefix(placement.item));
    const std::string model = normalizeKey(placement.model);
    auto contains = [](const std::string& value, const char* needle) {
        return value.find(needle) != std::string::npos;
    };

    if (IsIceTrapPlacement(placement)) {
        return false;
    }

    return contains(item, "TUNIC") || contains(model, "TUNIC") ||
           contains(item, "BOOTS") || contains(model, "BOOTS") ||
           contains(item, "SLINGSHOT") || contains(model, "SLINGSHOT") ||
           contains(item, "BOW") || contains(model, "BOW") ||
           contains(item, "HOOKSHOT") || contains(model, "HOOKSHOT") ||
           contains(item, "LONGSHOT") || contains(model, "LONGSHOT") ||
           contains(item, "BOMB_BAG") || contains(model, "BOMB_BAG") ||
           contains(item, "STRENGTH") || contains(model, "STRENGTH") ||
           contains(item, "BRACELET") || contains(model, "BRACELET") ||
           contains(item, "GAUNTLET") || contains(model, "GAUNTLET") ||
           contains(item, "SCALE") || contains(model, "SCALE") ||
           contains(item, "WALLET") || contains(model, "WALLET") ||
           contains(item, "LENS") || contains(model, "LENS") ||
           contains(item, "OCARINA") || contains(model, "OCARINA") ||
           contains(item, "SWORD") || contains(model, "SWORD") ||
           contains(item, "SHIELD") || contains(model, "SHIELD") ||
           contains(item, "HAMMER") || contains(model, "HAMMER") ||
           contains(item, "BOOMERANG") || contains(model, "BOOMERANG") ||
           contains(item, "ARROW") || contains(model, "ARROW") ||
           contains(item, "MAGIC") || contains(model, "MAGIC") ||
           contains(item, "DINS_FIRE") || contains(model, "DINS_FIRE") ||
           contains(item, "FARORES_WIND") || contains(model, "FARORES_WIND") ||
           contains(item, "NAYRUS_LOVE") || contains(model, "NAYRUS_LOVE") ||
           contains(item, "MEDALLION") || contains(model, "MEDALLION") ||
           contains(item, "SPIRITUAL_STONE") || contains(model, "SPIRITUAL_STONE") ||
           contains(item, "EMERALD") || contains(model, "EMERALD") ||
           contains(item, "RUBY") || contains(model, "RUBY") ||
           contains(item, "SAPPHIRE") || contains(model, "SAPPHIRE") ||
           contains(item, "SONG") || contains(model, "SONG") ||
           contains(item, "MINUET") || contains(model, "MINUET") ||
           contains(item, "BOLERO") || contains(model, "BOLERO") ||
           contains(item, "SERENADE") || contains(model, "SERENADE") ||
           contains(item, "REQUIEM") || contains(model, "REQUIEM") ||
           contains(item, "NOCTURNE") || contains(model, "NOCTURNE") ||
           contains(item, "PRELUDE") || contains(model, "PRELUDE") ||
           contains(item, "STONE_OF_AGONY") || contains(model, "STONE_OF_AGONY") ||
           contains(item, "GERUDO_MEMBERSHIP_CARD") || contains(model, "GERUDO_MEMBERSHIP_CARD") ||
           contains(item, "ROCS_FEATHER") || contains(model, "ROCS_FEATHER") ||
           contains(item, "FISHING_POLE") || contains(model, "FISHING_POLE") ||
           contains(item, "SKELETON_KEY") || contains(model, "SKELETON_KEY") ||
           contains(item, "TRIFORCE") || contains(model, "TRIFORCE");
}

bool ShouldShowForeignOotGetItemCutscene(const SharedPlacement& placement, RandoItemId visualItem) {
    if (IsMajorForeignOotPlacement(placement)) {
        return CVarGetInteger("gEnhancements.Cutscenes.SkipGetItemCutscenes", 0) < 3;
    }

    return Rando::StaticData::ShouldShowGetItemCutscene(visualItem);
}

void EnforceOoTxMmRandoSettings() {
    CVarSetInteger(Rando::StaticData::Options[RO_SHUFFLE_SWIM].cvar, RO_GENERIC_OFF);
    CVarSetInteger(Rando::StaticData::Options[RO_SHUFFLE_SONG_DOUBLE_TIME].cvar, RO_GENERIC_OFF);
    CVarSetInteger(Rando::StaticData::Options[RO_SHUFFLE_SONG_INVERTED_TIME].cvar, RO_GENERIC_OFF);
    CVarSetInteger(Rando::StaticData::Options[RO_PLENTIFUL_ITEMS].cvar, RO_GENERIC_OFF);
    CVarSetInteger(Rando::StaticData::Options[RO_SHUFFLE_FROGS].cvar, RO_GENERIC_OFF);
    CVarSetInteger(Rando::StaticData::Options[RO_SHUFFLE_TRIFORCE_PIECES].cvar, RO_GENERIC_OFF);
    CVarSetInteger(Rando::StaticData::Options[RO_SHUFFLE_TREE_DROPS].cvar, RO_GENERIC_OFF);
    CVarSetInteger(Rando::StaticData::Options[RO_ACCESS_DUNGEONS].cvar, RO_ACCESS_DUNGEONS_FORM_AND_SONG);
    if (IS_RANDO) {
        RANDO_SAVE_OPTIONS[RO_SHUFFLE_ENEMY_SOULS] =
            CVarGetInteger("gRandomizer.Combo.Shuffles.MM.EnemySouls", 0) != 0 ? RO_GENERIC_YES : RO_GENERIC_NO;
        if (CVarGetInteger("gRandomizer.Combo.Shuffles.OcarinaButtons", 0) != 0) {
            RANDO_SAVE_OPTIONS[RO_SHUFFLE_OCARINA_BUTTONS] = RO_GENERIC_YES;
        }
        RANDO_SAVE_OPTIONS[RO_ACCESS_DUNGEONS] = RO_ACCESS_DUNGEONS_FORM_AND_SONG;
    }
    CVarSetInteger("gEnhancements.Cutscenes.SkipGetItemCutscenes", 2);
}

RandoItemId ResolveMmVisualPlaceholderForOotItem(const std::string& rawItem) {
    const std::string item = StripItemPrefix(rawItem);
    std::string upperItem = item;
    std::transform(upperItem.begin(), upperItem.end(), upperItem.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    const bool isShopItem = upperItem.rfind("BUY ", 0) == 0 || upperItem.rfind("SOLD ", 0) == 0 ||
                            upperItem.find(" SHOP ") != std::string::npos;

    if (upperItem.find("PIECE OF HEART") != std::string::npos || item == "RG_PIECE_OF_HEART") {
        return RI_HEART_PIECE;
    }
    if (upperItem.find("HEART CONTAINER") != std::string::npos || item == "RG_HEART_CONTAINER") {
        return RI_HEART_CONTAINER;
    }
    if (upperItem.find("BOSS KEY") != std::string::npos || item.find("BOSS_KEY") != std::string::npos) {
        return RI_WOODFALL_BOSS_KEY;
    }
    if (upperItem.find("SMALL KEY") != std::string::npos || upperItem.find("KEY RING") != std::string::npos ||
        item.find("SMALL_KEY") != std::string::npos || item.find("KEY_RING") != std::string::npos) {
        return RI_WOODFALL_SMALL_KEY;
    }
    if (upperItem.find("SKULLTULA TOKEN") != std::string::npos || item.find("SKULLTULA_TOKEN") != std::string::npos) {
        return RI_GS_TOKEN_SWAMP;
    }
    if (upperItem.find("BUNNY HOOD") != std::string::npos || item.find("BUNNY_HOOD") != std::string::npos) {
        return RI_MASK_BUNNY;
    }
    if (upperItem.find("GORON MASK") != std::string::npos || item.find("GORON_MASK") != std::string::npos) {
        return RI_MASK_GORON;
    }
    if (upperItem.find("ZORA MASK") != std::string::npos || item.find("ZORA_MASK") != std::string::npos) {
        return RI_MASK_ZORA;
    }
    if (upperItem.find("KEATON MASK") != std::string::npos || item.find("KEATON_MASK") != std::string::npos) {
        return RI_MASK_KEATON;
    }
    if (upperItem.find("MASK") != std::string::npos || item.find("_MASK") != std::string::npos) {
        return RI_MASK_TRUTH;
    }

    if (upperItem.find("ARROWS (5)") != std::string::npos || item == "RG_ARROWS_5") {
        return RI_ARROWS_10;
    }
    if (upperItem.find("ARROWS (10)") != std::string::npos || item == "RG_ARROWS_10") {
        return RI_ARROWS_10;
    }
    if (upperItem.find("ARROWS (30)") != std::string::npos || item == "RG_ARROWS_30") {
        return RI_ARROWS_30;
    }
    if (upperItem.find("BOMBS (5)") != std::string::npos || item == "RG_BOMBS_5") {
        return RI_BOMBS_5;
    }
    if (upperItem.find("BOMBS (10)") != std::string::npos || item == "RG_BOMBS_10") {
        return RI_BOMBS_10;
    }
    if (upperItem.find("BOMBS (20)") != std::string::npos || item == "RG_BOMBS_20") {
        return RI_BOMBS_10;
    }
    if (upperItem.find("BOMBCHU") != std::string::npos || item.find("BOMBCHU") != std::string::npos) {
        return RI_BOMBCHU;
    }
    if (upperItem.find("BOMB BAG") != std::string::npos || item.find("BOMB_BAG") != std::string::npos) {
        return RI_BOMB_BAG_20;
    }
    if (upperItem.find("BOW") != std::string::npos || item == "RG_BOW") {
        return RI_HEART_PIECE;
    }
    if (upperItem.find("HOOKSHOT") != std::string::npos || item.find("HOOKSHOT") != std::string::npos ||
        upperItem.find("LONGSHOT") != std::string::npos || item == "RG_LONGSHOT") {
        return RI_HOOKSHOT;
    }
    if (upperItem.find("OCARINA") != std::string::npos || item.find("OCARINA") != std::string::npos) {
        return RI_OCARINA;
    }
    if (upperItem.find("LENS") != std::string::npos || item == "RG_LENS_OF_TRUTH") {
        return RI_LENS;
    }
    if (upperItem.find("MAGIC BEAN") != std::string::npos || item.find("MAGIC_BEAN") != std::string::npos) {
        return RI_MAGIC_BEAN;
    }
    if (upperItem.find("MAGIC METER") != std::string::npos || item.find("MAGIC_SINGLE") != std::string::npos ||
        item.find("PROGRESSIVE_MAGIC") != std::string::npos) {
        return RI_SINGLE_MAGIC;
    }
    if (upperItem.find("DOUBLE MAGIC") != std::string::npos || upperItem.find("ENHANCED MAGIC") != std::string::npos ||
        item.find("MAGIC_DOUBLE") != std::string::npos) {
        return RI_DOUBLE_MAGIC;
    }
    if (upperItem.find("DOUBLE DEFENSE") != std::string::npos || item.find("DOUBLE_DEFENSE") != std::string::npos) {
        return RI_DOUBLE_DEFENSE;
    }
    if (upperItem.find("KOKIRI SWORD") != std::string::npos || item.find("KOKIRI_SWORD") != std::string::npos) {
        return RI_SWORD_KOKIRI;
    }
    if (upperItem.find("MIRROR SHIELD") != std::string::npos || item.find("MIRROR_SHIELD") != std::string::npos) {
        return RI_HEART_PIECE;
    }
    if (upperItem.find("DEKU SHIELD") != std::string::npos || item.find("DEKU_SHIELD") != std::string::npos) {
        return RI_SHIELD_MIRROR;
    }
    if (upperItem.find("HYLIAN SHIELD") != std::string::npos || item.find("HYLIAN_SHIELD") != std::string::npos) {
        return RI_HEART_PIECE;
    }
    if (upperItem.find("SHIELD") != std::string::npos || item.find("_SHIELD") != std::string::npos) {
        return RI_SHIELD_MIRROR;
    }
    if (upperItem.find("SONG OF TIME") != std::string::npos || item == "RG_SONG_OF_TIME") {
        return RI_SONG_TIME;
    }
    if (upperItem.find("EPONA") != std::string::npos || item == "RG_EPONAS_SONG") {
        return RI_SONG_EPONA;
    }
    if (upperItem.find("SARIA") != std::string::npos || item == "RG_SARIAS_SONG") {
        return RI_SONG_SARIA;
    }
    if (upperItem.find("SUN'S SONG") != std::string::npos || upperItem.find("SUNS SONG") != std::string::npos ||
        item == "RG_SUNS_SONG") {
        return RI_SONG_SUN;
    }
    if (upperItem.find("SONG OF STORMS") != std::string::npos || item == "RG_SONG_OF_STORMS") {
        return RI_SONG_STORMS;
    }
    if (upperItem.find("DEKU NUT") != std::string::npos || item.find("DEKU_NUT") != std::string::npos) {
        return RI_DEKU_NUTS_5;
    }
    if (upperItem.find("DEKU STICK") != std::string::npos || item.find("DEKU_STICK") != std::string::npos) {
        return RI_DEKU_STICK;
    }
    if (upperItem.find("DEKU SEEDS") != std::string::npos || item.find("DEKU_SEEDS") != std::string::npos) {
        return RI_JUNK;
    }
    if (upperItem.find("BLUE RUPEE") != std::string::npos || item == "RG_BLUE_RUPEE") {
        return RI_RUPEE_BLUE;
    }
    if (upperItem.find("RED RUPEE") != std::string::npos || item == "RG_RED_RUPEE") {
        return RI_RUPEE_RED;
    }
    if (upperItem.find("PURPLE RUPEE") != std::string::npos || item == "RG_PURPLE_RUPEE") {
        return RI_RUPEE_PURPLE;
    }
    if (upperItem.find("HUGE RUPEE") != std::string::npos || item == "RG_HUGE_RUPEE") {
        return RI_RUPEE_HUGE;
    }
    if (upperItem.find("GREEN RUPEE") != std::string::npos || item == "RG_GREEN_RUPEE") {
        return RI_RUPEE_GREEN;
    }
    if (upperItem.find("BOTTLE WITH BLUE POTION") != std::string::npos || item == "RG_BOTTLE_WITH_BLUE_POTION") {
        return RI_BOTTLE_EMPTY;
    }
    if (upperItem.find("BOTTLE WITH FISH") != std::string::npos || item == "RG_FISH") {
        return RI_BOTTLE_EMPTY;
    }
    if (upperItem.find("BOTTLE WITH GREEN POTION") != std::string::npos ||
        item == "RG_BOTTLE_WITH_GREEN_POTION") {
        return RI_GREEN_POTION_REFILL;
    }
    if (upperItem.find("POE") != std::string::npos) {
        return RI_BOTTLE_EMPTY;
    }
    if (upperItem.find("ICE TRAP") != std::string::npos || item == "RG_ICE_TRAP") {
        return RI_TRAP;
    }
    if (upperItem.find("RECOVERY HEART") != std::string::npos || item == "RG_RECOVERY_HEART" ||
        item == "RG_MILK" || item == "RG_FISH") {
        return RI_JUNK;
    }

    if (isShopItem) {
        return RI_JUNK;
    }

    return RI_UNKNOWN;
}

RandoItemId ResolveMmVisualPlaceholderForModel(const std::string& modelId) {
    if (modelId == "xshared.ocarina_button.a") {
        return RI_OCARINA_BUTTON_A;
    }
    if (modelId == "xshared.ocarina_button.c_up") {
        return RI_OCARINA_BUTTON_C_UP;
    }
    if (modelId == "xshared.ocarina_button.c_left") {
        return RI_OCARINA_BUTTON_C_LEFT;
    }
    if (modelId == "xshared.ocarina_button.c_down") {
        return RI_OCARINA_BUTTON_C_DOWN;
    }
    if (modelId == "xshared.ocarina_button.c_right") {
        return RI_OCARINA_BUTTON_C_RIGHT;
    }
    if (modelId == "xoot.ice_trap") {
        return RI_TRAP;
    }
    if (modelId == "xoot.deku_shield" || modelId == "xoot.buy_deku_shield") {
        return RI_SHIELD_MIRROR;
    }
    if (modelId == "xoot.hylian_shield" || modelId == "xoot.unique.hylian_shield" ||
        modelId == "xoot.buy_hylian_shield" || modelId == "xoot.unique.buy_hylian_shield") {
        return RI_SHIELD_HERO;
    }
    if (modelId == "xoot.mirror_shield" || modelId == "xoot.unique.mirror_shield") {
        return RI_HEART_PIECE;
    }
    if (modelId == "xoot.progressive_bow" || modelId == "xoot.unique.progressive_bow" ||
        modelId == "xoot.rg_fairy_bow" || modelId == "xoot.fairy_bow" || modelId == "xoot.bow") {
        return RI_HEART_PIECE;
    }
    if (modelId == "xoot.hookshot" || modelId == "xoot.unique.hookshot" || modelId == "xoot.progressive_hookshot" ||
        modelId == "xoot.longshot" || modelId == "xoot.unique.longshot") {
        return RI_HOOKSHOT;
    }
    if (modelId == "xoot.progressive_bomb_bag" || modelId == "xoot.bomb_bag") {
        return RI_BOMB_BAG_20;
    }
    if (modelId == "xoot.rocs_feather" || modelId == "xoot.fishing_pole") {
        return RI_HEART_PIECE;
    }
    if (modelId == "xoot.progressive_slingshot") {
        return RI_HEART_PIECE;
    }
    if (modelId == "xoot.progressive_wallet" || modelId == "xoot.wallet") {
        return RI_WALLET_ADULT;
    }
    if (modelId == "xoot.strength.progressive_strength" || modelId == "xoot.strength_upgrade") {
        return RI_HEART_PIECE;
    }
    if (modelId == "xoot.progressive_nut_capacity" || modelId == "xoot.progressive_stick_capacity") {
        return RI_HEART_PIECE;
    }
    if (modelId == "xoot.skeleton_key" || modelId.rfind("xoot.key.small", 0) == 0 ||
        modelId.find("key_ring") != std::string::npos) {
        return RI_WOODFALL_SMALL_KEY;
    }
    if (modelId.find("boss_key") != std::string::npos) {
        return RI_WOODFALL_BOSS_KEY;
    }
    if (modelId.rfind("xoot.strength.", 0) == 0 || modelId.rfind("xoot.scale.", 0) == 0 ||
        modelId == "xoot.scale.progressive_scale" || modelId == "xoot.stone_of_agony" ||
        modelId == "xoot.gerudo_membership_card") {
        return RI_HEART_PIECE;
    }
    if (modelId == "xoot.lens_of_truth" || modelId == "xoot.rg_lens_of_truth" || modelId == "xoot.lens") {
        return RI_LENS;
    }
    if (modelId == "xoot.ocarina_of_time" || modelId == "xoot.fairy_ocarina" || modelId == "xoot.rg_ocarina_of_time" ||
        modelId == "xoot.rg_fairy_ocarina") {
        return RI_OCARINA;
    }
    if (modelId == "xoot.kokiri_sword" || modelId == "xoot.rg_kokiri_sword") {
        return RI_SWORD_KOKIRI;
    }
    if (modelId == "xoot.master_sword" || modelId == "xoot.rg_master_sword") {
        return RI_SWORD_KOKIRI;
    }
    if (modelId == "xoot.kokiris_emerald" || modelId == "xoot.gorons_ruby" || modelId == "xoot.zoras_sapphire") {
        return RI_MOONS_TEAR;
    }
    if (modelId.rfind("xoot.medallion.", 0) == 0 || modelId.rfind("xoot.spiritual_stone.", 0) == 0 ||
        modelId == "xoot.nocturne_of_shadow" || modelId == "xoot.minuet_of_forest" ||
        modelId == "xoot.bolero_of_fire" || modelId == "xoot.serenade_of_water" ||
        modelId == "xoot.requiem_of_spirit" || modelId == "xoot.prelude_of_light" ||
        modelId == "xoot.zeldas_lullaby" || modelId == "xoot.nayrus_love" ||
        modelId == "xoot.farores_wind" || modelId == "xoot.dins_fire") {
        return RI_HEART_PIECE;
    }
    if (modelId.rfind("xoot.tunic.", 0) == 0) {
        return RI_HEART_PIECE;
    }
    if (modelId.find("_soul") != std::string::npos || modelId.find(".soul") != std::string::npos) {
        return RI_SOUL_BOSS_MAJORA;
    }
    if (modelId == "xoot.gold_skulltula_token") {
        return RI_GS_TOKEN_SWAMP;
    }
    if (modelId == "xshared.heart_piece") {
        return RI_HEART_PIECE;
    }
    if (modelId == "xshared.heart_container" || modelId == "xshared.double_defense") {
        return RI_HEART_CONTAINER;
    }
    if (modelId == "xshared.recovery_heart") {
        return RI_RECOVERY_HEART;
    }
    if (modelId == "xshared.rupee") {
        return RI_RUPEE_GREEN;
    }
    if (modelId == "xshared.arrows") {
        return RI_ARROWS_30;
    }
    if (modelId == "xshared.arrows.fire" || modelId == "xshared.arrows.ice" || modelId == "xshared.arrows.light") {
        return RI_ARROWS_30;
    }
    if (modelId == "xshared.bombs") {
        return RI_BOMBS_10;
    }
    if (modelId == "xshared.bombchu") {
        return RI_BOMBCHU;
    }
    if (modelId == "xshared.magic_jar_small") {
        return RI_MAGIC_JAR_SMALL;
    }
    if (modelId == "xshared.magic_jar_large") {
        return RI_MAGIC_JAR_BIG;
    }
    if (modelId == "xshared.deku_seeds") {
        return RI_JUNK;
    }
    if (modelId == "xshared.deku_nuts") {
        return RI_DEKU_NUTS_5;
    }
    if (modelId == "xshared.deku_sticks") {
        return RI_DEKU_STICK;
    }
    if (modelId == "xshared.bomb_bag") {
        return RI_BOMB_BAG_20;
    }
    if (modelId == "xshared.magic_beans") {
        return RI_MAGIC_BEAN;
    }
    if (modelId == "xshared.bottle.empty") {
        return RI_BOTTLE_EMPTY;
    }
    if (modelId == "xshared.bottle.fairy") {
        return RI_FAIRY_REFILL;
    }
    if (modelId == "xshared.bottle.fish" || modelId == "xshared.bottle.bug" ||
        modelId == "xshared.bottle.small_poe" || modelId == "xshared.bottle.big_poe") {
        return RI_BOTTLE_EMPTY;
    }
    if (modelId == "xshared.bottle.milk" || modelId == "xshared.bottle.half_milk") {
        return RI_BOTTLE_MILK;
    }
    if (modelId == "xshared.bottle.red_potion") {
        return RI_BOTTLE_RED_POTION;
    }
    if (modelId == "xshared.bottle.blue_potion") {
        return RI_BLUE_POTION_REFILL;
    }
    if (modelId == "xshared.mask.goron") {
        return RI_MASK_GORON;
    }
    if (modelId == "xshared.mask.zora") {
        return RI_MASK_ZORA;
    }
    if (modelId == "xshared.mask.keaton") {
        return RI_MASK_KEATON;
    }
    if (modelId == "xshared.mask.truth") {
        return RI_MASK_TRUTH;
    }
    if (modelId == "xshared.mask.bunny") {
        return RI_MASK_BUNNY;
    }
    if (modelId == "xshared.ocarina_of_time") {
        return RI_OCARINA;
    }
    if (modelId == "xshared.song.time") {
        return RI_SONG_TIME;
    }
    if (modelId == "xshared.song.sun") {
        return RI_SONG_SUN;
    }
    if (modelId == "xshared.song.saria") {
        return RI_SONG_SARIA;
    }
    if (modelId == "xshared.song.epona") {
        return RI_SONG_EPONA;
    }
    if (modelId == "xshared.song.storms") {
        return RI_SONG_STORMS;
    }
    if (modelId == "xshared.lens") {
        return RI_LENS;
    }
    if (modelId == "xshared.wallet") {
        return RI_WALLET_ADULT;
    }

    return RI_UNKNOWN;
}

RandoItemId ResolveMmVisualPlaceholderForPlacement(const SharedPlacement& placement) {
    const RandoItemId modelPlaceholder = ResolveMmVisualPlaceholderForModel(placement.model);
    if (modelPlaceholder != RI_UNKNOWN) {
        return modelPlaceholder;
    }

    return ResolveMmVisualPlaceholderForOotItem(placement.item);
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
        { "POSTMAN_MASK", "Postman's Hat" },
        { "MASK_POSTMAN", "Postman's Hat" },
        { "SONG_OF_ELEGY", "Elegy of Emptiness" },
        { "SONG_ELEGY", "Elegy of Emptiness" },
        { "SHIELD_MIRROR", "Mirror Shield" },
        { "MIRROR_SHIELD", "Mirror Shield" },
        { "SONG_OF_OATH", "Oath to Order" },
        { "SONG_OATH", "Oath to Order" },
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
        { "sourceGame", "mm" },
        { "sourceCheck", placement.check },
        { "applied", false },
    });
    WriteInventory(inventory);

    SPDLOG_INFO("[OoTxMM] Queued {} grant from {}: {}", targetGame, placement.check, placement.item);
}

void ApplySharedPlacementsToNativeRando() {
    if (sSharedPlacementsAppliedToNativeRando || !IS_RANDO) {
        return;
    }

    sSharedPlacementsAppliedToNativeRando = true;
    LoadSharedPlacements();

    size_t local = 0;
    size_t crossGame = 0;
    size_t unknown = 0;
    for (const auto& [nativeCheck, placement] : sSharedPlacementsByNativeCheck) {
        const RandoCheckId randoCheckId = Rando::StaticData::GetCheckIdFromName(nativeCheck.c_str());
        if (randoCheckId == RC_UNKNOWN) {
            unknown++;
            continue;
        }

        auto& saveCheck = RANDO_SAVE_CHECKS[randoCheckId];
        if (placement.itemSourceGame == "mm") {
            const RandoItemId itemId = Rando::StaticData::GetItemIdFromName(StripItemPrefix(placement.item).c_str());
            if (itemId == RI_UNKNOWN) {
                unknown++;
                continue;
            }

            saveCheck.randoItemId = itemId;
            local++;
        } else if (placement.itemSourceGame == "oot") {
            saveCheck.randoItemId = ResolveMmVisualPlaceholderForPlacement(placement);
            crossGame++;
        }
    }

    SPDLOG_INFO("[OoTxMM] 2Ship applied shared placements to native rando ({} local, {} cross-game, {} unknown)", local,
                crossGame, unknown);
}

void ApplyPendingMmGrants() {
    if (!IS_RANDO) {
        return;
    }

    auto inventory = ReadInventory();
    bool changed = false;
    size_t applied = 0;
    const int saveSlot = GetCurrentSaveSlot();

    for (auto& grant : inventory["grants"]) {
        if (!grant.is_object() || grant.value("applied", false) || grant.value("targetGame", "") != "mm" ||
            grant.value("saveSlot", -1) != saveSlot) {
            continue;
        }

        const auto item = grant.value("item", "");
        if (IsIceTrapItem(item)) {
            grant["applied"] = true;
            changed = true;
            SPDLOG_INFO("[OoTxMM] Discarded pending MM ice trap grant {}", item);
            continue;
        }

        const RandoItemId itemId = Rando::StaticData::GetItemIdFromName(StripItemPrefix(item).c_str());
        if (itemId == RI_UNKNOWN) {
            SPDLOG_WARN("[OoTxMM] Could not apply unknown MM grant item {}", item);
            continue;
        }

        Rando::GiveItem(Rando::ConvertItem(itemId));
        grant["applied"] = true;
        changed = true;
        applied++;
        SPDLOG_INFO("[OoTxMM] Applied pending MM grant {}", item);
    }

    if (changed) {
        WriteInventory(inventory);
        SaveCurrentGameBeforeSwitch();
    }

    if (applied > 0) {
        SPDLOG_INFO("[OoTxMM] Applied {} pending MM grants", applied);
    }
}

void QueueCollectedSharedMmSharedItemGrantsForOot() {
    if (!IS_RANDO) {
        return;
    }

    size_t queued = 0;
    for (const auto& [nativeCheck, placement] : sSharedPlacementsByNativeCheck) {
        if (placement.itemSourceGame != "mm" || !ShouldGrantOotForMmSharedItem(placement.item)) {
            continue;
        }

        const RandoCheckId randoCheckId = Rando::StaticData::GetCheckIdFromName(nativeCheck.c_str());
        if (randoCheckId == RC_UNKNOWN) {
            continue;
        }

        const auto& saveCheck = RANDO_SAVE_CHECKS[randoCheckId];
        if (!saveCheck.obtained && !saveCheck.cycleObtained) {
            continue;
        }

        QueueForeignGrant(placement, "oot");
        queued++;
    }

    if (queued > 0) {
        SPDLOG_INFO("[OoTxMM] Queued {} collected shared MM item grants for OoT", queued);
    }
}

bool HandleQueuedSharedCheck(RandoCheckId randoCheckId) {
    if (!IS_RANDO || randoCheckId == RC_UNKNOWN) {
        return false;
    }

    const auto staticCheck = Rando::StaticData::Checks.find(randoCheckId);
    if (staticCheck == Rando::StaticData::Checks.end()) {
        return false;
    }

    auto placement = GetSharedPlacementByNativeCheck(staticCheck->second.name);
    if (!placement.has_value() || placement->itemSourceGame != "oot") {
        return false;
    }

    auto& saveCheck = RANDO_SAVE_CHECKS[randoCheckId];
    saveCheck.eligible = false;
    const RandoItemId visualItem = Rando::ConvertItem(saveCheck.randoItemId, randoCheckId);

    GameInteractor::Instance->events.emplace_back(GIEventGiveItem{
        .showGetItemCutscene = ShouldShowForeignOotGetItemCutscene(*placement, visualItem),
        .param = static_cast<int16_t>(randoCheckId),
        .giveItem =
            [](Actor* actor, PlayState* play) {
                const RandoCheckId queuedCheck = static_cast<RandoCheckId>(CUSTOM_ITEM_PARAM);
                const auto staticCheck = Rando::StaticData::Checks.find(queuedCheck);
                if (staticCheck == Rando::StaticData::Checks.end()) {
                    return;
                }

                auto placement = GetSharedPlacementByNativeCheck(staticCheck->second.name);
                if (!placement.has_value() || placement->itemSourceGame != "oot") {
                    return;
                }

                auto& saveCheck = RANDO_SAVE_CHECKS[queuedCheck];
                RandoItemId visualItem = Rando::ConvertItem(saveCheck.randoItemId, queuedCheck);
                if (IsIceTrapPlacement(*placement)) {
                    std::string message = GetTrapMessage();
                    if (CVarGetInteger("gEnhancements.Cutscenes.SkipGetItemCutscenes", 0) >= 2) {
                        message = CustomMessage::RemoveColorCodes(message);
                    }

                    CustomMessage::Entry entry = {
                        .textboxType = 2,
                        .icon = Rando::StaticData::GetIconForZMessage(RI_TRAP),
                        .msg = message,
                    };

                    if (CUSTOM_ITEM_FLAGS & CustomItem::GIVE_ITEM_CUTSCENE) {
                        CustomMessage::SetActiveCustomMessage(entry.msg, entry);
                    } else if (Rando::StaticData::ShouldShowGetItemCutscene(RI_TRAP)) {
                        CustomMessage::StartTextbox(entry.msg + "\x1C\x02\x10", entry);
                    } else {
                        Notification::Emit({
                            .itemIcon = Rando::StaticData::GetIconTexturePath(RI_TRAP),
                            .message = "",
                            .suffix = message,
                        });
                    }

                    Rando::GiveItem(RI_TRAP);
                    saveCheck.cycleObtained = true;
                    saveCheck.obtained = true;
                    saveCheck.eligible = false;
                    CUSTOM_ITEM_PARAM = queuedCheck;

                    SPDLOG_INFO("[OoTxMM] Completed MM check {} as local MM ice trap", placement->check);
                    return;
                }

                const std::string itemName = HumanizeItemNameWithSource(*placement);
                CustomMessage::Entry entry = {
                    .textboxType = 2,
                    .icon = Rando::StaticData::GetIconForZMessage(visualItem),
                    .msg = "You found " + itemName + "!",
                };

                if (CUSTOM_ITEM_FLAGS & CustomItem::GIVE_ITEM_CUTSCENE) {
                    CustomMessage::SetActiveCustomMessage(entry.msg, entry);
                } else if (ShouldShowForeignOotGetItemCutscene(*placement, visualItem)) {
                    CustomMessage::StartTextbox(entry.msg + "\x1C\x02\x10", entry);
                } else {
                    Notification::Emit({
                        .itemIcon = Rando::StaticData::GetIconTexturePath(visualItem),
                        .message = "You found",
                        .suffix = itemName,
                    });
                }

                if (IsOcarinaButtonItem(*placement)) {
                    Rando::GiveItem(Rando::ConvertItem(saveCheck.randoItemId));
                }

                QueueForeignGrant(*placement, "oot");
                saveCheck.cycleObtained = true;
                saveCheck.obtained = true;
                saveCheck.eligible = false;
                CUSTOM_ITEM_PARAM = queuedCheck;

                SPDLOG_INFO("[OoTxMM] Completed MM check {} as cross-game OoT grant {}", placement->check,
                            placement->item);
            },
        .drawItem =
            [](Actor* actor, PlayState* play) {
                const RandoCheckId queuedCheck = static_cast<RandoCheckId>(CUSTOM_ITEM_PARAM);
                auto& saveCheck = RANDO_SAVE_CHECKS[queuedCheck];
                const RandoItemId visualItem = Rando::ConvertItem(saveCheck.randoItemId, queuedCheck);

                Matrix_Scale(30.0f, 30.0f, 30.0f, MTXMODE_APPLY);
                Rando::DrawItem(visualItem, queuedCheck, actor);
            } });

    return true;
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

std::optional<std::string> GetOoTxMmSeedString() {
    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return std::nullopt;
    }

    const auto seedPath = *root / kSeedFileName;
    if (!std::filesystem::exists(seedPath)) {
        return std::nullopt;
    }

    try {
        const auto seedManifest = ReadJson(seedPath);
        if (seedManifest.value("mode", "") != "OoTxMM") {
            return std::nullopt;
        }

        const auto seedString = seedManifest.value("twoShipSeedString", "");
        if (seedString.empty()) {
            return std::nullopt;
        }

        return seedString;
    } catch (...) {
        return std::nullopt;
    }
}

int GetCurrentSaveSlot() {
    if (gSaveContext.fileNum >= 0 && gSaveContext.fileNum <= 2) {
        return gSaveContext.fileNum;
    }

    return 0;
}

std::filesystem::path GetMmSavePath(int saveSlot) {
    return Ship::Context::GetPathRelativeToAppDirectory("saves/" + SaveManager_GetFileName(saveSlot + 1), appShortName);
}

bool MmSaveSlotExists(int saveSlot) {
    if (saveSlot < 0 || saveSlot > 2) {
        return false;
    }

    return std::filesystem::exists(GetMmSavePath(saveSlot));
}

bool MmSaveSlotHasOwlSave(int saveSlot) {
    if (!MmSaveSlotExists(saveSlot)) {
        return false;
    }

    try {
        return ReadJson(GetMmSavePath(saveSlot)).contains("owlSave");
    } catch (...) {
        return false;
    }
}

void WriteCurrentNewCycleSave(int fileNum) {
    if (fileNum < 0 || fileNum > 2) {
        return;
    }

    gSaveContext.save.saveInfo.playerData.newf[0] = 'Z';
    gSaveContext.save.saveInfo.playerData.newf[1] = 'E';
    gSaveContext.save.saveInfo.playerData.newf[2] = 'L';
    gSaveContext.save.saveInfo.playerData.newf[3] = 'D';
    gSaveContext.save.saveInfo.playerData.newf[4] = 'A';
    gSaveContext.save.saveInfo.playerData.newf[5] = '3';

    SPDLOG_INFO("[OoTxMM] Writing MM new-cycle save slot {} to {}", fileNum + 1, GetMmSavePath(fileNum).string());
    gSaveContext.save.saveInfo.checksum = 0;
    gSaveContext.save.saveInfo.checksum = Sram_CalcChecksum(&gSaveContext.save, sizeof(Save));
    SysFlashrom_WriteDataSync(&gSaveContext.save, gFlashSaveStartPages[fileNum * FLASH_SAVE_MAIN_MULTIPLIER],
                              gFlashSpecialSaveNumPages[fileNum * FLASH_SAVE_MAIN_MULTIPLIER]);
    SaveManager_ThreadPoolWait();
    SPDLOG_INFO("[OoTxMM] MM new-cycle save slot {} exists after write: {}", fileNum + 1,
                std::filesystem::exists(GetMmSavePath(fileNum)));
}

void SaveCurrentGameBeforeSwitch() {
    if (gSaveContext.fileNum < 0 || gSaveContext.fileNum > 2) {
        SaveManager_ThreadPoolWait();
        return;
    }

    if (gPlayState == nullptr || !SavingEnhancements_CanSave()) {
        WriteCurrentNewCycleSave(gSaveContext.fileNum);
        return;
    }

    const bool currentOwlSaveState = gSaveContext.save.isOwlSave;
    gSaveContext.save.isOwlSave = true;
    SavingEnhancements_PersistSaveEntranceInfo();
    SavingEnhancements_AdvancePlaytime();
    Play_SaveCycleSceneFlags(gPlayState);
    gSaveContext.save.saveInfo.playerData.savedSceneId = gPlayState->sceneId;
    func_8014546C(&gPlayState->sramCtx);
    SysFlashrom_WriteDataSync(gPlayState->sramCtx.saveBuf,
                              gFlashOwlSaveStartPages[gSaveContext.fileNum * FLASH_SAVE_MAIN_MULTIPLIER],
                              gFlashOwlSaveNumPages[gSaveContext.fileNum * FLASH_SAVE_MAIN_MULTIPLIER]);
    gSaveContext.save.isOwlSave = currentOwlSaveState;
    SavingEnhancements_ClearSaveEntranceInfo();
    SaveManager_ThreadPoolWait();
}

bool RequestSwitchToOot(bool saveBeforeSwitch, const std::string& requestedEntrance = kOotTargetEntrance,
                        const std::string& returnEntrance = std::string()) {
    if (sSwitchRequested) {
        return true;
    }

    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return false;
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
    state["RequestedGame"] = "Oot";
    state["RequestedEntrance"] = requestedEntrance;
    state["ReturnEntrance"] = returnEntrance;
    if (returnEntrance.empty()) {
        state["OriginEntrance"] = nullptr;
    } else {
        state["OriginEntrance"] = TaggedEntrance("mm", gSaveContext.save.entrance);
    }
    state["SaveSlot"] = GetCurrentSaveSlot();
    state["CommandNonce"] = nonce;
    state["Status"] = "requested";
    state["LastError"] = nullptr;

    try {
        QueueCollectedSharedMmSharedItemGrantsForOot();
        if (saveBeforeSwitch) {
            SaveCurrentGameBeforeSwitch();
        } else {
            SaveManager_ThreadPoolWait();
        }
        WriteJsonAtomic(*root / kHandoffFileName, nlohmann::json{
            { "targetGame", "Oot" },
            { "targetEntrance", requestedEntrance },
            { "returnEntrance", returnEntrance },
            { "originEntrance", returnEntrance.empty() ? nlohmann::json(nullptr)
                                                       : nlohmann::json(TaggedEntrance("mm", gSaveContext.save.entrance)) },
            { "saveSlot", GetCurrentSaveSlot() },
            { "commandNonce", nonce },
        });
        WriteJsonAtomic(statePath, state);
        sSwitchRequested = true;
        Ship::Context::GetInstance()->GetWindow()->Close();
        return true;
    } catch (...) {
        return false;
    }
}

bool RequestResetToOotTitle() {
    return RequestSwitchToOot(true, kOotTitleScreenEntrance);
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
        if (handoff.value("targetGame", "") != kTargetGame &&
            handoff.value("targetGame", "") != "Mm") {
            return std::nullopt;
        }
        if (targetEntrance != kTargetEntrance && !ParseTaggedEntrance(targetEntrance, "mm:entrance:").has_value()) {
            return std::nullopt;
        }

        std::filesystem::remove(handoffPath);
        return handoff;
    } catch (...) {
        return std::nullopt;
    }
}

void BootToClockTownExit(const nlohmann::json& handoff) {
    const int saveSlot = handoff.value("saveSlot", 0);
    const int fileNum = (saveSlot >= 0 && saveSlot <= 2) ? saveSlot : 0;
    const bool loadExistingSave = MmSaveSlotExists(fileNum);

    SPDLOG_INFO("[OoTxMM] MM boot handoff saveSlot={} fileNum={} path={} existing={}", saveSlot, fileNum,
                GetMmSavePath(fileNum).string(), loadExistingSave);

    gSaveContext.seqId = NA_BGM_DISABLED;
    gSaveContext.ambienceId = AMBIENCE_ID_DISABLED;
    gSaveContext.gameMode = GAMEMODE_TITLE_SCREEN;

    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.fileNum = fileNum;
    if (loadExistingSave) {
        FileSelectState fileSelect = {};
        std::vector<u8> saveBuffer(SAVE_BUFFER_SIZE);
        fileSelect.buttonIndex = fileNum;
        fileSelect.sramCtx.saveBuf = saveBuffer.data();
        fileSelect.isOwlSave[fileNum + FILE_NUM_OWL_SAVE_OFFSET] = MmSaveSlotHasOwlSave(fileNum) ? 1 : 0;
        Sram_OpenSave(&fileSelect, &fileSelect.sramCtx);
    } else {
        Sram_InitNewSave();
        gSaveContext.sceneLayer = 0;
        gSaveContext.save.time = CLOCK_TIME(8, 0);
        gSaveContext.save.day = 1;
        gSaveContext.save.cutsceneIndex = 0;
        gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
        gSaveContext.save.linkAge = 0;

        for (size_t i = 0; i < ARRAY_COUNT(gSaveContext.eventInf); i++) {
            gSaveContext.eventInf[i] = 0;
        }

        for (int i = 0; i < ARRAY_COUNT(gSaveContext.cycleSceneFlags); i++) {
            gSaveContext.cycleSceneFlags[i].chest = gSaveContext.save.saveInfo.permanentSceneFlags[i].chest;
            gSaveContext.cycleSceneFlags[i].switch0 = gSaveContext.save.saveInfo.permanentSceneFlags[i].switch0;
            gSaveContext.cycleSceneFlags[i].switch1 = gSaveContext.save.saveInfo.permanentSceneFlags[i].switch1;
            gSaveContext.cycleSceneFlags[i].clearedRoom = gSaveContext.save.saveInfo.permanentSceneFlags[i].clearedRoom;
            gSaveContext.cycleSceneFlags[i].collectible = gSaveContext.save.saveInfo.permanentSceneFlags[i].collectible;
        }
    }

    gSaveContext.fileNum = fileNum;
    gSaveContext.sceneLayer = 0;
    gSaveContext.save.cutsceneIndex = 0;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    gSaveContext.save.linkAge = 0;
    const int32_t targetEntrance = ParseTaggedEntrance(handoff.value("targetEntrance", ""), "mm:entrance:")
                                       .value_or(ENTRANCE(SOUTH_CLOCK_TOWN, 0));
    sDungeonReturnEntrance = handoff.value("returnEntrance", "");
    if (sDungeonReturnEntrance.rfind("oot:entrance:", 0) == 0 && IsMmTempleEntrance(targetEntrance)) {
        sSuppressForeignMmDungeonOpenFlagsFrames = 120;
        SuppressForeignMmDungeonOpenFlags();
    }
    if (sDungeonReturnEntrance.empty()) {
        if (const auto origin = ResolveComboDungeonOriginForTarget("mm", targetEntrance);
            origin.has_value() && std::string(origin->game) == "oot") {
            sDungeonReturnEntrance = TaggedEntrance("oot", origin->returnEntrance);
            if (IsMmTempleEntrance(targetEntrance)) {
                sSuppressForeignMmDungeonOpenFlagsFrames = 120;
                SuppressForeignMmDungeonOpenFlags();
            }
        }
    }
    MapSelect_LoadGame(reinterpret_cast<MapSelectState*>(gGameState), targetEntrance, 0);

    if (!loadExistingSave) {
        const bool hasCrossoverSeed = GetOoTxMmSeedString().has_value();
        const int previousRandoEnabled = CVarGetInteger("gRando.Enabled", 0);
        const int previousSpoilerFileIndex = CVarGetInteger("gRando.SpoilerFileIndex", 0);
        if (hasCrossoverSeed) {
            CVarSetInteger("gRando.Enabled", 1);
            CVarSetInteger("gRando.SpoilerFileIndex", 0);
        }
        GameInteractor_ExecuteOnSaveInit(gSaveContext.fileNum);
        if (hasCrossoverSeed) {
            CVarSetInteger("gRando.Enabled", previousRandoEnabled);
            CVarSetInteger("gRando.SpoilerFileIndex", previousSpoilerFileIndex);
        }
    }
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    gSaveContext.save.entrance = targetEntrance;
    if (!loadExistingSave) {
        if (const auto root = FindCrossoverRoot(); root.has_value()) {
            RebuildPlacementManifest(*root);
            sSharedPlacementsLoaded = false;
            sSharedPlacementsAppliedToNativeRando = false;
            sGossipHintsLoaded = false;
        }
    }
    LoadSharedPlacements();
    ApplySharedPlacementsToNativeRando();
    ApplyPendingMmGrants();

    if (!loadExistingSave) {
        WriteCurrentNewCycleSave(fileNum);
    }
}

void RegisterHooks() {
    EnforceOoTxMmRandoSettings();

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnConsoleLogoUpdate>([]() {
        EnforceOoTxMmRandoSettings();
        if (sBootHandoffConsumed) {
            return;
        }

        auto handoff = ConsumeHandoff();
        if (!handoff.has_value()) {
            return;
        }

        sBootHandoffConsumed = true;
        BootToClockTownExit(*handoff);
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](int32_t fileNum) {
        (void)fileNum;
        EnforceOoTxMmRandoSettings();
        LoadSharedPlacements();
        ApplySharedPlacementsToNativeRando();
        ApplyPendingMmGrants();
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>([]() {
        if (sSwitchRequested || gPlayState == nullptr) {
            return;
        }

        if (sSuppressForeignMmDungeonOpenFlagsFrames > 0) {
            SuppressForeignMmDungeonOpenFlags();
            sSuppressForeignMmDungeonOpenFlagsFrames--;
        }
        if (sLastAppliedMmDungeonReturnFrames > 0) {
            if (gPlayState->transitionTrigger == TRANS_TRIGGER_OFF) {
                sLastAppliedMmDungeonReturnFrames--;
            }
            if (sLastAppliedMmDungeonReturnFrames == 0) {
                sLastAppliedMmDungeonReturnEntrance.clear();
            }
        }

        if (gPlayState->transitionTrigger != TRANS_TRIGGER_START) {
            sComboDungeonTransitionHandled = false;
        }

        if (gPlayState->transitionTrigger == TRANS_TRIGGER_START && !sComboDungeonTransitionHandled) {
            if (IsComboDungeonReturnExit("mm", gPlayState->nextEntrance) ||
                IsStoneTowerInverterTransition(gPlayState->nextEntrance) ||
                !sDungeonReturnEntrance.empty() || !sLastAppliedMmDungeonReturnEntrance.empty()) {
                WriteComboDungeonReturnEventDebug("game-state-transition-start", gPlayState->nextEntrance);
            }
            if (HandlePendingMmDungeonReturnTransition()) {
                return;
            }

            const int32_t comboEntrance = gPlayState->nextEntrance;
            const bool observedDungeonEntrance =
                FindComboDungeonEndpointByEntrance("mm", comboEntrance).has_value();
            auto origin = observedDungeonEntrance
                              ? FindMmComboDungeonEndpointByExteriorScene(gPlayState->sceneId, comboEntrance)
                              : std::optional<ComboDungeonEndpoint>(std::nullopt);
            if (!origin.has_value()) {
                origin = FindComboDungeonEndpointByEntrance("mm", comboEntrance);
            }

            if (origin.has_value()) {
                if (const auto target = ResolveComboDungeonTarget(*origin); target.has_value()) {
                    sComboDungeonTransitionHandled = true;
                    WriteComboDungeonTransitionDebug(*origin, *target, comboEntrance);
                    if (std::string(target->game) == "mm") {
                        sDungeonReturnEntrance = TaggedEntrance("mm", origin->returnEntrance);
                        StoreDungeonReturnEntranceInState(sDungeonReturnEntrance, TaggedEntrance("mm", target->entrance));
                        gPlayState->nextEntrance = target->entrance;
                    } else {
                        if (!FindComboDungeonEndpointByEntrance("oot", target->entrance).has_value()) {
                            return;
                        }
                        gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
                        RequestSwitchToOot(true, TaggedEntrance("oot", target->entrance),
                                           TaggedEntrance("mm", origin->returnEntrance));
                        return;
                    }
                }
            }

            if (gPlayState->nextEntrance == ENTRANCE(CLOCK_TOWER_INTERIOR, 1)) {
                sComboDungeonTransitionHandled = true;
                gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
                RequestSwitchToOot(true);
            }
        }
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](s8 sceneId, s8 spawnNum) {
        if (sceneId == SCENE_F40 || sceneId == SCENE_F41 || sceneId == SCENE_21MITURINMAE ||
            sceneId == SCENE_INISIE_R || sceneId == SCENE_INISIE_N) {
            WriteComboDungeonReturnEventDebug("scene-init", gSaveContext.save.entrance, spawnNum);
        }
        if (sSuppressForeignMmDungeonOpenFlagsFrames > 0) {
            SuppressForeignMmDungeonOpenFlags();
        }

        if ((sceneId == SCENE_F40 || sceneId == SCENE_F41) && !sSwitchRequested) {
            RestoreDungeonReturnEntranceFromState();
            if (!sDungeonReturnEntrance.empty()) {
                ApplyPendingDungeonReturn();
                return;
            }
        }

        if (sceneId == SCENE_INSIDETOWER && gSaveContext.save.entrance == ENTRANCE(CLOCK_TOWER_INTERIOR, 1)) {
            RequestSwitchToOot(false);
        }
    });

    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorUpdate>(
        ACTOR_PLAYER, [](Actor* actor) {
            (void)actor;
            if (gPlayState != nullptr && gPlayState->transitionTrigger == TRANS_TRIGGER_START &&
                (IsComboDungeonReturnExit("mm", gPlayState->nextEntrance) ||
                 IsStoneTowerInverterTransition(gPlayState->nextEntrance) ||
                 !sDungeonReturnEntrance.empty() || !sLastAppliedMmDungeonReturnEntrance.empty())) {
                WriteComboDungeonReturnEventDebug("player-update-transition", gPlayState->nextEntrance);
            }
            HandlePendingMmDungeonReturnTransition();
        });
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorUpdate>(
        ACTOR_OBJ_WTURN, [](Actor* actor) {
            (void)actor;
            ReapplyRecentMmDungeonReturn();
        });
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::ShouldActorUpdate>(
        ACTOR_OBJ_WTURN, [](Actor* actor, bool* should) {
            (void)actor;
            if (ShouldSuppressStoneTowerInverter()) {
                if (gPlayState != nullptr && gPlayState->transitionTrigger != TRANS_TRIGGER_START) {
                    WriteComboDungeonReturnEventDebug("obj-wturn-start-return",
                                                      gPlayState != nullptr ? gPlayState->nextEntrance : -1);
                    StartRecentMmDungeonReturnTransition();
                }
                *should = false;
            }
        });
}

} // namespace TwoShipCrossover

static RegisterShipInitFunc initCrossover(TwoShipCrossover::RegisterHooks);
