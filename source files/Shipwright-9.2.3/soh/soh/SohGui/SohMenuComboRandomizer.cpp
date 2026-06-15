#include "SohMenu.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/OTRGlobals.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

extern "C" {
#include "variables.h"
}

namespace SohGui {

using namespace UIWidgets;

namespace {

constexpr int32_t RO_GENERIC_OFF = 0;
constexpr int32_t RO_GENERIC_ON = 1;
constexpr int32_t RO_SHOPSANITY_OFF = 0;
constexpr int32_t RO_SHOPSANITY_SPECIFIC_COUNT = 1;
constexpr int32_t RO_SCRUBS_OFF = 0;
constexpr int32_t RO_SCRUBS_ALL = 2;
constexpr int32_t RO_MERCHANTS_OFF = 0;
constexpr int32_t RO_MERCHANTS_ALL = 3;
constexpr int32_t RO_BOSS_SOULS_OFF = 0;
constexpr int32_t RO_BOSS_SOULS_ON = 1;
constexpr int32_t RO_DUNGEON_ITEM_LOC_STARTWITH = 0;
constexpr int32_t RO_DUNGEON_ITEM_LOC_OWN_DUNGEON = 2;
constexpr int32_t RO_DUNGEON_ITEM_LOC_ANY_DUNGEON = 3;
constexpr int32_t RO_DUNGEON_ITEM_LOC_ANYWHERE = 5;
constexpr int32_t RO_DUNGEON_REWARDS_VANILLA = 0;
constexpr int32_t RO_DUNGEON_REWARDS_END_OF_DUNGEON = 1;
constexpr int32_t RO_DUNGEON_REWARDS_ANYWHERE = 4;
constexpr int32_t RO_KEYRINGS_OFF = 0;
constexpr int32_t RO_KEYRINGS_COUNT = 2;
constexpr int32_t RO_DUNGEON_ENTRANCE_SHUFFLE_OFF = 0;
constexpr int32_t RO_DUNGEON_ENTRANCE_SHUFFLE_ON = 1;
constexpr int32_t RO_DUNGEON_ENTRANCE_SHUFFLE_ON_PLUS_GANON = 2;
constexpr int32_t RO_CLOSED_FOREST_ON = 0;
constexpr int32_t RO_CLOSED_FOREST_OFF = 2;
constexpr int32_t RO_DOOROFTIME_CLOSED = 0;
constexpr int32_t RO_DOOROFTIME_OPEN = 2;
constexpr int32_t RO_ZF_CLOSED = 0;
constexpr int32_t RO_ZF_OPEN = 2;
constexpr int32_t RO_JABU_CLOSED = 0;
constexpr int32_t RO_JABU_OPEN = 1;
constexpr int32_t RO_KAK_GATE_CLOSED = 0;
constexpr int32_t RO_KAK_GATE_OPEN = 1;
constexpr int32_t RO_GERUDO_KEYS_VANILLA = 0;
constexpr int32_t RO_GERUDO_KEYS_ANYWHERE = 3;
constexpr int32_t RO_TOKENSANITY_OFF = 0;
constexpr int32_t RO_TOKENSANITY_ALL = 3;
constexpr int32_t RO_SHUFFLE_FREESTANDING_OFF = 0;
constexpr int32_t RO_SHUFFLE_FREESTANDING_ALL = 3;
constexpr int32_t RO_SHUFFLE_POTS_OFF = 0;
constexpr int32_t RO_SHUFFLE_POTS_ALL = 3;
constexpr int32_t RO_SHUFFLE_GRASS_OFF = 0;
constexpr int32_t RO_SHUFFLE_GRASS_ALL = 3;
constexpr int32_t RO_SHUFFLE_CRATES_OFF = 0;
constexpr int32_t RO_SHUFFLE_CRATES_ALL = 3;
constexpr int32_t RO_LINKS_POCKET_DUNGEON_REWARD = 0;
constexpr int32_t RO_LINKS_POCKET_ADVANCEMENT = 1;
constexpr int32_t RO_LINKS_POCKET_ANYTHING = 2;
constexpr int32_t RO_LINKS_POCKET_NOTHING = 3;
constexpr int32_t RO_GOSSIP_STONES_NEED_NOTHING = 1;
constexpr int32_t RO_SONG_SHUFFLE_SONG_LOCATIONS = 1;
constexpr int32_t RO_SONG_SHUFFLE_ANYWHERE = 3;
constexpr int32_t RO_BOMBCHU_BAG_NONE = 0;
constexpr int32_t RO_BOMBCHU_BAG_SINGLE = 1;
constexpr int32_t RO_BOMBCHU_BAG_PROGRESSIVE = 2;
constexpr int32_t RO_MASK_QUEST_SHUFFLE = 2;

static const std::vector<std::pair<const char*, int32_t>> kComboDefaults = {
    { "General.ContainerStyleMatchesContents", 0 },
    { "General.SkipGetItemAnimations", 1 },
    { "General.QuestItemFanfares", 0 },
    { "General.MysteriousShuffledItems", 0 },
    { "General.SimplerBossSouls", 0 },
    { "General.RandoRelevantNavi", 1 },
    { "General.CustomKeyModels", 1 },
    { "General.ColoredMapsCompasses", 0 },
    { "Logic.DungeonEntrances", 0 },
    { "Logic.BigPoeTargetCount", 0 },
    { "Logic.SkipChildZelda", 0 },
    { "Logic.SkipChildStealth", 0 },
    { "Logic.SkipEponaRace", 0 },
    { "Logic.MM.MajoraRemainsRequired", 0 },
    { "Logic.MM.MajoraMasksRequired", 0 },
    { "Logic.MM.TrialsAccess", 0 },
    { "OoT.Access.Forest", 1 },
    { "OoT.Access.KakarikoGate", 1 },
    { "OoT.Access.DoorOfTime", 1 },
    { "OoT.Access.ZorasFountain", 1 },
    { "OoT.Access.JabuJabu", 1 },
    { "Dungeons.MapsCompasses", 0 },
    { "Dungeons.SmallKeys", 1 },
    { "Dungeons.BossKeys", 1 },
    { "Dungeons.Rewards", 1 },
    { "Dungeons.GerudoKeys", 0 },
    { "Dungeons.BossSouls", 0 },
    { "Dungeons.KeyRings", 0 },
    { "Items.GoldSkulltulaTokens", 0 },
    { "Items.MM.StrayFairies", 0 },
    { "Items.Swords", 0 },
    { "Items.Ocarinas", 0 },
    { "Items.Cows", 0 },
    { "Items.Pots", 0 },
    { "Items.Grass", 0 },
    { "Items.Crates", 0 },
    { "Items.Beehives", 0 },
    { "Items.OoT.FrogSongRupees", 0 },
    { "Items.OoT.AdultTrade", 0 },
    { "Items.OoT.FountainFairies", 0 },
    { "Items.MM.SunsSong", 0 },
    { "Shuffles.Shops", 0 },
    { "Shuffles.MM.BossRemains", 0 },
    { "Shuffles.MM.OwlStatues", 0 },
    { "Shuffles.ShopsAffordablePrices", 0 },
    { "Shuffles.OoT.Scrubs", 0 },
    { "Shuffles.OoT.MerchantAffordablePrices", 0 },
    { "Shuffles.OoT.TycoonWallet", 0 },
    { "Shuffles.OoT.FishingPole", 0 },
    { "Shuffles.OcarinaButtons", 0 },
    { "Shuffles.OoT.RocsFeather", 0 },
    { "Shuffles.OoT.SkeletonKey", 0 },
    { "Shuffles.MM.EnemySouls", 0 },
    { "Shuffles.MM.Time", 0 },
    { "HintsTraps.IceTrapCount", 0 },
    { "Hints.GossipStones", 1 },
    { "Hints.OoT.WarpSongs", 1 },
    { "Hints.OoT.ScrubText", 1 },
    { "Hints.OoT.MerchantText", 1 },
    { "Hints.MM.SpiderHouse", 1 },
    { "Starting.OoT.LinksPocket", 0 },
    { "Starting.OoT.KokiriSword", 0 },
    { "Starting.OoT.MasterSword", 0 },
    { "Starting.OoT.DekuShield", 0 },
    { "Starting.OoT.Ocarina", 0 },
    { "Starting.SongOfTime", 0 },
    { "Starting.OoT.PreludeOfLight", 0 },
    { "Starting.MM.FullWallet", 0 },
    { "Starting.MM.FullConsumables", 0 },
    { "Starting.MM.Health", 3 },
};

static const char* ComboCVar(const char* key) {
    static std::unordered_map<std::string, std::string> cvars;
    auto [it, inserted] = cvars.try_emplace(key, std::string("gRandomizer.Combo.") + key);
    return it->second.c_str();
}

static std::map<int32_t, const char*> offOn = {
    { 0, "Off" },
    { 1, "On" },
};

static std::map<int32_t, const char*> openClosed = {
    { 0, "Open" },
    { 1, "Closed" },
};

static std::map<int32_t, const char*> skipGetItemAnimations = {
    { 0, "Disabled" },
    { 1, "Junk Items" },
};

static std::map<int32_t, const char*> dungeonEntrances = {
    { 0, "Off" },
    { 1, "On" },
    { 2, "On + Ganon + Majora" },
};

static std::map<int32_t, const char*> trialsAccess = {
    { 0, "Requires Associated Remains" },
    { 1, "Open" },
};

static std::map<int32_t, const char*> dungeonItemLocation = {
    { 0, "Start With" },
    { 1, "Own Dungeon" },
    { 2, "Any Dungeon" },
    { 3, "Anywhere" },
};

static std::map<int32_t, const char*> dungeonRewards = {
    { 0, "Vanilla" },
    { 1, "End of Dungeons" },
    { 2, "Anywhere" },
};

static std::map<int32_t, const char*> gerudoKeys = {
    { 0, "Vanilla" },
    { 1, "Anywhere" },
};

static std::map<int32_t, const char*> linksPocket = {
    { 0, "Dungeon Reward" },
    { 1, "Advancement" },
    { 2, "Anything" },
    { 3, "Nothing" },
};

static std::map<int32_t, const char*> startingOcarina = {
    { 0, "Off" },
    { 1, "Fairy Ocarina" },
    { 2, "Ocarina of Time" },
};

static void SetInt(const char* cvar, int32_t value) {
    CVarSetInteger(cvar, value);
}

static void SetBoolAsInt(const char* cvar, bool enabled) {
    CVarSetInteger(cvar, enabled ? 1 : 0);
}

static int32_t ComboGet(const char* key, int32_t defaultValue = 0) {
    return CVarGetInteger(ComboCVar(key), defaultValue);
}

static int32_t ComboGetFirst(const char* key, const char* fallbackKey, int32_t defaultValue = 0) {
    const int32_t sentinel = -987654321;
    const int32_t value = CVarGetInteger(ComboCVar(key), sentinel);
    if (value != sentinel) {
        return value;
    }
    return ComboGet(fallbackKey, defaultValue);
}

static void ComboSet(const char* key, int32_t value) {
    CVarSetInteger(ComboCVar(key), value);
}

static void EnsureSoHRandomizerOptionsCreated() {
    static bool created = false;
    if (created) {
        return;
    }

    Rando::Settings::GetInstance()->CreateOptions();
    created = true;
}

static std::optional<std::filesystem::path> FindCrossoverRoot() {
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

static std::optional<std::filesystem::path> GetSharedComboSettingsPath() {
    const auto root = FindCrossoverRoot();
    if (!root.has_value()) {
        return std::nullopt;
    }
    return *root / "crossover_combo_settings.json";
}

static void EnsureComboDefaults() {
    constexpr int32_t sentinel = -987654321;
    for (const auto& [key, defaultValue] : kComboDefaults) {
        if (CVarGetInteger(ComboCVar(key), sentinel) == sentinel) {
            ComboSet(key, defaultValue);
        }
    }
}

static void LoadSharedComboSettings() {
    EnsureComboDefaults();
    const auto path = GetSharedComboSettingsPath();
    if (!path.has_value() || !std::filesystem::exists(*path)) {
        return;
    }

    try {
        std::ifstream stream(*path);
        nlohmann::json json;
        stream >> json;
        const auto& settings = json.contains("settings") && json["settings"].is_object() ? json["settings"] : json;
        for (const auto& [key, defaultValue] : kComboDefaults) {
            if (settings.contains(key) && settings[key].is_number_integer()) {
                ComboSet(key, settings[key].get<int32_t>());
            }
        }
    } catch (...) {
    }
}

static void SaveSharedComboSettings() {
    const auto path = GetSharedComboSettingsPath();
    if (!path.has_value()) {
        return;
    }

    nlohmann::json settings = nlohmann::json::object();
    for (const auto& [key, defaultValue] : kComboDefaults) {
        settings[key] = ComboGet(key, defaultValue);
    }

    const auto tempPath = path->string() + ".tmp";
    try {
        {
            std::ofstream stream(tempPath);
            stream << std::setw(4) << nlohmann::json{
                { "version", 1 },
                { "sourceGame", "oot" },
                { "settings", settings },
            } << std::endl;
        }
        if (std::filesystem::exists(*path)) {
            std::filesystem::remove(*path);
        }
        std::filesystem::rename(tempPath, *path);
    } catch (...) {
    }
}

static void ApplyComboSettingsToSoHInternal() {
    SetBoolAsInt(CVAR_ENHANCEMENT("ChestSizeAndTextureMatchContents"),
                 ComboGet("General.ContainerStyleMatchesContents") != 0);
    SetBoolAsInt("gRando.CSMC", ComboGet("General.ContainerStyleMatchesContents") != 0);
    SetInt(CVAR_RANDOMIZER_ENHANCEMENT("TimeSavers.SkipGetItemAnimation"),
           ComboGet("General.SkipGetItemAnimations", 1));
    SetBoolAsInt(CVAR_RANDOMIZER_ENHANCEMENT("QuestItemFanfares"), ComboGet("General.QuestItemFanfares") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"), ComboGet("General.MysteriousShuffledItems") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_ENHANCEMENT("SimplerBossSoulModels"), ComboGet("General.SimplerBossSouls") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_ENHANCEMENT("RandoRelevantNavi"), ComboGet("General.RandoRelevantNavi", 1) != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_ENHANCEMENT("CustomKeyModels"), ComboGet("General.CustomKeyModels", 1) != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_ENHANCEMENT("ColoredMapsAndCompasses"),
                 ComboGet("General.ColoredMapsCompasses") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_ENHANCEMENT("RandomizeRupeeNames"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleOpenChest"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("GenericJabberNutModel"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("BlueFireArrows"), true);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("SunlightArrows"), true);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("FullWallets"), true);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("SlingBowBreakBeehives"), true);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("SkipPlantingBeans"), true);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("SkipScarecrowsSong"), true);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("TriforceHunt"), false);

    SetInt(CVAR_RANDOMIZER_SETTING("ShuffleDungeonsEntrances"), RO_DUNGEON_ENTRANCE_SHUFFLE_OFF);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleOverworldEntrances"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleInteriorsEntrances"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleGrottosEntrances"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleBossEntrances"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleOverworldSpawns"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("MixedEntrances"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("DecoupleEntrances"), false);
    SetInt(CVAR_RANDOMIZER_SETTING("BigPoeTargetCount"), ComboGet("Logic.BigPoeTargetCount"));
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("SkipChildZelda"), ComboGet("Logic.SkipChildZelda") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("SkipChildStealth"), ComboGet("Logic.SkipChildStealth") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("SkipEponaRace"), ComboGet("Logic.SkipEponaRace") != 0);
    SetInt(CVAR_RANDOMIZER_SETTING("ClosedForest"),
           ComboGet("OoT.Access.Forest", 1) == 0 ? RO_CLOSED_FOREST_OFF : RO_CLOSED_FOREST_ON);
    SetInt(CVAR_RANDOMIZER_SETTING("KakarikoGate"),
           ComboGet("OoT.Access.KakarikoGate", 1) == 0 ? RO_KAK_GATE_OPEN : RO_KAK_GATE_CLOSED);
    SetInt(CVAR_RANDOMIZER_SETTING("DoorOfTime"),
           ComboGet("OoT.Access.DoorOfTime", 1) == 0 ? RO_DOOROFTIME_OPEN : RO_DOOROFTIME_CLOSED);
    SetInt(CVAR_RANDOMIZER_SETTING("ZorasFountain"),
           ComboGet("OoT.Access.ZorasFountain", 1) == 0 ? RO_ZF_OPEN : RO_ZF_CLOSED);
    SetInt(CVAR_RANDOMIZER_SETTING("JabuJabu"),
           ComboGet("OoT.Access.JabuJabu", 1) == 0 ? RO_JABU_OPEN : RO_JABU_CLOSED);

    static const std::map<int32_t, int32_t> dungeonItemMap = {
        { 0, RO_DUNGEON_ITEM_LOC_STARTWITH },
        { 1, RO_DUNGEON_ITEM_LOC_OWN_DUNGEON },
        { 2, RO_DUNGEON_ITEM_LOC_ANY_DUNGEON },
        { 3, RO_DUNGEON_ITEM_LOC_ANYWHERE },
    };
    const auto applyMapped = [](const char* targetCvar, const std::map<int32_t, int32_t>& valueMap,
                                const char* comboKey, int32_t defaultValue = 0) {
        const int32_t comboValue = ComboGet(comboKey, defaultValue);
        const auto it = valueMap.find(comboValue);
        SetInt(targetCvar, it != valueMap.end() ? it->second : comboValue);
    };
    applyMapped(CVAR_RANDOMIZER_SETTING("StartingMapsCompasses"), dungeonItemMap, "Dungeons.MapsCompasses", 0);
    applyMapped(CVAR_RANDOMIZER_SETTING("Keysanity"), dungeonItemMap, "Dungeons.SmallKeys", 1);
    applyMapped(CVAR_RANDOMIZER_SETTING("BossKeysanity"), dungeonItemMap, "Dungeons.BossKeys", 1);
    applyMapped(CVAR_RANDOMIZER_SETTING("ShuffleDungeonReward"),
                { { 0, RO_DUNGEON_REWARDS_VANILLA },
                  { 1, RO_DUNGEON_REWARDS_END_OF_DUNGEON },
                  { 2, RO_DUNGEON_REWARDS_ANYWHERE } },
                "Dungeons.Rewards", 1);
    applyMapped(CVAR_RANDOMIZER_SETTING("GerudoKeys"),
                { { 0, RO_GERUDO_KEYS_VANILLA }, { 1, RO_GERUDO_KEYS_ANYWHERE } }, "Dungeons.GerudoKeys", 0);
    SetInt(CVAR_RANDOMIZER_SETTING("ShuffleBossSouls"),
           ComboGet("Dungeons.BossSouls") ? RO_BOSS_SOULS_ON : RO_BOSS_SOULS_OFF);
    SetInt(CVAR_RANDOMIZER_SETTING("ShuffleKeyRings"), ComboGet("Dungeons.KeyRings") ? RO_KEYRINGS_COUNT : RO_KEYRINGS_OFF);
    if (ComboGet("Dungeons.KeyRings") != 0) {
        SetInt(CVAR_RANDOMIZER_SETTING("ShuffleKeyRingsRandomCount"), 8);
    }
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("MQDungeons"), false);

    SetInt(CVAR_RANDOMIZER_SETTING("ShuffleTokens"),
           ComboGet("Items.GoldSkulltulaTokens") ? RO_TOKENSANITY_ALL : RO_TOKENSANITY_OFF);
    const bool swords = ComboGet("Items.Swords") != 0;
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleKokiriSword"), swords);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleMasterSword"), swords);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleOcarinas"), ComboGet("Items.Ocarinas") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleCows"), ComboGet("Items.Cows") != 0);
    SetInt(CVAR_RANDOMIZER_SETTING("ShufflePots"), ComboGet("Items.Pots") ? RO_SHUFFLE_POTS_ALL : RO_SHUFFLE_POTS_OFF);
    SetInt(CVAR_RANDOMIZER_SETTING("ShuffleGrass"), ComboGet("Items.Grass") ? RO_SHUFFLE_GRASS_ALL : RO_SHUFFLE_GRASS_OFF);
    SetInt(CVAR_RANDOMIZER_SETTING("ShuffleCrates"), ComboGet("Items.Crates") ? RO_SHUFFLE_CRATES_ALL : RO_SHUFFLE_CRATES_OFF);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleBeehives"), ComboGet("Items.Beehives") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleFrogSongRupees"), ComboGet("Items.OoT.FrogSongRupees") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleAdultTrade"), ComboGet("Items.OoT.AdultTrade") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleFountainFairies"), ComboGet("Items.OoT.FountainFairies") != 0);
    SetInt(CVAR_RANDOMIZER_SETTING("ShuffleSongs"), RO_SONG_SHUFFLE_ANYWHERE);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleWeirdEgg"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleGerudoToken"), true);
    SetInt(CVAR_RANDOMIZER_SETTING("ShuffleFreestanding"), RO_SHUFFLE_FREESTANDING_ALL);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleTrees"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleBushes"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("Shuffle100GSReward"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleStoneFairies"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleBeanFairies"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleFairySpots"), false);

    const bool shops = ComboGet("Shuffles.Shops") != 0;
    SetInt(CVAR_RANDOMIZER_SETTING("Shopsanity"), shops ? RO_SHOPSANITY_SPECIFIC_COUNT : RO_SHOPSANITY_OFF);
    SetInt(CVAR_RANDOMIZER_SETTING("ShopsanityCount"), shops ? 7 : 0);
    SetInt(CVAR_RANDOMIZER_SETTING("ShopsanityPrices"), 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShopsanityPricesAffordable"),
                 ComboGet("Shuffles.ShopsAffordablePrices") != 0);
    SetInt(CVAR_RANDOMIZER_SETTING("Scrubsanity"), ComboGet("Shuffles.OoT.Scrubs") ? RO_SCRUBS_ALL : RO_SCRUBS_OFF);
    SetInt(CVAR_RANDOMIZER_SETTING("ScrubsPrices"), 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("MerchantsPricesAffordable"),
                 ComboGet("Shuffles.OoT.MerchantAffordablePrices") != 0);
    SetInt(CVAR_RANDOMIZER_SETTING("ShuffleMerchants"), RO_MERCHANTS_ALL);
    SetInt(CVAR_RANDOMIZER_SETTING("MerchantPrices"), 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("IncludeTycoonWallet"), ComboGet("Shuffles.OoT.TycoonWallet") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleFishingPole"), ComboGet("Shuffles.OoT.FishingPole") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleOcarinaButtons"), ComboGet("Shuffles.OcarinaButtons") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("RocsFeather"), ComboGet("Shuffles.OoT.RocsFeather") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("SkeletonKey"), ComboGet("Shuffles.OoT.SkeletonKey") != 0);
    SetInt(CVAR_RANDOMIZER_SETTING("BombchuBag"), RO_BOMBCHU_BAG_NONE);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("EnableBombchuDrops"), true);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleChildWallet"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleSwim"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleGrab"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleClimb"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleCrawl"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleSpeak"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleBeanSouls"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleDekuStickBag"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleDekuNutBag"), false);

    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("BaseIceTraps"), ComboGet("HintsTraps.IceTrapCount") > 0);
    SetInt(CVAR_RANDOMIZER_SETTING("GossipStoneHints"),
           ComboGet("Hints.GossipStones", 1) ? RO_GOSSIP_STONES_NEED_NOTHING : 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("WarpSongText"), ComboGet("Hints.OoT.WarpSongs", 1) != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ScrubText"), ComboGet("Hints.OoT.ScrubText", 1) != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("MerchantText"), ComboGet("Hints.OoT.MerchantText", 1) != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("100GSHint"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("MaskShopHint"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("LoachHint"), false);
    SetInt(CVAR_RANDOMIZER_SETTING("CompleteMaskQuest"), RO_MASK_QUEST_SHUFFLE);

    applyMapped(CVAR_RANDOMIZER_SETTING("LinksPocket"),
                { { 0, RO_LINKS_POCKET_DUNGEON_REWARD },
                  { 1, RO_LINKS_POCKET_ADVANCEMENT },
                  { 2, RO_LINKS_POCKET_ANYTHING },
                  { 3, RO_LINKS_POCKET_NOTHING } },
                "Starting.OoT.LinksPocket", 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("StartingKokiriSword"), ComboGet("Starting.OoT.KokiriSword") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("StartingMasterSword"), ComboGet("Starting.OoT.MasterSword") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("StartingDekuShield"), ComboGet("Starting.OoT.DekuShield") != 0);
    SetInt(CVAR_RANDOMIZER_SETTING("StartingOcarina"), ComboGet("Starting.OoT.Ocarina"));
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("StartingSongOfTime"), ComboGet("Starting.SongOfTime") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("StartingPreludeOfLight"), ComboGet("Starting.OoT.PreludeOfLight") != 0);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("StartingDekuSticks"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("StartingDekuNuts"), false);
    SetBoolAsInt(CVAR_RANDOMIZER_SETTING("StartingBeans"), false);
    SetInt(CVAR_RANDOMIZER_SETTING("StartingSkulltulaToken"), 0);
    SetInt(CVAR_RANDOMIZER_SETTING("StartingHearts"), 3);
}

