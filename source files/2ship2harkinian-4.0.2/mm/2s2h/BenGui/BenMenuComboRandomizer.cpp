#include "BenMenu.h"
#include "2s2h/Rando/Rando.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

extern "C" {
#include "overlays/actors/ovl_En_Sth/z_en_sth.h"
}

namespace BenGui {

using namespace UIWidgets;

namespace {

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

static std::unordered_map<int32_t, const char*> offOn = {
    { 0, "Off" },
    { 1, "On" },
};

static std::unordered_map<int32_t, const char*> openClosed = {
    { 0, "Open" },
    { 1, "Closed" },
};

static std::unordered_map<int32_t, const char*> skipGetItemAnimations = {
    { 0, "Disabled" },
    { 1, "Junk Items" },
};

static std::unordered_map<int32_t, const char*> dungeonEntrances = {
    { 0, "Off" },
    { 1, "On" },
    { 2, "On + Ganon + Majora" },
};

static std::unordered_map<int32_t, const char*> trialsAccess = {
    { 0, "Requires Associated Remains" },
    { 1, "Open" },
};

static std::unordered_map<int32_t, const char*> dungeonItemLocation = {
    { 0, "Start With" },
    { 1, "Own Dungeon" },
    { 2, "Any Dungeon" },
    { 3, "Anywhere" },
};

static std::unordered_map<int32_t, const char*> dungeonRewards = {
    { 0, "Vanilla" },
    { 1, "End of Dungeons" },
    { 2, "Anywhere" },
};

static std::unordered_map<int32_t, const char*> gerudoKeys = {
    { 0, "Vanilla" },
    { 1, "Anywhere" },
};

static std::unordered_map<int32_t, const char*> linksPocket = {
    { 0, "Dungeon Reward" },
    { 1, "Advancement" },
    { 2, "Anything" },
    { 3, "Nothing" },
};

static std::unordered_map<int32_t, const char*> startingOcarina = {
    { 0, "Off" },
    { 1, "Fairy Ocarina" },
    { 2, "Ocarina of Time" },
};

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
                { "sourceGame", "mm" },
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

static void SetOption(RandoOptionId option, int32_t value) {
    CVarSetInteger(Rando::StaticData::Options[option].cvar, value);
}

static int32_t GetOption(RandoOptionId option, int32_t defaultValue = 0) {
    return CVarGetInteger(Rando::StaticData::Options[option].cvar, defaultValue);
}

static void SetBoolOption(RandoOptionId option, bool enabled) {
    SetOption(option, enabled ? RO_GENERIC_ON : RO_GENERIC_OFF);
}

static void ApplyComboSettingsToMmInternal() {
    CVarSetInteger("gRando.CSMC", ComboGet("General.ContainerStyleMatchesContents") != 0);
    CVarSetInteger("gRando.JunkItems", ComboGet("General.SkipGetItemAnimations", 1) != 0 ? 0 : 1);
    CVarSetInteger("gRando.GenerateSpoiler", 1);

    SetOption(RO_LOGIC, RO_LOGIC_GLITCHLESS);
    SetOption(RO_ACCESS_DUNGEONS, RO_ACCESS_DUNGEONS_FORM_AND_SONG);
    CVarSetInteger(ComboCVar("Logic.DungeonEntrances"), ComboGet("Logic.DungeonEntrances", 0));
    SetOption(RO_ACCESS_MAJORA_REMAINS_COUNT, ComboGet("Logic.MM.MajoraRemainsRequired") ? 4 : 0);
    SetOption(RO_ACCESS_MAJORA_MASKS_COUNT, ComboGet("Logic.MM.MajoraMasksRequired") ? 4 : 0);
    SetOption(RO_ACCESS_TRIALS, ComboGet("Logic.MM.TrialsAccess") ? RO_ACCESS_TRIALS_OPEN : RO_ACCESS_TRIALS_REMAINS);
    SetOption(RO_ACCESS_MOON_MASKS_COUNT, 0);
    SetOption(RO_ACCESS_MOON_REMAINS_COUNT, 4);

    const bool dungeonItemsStartWith = ComboGet("Dungeons.MapsCompasses") == 0;
    SetBoolOption(RO_STARTING_MAPS_AND_COMPASSES, dungeonItemsStartWith);
    SetBoolOption(RO_SHUFFLE_BOSS_SOULS, ComboGet("Dungeons.BossSouls") != 0);

    SetBoolOption(RO_SHUFFLE_GOLD_SKULLTULAS, ComboGet("Items.GoldSkulltulaTokens") != 0);
    SetOption(RO_SKULLTULA_TOKENS_MAX, SPIDER_HOUSE_TOKENS_REQUIRED);
    SetOption(RO_SKULLTULA_TOKENS_REQUIRED, SPIDER_HOUSE_TOKENS_REQUIRED);
    SetOption(RO_STRAY_FAIRIES_MAX, ComboGet("Items.MM.StrayFairies") ? STRAY_FAIRY_SCATTERED_TOTAL : 0);
    SetOption(RO_STRAY_FAIRIES_REQUIRED, ComboGet("Items.MM.StrayFairies") ? STRAY_FAIRY_SCATTERED_TOTAL : 0);
    SetBoolOption(RO_SHUFFLE_COWS, ComboGet("Items.Cows") != 0);
    SetBoolOption(RO_SHUFFLE_POT_DROPS, ComboGet("Items.Pots") != 0);
    SetBoolOption(RO_SHUFFLE_GRASS_DROPS, ComboGet("Items.Grass") != 0);
    SetBoolOption(RO_SHUFFLE_CRATE_DROPS, ComboGet("Items.Crates") != 0);
    SetBoolOption(RO_SHUFFLE_SONG_SUN, ComboGet("Items.MM.SunsSong") != 0);

    SetBoolOption(RO_SHUFFLE_SHOPS, ComboGet("Shuffles.Shops") != 0);
    SetBoolOption(RO_SHUFFLE_BOSS_REMAINS, ComboGet("Shuffles.MM.BossRemains") != 0);
    SetBoolOption(RO_SHUFFLE_OWL_STATUES, ComboGet("Shuffles.MM.OwlStatues") != 0);
    SetBoolOption(RO_SHUFFLE_BARREL_DROPS, ComboGet("Items.Crates") != 0);
    SetBoolOption(RO_SHUFFLE_FREESTANDING_ITEMS, ComboGet("Shuffles.FreestandingItems") != 0);
    SetBoolOption(RO_SHUFFLE_OCARINA_BUTTONS, ComboGet("Shuffles.OcarinaButtons") != 0);
    SetBoolOption(RO_SHUFFLE_ENEMY_SOULS, ComboGet("Shuffles.MM.EnemySouls") != 0);
    SetBoolOption(RO_CLOCK_SHUFFLE, ComboGet("Shuffles.MM.Time") != 0);
    SetOption(RO_CLOCK_SHUFFLE_PROGRESSIVE, RO_CLOCK_SHUFFLE_RANDOM);
    SetOption(RO_CLOCK_TERMINAL_TIME, 0);
    SetBoolOption(RO_PLENTIFUL_ITEMS, false);
    SetBoolOption(RO_SHUFFLE_TRIFORCE_PIECES, false);
    SetBoolOption(RO_SHUFFLE_SWIM, false);
    SetBoolOption(RO_SHUFFLE_TINGLE_SHOPS, ComboGet("Shuffles.Shops") != 0);
    SetBoolOption(RO_SHUFFLE_TREE_DROPS, false);

    const int32_t trapCount = ComboGet("HintsTraps.IceTrapCount");
    SetBoolOption(RO_SHUFFLE_TRAPS, trapCount > 0);
    SetOption(RO_TRAP_AMOUNT, trapCount > 0 ? trapCount : 5);
    SetBoolOption(RO_HINTS_GOSSIP_STONES, ComboGet("Hints.GossipStones", 1) != 0);
    SetBoolOption(RO_HINTS_SPIDER_HOUSES, ComboGet("Hints.MM.SpiderHouse", 1) != 0);
    SetBoolOption(RO_HINTS_PURCHASEABLE, ComboGet("Hints.GossipStones", 1) != 0);
    SetBoolOption(RO_HINTS_BOSS_REMAINS, ComboGet("Hints.MM.BossRemains", 1) != 0);
    SetBoolOption(RO_HINTS_OATH_TO_ORDER, ComboGet("Hints.MM.OathToOrder", 1) != 0);
    SetBoolOption(RO_HINTS_HOOKSHOT, ComboGet("Hints.MM.Hookshot", 1) != 0);
    SetBoolOption(RO_HINTS_SONG_OF_SOARING, ComboGet("Hints.MM.SongOfSoaring", 1) != 0);

    SetBoolOption(RO_STARTING_RUPEES, ComboGet("Starting.MM.FullWallet") != 0);
    SetBoolOption(RO_STARTING_CONSUMABLES, ComboGet("Starting.MM.FullConsumables") != 0);
    SetOption(RO_STARTING_HEALTH, ComboGet("Starting.MM.Health", 3));
}

static void SyncComboFromMm() {
    ComboSet("General.ContainerStyleMatchesContents", CVarGetInteger("gRando.CSMC", 1));
    ComboSet("General.SkipGetItemAnimations", CVarGetInteger("gRando.JunkItems", 0) == 0 ? 1 : 0);
    if (GetOption(RO_ACCESS_DUNGEONS) != RO_ACCESS_DUNGEONS_OPEN) {
        ComboSet("Logic.DungeonEntrances", 0);
    }
    ComboSet("Logic.MM.MajoraRemainsRequired", GetOption(RO_ACCESS_MAJORA_REMAINS_COUNT) >= 4);
    ComboSet("Logic.MM.MajoraMasksRequired", GetOption(RO_ACCESS_MAJORA_MASKS_COUNT) >= 4);
    ComboSet("Logic.MM.TrialsAccess", GetOption(RO_ACCESS_TRIALS) == RO_ACCESS_TRIALS_OPEN);
    ComboSet("Dungeons.BossSouls", GetOption(RO_SHUFFLE_BOSS_SOULS));
    ComboSet("Items.GoldSkulltulaTokens", GetOption(RO_SHUFFLE_GOLD_SKULLTULAS));
    ComboSet("Items.MM.StrayFairies", GetOption(RO_STRAY_FAIRIES_MAX) != 0);
    ComboSet("Items.Cows", GetOption(RO_SHUFFLE_COWS));
    ComboSet("Items.Pots", GetOption(RO_SHUFFLE_POT_DROPS));
    ComboSet("Items.Grass", GetOption(RO_SHUFFLE_GRASS_DROPS));
    ComboSet("Items.Crates", GetOption(RO_SHUFFLE_CRATE_DROPS) || GetOption(RO_SHUFFLE_BARREL_DROPS));
    ComboSet("Items.MM.SunsSong", GetOption(RO_SHUFFLE_SONG_SUN));
    ComboSet("Shuffles.Shops", GetOption(RO_SHUFFLE_SHOPS));
    ComboSet("Shuffles.MM.BossRemains", GetOption(RO_SHUFFLE_BOSS_REMAINS));
    ComboSet("Shuffles.MM.OwlStatues", GetOption(RO_SHUFFLE_OWL_STATUES));
    ComboSet("Shuffles.FreestandingItems", GetOption(RO_SHUFFLE_FREESTANDING_ITEMS));
    ComboSet("Shuffles.OcarinaButtons", GetOption(RO_SHUFFLE_OCARINA_BUTTONS));
    ComboSet("Shuffles.MM.EnemySouls", GetOption(RO_SHUFFLE_ENEMY_SOULS));
    ComboSet("Shuffles.MM.Time", GetOption(RO_CLOCK_SHUFFLE));
    ComboSet("HintsTraps.IceTrapCount", GetOption(RO_SHUFFLE_TRAPS) ? GetOption(RO_TRAP_AMOUNT, 5) : 0);
    ComboSet("Hints.GossipStones", GetOption(RO_HINTS_GOSSIP_STONES) || GetOption(RO_HINTS_PURCHASEABLE));
    ComboSet("Hints.MM.SpiderHouse", GetOption(RO_HINTS_SPIDER_HOUSES));
    ComboSet("Starting.MM.FullWallet", GetOption(RO_STARTING_RUPEES));
    ComboSet("Starting.MM.FullConsumables", GetOption(RO_STARTING_CONSUMABLES));
    ComboSet("Starting.MM.Health", GetOption(RO_STARTING_HEALTH, 3));
}

static void AddNote(BenMenu* menu, WidgetPath& path, const char* text) {
    menu->AddWidget(path, text, WIDGET_TEXT);
}

static void ApplyComboWidget(WidgetInfo& info) {
    SaveSharedComboSettings();
    ApplyComboSettingsToMmInternal();
}

static void SyncContainerStyleMatchesContents(WidgetInfo& info) {
    const bool enabled = CVarGetInteger(info.cVar, 0) != 0;
    CVarSetInteger("gRando.CSMC", enabled);
    CVarSetInteger("gEnhancements.ChestSizeAndTextureMatchContents", enabled);
    SaveSharedComboSettings();
    ApplyComboSettingsToMmInternal();
}

static WidgetInfo& AddComboCheckbox(BenMenu* menu, WidgetPath& path, const char* label, const char* key,
                                    const char* tooltip = nullptr, WidgetFunc callback = nullptr,
                                    bool defaultValue = false) {
    auto& widget = menu->AddWidget(path, label, WIDGET_CVAR_CHECKBOX)
                       .CVar(ComboCVar(key))
                       .Options(CheckboxOptions().DefaultValue(defaultValue).Tooltip(tooltip == nullptr ? "" : tooltip));
    if (callback != nullptr) {
        widget.Callback(callback);
    } else {
        widget.Callback(ApplyComboWidget);
    }
    return widget;
}

static WidgetInfo& AddComboBox(BenMenu* menu, WidgetPath& path, const char* label, const char* key,
                               std::unordered_map<int32_t, const char*>& options, int32_t defaultIndex = 0,
                               const char* tooltip = nullptr, WidgetFunc callback = nullptr) {
    auto& widget = menu->AddWidget(path, label, WIDGET_CVAR_COMBOBOX)
                       .CVar(ComboCVar(key))
                       .Options(ComboboxOptions()
                                    .ComboMap(&options)
                                    .DefaultIndex(defaultIndex)
                                    .Tooltip(tooltip == nullptr ? "" : tooltip));
    if (callback != nullptr) {
        widget.Callback(callback);
    } else {
        widget.Callback(ApplyComboWidget);
    }
    return widget;
}

static WidgetInfo& AddComboSlider(BenMenu* menu, WidgetPath& path, const char* label, const char* key, int32_t min,
                                  int32_t max, int32_t defaultValue = 0, const char* tooltip = nullptr,
                                  WidgetFunc callback = nullptr) {
    auto& widget = menu->AddWidget(path, label, WIDGET_CVAR_SLIDER_INT)
                       .CVar(ComboCVar(key))
                       .Options(IntSliderOptions()
                                    .Min(min)
                                    .Max(max)
                                    .DefaultValue(defaultValue)
                                    .ShowAdjustmentButtons(true)
                                    .Tooltip(tooltip == nullptr ? "" : tooltip));
    if (callback != nullptr) {
        widget.Callback(callback);
    } else {
        widget.Callback(ApplyComboWidget);
    }
    return widget;
}

static void AddAreaAccessPlaceholders(BenMenu* menu, WidgetPath& path) {
    menu->AddWidget(path, "OoT Area Access", WIDGET_SEPARATOR_TEXT);
    AddComboBox(menu, path, "Forest", "OoT.Access.Forest", openClosed, 1);
    AddComboBox(menu, path, "Kakariko Gate", "OoT.Access.KakarikoGate", openClosed, 1);
    AddComboBox(menu, path, "Door of Time", "OoT.Access.DoorOfTime", openClosed, 1);
    AddComboBox(menu, path, "Zora's Fountain", "OoT.Access.ZorasFountain", openClosed, 1);
    AddComboBox(menu, path, "Jabu-Jabu", "OoT.Access.JabuJabu", openClosed, 1);
}

} // namespace

void BenMenu::AddComboRandomizer() {
    LoadSharedComboSettings();
    ApplyComboSettingsToMmInternal();
    ComboSet("Initialized", 1);

    AddMenuEntry("Combo Randomizer", "gSettings.Menu.ComboRandomizerSidebarSection");

    WidgetPath path = { "Combo Randomizer", "General", SECTION_COLUMN_1 };
    AddSidebarEntry("Combo Randomizer", path.sidebarName, 2);
    AddWidget(path, "Seed", WIDGET_SEPARATOR_TEXT);
    AddNote(this, path,
            "Combo settings are mirrored automatically between OoT and MM.");
    AddWidget(path, "Generate Combo Randomizer", WIDGET_BUTTON)
        .Callback([](WidgetInfo& info) {
            SaveSharedComboSettings();
            ApplyComboSettingsToMmInternal();
            CVarSetInteger("gRando.Enabled", 1);
            CVarSetInteger("gRando.SpoilerFileIndex", 0);
            CVarSetString("gRando.SpoilerFile", "");
        })
        .Options(ButtonOptions()
                     .Size(ImVec2(250.f, 0.f))
                     .Tooltip("Enables MM rando generation for the next new file using the shared combo settings."));
    AddComboCheckbox(this, path, "Container Style Matches Contents", "General.ContainerStyleMatchesContents", nullptr,
                     SyncContainerStyleMatchesContents);
    AddComboBox(this, path, "Skip Get Item Animations", "General.SkipGetItemAnimations", skipGetItemAnimations, 1);
    AddComboCheckbox(this, path, "Quest Item Fanfares", "General.QuestItemFanfares");
    AddComboCheckbox(this, path, "Mysterious Shuffled Items", "General.MysteriousShuffledItems");
    AddComboCheckbox(this, path, "Simpler Boss Souls", "General.SimplerBossSouls");

    path.column = SECTION_COLUMN_2;
    AddWidget(path, "Forced Defaults", WIDGET_SEPARATOR_TEXT);
    AddComboCheckbox(this, path, "Rando-Relevant Navi Hints", "General.RandoRelevantNavi", nullptr, nullptr, true);
    AddComboCheckbox(this, path, "Use Custom Key Models", "General.CustomKeyModels", nullptr, nullptr, true);
    AddComboCheckbox(this, path, "Map/Compass Colors Match Dungeon", "General.ColoredMapsCompasses");

    path = { "Combo Randomizer", "Logic / Access", SECTION_COLUMN_1 };
    AddSidebarEntry("Combo Randomizer", path.sidebarName, 2);
    AddComboBox(this, path, "Dungeon Entrances", "Logic.DungeonEntrances", dungeonEntrances);
    AddComboSlider(this, path, "Big Poe Target Count: %d", "Logic.BigPoeTargetCount", 0, 10);
    AddComboCheckbox(this, path, "Skip Child Zelda", "Logic.SkipChildZelda");
    AddComboCheckbox(this, path, "Skip Child Stealth", "Logic.SkipChildStealth");
    AddComboCheckbox(this, path, "Skip Epona Race", "Logic.SkipEponaRace");

    path.column = SECTION_COLUMN_2;
    AddComboCheckbox(this, path, "Majora Access Remains Required", "Logic.MM.MajoraRemainsRequired");
    AddComboCheckbox(this, path, "Majora Access Masks Required", "Logic.MM.MajoraMasksRequired");
    AddComboBox(this, path, "Trials Access", "Logic.MM.TrialsAccess", trialsAccess);
    AddAreaAccessPlaceholders(this, path);

    path = { "Combo Randomizer", "Dungeons", SECTION_COLUMN_1 };
    AddSidebarEntry("Combo Randomizer", path.sidebarName, 2);
    AddComboBox(this, path, "Maps/Compasses", "Dungeons.MapsCompasses", dungeonItemLocation);
    AddComboBox(this, path, "Small Key Shuffle", "Dungeons.SmallKeys", dungeonItemLocation, 1);
    AddComboBox(this, path, "Boss Key Shuffle", "Dungeons.BossKeys", dungeonItemLocation, 1);
    AddComboBox(this, path, "Shuffle Dungeon Rewards", "Dungeons.Rewards", dungeonRewards, 1);

    path.column = SECTION_COLUMN_2;
    AddComboBox(this, path, "Gerudo Fortress Keys", "Dungeons.GerudoKeys", gerudoKeys);
    AddComboCheckbox(this, path, "Shuffle Boss Souls", "Dungeons.BossSouls");
    AddComboCheckbox(this, path, "Key Rings", "Dungeons.KeyRings");

    path = { "Combo Randomizer", "Items", SECTION_COLUMN_1 };
    AddSidebarEntry("Combo Randomizer", path.sidebarName, 2);
    AddComboCheckbox(this, path, "Shuffle GS Tokens", "Items.GoldSkulltulaTokens");
    AddComboCheckbox(this, path, "Shuffle Stray Fairies", "Items.MM.StrayFairies");
    AddComboCheckbox(this, path, "Shuffle Starter Sword + Master Sword", "Items.Swords");
    AddComboCheckbox(this, path, "Shuffle Ocarinas", "Items.Ocarinas");
    AddComboCheckbox(this, path, "Shuffle Cows", "Items.Cows");
    AddComboCheckbox(this, path, "Shuffle Pots", "Items.Pots");
    AddComboCheckbox(this, path, "Shuffle Grass", "Items.Grass");
    AddComboCheckbox(this, path, "Shuffle Crates", "Items.Crates");

    path.column = SECTION_COLUMN_2;
    AddComboCheckbox(this, path, "Shuffle Beehives", "Items.Beehives");
    AddComboCheckbox(this, path, "Shuffle Frog Song Rupees", "Items.OoT.FrogSongRupees");
    AddComboCheckbox(this, path, "Shuffle Adult Trade", "Items.OoT.AdultTrade");
    AddComboCheckbox(this, path, "Shuffle Fairies in Fountains", "Items.OoT.FountainFairies");
    AddComboCheckbox(this, path, "Shuffle Sun's Song", "Items.MM.SunsSong");

    path = { "Combo Randomizer", "Shuffles", SECTION_COLUMN_1 };
    AddSidebarEntry("Combo Randomizer", path.sidebarName, 2);
    AddComboCheckbox(this, path, "Shuffle Shops", "Shuffles.Shops");
    AddComboCheckbox(this, path, "Shuffle Boss Remains", "Shuffles.MM.BossRemains");
    AddComboCheckbox(this, path, "Shuffle Owl Statues", "Shuffles.MM.OwlStatues");
    AddComboCheckbox(this, path, "Shops Affordable Prices", "Shuffles.ShopsAffordablePrices");
    AddComboCheckbox(this, path, "Scrubs Shuffle", "Shuffles.OoT.Scrubs");
    AddComboCheckbox(this, path, "Merchant Affordable Prices", "Shuffles.OoT.MerchantAffordablePrices");

    path.column = SECTION_COLUMN_2;
    AddComboCheckbox(this, path, "Include Tycoon Wallet", "Shuffles.OoT.TycoonWallet");
    AddComboCheckbox(this, path, "Shuffle Fishing Pole", "Shuffles.OoT.FishingPole");
    AddComboCheckbox(this, path, "Shuffle Ocarina Buttons", "Shuffles.OcarinaButtons");
    AddComboCheckbox(this, path, "Shuffle Roc's Feather", "Shuffles.OoT.RocsFeather");
    AddComboCheckbox(this, path, "Shuffle Skeleton Key", "Shuffles.OoT.SkeletonKey");
    AddComboCheckbox(this, path, "Shuffle Enemy Souls (MM)", "Shuffles.MM.EnemySouls");
    AddComboCheckbox(this, path, "Shuffle Time", "Shuffles.MM.Time");

    path = { "Combo Randomizer", "Hints / Traps", SECTION_COLUMN_1 };
    AddSidebarEntry("Combo Randomizer", path.sidebarName, 2);
    AddComboSlider(this, path, "Ice Traps in Pool: %d", "HintsTraps.IceTrapCount", 0, 200);
    AddComboCheckbox(this, path, "Gossip Stone Hints", "Hints.GossipStones", nullptr, nullptr, true);
    AddComboCheckbox(this, path, "Warp Song Hints", "Hints.OoT.WarpSongs", nullptr, nullptr, true);
    AddComboCheckbox(this, path, "Scrub Text Hints", "Hints.OoT.ScrubText", nullptr, nullptr, true);
    path.column = SECTION_COLUMN_2;
    AddComboCheckbox(this, path, "Merchant Text Hints", "Hints.OoT.MerchantText", nullptr, nullptr, true);
    AddComboCheckbox(this, path, "MM Spider House Hints", "Hints.MM.SpiderHouse", nullptr, nullptr, true);

    path = { "Combo Randomizer", "Starting Items", SECTION_COLUMN_1 };
    AddSidebarEntry("Combo Randomizer", path.sidebarName, 2);
    AddComboBox(this, path, "OoT Link's Pocket", "Starting.OoT.LinksPocket", linksPocket);
    AddComboCheckbox(this, path, "Start with Kokiri Sword", "Starting.OoT.KokiriSword");
    AddComboCheckbox(this, path, "Start with Master Sword", "Starting.OoT.MasterSword");
    AddComboCheckbox(this, path, "Start with Deku Shield", "Starting.OoT.DekuShield");
    AddComboBox(this, path, "Start with Ocarina", "Starting.OoT.Ocarina", startingOcarina);
    path.column = SECTION_COLUMN_2;
    AddComboCheckbox(this, path, "Start with Song of Time", "Starting.SongOfTime");
    AddComboCheckbox(this, path, "Start with Prelude of Light", "Starting.OoT.PreludeOfLight");
    AddComboCheckbox(this, path, "MM Start with Full Wallet", "Starting.MM.FullWallet");
    AddComboCheckbox(this, path, "MM Start with Full Consumables", "Starting.MM.FullConsumables");
    AddComboSlider(this, path, "MM Starting Health: %d", "Starting.MM.Health", 1, 20, 3);
}

} // namespace BenGui