static void CommitComboSettingsToSoH() {
    EnsureSoHRandomizerOptionsCreated();
    SaveSharedComboSettings();
    ApplyComboSettingsToSoHInternal();
}

static void SyncComboFromSoH() {
    ComboSet("General.ContainerStyleMatchesContents", CVarGetInteger(CVAR_ENHANCEMENT("ChestSizeAndTextureMatchContents"), 0));
    ComboSet("General.SkipGetItemAnimations",
             CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("TimeSavers.SkipGetItemAnimation"), 1));
    ComboSet("General.QuestItemFanfares", CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("QuestItemFanfares"), 0));
    ComboSet("General.MysteriousShuffledItems", CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"), 0));
    ComboSet("General.SimplerBossSouls", CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("SimplerBossSoulModels"), 0));
    ComboSet("General.RandoRelevantNavi", CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("RandoRelevantNavi"), 1));
    ComboSet("General.CustomKeyModels", CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("CustomKeyModels"), 1));
    ComboSet("General.ColoredMapsCompasses", CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("ColoredMapsAndCompasses"), 0));

    const int32_t nativeDungeonEntrances = CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleDungeonsEntrances"), 0);
    if (nativeDungeonEntrances != RO_DUNGEON_ENTRANCE_SHUFFLE_OFF) {
        ComboSet("Logic.DungeonEntrances", nativeDungeonEntrances);
    }
    ComboSet("Logic.BigPoeTargetCount", CVarGetInteger(CVAR_RANDOMIZER_SETTING("BigPoeTargetCount"), 0));
    ComboSet("Logic.SkipChildZelda", CVarGetInteger(CVAR_RANDOMIZER_SETTING("SkipChildZelda"), 0));
    ComboSet("Logic.SkipChildStealth", CVarGetInteger(CVAR_RANDOMIZER_SETTING("SkipChildStealth"), 0));
    ComboSet("Logic.SkipEponaRace", CVarGetInteger(CVAR_RANDOMIZER_SETTING("SkipEponaRace"), 0));
    ComboSet("OoT.Access.Forest",
             CVarGetInteger(CVAR_RANDOMIZER_SETTING("ClosedForest"), RO_CLOSED_FOREST_ON) == RO_CLOSED_FOREST_OFF ? 0 : 1);
    ComboSet("OoT.Access.KakarikoGate",
             CVarGetInteger(CVAR_RANDOMIZER_SETTING("KakarikoGate"), RO_KAK_GATE_CLOSED) == RO_KAK_GATE_OPEN ? 0 : 1);
    ComboSet("OoT.Access.DoorOfTime",
             CVarGetInteger(CVAR_RANDOMIZER_SETTING("DoorOfTime"), RO_DOOROFTIME_CLOSED) == RO_DOOROFTIME_OPEN ? 0 : 1);
    ComboSet("OoT.Access.ZorasFountain",
             CVarGetInteger(CVAR_RANDOMIZER_SETTING("ZorasFountain"), RO_ZF_CLOSED) == RO_ZF_OPEN ? 0 : 1);
    ComboSet("OoT.Access.JabuJabu",
             CVarGetInteger(CVAR_RANDOMIZER_SETTING("JabuJabu"), RO_JABU_CLOSED) == RO_JABU_OPEN ? 0 : 1);

    ComboSet("Dungeons.BossSouls", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleBossSouls"), 0) != 0);
    ComboSet("Dungeons.KeyRings", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleKeyRings"), 0) != 0);
    ComboSet("Items.GoldSkulltulaTokens", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleTokens"), 0) != 0);
    ComboSet("Items.Swords", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleKokiriSword"), 0) != 0 ||
                                  CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleMasterSword"), 0) != 0);
    ComboSet("Items.Ocarinas", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleOcarinas"), 0));
    ComboSet("Items.Cows", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleCows"), 0));
    ComboSet("Items.Pots", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShufflePots"), 0) != 0);
    ComboSet("Items.Grass", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleGrass"), 0) != 0);
    ComboSet("Items.Crates", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleCrates"), 0) != 0);
    ComboSet("Items.Beehives", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleBeehives"), 0));
    ComboSet("Items.OoT.FrogSongRupees", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleFrogSongRupees"), 0));
    ComboSet("Items.OoT.AdultTrade", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleAdultTrade"), 0));
    ComboSet("Items.OoT.FountainFairies", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleFountainFairies"), 0));

    ComboSet("Shuffles.Shops", CVarGetInteger(CVAR_RANDOMIZER_SETTING("Shopsanity"), 0) != 0);
    ComboSet("Shuffles.ShopsAffordablePrices", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShopsanityPricesAffordable"), 0));
    ComboSet("Shuffles.OoT.Scrubs", CVarGetInteger(CVAR_RANDOMIZER_SETTING("Scrubsanity"), 0) != 0);
    ComboSet("Shuffles.OoT.MerchantAffordablePrices",
             CVarGetInteger(CVAR_RANDOMIZER_SETTING("MerchantsPricesAffordable"), 0));
    ComboSet("Shuffles.OoT.Merchants", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleMerchants"), 0) != 0);
    ComboSet("Shuffles.OoT.TycoonWallet", CVarGetInteger(CVAR_RANDOMIZER_SETTING("IncludeTycoonWallet"), 0));
    ComboSet("Shuffles.OoT.FishingPole", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleFishingPole"), 0));
    ComboSet("Shuffles.OcarinaButtons", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleOcarinaButtons"), 0));
    ComboSet("Shuffles.OoT.RocsFeather", CVarGetInteger(CVAR_RANDOMIZER_SETTING("RocsFeather"), 0));
    ComboSet("Shuffles.OoT.SkeletonKey", CVarGetInteger(CVAR_RANDOMIZER_SETTING("SkeletonKey"), 0));
    ComboSet("Shuffles.FreestandingItems", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleFreestanding"), 0) != 0);
    ComboSet("Shuffles.OoT.Songs",
             CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleSongs"), RO_SONG_SHUFFLE_SONG_LOCATIONS) ==
                 RO_SONG_SHUFFLE_ANYWHERE);
    ComboSet("Shuffles.OoT.GerudoMembershipCard", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleGerudoToken"), 0));
    ComboSet("Shuffles.OoT.BombchuBag", CVarGetInteger(CVAR_RANDOMIZER_SETTING("BombchuBag"), 0));

    ComboSet("HintsTraps.IceTrapCount", CVarGetInteger(CVAR_RANDOMIZER_SETTING("BaseIceTraps"), 0) ? 1 : 0);
    ComboSet("Hints.GossipStones", CVarGetInteger(CVAR_RANDOMIZER_SETTING("GossipStoneHints"), 1) != 0);
    ComboSet("Hints.OoT.WarpSongs", CVarGetInteger(CVAR_RANDOMIZER_SETTING("WarpSongText"), 1));
    ComboSet("Hints.OoT.ScrubText", CVarGetInteger(CVAR_RANDOMIZER_SETTING("ScrubText"), 1));
    ComboSet("Hints.OoT.MerchantText", CVarGetInteger(CVAR_RANDOMIZER_SETTING("MerchantText"), 1));

    ComboSet("Starting.OoT.LinksPocket", CVarGetInteger(CVAR_RANDOMIZER_SETTING("LinksPocket"), 0));
    ComboSet("Starting.OoT.KokiriSword", CVarGetInteger(CVAR_RANDOMIZER_SETTING("StartingKokiriSword"), 0));
    ComboSet("Starting.OoT.MasterSword", CVarGetInteger(CVAR_RANDOMIZER_SETTING("StartingMasterSword"), 0));
    ComboSet("Starting.OoT.DekuShield", CVarGetInteger(CVAR_RANDOMIZER_SETTING("StartingDekuShield"), 0));
    ComboSet("Starting.OoT.Ocarina", CVarGetInteger(CVAR_RANDOMIZER_SETTING("StartingOcarina"), 0));
    ComboSet("Starting.SongOfTime", CVarGetInteger(CVAR_RANDOMIZER_SETTING("StartingSongOfTime"), 0));
    ComboSet("Starting.OoT.PreludeOfLight", CVarGetInteger(CVAR_RANDOMIZER_SETTING("StartingPreludeOfLight"), 0));
}

static WidgetFunc SyncInt(const char* targetCvar) {
    return [targetCvar](WidgetInfo& info) { SetInt(targetCvar, CVarGetInteger(info.cVar, 0)); };
}

static WidgetFunc SyncBool(const char* targetCvar) {
    return [targetCvar](WidgetInfo& info) { SetBoolAsInt(targetCvar, CVarGetInteger(info.cVar, 0) != 0); };
}

static WidgetFunc SyncContainerStyleMatchesContents() {
    return [](WidgetInfo& info) {
        const bool enabled = CVarGetInteger(info.cVar, 0) != 0;
        SetBoolAsInt(CVAR_ENHANCEMENT("ChestSizeAndTextureMatchContents"), enabled);
        SetBoolAsInt("gRando.CSMC", enabled);
    };
}

static WidgetFunc SyncMappedInt(const char* targetCvar, std::map<int32_t, int32_t> valueMap) {
    return [targetCvar, valueMap](WidgetInfo& info) {
        const int32_t value = CVarGetInteger(info.cVar, 0);
        const auto it = valueMap.find(value);
        SetInt(targetCvar, it != valueMap.end() ? it->second : value);
    };
}

static WidgetInfo& AddComboCheckbox(SohMenu* menu, WidgetPath& path, const char* label, const char* key,
                                    const char* tooltip = nullptr, WidgetFunc callback = nullptr,
                                    bool defaultValue = false) {
    auto& widget = menu->AddWidget(path, label, WIDGET_CVAR_CHECKBOX)
                       .CVar(ComboCVar(key))
                       .RaceDisable(false)
                       .Options(CheckboxOptions().DefaultValue(defaultValue).Tooltip(tooltip == nullptr ? "" : tooltip));
    widget.Callback([](WidgetInfo& info) {
        CommitComboSettingsToSoH();
    });
    return widget;
}

static WidgetInfo& AddComboBox(SohMenu* menu, WidgetPath& path, const char* label, const char* key,
                               std::map<int32_t, const char*>& options, int32_t defaultIndex = 0,
                               const char* tooltip = nullptr, WidgetFunc callback = nullptr) {
    auto& widget = menu->AddWidget(path, label, WIDGET_CVAR_COMBOBOX)
                       .CVar(ComboCVar(key))
                       .RaceDisable(false)
                       .Options(ComboboxOptions().ComboMap(options).DefaultIndex(defaultIndex).Tooltip(
                           tooltip == nullptr ? "" : tooltip));
    widget.Callback([](WidgetInfo& info) {
        CommitComboSettingsToSoH();
    });
    return widget;
}

static WidgetInfo& AddComboSlider(SohMenu* menu, WidgetPath& path, const char* label, const char* key, int32_t min,
                                  int32_t max, int32_t defaultValue = 0, const char* tooltip = nullptr,
                                  WidgetFunc callback = nullptr) {
    auto& widget = menu->AddWidget(path, label, WIDGET_CVAR_SLIDER_INT)
                       .CVar(ComboCVar(key))
                       .RaceDisable(false)
                       .Options(IntSliderOptions()
                                    .Min(min)
                                    .Max(max)
                                    .DefaultValue(defaultValue)
                                    .ShowButtons(true)
                                    .Tooltip(tooltip == nullptr ? "" : tooltip));
    widget.Callback([](WidgetInfo& info) {
        CommitComboSettingsToSoH();
    });
    return widget;
}

static void AddNote(SohMenu* menu, WidgetPath& path, const char* text) {
    menu->AddWidget(path, text, WIDGET_TEXT).RaceDisable(false).Options(TextOptions().Color(UIWidgets::Colors::Gray));
}

static void AddAreaAccessPlaceholders(SohMenu* menu, WidgetPath& path) {
    menu->AddWidget(path, "OoT Area Access", WIDGET_SEPARATOR_TEXT).RaceDisable(false);
    AddComboBox(menu, path, "Forest", "OoT.Access.Forest", openClosed, 1);
    AddComboBox(menu, path, "Kakariko Gate", "OoT.Access.KakarikoGate", openClosed, 1);
    AddComboBox(menu, path, "Door of Time", "OoT.Access.DoorOfTime", openClosed, 1);
    AddComboBox(menu, path, "Zora's Fountain", "OoT.Access.ZorasFountain", openClosed, 1);
    AddComboBox(menu, path, "Jabu-Jabu", "OoT.Access.JabuJabu", openClosed, 1);
}

} // namespace

void ApplyComboSettingsToSoH() {
    LoadSharedComboSettings();
    CommitComboSettingsToSoH();
}

void SohMenu::AddMenuComboRandomizer() {
    EnsureSoHRandomizerOptionsCreated();
    LoadSharedComboSettings();
    ApplyComboSettingsToSoHInternal();
    ComboSet("Initialized", 1);

    AddMenuEntry("Combo Randomizer", CVAR_SETTING("Menu.ComboRandomizerSidebarSection"));

    WidgetPath path = { "Combo Randomizer", "General", SECTION_COLUMN_1 };
    AddSidebarEntry("Combo Randomizer", path.sidebarName, 2);

    AddWidget(path, "Seed", WIDGET_SEPARATOR_TEXT).RaceDisable(false);
    AddNote(this, path,
            "Combo settings are mirrored automatically between OoT and MM.");
    AddWidget(path, "Generate Combo Randomizer", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            ApplyComboSettingsToSoH();
            if (OTRGlobals::Instance->gRandoContext != nullptr) {
                OTRGlobals::Instance->gRandoContext->SetSpoilerLoaded(false);
            }
            GenerateRandomizer("");
        })
        .PreFunc([](WidgetInfo& info) { info.options->disabled = gSaveContext.gameMode != GAMEMODE_FILE_SELECT; })
        .Options(ButtonOptions()
                     .Size(ImVec2(250.f, 0.f))
                     .DisabledTooltip("Must be on File Select to generate a combo randomizer seed."));
    AddComboCheckbox(this, path, "Container Style Matches Contents", "General.ContainerStyleMatchesContents",
                     "Shared container style setting for both games.",
                     SyncContainerStyleMatchesContents());
    AddComboBox(this, path, "Skip Get Item Animations", "General.SkipGetItemAnimations", skipGetItemAnimations, 1,
                "Combo menu exposes Disabled and Junk Items only.",
                SyncInt(CVAR_RANDOMIZER_ENHANCEMENT("TimeSavers.SkipGetItemAnimation")));
    AddComboCheckbox(this, path, "Quest Item Fanfares", "General.QuestItemFanfares", nullptr,
                     SyncBool(CVAR_RANDOMIZER_ENHANCEMENT("QuestItemFanfares")));
    AddComboCheckbox(this, path, "Mysterious Shuffled Items", "General.MysteriousShuffledItems", nullptr,
                     SyncBool(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle")));
    AddComboCheckbox(this, path, "Simpler Boss Souls", "General.SimplerBossSouls", nullptr,
                     SyncBool(CVAR_RANDOMIZER_ENHANCEMENT("SimplerBossSoulModels")));

    path.column = SECTION_COLUMN_2;
    AddWidget(path, "Forced Defaults", WIDGET_SEPARATOR_TEXT).RaceDisable(false);
    AddComboCheckbox(this, path, "Rando-Relevant Navi Hints", "General.RandoRelevantNavi", nullptr,
                     SyncBool(CVAR_RANDOMIZER_ENHANCEMENT("RandoRelevantNavi")), true);
    AddComboCheckbox(this, path, "Use Custom Key Models", "General.CustomKeyModels", nullptr,
                     SyncBool(CVAR_RANDOMIZER_ENHANCEMENT("CustomKeyModels")), true);
    AddComboCheckbox(this, path, "Map/Compass Colors Match Dungeon", "General.ColoredMapsCompasses", nullptr,
                     SyncBool(CVAR_RANDOMIZER_ENHANCEMENT("ColoredMapsAndCompasses")));

    path = { "Combo Randomizer", "Logic / Access", SECTION_COLUMN_1 };
    AddSidebarEntry("Combo Randomizer", path.sidebarName, 2);
    AddComboBox(this, path, "Dungeon Entrances", "Logic.DungeonEntrances", dungeonEntrances, 0,
                "Future cross-game dungeon entrance shuffle. Off/On/On + final bosses.",
                [](WidgetInfo&) {
                    SetInt(CVAR_RANDOMIZER_SETTING("ShuffleDungeonsEntrances"), RO_DUNGEON_ENTRANCE_SHUFFLE_OFF);
                });
    AddComboSlider(this, path, "Big Poe Target Count: %d", "Logic.BigPoeTargetCount", 0, 10, 0, nullptr,
                   SyncInt(CVAR_RANDOMIZER_SETTING("BigPoeTargetCount")));
    AddComboCheckbox(this, path, "Skip Child Zelda", "Logic.SkipChildZelda", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("SkipChildZelda")));
    AddComboCheckbox(this, path, "Skip Child Stealth", "Logic.SkipChildStealth", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("SkipChildStealth")));
    AddComboCheckbox(this, path, "Skip Epona Race", "Logic.SkipEponaRace", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("SkipEponaRace")));

    path.column = SECTION_COLUMN_2;
    AddComboCheckbox(this, path, "Majora Access Remains Required", "Logic.MM.MajoraRemainsRequired",
                     "MM side reads this as 4 remains when enabled, 0 when disabled.");
    AddComboCheckbox(this, path, "Majora Access Masks Required", "Logic.MM.MajoraMasksRequired",
                     "MM side reads this as 4 masks when enabled, 0 when disabled.");
    AddComboBox(this, path, "Trials Access", "Logic.MM.TrialsAccess", trialsAccess, 0);
    AddAreaAccessPlaceholders(this, path);

    path = { "Combo Randomizer", "Dungeons", SECTION_COLUMN_1 };
    AddSidebarEntry("Combo Randomizer", path.sidebarName, 2);
    AddComboBox(this, path, "Maps/Compasses", "Dungeons.MapsCompasses", dungeonItemLocation, 0,
                "Shared dungeon-item option. OoT maps 0/1/2/3 to Start With/Own Dungeon/Any Dungeon/Anywhere.",
                SyncMappedInt(CVAR_RANDOMIZER_SETTING("StartingMapsCompasses"),
                              { { 0, RO_DUNGEON_ITEM_LOC_STARTWITH },
                                { 1, RO_DUNGEON_ITEM_LOC_OWN_DUNGEON },
                                { 2, RO_DUNGEON_ITEM_LOC_ANY_DUNGEON },
                                { 3, RO_DUNGEON_ITEM_LOC_ANYWHERE } }));
    AddComboBox(this, path, "Small Key Shuffle", "Dungeons.SmallKeys", dungeonItemLocation, 1, nullptr,
                SyncMappedInt(CVAR_RANDOMIZER_SETTING("Keysanity"),
                              { { 0, RO_DUNGEON_ITEM_LOC_STARTWITH },
                                { 1, RO_DUNGEON_ITEM_LOC_OWN_DUNGEON },
                                { 2, RO_DUNGEON_ITEM_LOC_ANY_DUNGEON },
                                { 3, RO_DUNGEON_ITEM_LOC_ANYWHERE } }));
    AddComboBox(this, path, "Boss Key Shuffle", "Dungeons.BossKeys", dungeonItemLocation, 1, nullptr,
                SyncMappedInt(CVAR_RANDOMIZER_SETTING("BossKeysanity"),
                              { { 0, RO_DUNGEON_ITEM_LOC_STARTWITH },
                                { 1, RO_DUNGEON_ITEM_LOC_OWN_DUNGEON },
                                { 2, RO_DUNGEON_ITEM_LOC_ANY_DUNGEON },
                                { 3, RO_DUNGEON_ITEM_LOC_ANYWHERE } }));
    AddComboBox(this, path, "Shuffle Dungeon Rewards", "Dungeons.Rewards", dungeonRewards, 1, nullptr,
                SyncMappedInt(CVAR_RANDOMIZER_SETTING("ShuffleDungeonReward"),
                              { { 0, RO_DUNGEON_REWARDS_VANILLA },
                                { 1, RO_DUNGEON_REWARDS_END_OF_DUNGEON },
                                { 2, RO_DUNGEON_REWARDS_ANYWHERE } }));

    path.column = SECTION_COLUMN_2;
    AddComboBox(this, path, "Gerudo Fortress Keys", "Dungeons.GerudoKeys", gerudoKeys, 0, nullptr,
                SyncMappedInt(CVAR_RANDOMIZER_SETTING("GerudoKeys"),
                              { { 0, RO_GERUDO_KEYS_VANILLA }, { 1, RO_GERUDO_KEYS_ANYWHERE } }));
    AddComboCheckbox(this, path, "Shuffle Boss Souls", "Dungeons.BossSouls", nullptr,
                     SyncMappedInt(CVAR_RANDOMIZER_SETTING("ShuffleBossSouls"),
                                   { { 0, RO_BOSS_SOULS_OFF }, { 1, RO_BOSS_SOULS_ON } }));
    AddComboCheckbox(this, path, "Key Rings", "Dungeons.KeyRings",
                     "When enabled, combo generation should use keyrings count 8 and omit loose keys.",
                     SyncMappedInt(CVAR_RANDOMIZER_SETTING("ShuffleKeyRings"),
                                   { { 0, RO_KEYRINGS_OFF }, { 1, RO_KEYRINGS_COUNT } }));

    path = { "Combo Randomizer", "Items", SECTION_COLUMN_1 };
    AddSidebarEntry("Combo Randomizer", path.sidebarName, 2);
    AddComboCheckbox(this, path, "Shuffle GS Tokens", "Items.GoldSkulltulaTokens", nullptr,
                     SyncMappedInt(CVAR_RANDOMIZER_SETTING("ShuffleTokens"),
                                   { { 0, RO_TOKENSANITY_OFF }, { 1, RO_TOKENSANITY_ALL } }));
    AddComboCheckbox(this, path, "Shuffle Stray Fairies", "Items.MM.StrayFairies");
    AddComboCheckbox(this, path, "Shuffle Starter Sword + Master Sword", "Items.Swords", nullptr, [](WidgetInfo& info) {
        const bool enabled = CVarGetInteger(info.cVar, 0) != 0;
        SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleKokiriSword"), enabled);
        SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleMasterSword"), enabled);
    });
    AddComboCheckbox(this, path, "Shuffle Ocarinas", "Items.Ocarinas", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("ShuffleOcarinas")));
    AddComboCheckbox(this, path, "Shuffle Cows", "Items.Cows", "Shared only if both logic tables support cow checks.",
                     SyncBool(CVAR_RANDOMIZER_SETTING("ShuffleCows")));
    AddComboCheckbox(this, path, "Shuffle Pots", "Items.Pots", nullptr,
                     SyncMappedInt(CVAR_RANDOMIZER_SETTING("ShufflePots"),
                                   { { 0, RO_SHUFFLE_POTS_OFF }, { 1, RO_SHUFFLE_POTS_ALL } }));
    AddComboCheckbox(this, path, "Shuffle Grass", "Items.Grass", nullptr,
                     SyncMappedInt(CVAR_RANDOMIZER_SETTING("ShuffleGrass"),
                                   { { 0, RO_SHUFFLE_GRASS_OFF }, { 1, RO_SHUFFLE_GRASS_ALL } }));
    AddComboCheckbox(this, path, "Shuffle Crates", "Items.Crates", nullptr,
                     SyncMappedInt(CVAR_RANDOMIZER_SETTING("ShuffleCrates"),
                                   { { 0, RO_SHUFFLE_CRATES_OFF }, { 1, RO_SHUFFLE_CRATES_ALL } }));

    path.column = SECTION_COLUMN_2;
    AddComboCheckbox(this, path, "Shuffle Beehives", "Items.Beehives", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("ShuffleBeehives")));
    AddComboCheckbox(this, path, "Shuffle Frog Song Rupees", "Items.OoT.FrogSongRupees", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("ShuffleFrogSongRupees")));
    AddComboCheckbox(this, path, "Shuffle Adult Trade", "Items.OoT.AdultTrade", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("ShuffleAdultTrade")));
    AddComboCheckbox(this, path, "Shuffle Fairies in Fountains", "Items.OoT.FountainFairies", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("ShuffleFountainFairies")));
    AddComboCheckbox(this, path, "Shuffle Sun's Song", "Items.MM.SunsSong");

    path = { "Combo Randomizer", "Shuffles", SECTION_COLUMN_1 };
    AddSidebarEntry("Combo Randomizer", path.sidebarName, 2);
    AddComboCheckbox(this, path, "Shuffle Shops", "Shuffles.Shops", nullptr, [](WidgetInfo& info) {
        const bool enabled = CVarGetInteger(info.cVar, 0) != 0;
        SetInt(CVAR_RANDOMIZER_SETTING("Shopsanity"), enabled ? RO_SHOPSANITY_SPECIFIC_COUNT : RO_SHOPSANITY_OFF);
        SetInt(CVAR_RANDOMIZER_SETTING("ShopsanityCount"), enabled ? 7 : 0);
    });
    AddComboCheckbox(this, path, "Shuffle Boss Remains", "Shuffles.MM.BossRemains");
    AddComboCheckbox(this, path, "Shuffle Owl Statues", "Shuffles.MM.OwlStatues");
    AddComboCheckbox(this, path, "Shops Affordable Prices", "Shuffles.ShopsAffordablePrices", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("ShopsanityPricesAffordable")));
    AddComboCheckbox(this, path, "Scrubs Shuffle", "Shuffles.OoT.Scrubs", nullptr,
                     SyncMappedInt(CVAR_RANDOMIZER_SETTING("Scrubsanity"),
                                   { { 0, RO_SCRUBS_OFF }, { 1, RO_SCRUBS_ALL } }));
    AddComboCheckbox(this, path, "Merchant Affordable Prices", "Shuffles.OoT.MerchantAffordablePrices", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("MerchantsPricesAffordable")));

    path.column = SECTION_COLUMN_2;
    AddComboCheckbox(this, path, "Include Tycoon Wallet", "Shuffles.OoT.TycoonWallet", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("IncludeTycoonWallet")));
    AddComboCheckbox(this, path, "Shuffle Fishing Pole", "Shuffles.OoT.FishingPole", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("ShuffleFishingPole")));
    AddComboCheckbox(this, path, "Shuffle Ocarina Buttons", "Shuffles.OcarinaButtons", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("ShuffleOcarinaButtons")));
    AddComboCheckbox(this, path, "Shuffle Roc's Feather", "Shuffles.OoT.RocsFeather", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("RocsFeather")));
    AddComboCheckbox(this, path, "Shuffle Skeleton Key", "Shuffles.OoT.SkeletonKey",
                     "Skeleton Key is OoT-only until the 2ship setting exists.",
                     SyncBool(CVAR_RANDOMIZER_SETTING("SkeletonKey")));
    AddComboCheckbox(this, path, "Shuffle Enemy Souls (MM)", "Shuffles.MM.EnemySouls",
                     "Shuffles Majora's Mask enemy souls into the shared combo pool. OoT bean souls stay disabled.",
                     [](WidgetInfo& info) {
                         SetBoolAsInt(CVAR_RANDOMIZER_SETTING("ShuffleBeanSouls"), false);
                     });
    AddComboCheckbox(this, path, "Shuffle Time", "Shuffles.MM.Time");

    path = { "Combo Randomizer", "Hints / Traps", SECTION_COLUMN_1 };
    AddSidebarEntry("Combo Randomizer", path.sidebarName, 2);
    AddComboSlider(this, path, "Ice Traps in Pool: %d", "HintsTraps.IceTrapCount", 0, 200, 0,
                   "Shared trap-count slider for both games. OoT Base Ice Traps is enabled whenever this is above 0.",
                   [](WidgetInfo& info) {
                       SetBoolAsInt(CVAR_RANDOMIZER_SETTING("BaseIceTraps"), CVarGetInteger(info.cVar, 0) > 0);
                   });
    AddComboCheckbox(this, path, "Gossip Stone Hints", "Hints.GossipStones", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("GossipStoneHints")), true);
    AddComboCheckbox(this, path, "Warp Song Hints", "Hints.OoT.WarpSongs", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("WarpSongText")), true);
    AddComboCheckbox(this, path, "Scrub Text Hints", "Hints.OoT.ScrubText", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("ScrubText")), true);
    path.column = SECTION_COLUMN_2;
    AddComboCheckbox(this, path, "Merchant Text Hints", "Hints.OoT.MerchantText", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("MerchantText")), true);
    AddComboCheckbox(this, path, "MM Spider House Hints", "Hints.MM.SpiderHouse", nullptr, nullptr, true);

    path = { "Combo Randomizer", "Starting Items", SECTION_COLUMN_1 };
    AddSidebarEntry("Combo Randomizer", path.sidebarName, 2);
    AddComboBox(this, path, "OoT Link's Pocket", "Starting.OoT.LinksPocket", linksPocket, 0, nullptr,
                SyncMappedInt(CVAR_RANDOMIZER_SETTING("LinksPocket"),
                              { { 0, RO_LINKS_POCKET_DUNGEON_REWARD },
                                { 1, RO_LINKS_POCKET_ADVANCEMENT },
                                { 2, RO_LINKS_POCKET_ANYTHING },
                                { 3, RO_LINKS_POCKET_NOTHING } }));
    AddComboCheckbox(this, path, "Start with Kokiri Sword", "Starting.OoT.KokiriSword", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("StartingKokiriSword")));
    AddComboCheckbox(this, path, "Start with Master Sword", "Starting.OoT.MasterSword", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("StartingMasterSword")));
    AddComboCheckbox(this, path, "Start with Deku Shield", "Starting.OoT.DekuShield", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("StartingDekuShield")));
    AddComboBox(this, path, "Start with Ocarina", "Starting.OoT.Ocarina", startingOcarina, 0, nullptr,
                SyncInt(CVAR_RANDOMIZER_SETTING("StartingOcarina")));
    path.column = SECTION_COLUMN_2;
    AddComboCheckbox(this, path, "Start with Song of Time", "Starting.SongOfTime", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("StartingSongOfTime")));
    AddComboCheckbox(this, path, "Start with Prelude of Light", "Starting.OoT.PreludeOfLight", nullptr,
                     SyncBool(CVAR_RANDOMIZER_SETTING("StartingPreludeOfLight")));
    AddComboCheckbox(this, path, "MM Start with Full Wallet", "Starting.MM.FullWallet");
    AddComboCheckbox(this, path, "MM Start with Full Consumables", "Starting.MM.FullConsumables");
    AddComboSlider(this, path, "MM Starting Health: %d", "Starting.MM.Health", 1, 20, 3);
}

} // namespace SohGui
