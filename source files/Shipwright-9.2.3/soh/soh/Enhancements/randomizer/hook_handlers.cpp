#include <libultraship/bridge.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <string>
#include <unordered_map>
#include <fast/resource/type/DisplayList.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/dungeon.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Crossover/Crossover.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/SohGui/ImGuiUtils.h"
#include "soh/Notification/Notification.h"
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "item_category_adj.h"
#include "assets/objects/object_gi_rupy/object_gi_rupy.h"

extern "C" {
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "soh/Enhancements/randomizer/ShuffleTradeItems.h"
#include "soh/Enhancements/randomizer/randomizer_entrance.h"
#include "soh/Enhancements/randomizer/randomizer_grotto.h"
#include "src/overlays/actors/ovl_Bg_Treemouth/z_bg_treemouth.h"
#include "src/overlays/actors/ovl_Bg_Jya_Bigmirror/z_bg_jya_bigmirror.h"
#include "src/overlays/actors/ovl_En_Si/z_en_si.h"
#include "src/overlays/actors/ovl_En_Ossan/z_en_ossan.h"
#include "src/overlays/actors/ovl_En_Shopnuts/z_en_shopnuts.h"
#include "src/overlays/actors/ovl_En_Dns/z_en_dns.h"
#include "src/overlays/actors/ovl_Item_B_Heart/z_item_b_heart.h"
#include "src/overlays/actors/ovl_En_Ko/z_en_ko.h"
#include "src/overlays/actors/ovl_En_Mk/z_en_mk.h"
#include "src/overlays/actors/ovl_En_Nb/z_en_nb.h"
#include "src/overlays/actors/ovl_En_Niw_Lady/z_en_niw_lady.h"
#include "src/overlays/actors/ovl_En_Kz/z_en_kz.h"
#include "src/overlays/actors/ovl_En_Ms/z_en_ms.h"
#include "src/overlays/actors/ovl_En_Fr/z_en_fr.h"
#include "src/overlays/actors/ovl_En_Syateki_Man/z_en_syateki_man.h"
#include "src/overlays/actors/ovl_En_Sth/z_en_sth.h"
#include "src/overlays/actors/ovl_Item_Etcetera/z_item_etcetera.h"
#include "src/overlays/actors/ovl_En_Box/z_en_box.h"
#include "src/overlays/actors/ovl_En_Skj/z_en_skj.h"
#include "src/overlays/actors/ovl_En_Hy/z_en_hy.h"
#include "src/overlays/actors/ovl_En_Bom_Bowl_Pit/z_en_bom_bowl_pit.h"
#include "src/overlays/actors/ovl_En_Ge1/z_en_ge1.h"
#include "src/overlays/actors/ovl_En_Ge2/z_en_ge2.h"
#include "src/overlays/actors/ovl_En_Ds/z_en_ds.h"
#include "src/overlays/actors/ovl_En_Dnt_Jiji/z_en_dnt_jiji.h"
#include "src/overlays/actors/ovl_En_Gm/z_en_gm.h"
#include "src/overlays/actors/ovl_En_Js/z_en_js.h"
#include "src/overlays/actors/ovl_En_Okarina_Tag/z_en_okarina_tag.h"
#include "src/overlays/actors/ovl_En_Door/z_en_door.h"
#include "src/overlays/actors/ovl_Door_Shutter/z_door_shutter.h"
#include "src/overlays/actors/ovl_Door_Gerudo/z_door_gerudo.h"
#include "src/overlays/actors/ovl_En_Xc/z_en_xc.h"
#include "src/overlays/actors/ovl_Fishing/z_fishing.h"
#include "src/overlays/actors/ovl_Obj_Bean/z_obj_bean.h"
#include "src/overlays/actors/ovl_En_Heishi2/z_en_heishi2.h"
#include "draw.h"

static ObjectExtension::Register<DnsItemEntry> RegisterDnsItemEntryOverride;
static ObjectExtension::Register<ScrubIdentity> RegisterScrubIdentity;

extern SaveContext gSaveContext;
extern PlayState* gPlayState;
extern void func_8084DFAC(PlayState* play, Player* player);
extern void func_80B8FE00(ObjBean*); // trigger planting
extern void Player_SetupActionPreserveAnimMovement(PlayState* play, Player* player, PlayerActionFunc actionFunc,
                                                   s32 flags);
extern s32 Player_SetupWaitForPutAway(PlayState* play, Player* player, AfterPutAwayFunc func);
extern void Play_InitEnvironment(PlayState* play, s16 skyboxId);
extern void EnMk_Wait(EnMk* enMk, PlayState* play);
extern void func_80ABA778(EnNiwLady* enNiwLady, PlayState* play);
extern void EnDntJiji_GivePrize(EnDntJiji* enDntJiji, PlayState* play);
extern void EnGe1_Wait_Archery(EnGe1* enGe1, PlayState* play);
extern void EnGe1_SetAnimationIdle(EnGe1* enGe1);
extern void EnGe1_SetAnimationIdle(EnGe1* enGe1);
extern void EnGe2_SetupCapturePlayer(EnGe2* enGe2, PlayState* play);
}

bool LocMatchesQuest(Rando::Location loc) {
    if (loc.GetQuest() == RCQUEST_BOTH) {
        return true;
    } else {
        auto dungeon = OTRGlobals::Instance->gRandoContext->GetDungeons()->GetDungeonFromScene(loc.GetScene());
        return (dungeon->IsMQ() && loc.GetQuest() == RCQUEST_MQ) ||
               (dungeon->IsVanilla() && loc.GetQuest() == RCQUEST_VANILLA);
    }
}

RandomizerCheck GetRandomizerCheckFromFlag(int16_t flagType, int16_t flag) {
    for (auto& loc : Rando::StaticData::GetLocationTable()) {
        if ((loc.GetCollectionCheck().flag == flag &&
                 ((flagType == FLAG_INF_TABLE && loc.GetCollectionCheck().type == SPOILER_CHK_INF_TABLE) ||
                  (flagType == FLAG_EVENT_CHECK_INF && loc.GetCollectionCheck().type == SPOILER_CHK_EVENT_CHK_INF) ||
                  (flagType == FLAG_ITEM_GET_INF && loc.GetCollectionCheck().type == SPOILER_CHK_ITEM_GET_INF) ||
                  (flagType == FLAG_RANDOMIZER_INF && loc.GetCollectionCheck().type == SPOILER_CHK_RANDOMIZER_INF)) ||
             (loc.GetActorParams() == flag && flagType == FLAG_GS_TOKEN &&
              loc.GetCollectionCheck().type == SPOILER_CHK_GOLD_SKULLTULA)) &&
            LocMatchesQuest(loc)) {
            return loc.GetRandomizerCheck();
        }
    }

    return RC_UNKNOWN_CHECK;
}

RandomizerCheck GetRandomizerCheckFromSceneFlag(int16_t sceneNum, int16_t flagType, int16_t flag) {
    for (auto& loc : Rando::StaticData::GetLocationTable()) {
        if (loc.GetCollectionCheck().scene == sceneNum && loc.GetCollectionCheck().flag == flag &&
            ((flagType == FLAG_SCENE_TREASURE && loc.GetCollectionCheck().type == SPOILER_CHK_CHEST) ||
             (flagType == FLAG_SCENE_COLLECTIBLE && loc.GetCollectionCheck().type == SPOILER_CHK_COLLECTABLE) ||
             (flagType == FLAG_GS_TOKEN && loc.GetCollectionCheck().type == SPOILER_CHK_GOLD_SKULLTULA)) &&
            LocMatchesQuest(loc)) {
            return loc.GetRandomizerCheck();
        }
    }

    return RC_UNKNOWN_CHECK;
}

bool MeetsLACSRequirements() {
    switch (RAND_GET_OPTION(RSK_GANONS_BOSS_KEY).Get()) {
        case RO_GANON_BOSS_KEY_LACS_STONES:
            if ((CheckStoneCount() + CheckLACSRewardCount()) >= RAND_GET_OPTION(RSK_LACS_STONE_COUNT).Get()) {
                return true;
            }
            break;
        case RO_GANON_BOSS_KEY_LACS_MEDALLIONS:
            if ((CheckMedallionCount() + CheckLACSRewardCount()) >= RAND_GET_OPTION(RSK_LACS_MEDALLION_COUNT).Get()) {
                return true;
            }
            break;
        case RO_GANON_BOSS_KEY_LACS_REWARDS:
            if ((CheckMedallionCount() + CheckStoneCount() + CheckLACSRewardCount()) >=
                RAND_GET_OPTION(RSK_LACS_REWARD_COUNT).Get()) {
                return true;
            }
            break;
        case RO_GANON_BOSS_KEY_LACS_DUNGEONS:
            if ((CheckDungeonCount() + CheckLACSRewardCount()) >= RAND_GET_OPTION(RSK_LACS_DUNGEON_COUNT).Get()) {
                return true;
            }
            break;
        case RO_GANON_BOSS_KEY_LACS_TOKENS:
            if (gSaveContext.inventory.gsTokens >= RAND_GET_OPTION(RSK_LACS_TOKEN_COUNT).Get()) {
                return true;
            }
            break;
        default:
            if (CHECK_QUEST_ITEM(QUEST_MEDALLION_SPIRIT) && CHECK_QUEST_ITEM(QUEST_MEDALLION_SHADOW)) {
                return true;
            }
            break;
    }

    return false;
}

bool CompletedAllTrials() {
    return Flags_GetEventChkInf(EVENTCHKINF_COMPLETED_WATER_TRIAL) &&
           Flags_GetEventChkInf(EVENTCHKINF_COMPLETED_LIGHT_TRIAL) &&
           Flags_GetEventChkInf(EVENTCHKINF_COMPLETED_FIRE_TRIAL) &&
           Flags_GetEventChkInf(EVENTCHKINF_COMPLETED_SHADOW_TRIAL) &&
           Flags_GetEventChkInf(EVENTCHKINF_COMPLETED_SPIRIT_TRIAL) &&
           Flags_GetEventChkInf(EVENTCHKINF_COMPLETED_FOREST_TRIAL);
}

bool MeetsRainbowBridgeRequirements() {
    switch (RAND_GET_OPTION(RSK_RAINBOW_BRIDGE).Get()) {
        case RO_BRIDGE_VANILLA: {
            if (CHECK_QUEST_ITEM(QUEST_MEDALLION_SPIRIT) && CHECK_QUEST_ITEM(QUEST_MEDALLION_SHADOW) &&
                (INV_CONTENT(ITEM_ARROW_LIGHT) == ITEM_ARROW_LIGHT)) {
                return true;
            }
            break;
        }
        case RO_BRIDGE_STONES: {
            if ((CheckStoneCount() + CheckBridgeRewardCount()) >=
                RAND_GET_OPTION(RSK_RAINBOW_BRIDGE_STONE_COUNT).Get()) {
                return true;
            }
            break;
        }
        case RO_BRIDGE_MEDALLIONS: {
            if ((CheckMedallionCount() + CheckBridgeRewardCount()) >=
                RAND_GET_OPTION(RSK_RAINBOW_BRIDGE_MEDALLION_COUNT).Get()) {
                return true;
            }
            break;
        }
        case RO_BRIDGE_DUNGEON_REWARDS: {
            if ((CheckMedallionCount() + CheckStoneCount() + CheckBridgeRewardCount()) >=
                RAND_GET_OPTION(RSK_RAINBOW_BRIDGE_REWARD_COUNT).Get()) {
                return true;
            }
            break;
        }
        case RO_BRIDGE_DUNGEONS: {
            if ((CheckDungeonCount() + CheckBridgeRewardCount()) >=
                RAND_GET_OPTION(RSK_RAINBOW_BRIDGE_DUNGEON_COUNT).Get()) {
                return true;
            }
            break;
        }
        case RO_BRIDGE_TOKENS: {
            if (gSaveContext.inventory.gsTokens >= RAND_GET_OPTION(RSK_RAINBOW_BRIDGE_TOKEN_COUNT).Get()) {
                return true;
            }
            break;
        }
        case RO_BRIDGE_GREG: {
            if (Flags_GetRandomizerInf(RAND_INF_GREG_FOUND)) {
                return true;
            }
            break;
        }
        case RO_BRIDGE_ALWAYS_OPEN: {
            return true;
        }
    }

    return false;
}

// Todo Move this to randomizer context, clear it out on save load etc
static std::queue<RandomizerCheck> randomizerQueuedChecks;
static RandomizerCheck randomizerQueuedCheck = RC_UNKNOWN_CHECK;
static GetItemEntry randomizerQueuedItemEntry = GET_ITEM_NONE;
static bool randomizerQueuedItemGiveStarted = false;
static uint8_t randomizerQueuedItemGiveSettledFrames = 0;
static bool randomizerQueuedEmitWindowMessage = false;
static void RandomizerCompleteQueuedCheckAfterNativeGive();

static void EmitWindowMessageForGetItemEntry(GetItemEntry getItemEntry) {
    if (getItemEntry.modIndex == MOD_NONE) {
        std::string message;

        switch (gSaveContext.language) {
            case LANGUAGE_FRA:
                message = "Vous obtenez: ";
                break;
            case LANGUAGE_GER:
                message = "Du erhältst: ";
                break;
            case LANGUAGE_ENG:
            default:
                message = "You found ";
                break;
        }

        Notification::Emit({
            .itemIcon = GetTextureForItemId(getItemEntry.itemId),
            .message = message,
            .suffix = SohUtils::GetItemName(getItemEntry.itemId),
        });
        return;
    }

    if (getItemEntry.modIndex == MOD_RANDOMIZER) {
        std::string message;
        std::string itemName;

        switch (gSaveContext.language) {
            case LANGUAGE_FRA:
                message = "Vous obtenez: ";
                itemName =
                    Rando::StaticData::RetrieveItem((RandomizerGet)getItemEntry.getItemId).GetName().french;
                break;
            case LANGUAGE_GER:
                message = "Du erhältst: ";
                itemName =
                    Rando::StaticData::RetrieveItem((RandomizerGet)getItemEntry.getItemId).GetName().german;
                break;
            case LANGUAGE_ENG:
            default:
                message = "You found ";
                itemName =
                    Rando::StaticData::RetrieveItem((RandomizerGet)getItemEntry.getItemId).GetName().english;
                break;
        }

        Notification::Emit({
            .message = message,
            .suffix = itemName,
        });
    }
}

static void EmitWindowMessageForRandomizerCheck(RandomizerCheck rc, GetItemEntry getItemEntry) {
    if (SohCrossover::IsCrossGameMmCheck(rc)) {
        const auto crossoverMessage = SohCrossover::GetCrossGameMmPickupMessage(rc);
        if (crossoverMessage.has_value()) {
            Notification::Emit({
                .message = *crossoverMessage,
            });
            return;
        }
    }

    EmitWindowMessageForGetItemEntry(getItemEntry);
}

enum OoTxMmForeignDrawId : uint16_t {
    OOTXMM_DRAW_MM_COUPLES_MASK = 0x7F00,
    OOTXMM_DRAW_MM_DEKU_MASK,
    OOTXMM_DRAW_MM_FIERCE_DEITY_MASK,
    OOTXMM_DRAW_MM_GIANT_MASK,
    OOTXMM_DRAW_MM_POSTMAN_MASK,
    OOTXMM_DRAW_MM_GREAT_FAIRY_MASK,
    OOTXMM_DRAW_MM_BLAST_MASK,
    OOTXMM_DRAW_MM_GIBDO_MASK,
    OOTXMM_DRAW_MM_GARO_MASK,
    OOTXMM_DRAW_MM_KAFEI_MASK,
    OOTXMM_DRAW_MM_ALL_NIGHT_MASK,
    OOTXMM_DRAW_MM_ROMANI_MASK,
    OOTXMM_DRAW_MM_CIRCUS_LEADER_MASK,
    OOTXMM_DRAW_MM_DON_GERO_MASK,
    OOTXMM_DRAW_MM_KAMARO_MASK,
    OOTXMM_DRAW_MM_CAPTAIN_MASK,
    OOTXMM_DRAW_MM_BREMEN_MASK,
    OOTXMM_DRAW_MM_MASK_OF_SCENTS,
    OOTXMM_DRAW_MM_STONE_MASK,
    OOTXMM_DRAW_MM_DUNGEON_MAP,
    OOTXMM_DRAW_MM_TINGLE_MAP,
    OOTXMM_DRAW_MM_COMPASS,
    OOTXMM_DRAW_MM_OWL_STATUE,
    OOTXMM_DRAW_MM_SKULLTULA_TOKEN,
    OOTXMM_DRAW_MM_SMALL_KEY,
    OOTXMM_DRAW_MM_BOSS_KEY,
    OOTXMM_DRAW_MM_STRAY_FAIRY,
    OOTXMM_DRAW_MM_ROOM_KEY,
    OOTXMM_DRAW_MM_LETTER_TO_MAMA,
    OOTXMM_DRAW_MM_LETTER_TO_KAFEI,
    OOTXMM_DRAW_MM_PENDANT,
    OOTXMM_DRAW_MM_MOONS_TEAR,
    OOTXMM_DRAW_MM_TITLE_DEED,
    OOTXMM_DRAW_MM_TITLE_DEED_LAND,
    OOTXMM_DRAW_MM_TITLE_DEED_SWAMP,
    OOTXMM_DRAW_MM_TITLE_DEED_MOUNTAIN,
    OOTXMM_DRAW_MM_TITLE_DEED_OCEAN,
    OOTXMM_DRAW_MM_BOMBERS_NOTEBOOK,
    OOTXMM_DRAW_MM_PICTO_BOX,
    OOTXMM_DRAW_MM_POWDER_KEG,
    OOTXMM_DRAW_MM_GOLD_DUST,
    OOTXMM_DRAW_MM_CHATEAU,
    OOTXMM_DRAW_MM_ZORA_EGG,
    OOTXMM_DRAW_MM_SEAHORSE,
    OOTXMM_DRAW_MM_HYLIAN_LOACH,
    OOTXMM_DRAW_MM_HERO_SHIELD,
    OOTXMM_DRAW_MM_MIRROR_SHIELD,
    OOTXMM_DRAW_MM_KOKIRI_SWORD,
    OOTXMM_DRAW_MM_RAZOR_SWORD,
    OOTXMM_DRAW_MM_GILDED_SWORD,
    OOTXMM_DRAW_MM_GREAT_FAIRY_SWORD,
    OOTXMM_DRAW_MM_HOOKSHOT,
    OOTXMM_DRAW_MM_LENS,
    OOTXMM_DRAW_MM_BOW,
    OOTXMM_DRAW_MM_BOMB_BAG,
    OOTXMM_DRAW_MM_QUIVER,
    OOTXMM_DRAW_MM_WALLET,
    OOTXMM_DRAW_MM_REMAINS_ODOLWA,
    OOTXMM_DRAW_MM_REMAINS_GOHT,
    OOTXMM_DRAW_MM_REMAINS_GYORG,
    OOTXMM_DRAW_MM_REMAINS_TWINMOLD,
    OOTXMM_DRAW_MM_SOUL_GENERIC,
    OOTXMM_DRAW_MM_SUN_MASK,
    OOTXMM_DRAW_MM_SONG_NOTE,
    OOTXMM_DRAW_MM_SONG_EPONA,
    OOTXMM_DRAW_MM_SONG_NOVA,
    OOTXMM_DRAW_MM_SONG_SONATA,
    OOTXMM_DRAW_MM_SONG_SOARING,
    OOTXMM_DRAW_MM_SONG_ELEGY,
    OOTXMM_DRAW_MM_SONG_LULLABY_INTRO,
    OOTXMM_DRAW_MM_SONG_LULLABY,
    OOTXMM_DRAW_MM_SONG_OATH,
    OOTXMM_DRAW_MM_TIME_DAY,
    OOTXMM_DRAW_MM_TIME_NIGHT,
    OOTXMM_DRAW_MM_FROG_BLUE,
    OOTXMM_DRAW_MM_FROG_CYAN,
    OOTXMM_DRAW_MM_FROG_PINK,
    OOTXMM_DRAW_MM_FROG_WHITE,
    OOTXMM_DRAW_MM_OCARINA_BUTTON_A,
    OOTXMM_DRAW_MM_OCARINA_BUTTON_C_DOWN,
    OOTXMM_DRAW_MM_OCARINA_BUTTON_C_RIGHT,
    OOTXMM_DRAW_MM_OCARINA_BUTTON_C_LEFT,
    OOTXMM_DRAW_MM_OCARINA_BUTTON_C_UP,
};

struct OoTxMmForeignDrawInfo {
    const char* path;
    const char* path2;
    float baseTranslateY;
    float scale;
    float rotateX;
    float rotateY;
    float rotateZ;
    bool loadFromMmO2r = true;
    const char* path3 = nullptr;
    const char* path4 = nullptr;
};

constexpr float kOoTxMmForeignModelScaleMultiplier = 3.4f;
constexpr float kOoTxMmForeignModelHeightUnit = 72.0f;

static const std::unordered_map<uint16_t, OoTxMmForeignDrawInfo> sOoTxMmForeignDraws = {
    { OOTXMM_DRAW_MM_COUPLES_MASK,
      { "__OTR__objects/object_gi_mask13/gGiCouplesMaskFullDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_DEKU_MASK,
      { "__OTR__objects/object_gi_nutsmask/gGiDekuMaskDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_FIERCE_DEITY_MASK,
      { "__OTR__objects/object_gi_mask03/gGiFierceDeityMaskHairAndHatDL",
        "__OTR__objects/object_gi_mask03/gGiFierceDeityMaskFaceDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_GIANT_MASK,
      { "__OTR__objects/object_gi_mask23/gGiGiantMaskDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_POSTMAN_MASK,
      { "__OTR__objects/object_gi_mask12/gGiPostmanHatCapDL",
        "__OTR__objects/object_gi_mask12/gGiPostmanHatBunnyLogoDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_GREAT_FAIRY_MASK,
      { "__OTR__objects/object_gi_mask14/gGiGreatFairyMaskLeavesDL",
        "__OTR__objects/object_gi_mask14/gGiGreatFairyMaskFaceDL", -72.0f, 0.3168f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_BLAST_MASK,
      { "__OTR__objects/object_gi_mask21/gGiBlastMaskDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_GIBDO_MASK,
      { "__OTR__objects/object_gi_mask15/gGiGibdoMaskDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_GARO_MASK,
      { "__OTR__objects/object_gi_mask09/gGiGarosMaskCloakDL",
        "__OTR__objects/object_gi_mask09/gGiGarosMaskFaceDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_KAFEI_MASK,
      { "__OTR__objects/object_gi_mask05/gGiKafeiMaskDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_ALL_NIGHT_MASK,
      { "__OTR__objects/object_gi_mask06/gGiAllNightMaskFaceDL",
        "__OTR__objects/object_gi_mask06/gGiAllNightMaskEyesDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_ROMANI_MASK,
      { "__OTR__objects/object_gi_mask10/gGiRomaniMaskCapDL",
        "__OTR__objects/object_gi_mask10/gGiRomaniMaskNoseEyeDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_CIRCUS_LEADER_MASK,
      { "__OTR__objects/object_gi_mask11/gGiCircusLeaderMaskFaceDL",
        "__OTR__objects/object_gi_mask11/gGiCircusLeaderMaskEyebrowsDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_DON_GERO_MASK,
      { "__OTR__objects/object_gi_mask16/gGiDonGeroMaskBodyDL",
        "__OTR__objects/object_gi_mask16/gGiDonGeroMaskFaceDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_KAMARO_MASK,
      { "__OTR__objects/object_gi_mask17/gGiKamaroMaskDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_CAPTAIN_MASK,
      { "__OTR__objects/object_gi_mask18/gGiCaptainsHatBodyDL",
        "__OTR__objects/object_gi_mask18/gGiCaptainsHatFaceDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_BREMEN_MASK,
      { "__OTR__objects/object_gi_mask20/gGiBremenMaskDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_MASK_OF_SCENTS,
      { "__OTR__objects/object_gi_mask22/gGiMaskOfScentsFaceDL",
        "__OTR__objects/object_gi_mask22/gGiMaskOfScentsTeethDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_STONE_MASK,
      { "__OTR__objects/object_gi_stonemask/gGiStoneMaskDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_DUNGEON_MAP,
      { "__OTR__objects/object_gi_map/gGiDungeonMapDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_TINGLE_MAP,
      { "__OTR__objects/object_gi_fieldmap/gGiTingleMapDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_COMPASS,
      { "__OTR__objects/object_gi_compass/gGiCompassDL", "__OTR__objects/object_gi_compass/gGiCompassGlassDL", -72.0f,
        0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_OWL_STATUE,
      { "__OTR__objects/object_sek/gOwlStatueOpenedDL", nullptr, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_SKULLTULA_TOKEN,
      { "__OTR__objects/object_gi_sutaru/gGiSkulltulaTokenDL",
        "__OTR__objects/object_gi_sutaru/gGiSkulltulaTokenFlameDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_SMALL_KEY,
      { "__OTR__objects/object_gi_key/gGiSmallKeyDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_BOSS_KEY,
      { "__OTR__objects/object_gi_bosskey/gGiBossKeyDL", "__OTR__objects/object_gi_bosskey/gGiBossKeyGemDL", -72.0f,
        0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_STRAY_FAIRY,
      { "__OTR__objects/object_gi_soul/gGiFairyContainerGlassDL",
        "__OTR__objects/object_gi_soul/gGiFairyContainerContentsDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_ROOM_KEY,
      { "__OTR__objects/object_gi_reserve_b_00/gGiRoomKeyDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_LETTER_TO_MAMA,
      { "__OTR__objects/object_gi_reserve_b_01/gGiLetterToMamaEnvelopeLetterDL",
        "__OTR__objects/object_gi_reserve_b_01/gGiLetterToMamaInscriptionsDL", -72.0f, 0.3456f, 0.0f, 0.0f,
        0.0f } },
    { OOTXMM_DRAW_MM_LETTER_TO_KAFEI,
      { "__OTR__objects/object_gi_reserve_c_00/gGiLetterToKafeiEnvelopeLetterDL",
        "__OTR__objects/object_gi_reserve_c_00/gGiLetterToKafeiInscriptionsDL", -72.0f, 0.3456f, 0.0f, 0.0f,
        0.0f } },
    { OOTXMM_DRAW_MM_PENDANT,
      { "__OTR__objects/object_gi_reserve_c_01/gGiPendantOfMemoriesDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f,
        0.0f } },
    { OOTXMM_DRAW_MM_MOONS_TEAR,
      { "__OTR__objects/object_gi_reserve00/gGiMoonsTearItemDL", "__OTR__objects/object_gi_reserve00/gGiMoonsTearGlowDL",
        -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_TITLE_DEED,
      { "__OTR__objects/object_gi_reserve01/gGiTitleDeedPaperDL",
        "__OTR__objects/object_gi_reserve01/gGiTitleDeedSealAndRibbonDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_TITLE_DEED_LAND,
      { "__OTR__objects/object_gi_reserve01/gGiTitleDeedPaperDL",
        "__OTR__objects/object_gi_reserve01/gGiTitleDeedLandColorDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f, true,
        "__OTR__objects/object_gi_reserve01/gGiTitleDeedSealAndRibbonDL" } },
    { OOTXMM_DRAW_MM_TITLE_DEED_SWAMP,
      { "__OTR__objects/object_gi_reserve01/gGiTitleDeedPaperDL",
        "__OTR__objects/object_gi_reserve01/gGiTitleDeedSwampColorDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f, true,
        "__OTR__objects/object_gi_reserve01/gGiTitleDeedSealAndRibbonDL" } },
    { OOTXMM_DRAW_MM_TITLE_DEED_MOUNTAIN,
      { "__OTR__objects/object_gi_reserve01/gGiTitleDeedPaperDL",
        "__OTR__objects/object_gi_reserve01/gGiTitleDeedMountainColorDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f, true,
        "__OTR__objects/object_gi_reserve01/gGiTitleDeedSealAndRibbonDL" } },
    { OOTXMM_DRAW_MM_TITLE_DEED_OCEAN,
      { "__OTR__objects/object_gi_reserve01/gGiTitleDeedPaperDL",
        "__OTR__objects/object_gi_reserve01/gGiTitleDeedOceanColorDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f, true,
        "__OTR__objects/object_gi_reserve01/gGiTitleDeedSealAndRibbonDL" } },
    { OOTXMM_DRAW_MM_BOMBERS_NOTEBOOK,
      { "__OTR__objects/object_gi_schedule/gGiBombersNotebookDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_PICTO_BOX,
      { "__OTR__objects/object_gi_camera/gGiPictoBoxBodyAndLensDL",
        "__OTR__objects/object_gi_camera/gGiPictoBoxFrameDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_POWDER_KEG,
      { "__OTR__objects/object_gi_bigbomb/gGiPowderKegBarrelDL",
        "__OTR__objects/object_gi_bigbomb/gGiPowderKegGoronSkullAndFuseDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_GOLD_DUST,
      { "__OTR__objects/object_gi_gold_dust/gGiGoldDustPowderDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_CHATEAU,
      { "__OTR__objects/object_gi_bottle_21/gGiChateauRomaniBottleDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f,
        0.0f } },
    { OOTXMM_DRAW_MM_ZORA_EGG,
      { "__OTR__objects/object_gi_bottle_15/gGiZoraEggBottleGlassAndCorkDL",
        "__OTR__objects/object_gi_bottle_15/gGiZoraEggBottleContentsDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_SEAHORSE,
      { "__OTR__objects/object_gi_bottle_16/gGiSeahorseBottleGlassAndCorkDL", nullptr, -72.0f, 0.3456f, 0.0f,
        0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_HYLIAN_LOACH,
      { "__OTR__objects/object_gi_bottle_22/gGiHylianLoachBottleGlassCorkWaterDL",
        "__OTR__objects/object_gi_bottle_22/gGiHylianLoachBottleContentsDL", -72.0f, 0.3456f, 0.0f, 0.0f,
        0.0f } },
    { OOTXMM_DRAW_MM_HERO_SHIELD,
      { "__OTR__objects/object_gi_shield_2/gGiHerosShieldDL",
        "__OTR__objects/object_gi_shield_2/gGiHerosShieldEmblemDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_MIRROR_SHIELD,
      { "__OTR__objects/object_gi_shield_3/gGiMirrorShieldDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_KOKIRI_SWORD,
      { "__OTR__objects/object_gi_sword_1/gGiKokiriSwordBladeHiltDL",
        "__OTR__objects/object_gi_sword_1/gGiKokiriSwordGuardDL", -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_RAZOR_SWORD,
      { "__OTR__objects/object_gi_sword_2/gGiRazorSwordDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_GILDED_SWORD,
      { "__OTR__objects/object_gi_sword_3/gGiGildedSwordDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_GREAT_FAIRY_SWORD,
      { "__OTR__objects/object_gi_sword_4/gGiGreatFairysSwordBladeDL",
        "__OTR__objects/object_gi_sword_4/gGiGreatFairysSwordHiltEmblemDL", -72.0f, 0.3456f, 0.0f, 0.0f,
        0.0f } },
    { OOTXMM_DRAW_MM_HOOKSHOT,
      { "__OTR__objects/object_gi_hookshot/gGiHookshotDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_LENS,
      { "__OTR__objects/object_gi_glasses/gGiLensDL", "__OTR__objects/object_gi_glasses/gGiLensGlassDL", -72.0f,
        0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_BOW,
      { "__OTR__objects/object_gi_bow/gGiBowHandleDL", "__OTR__objects/object_gi_bow/gGiBowStringDL", -72.0f,
        0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_BOMB_BAG,
      { "__OTR__objects/object_gi_bombpouch/gGiBombBagDL", "__OTR__objects/object_gi_bombpouch/gGiBombBagRingDL",
        -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_QUIVER,
      { "__OTR__objects/object_gi_arrowcase/gGiQuiverInnerDL", "__OTR__objects/object_gi_arrowcase/gGiQuiverOuterDL",
        -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_WALLET,
      { "__OTR__objects/object_gi_purse/gGiWalletDL", "__OTR__objects/object_gi_purse/gGiWalletRupeeOuterDL", -72.0f,
        0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_REMAINS_ODOLWA,
      { "__OTR__objects/object_ob/gMoonChildOdolwasMaskDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_REMAINS_GOHT,
      { "__OTR__objects/object_ob/gMoonChildGohtsMaskDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_REMAINS_GYORG,
      { "__OTR__objects/object_ob/gMoonChildGyorgsMaskDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_REMAINS_TWINMOLD,
      { "__OTR__objects/object_ob/gMoonChildTwinmoldsMaskDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_SOUL_GENERIC,
      { "__OTR__objects/object_boss_soul/gGIBossSoulSkullDL", nullptr, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, false } },
    { OOTXMM_DRAW_MM_SUN_MASK,
      { "__OTR__objects/object_gi_mssa/gGiSunMaskFaceDL", "__OTR__objects/object_gi_mssa/gGiSunMaskEyesSidePatternsDL",
        -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_SONG_NOTE,
      { "__OTR__objects/object_gi_melody/gGiSongNoteDL", "__OTR__objects/object_gi_melody/gGiMinuetColorDL", -72.0f,
        0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_SONG_EPONA,
      { "__OTR__objects/object_gi_melody/gGiSongNoteDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_SONG_NOVA,
      { "__OTR__objects/object_gi_melody/gGiSongNoteDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_SONG_SONATA,
      { "__OTR__objects/object_gi_melody/gGiSongNoteDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_SONG_SOARING,
      { "__OTR__objects/object_gi_melody/gGiSongNoteDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_SONG_ELEGY,
      { "__OTR__objects/object_gi_melody/gGiSongNoteDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_SONG_LULLABY_INTRO,
      { "__OTR__objects/object_gi_melody/gGiSongNoteDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_SONG_LULLABY,
      { "__OTR__objects/object_gi_melody/gGiSongNoteDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_SONG_OATH,
      { "__OTR__objects/object_gi_melody/gGiSongNoteDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_TIME_DAY,
      { "__OTR__objects/object_obj_tokeidai/gClockTowerClockFaceDL",
        "__OTR__objects/object_obj_tokeidai/gClockTowerClockCenterAndHandDL", 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, true,
        "__OTR__objects/object_obj_tokeidai/gClockTowerSunAndMoonPanelDL" } },
    { OOTXMM_DRAW_MM_TIME_NIGHT,
      { "__OTR__objects/object_obj_tokeidai/gClockTowerClockFaceDL",
        "__OTR__objects/object_obj_tokeidai/gClockTowerClockCenterAndHandDL", 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, true,
        "__OTR__objects/object_obj_tokeidai/gClockTowerSunAndMoonPanelDL" } },
    { OOTXMM_DRAW_MM_FROG_BLUE,
      { "__OTR__objects/object_fr/gFrogHeadDL", "__OTR__objects/object_fr/gFrogUpperBodyDL", -72.0f, 0.12f, 0.0f, 0.0f,
        0.0f, true, "__OTR__objects/object_fr/gFrogLowerBodyDL" } },
    { OOTXMM_DRAW_MM_FROG_CYAN,
      { "__OTR__objects/object_fr/gFrogHeadDL", "__OTR__objects/object_fr/gFrogUpperBodyDL", -72.0f, 0.12f, 0.0f, 0.0f,
        0.0f, true, "__OTR__objects/object_fr/gFrogLowerBodyDL" } },
    { OOTXMM_DRAW_MM_FROG_PINK,
      { "__OTR__objects/object_fr/gFrogHeadDL", "__OTR__objects/object_fr/gFrogUpperBodyDL", -72.0f, 0.12f, 0.0f, 0.0f,
        0.0f, true, "__OTR__objects/object_fr/gFrogLowerBodyDL" } },
    { OOTXMM_DRAW_MM_FROG_WHITE,
      { "__OTR__objects/object_fr/gFrogHeadDL", "__OTR__objects/object_fr/gFrogUpperBodyDL", -72.0f, 0.12f, 0.0f, 0.0f,
        0.0f, true, "__OTR__objects/object_fr/gFrogLowerBodyDL" } },
    { OOTXMM_DRAW_MM_OCARINA_BUTTON_A,
      { "__OTR__objects/object_ocarina_a_button/gOcarinaAButtonDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_OCARINA_BUTTON_C_DOWN,
      { "__OTR__objects/object_ocarina_c_down_button/gOcarinaCDownButtonDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f,
        0.0f } },
    { OOTXMM_DRAW_MM_OCARINA_BUTTON_C_RIGHT,
      { "__OTR__objects/object_ocarina_c_right_button/gOcarinaCRightButtonDL", nullptr, -72.0f, 0.3456f, 0.0f,
        0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_OCARINA_BUTTON_C_LEFT,
      { "__OTR__objects/object_ocarina_c_left_button/gOcarinaCLeftButtonDL", nullptr, -72.0f, 0.3456f, 0.0f,
        0.0f, 0.0f } },
    { OOTXMM_DRAW_MM_OCARINA_BUTTON_C_UP,
      { "__OTR__objects/object_ocarina_c_up_button/gOcarinaCUpButtonDL", nullptr, -72.0f, 0.3456f, 0.0f, 0.0f,
        0.0f } },
};

static std::optional<uint16_t> ResolveOoTxMmForeignDrawId(const std::string& modelId) {
    std::string normalizedModelId = modelId;
    std::replace(normalizedModelId.begin(), normalizedModelId.end(), ':', '.');
    std::transform(normalizedModelId.begin(), normalizedModelId.end(), normalizedModelId.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (normalizedModelId.find("frog") != std::string::npos) {
        return std::nullopt;
    }

    static const std::unordered_map<std::string, uint16_t> modelDraws = {
        { "xmm.mask.couple", OOTXMM_DRAW_MM_COUPLES_MASK },
        { "xmm.mask.deku", OOTXMM_DRAW_MM_DEKU_MASK },
        { "xmm.mask.nuts", OOTXMM_DRAW_MM_DEKU_MASK },
        { "xmm.mask.fierce_deity", OOTXMM_DRAW_MM_FIERCE_DEITY_MASK },
        { "xmm.mask.giant", OOTXMM_DRAW_MM_GIANT_MASK },
        { "xmm.mask.kyojin", OOTXMM_DRAW_MM_GIANT_MASK },
        { "xmm.mask.postman", OOTXMM_DRAW_MM_POSTMAN_MASK },
        { "xmm.mask.posthat", OOTXMM_DRAW_MM_POSTMAN_MASK },
        { "xmm.mask.great_fairy", OOTXMM_DRAW_MM_GREAT_FAIRY_MASK },
        { "xmm.mask.bigelf", OOTXMM_DRAW_MM_GREAT_FAIRY_MASK },
        { "xmm.mask.blast", OOTXMM_DRAW_MM_BLAST_MASK },
        { "xmm.mask.bakuretu", OOTXMM_DRAW_MM_BLAST_MASK },
        { "xmm.mask.gibdo", OOTXMM_DRAW_MM_GIBDO_MASK },
        { "xmm.mask.gibudo", OOTXMM_DRAW_MM_GIBDO_MASK },
        { "xmm.mask.garo", OOTXMM_DRAW_MM_GARO_MASK },
        { "xmm.mask.kafei", OOTXMM_DRAW_MM_KAFEI_MASK },
        { "xmm.mask.yofukasi", OOTXMM_DRAW_MM_ALL_NIGHT_MASK },
        { "xmm.mask.all_night", OOTXMM_DRAW_MM_ALL_NIGHT_MASK },
        { "xmm.mask.romani", OOTXMM_DRAW_MM_ROMANI_MASK },
        { "xmm.mask.romerny", OOTXMM_DRAW_MM_ROMANI_MASK },
        { "xmm.mask.circus_leader", OOTXMM_DRAW_MM_CIRCUS_LEADER_MASK },
        { "xmm.mask.zacho", OOTXMM_DRAW_MM_CIRCUS_LEADER_MASK },
        { "xmm.mask.don_gero", OOTXMM_DRAW_MM_DON_GERO_MASK },
        { "xmm.mask.gero", OOTXMM_DRAW_MM_DON_GERO_MASK },
        { "xmm.mask.kamaro", OOTXMM_DRAW_MM_KAMARO_MASK },
        { "xmm.mask.dancer", OOTXMM_DRAW_MM_KAMARO_MASK },
        { "xmm.mask.captain", OOTXMM_DRAW_MM_CAPTAIN_MASK },
        { "xmm.mask.bremen", OOTXMM_DRAW_MM_BREMEN_MASK },
        { "xmm.mask.bree", OOTXMM_DRAW_MM_BREMEN_MASK },
        { "xmm.mask.scents", OOTXMM_DRAW_MM_MASK_OF_SCENTS },
        { "xmm.mask.bu_san", OOTXMM_DRAW_MM_MASK_OF_SCENTS },
        { "xmm.mask.stone", OOTXMM_DRAW_MM_STONE_MASK },
        { "xmm.key.small", OOTXMM_DRAW_MM_SMALL_KEY },
        { "xmm.key.boss", OOTXMM_DRAW_MM_BOSS_KEY },
        { "xmm.owl_statue", OOTXMM_DRAW_MM_OWL_STATUE },
        { "xmm.skulltula_token", OOTXMM_DRAW_MM_SKULLTULA_TOKEN },
        { "xmm.quest.deed_land", OOTXMM_DRAW_MM_TITLE_DEED_LAND },
        { "xmm.quest.deed_swamp", OOTXMM_DRAW_MM_TITLE_DEED_SWAMP },
        { "xmm.quest.deed_mountain", OOTXMM_DRAW_MM_TITLE_DEED_MOUNTAIN },
        { "xmm.quest.deed_ocean", OOTXMM_DRAW_MM_TITLE_DEED_OCEAN },
    };

    const auto drawId = modelDraws.find(normalizedModelId);
    if (drawId != modelDraws.end()) {
        return drawId->second;
    }

    if (normalizedModelId.find("owl") != std::string::npos) {
        return OOTXMM_DRAW_MM_OWL_STATUE;
    }
    if (normalizedModelId.find("gs_token") != std::string::npos ||
        normalizedModelId.find("skulltula") != std::string::npos) {
        return OOTXMM_DRAW_MM_SKULLTULA_TOKEN;
    }
    if (normalizedModelId.find("soul_enemy") != std::string::npos ||
        normalizedModelId.find("enemy_soul") != std::string::npos ||
        normalizedModelId.find("soul_boss_majora") != std::string::npos ||
        normalizedModelId.find("boss_majora") != std::string::npos) {
        return OOTXMM_DRAW_MM_SOUL_GENERIC;
    }
    if (normalizedModelId.find("time_night") != std::string::npos ||
        normalizedModelId.find("night_") != std::string::npos ||
        normalizedModelId.find("xmm.time.night") != std::string::npos) {
        return OOTXMM_DRAW_MM_TIME_NIGHT;
    }
    if (normalizedModelId.find("time_day") != std::string::npos ||
        normalizedModelId.find("day_") != std::string::npos ||
        normalizedModelId.find("progressive_time") != std::string::npos ||
        normalizedModelId.find("xmm.time.day") != std::string::npos) {
        return OOTXMM_DRAW_MM_TIME_DAY;
    }
    if (normalizedModelId.find("deed_land") != std::string::npos || normalizedModelId.find("land_deed") != std::string::npos) {
        return OOTXMM_DRAW_MM_TITLE_DEED_LAND;
    }
    if (normalizedModelId.find("deed_swamp") != std::string::npos || normalizedModelId.find("swamp_deed") != std::string::npos) {
        return OOTXMM_DRAW_MM_TITLE_DEED_SWAMP;
    }
    if (normalizedModelId.find("deed_mountain") != std::string::npos ||
        normalizedModelId.find("mountain_deed") != std::string::npos) {
        return OOTXMM_DRAW_MM_TITLE_DEED_MOUNTAIN;
    }
    if (normalizedModelId.find("deed_ocean") != std::string::npos || normalizedModelId.find("ocean_deed") != std::string::npos) {
        return OOTXMM_DRAW_MM_TITLE_DEED_OCEAN;
    }
    if (normalizedModelId.find("deed") != std::string::npos) {
        return OOTXMM_DRAW_MM_TITLE_DEED;
    }
    if (normalizedModelId.find("odolwa") != std::string::npos) {
        return OOTXMM_DRAW_MM_REMAINS_ODOLWA;
    }
    if (normalizedModelId.find("goht") != std::string::npos) {
        return OOTXMM_DRAW_MM_REMAINS_GOHT;
    }
    if (normalizedModelId.find("gyorg") != std::string::npos) {
        return OOTXMM_DRAW_MM_REMAINS_GYORG;
    }
    if (normalizedModelId.find("twinmold") != std::string::npos) {
        return OOTXMM_DRAW_MM_REMAINS_TWINMOLD;
    }
    if (normalizedModelId.find("mask_") != std::string::npos || normalizedModelId.find("_mask") != std::string::npos) {
        if (normalizedModelId.find("fierce_deity") != std::string::npos) {
            return OOTXMM_DRAW_MM_FIERCE_DEITY_MASK;
        }
        if (normalizedModelId.find("deku") != std::string::npos || normalizedModelId.find("nuts") != std::string::npos) {
            return OOTXMM_DRAW_MM_DEKU_MASK;
        }
        if (normalizedModelId.find("couple") != std::string::npos) {
            return OOTXMM_DRAW_MM_COUPLES_MASK;
        }
        if (normalizedModelId.find("postman") != std::string::npos || normalizedModelId.find("posthat") != std::string::npos) {
            return OOTXMM_DRAW_MM_POSTMAN_MASK;
        }
        if (normalizedModelId.find("giant") != std::string::npos || normalizedModelId.find("kyojin") != std::string::npos) {
            return OOTXMM_DRAW_MM_GIANT_MASK;
        }
        if (normalizedModelId.find("great_fairy") != std::string::npos || normalizedModelId.find("bigelf") != std::string::npos) {
            return OOTXMM_DRAW_MM_GREAT_FAIRY_MASK;
        }
        if (normalizedModelId.find("blast") != std::string::npos || normalizedModelId.find("bakuretu") != std::string::npos) {
            return OOTXMM_DRAW_MM_BLAST_MASK;
        }
        if (normalizedModelId.find("gibdo") != std::string::npos || normalizedModelId.find("gibudo") != std::string::npos) {
            return OOTXMM_DRAW_MM_GIBDO_MASK;
        }
        if (normalizedModelId.find("garo") != std::string::npos) {
            return OOTXMM_DRAW_MM_GARO_MASK;
        }
        if (normalizedModelId.find("kafei") != std::string::npos) {
            return OOTXMM_DRAW_MM_KAFEI_MASK;
        }
        if (normalizedModelId.find("all_night") != std::string::npos || normalizedModelId.find("yofukasi") != std::string::npos) {
            return OOTXMM_DRAW_MM_ALL_NIGHT_MASK;
        }
        if (normalizedModelId.find("romani") != std::string::npos || normalizedModelId.find("romerny") != std::string::npos) {
            return OOTXMM_DRAW_MM_ROMANI_MASK;
        }
        if (normalizedModelId.find("circus_leader") != std::string::npos || normalizedModelId.find("zacho") != std::string::npos) {
            return OOTXMM_DRAW_MM_CIRCUS_LEADER_MASK;
        }
        if (normalizedModelId.find("don_gero") != std::string::npos || normalizedModelId.find("gero") != std::string::npos) {
            return OOTXMM_DRAW_MM_DON_GERO_MASK;
        }
        if (normalizedModelId.find("kamaro") != std::string::npos || normalizedModelId.find("dancer") != std::string::npos) {
            return OOTXMM_DRAW_MM_KAMARO_MASK;
        }
        if (normalizedModelId.find("captain") != std::string::npos) {
            return OOTXMM_DRAW_MM_CAPTAIN_MASK;
        }
        if (normalizedModelId.find("bremen") != std::string::npos || normalizedModelId.find("bree") != std::string::npos) {
            return OOTXMM_DRAW_MM_BREMEN_MASK;
        }
        if (normalizedModelId.find("scents") != std::string::npos || normalizedModelId.find("bu_san") != std::string::npos) {
            return OOTXMM_DRAW_MM_MASK_OF_SCENTS;
        }
        if (normalizedModelId.find("stone") != std::string::npos) {
            return OOTXMM_DRAW_MM_STONE_MASK;
        }
    }
    if (normalizedModelId.find("progressive_sword") != std::string::npos) {
        return std::nullopt;
    }

    if (normalizedModelId.rfind("xmm.tingle_map.", 0) == 0 || normalizedModelId.find("tingle_map") != std::string::npos) {
        return OOTXMM_DRAW_MM_TINGLE_MAP;
    }
    if (normalizedModelId.rfind("xmm.map.", 0) == 0 || normalizedModelId.find("_map") != std::string::npos) {
        return OOTXMM_DRAW_MM_DUNGEON_MAP;
    }
    if (normalizedModelId.rfind("xmm.compass.", 0) == 0 || normalizedModelId.find("compass") != std::string::npos) {
        return OOTXMM_DRAW_MM_COMPASS;
    }
    if (normalizedModelId.rfind("xmm.quest.", 0) == 0) {
        if (normalizedModelId.find("deed_land") != std::string::npos || normalizedModelId.find("land_deed") != std::string::npos) {
            return OOTXMM_DRAW_MM_TITLE_DEED_LAND;
        }
        if (normalizedModelId.find("deed_swamp") != std::string::npos || normalizedModelId.find("swamp_deed") != std::string::npos) {
            return OOTXMM_DRAW_MM_TITLE_DEED_SWAMP;
        }
        if (normalizedModelId.find("deed_mountain") != std::string::npos ||
            normalizedModelId.find("mountain_deed") != std::string::npos) {
            return OOTXMM_DRAW_MM_TITLE_DEED_MOUNTAIN;
        }
        if (normalizedModelId.find("deed_ocean") != std::string::npos || normalizedModelId.find("ocean_deed") != std::string::npos) {
            return OOTXMM_DRAW_MM_TITLE_DEED_OCEAN;
        }
        if (normalizedModelId.find("deed") != std::string::npos) {
            return OOTXMM_DRAW_MM_TITLE_DEED;
        }
        if (normalizedModelId.find("room_key") != std::string::npos) {
            return OOTXMM_DRAW_MM_ROOM_KEY;
        }
        if (normalizedModelId.find("mama") != std::string::npos || normalizedModelId.find("express_mail") != std::string::npos) {
            return OOTXMM_DRAW_MM_LETTER_TO_MAMA;
        }
        if (normalizedModelId.find("kafei") != std::string::npos || normalizedModelId.find("letter") != std::string::npos) {
            return OOTXMM_DRAW_MM_LETTER_TO_KAFEI;
        }
        if (normalizedModelId.find("pendant") != std::string::npos) {
            return OOTXMM_DRAW_MM_PENDANT;
        }
        if (normalizedModelId.find("moon") != std::string::npos || normalizedModelId.find("tear") != std::string::npos) {
            return OOTXMM_DRAW_MM_MOONS_TEAR;
        }
        if (normalizedModelId.find("deed") != std::string::npos) {
            return OOTXMM_DRAW_MM_TITLE_DEED;
        }
        if (normalizedModelId.find("notebook") != std::string::npos || normalizedModelId.find("schedule") != std::string::npos) {
            return OOTXMM_DRAW_MM_BOMBERS_NOTEBOOK;
        }
        if (normalizedModelId.find("sun") != std::string::npos) {
            return OOTXMM_DRAW_MM_SUN_MASK;
        }
    }
    if (normalizedModelId.rfind("xmm.equipment.", 0) == 0 || normalizedModelId.rfind("mm.ri_", 0) == 0) {
        if (normalizedModelId.find("progressive_sword") != std::string::npos) {
            return std::nullopt;
        }
        if (normalizedModelId.find("hookshot") != std::string::npos) {
            return OOTXMM_DRAW_MM_HOOKSHOT;
        }
        if (normalizedModelId.find("lens") != std::string::npos) {
            return OOTXMM_DRAW_MM_LENS;
        }
        if (normalizedModelId.find("progressive_bow") != std::string::npos || normalizedModelId.find("_bow") != std::string::npos) {
            return OOTXMM_DRAW_MM_BOW;
        }
        if (normalizedModelId.find("bomb_bag") != std::string::npos || normalizedModelId.find("bombbag") != std::string::npos) {
            return OOTXMM_DRAW_MM_BOMB_BAG;
        }
        if (normalizedModelId.find("quiver") != std::string::npos || normalizedModelId.find("arrowcase") != std::string::npos) {
            return OOTXMM_DRAW_MM_QUIVER;
        }
        if (normalizedModelId.find("wallet") != std::string::npos || normalizedModelId.find("purse") != std::string::npos) {
            return OOTXMM_DRAW_MM_WALLET;
        }
        if (normalizedModelId.find("mirror") != std::string::npos) {
            return OOTXMM_DRAW_MM_MIRROR_SHIELD;
        }
        if (normalizedModelId.find("shield") != std::string::npos || normalizedModelId.find("hero") != std::string::npos) {
            return OOTXMM_DRAW_MM_HERO_SHIELD;
        }
        if (normalizedModelId.find("fairy") != std::string::npos) {
            return OOTXMM_DRAW_MM_GREAT_FAIRY_SWORD;
        }
        if (normalizedModelId.find("gilded") != std::string::npos) {
            return OOTXMM_DRAW_MM_GILDED_SWORD;
        }
        if (normalizedModelId.find("razor") != std::string::npos) {
            return OOTXMM_DRAW_MM_RAZOR_SWORD;
        }
        if (normalizedModelId.find("sword") != std::string::npos) {
            return OOTXMM_DRAW_MM_KOKIRI_SWORD;
        }
    }
    if (normalizedModelId.find("progressive_bow") != std::string::npos ||
        normalizedModelId.find("ri_bow") != std::string::npos || normalizedModelId.find(".bow") != std::string::npos) {
        return OOTXMM_DRAW_MM_BOW;
    }
    if (normalizedModelId.find("hookshot") != std::string::npos) {
        return OOTXMM_DRAW_MM_HOOKSHOT;
    }
    if (normalizedModelId.find("bomb_bag") != std::string::npos || normalizedModelId.find("bombbag") != std::string::npos) {
        return OOTXMM_DRAW_MM_BOMB_BAG;
    }
    if (normalizedModelId.find("mirror") != std::string::npos && normalizedModelId.find("shield") != std::string::npos) {
        return OOTXMM_DRAW_MM_MIRROR_SHIELD;
    }
    if (normalizedModelId.find("shield_hero") != std::string::npos || normalizedModelId.find("hero_shield") != std::string::npos) {
        return OOTXMM_DRAW_MM_HERO_SHIELD;
    }
    if (normalizedModelId.rfind("xmm.bottle.", 0) == 0) {
        if (normalizedModelId.find("chateau") != std::string::npos) {
            return OOTXMM_DRAW_MM_CHATEAU;
        }
        if (normalizedModelId.find("gold_dust") != std::string::npos) {
            return OOTXMM_DRAW_MM_GOLD_DUST;
        }
        if (normalizedModelId.find("zora_egg") != std::string::npos || normalizedModelId.find("egg") != std::string::npos) {
            return OOTXMM_DRAW_MM_ZORA_EGG;
        }
        if (normalizedModelId.find("seahorse") != std::string::npos) {
            return OOTXMM_DRAW_MM_SEAHORSE;
        }
        if (normalizedModelId.find("loach") != std::string::npos) {
            return OOTXMM_DRAW_MM_HYLIAN_LOACH;
        }
    }
    if (normalizedModelId.find("powder_keg") != std::string::npos || normalizedModelId.find("bigbomb") != std::string::npos) {
        return OOTXMM_DRAW_MM_POWDER_KEG;
    }
    if (normalizedModelId.find("picto") != std::string::npos || normalizedModelId.find("camera") != std::string::npos) {
        return OOTXMM_DRAW_MM_PICTO_BOX;
    }
    if (normalizedModelId.find("gold_dust") != std::string::npos) {
        return OOTXMM_DRAW_MM_GOLD_DUST;
    }
    if (normalizedModelId.rfind("xmm.song.", 0) == 0 || normalizedModelId.find("song") != std::string::npos ||
        normalizedModelId.find("sonata") != std::string::npos || normalizedModelId.find("lullaby") != std::string::npos ||
        normalizedModelId.find("bossa") != std::string::npos || normalizedModelId.find("elegy") != std::string::npos ||
        normalizedModelId.find("oath") != std::string::npos || normalizedModelId.find("nova") != std::string::npos ||
        normalizedModelId.find("soaring") != std::string::npos) {
        if (normalizedModelId.find("epona") != std::string::npos) {
            return OOTXMM_DRAW_MM_SONG_EPONA;
        }
        if (normalizedModelId.find("nova") != std::string::npos || normalizedModelId.find("bossa") != std::string::npos) {
            return OOTXMM_DRAW_MM_SONG_NOVA;
        }
        if (normalizedModelId.find("sonata") != std::string::npos) {
            return OOTXMM_DRAW_MM_SONG_SONATA;
        }
        if (normalizedModelId.find("soaring") != std::string::npos) {
            return OOTXMM_DRAW_MM_SONG_SOARING;
        }
        if (normalizedModelId.find("elegy") != std::string::npos) {
            return OOTXMM_DRAW_MM_SONG_ELEGY;
        }
        if (normalizedModelId.find("lullaby_intro") != std::string::npos) {
            return OOTXMM_DRAW_MM_SONG_LULLABY_INTRO;
        }
        if (normalizedModelId.find("lullaby") != std::string::npos || normalizedModelId.find("progressive_lullaby") != std::string::npos) {
            return OOTXMM_DRAW_MM_SONG_LULLABY;
        }
        if (normalizedModelId.find("oath") != std::string::npos) {
            return OOTXMM_DRAW_MM_SONG_OATH;
        }
        return OOTXMM_DRAW_MM_SONG_NOTE;
    }
    if (normalizedModelId.find("ocarina_button") != std::string::npos) {
        if (normalizedModelId.find("c_up") != std::string::npos) {
            return OOTXMM_DRAW_MM_OCARINA_BUTTON_C_UP;
        }
        if (normalizedModelId.find("c_left") != std::string::npos) {
            return OOTXMM_DRAW_MM_OCARINA_BUTTON_C_LEFT;
        }
        if (normalizedModelId.find("c_right") != std::string::npos) {
            return OOTXMM_DRAW_MM_OCARINA_BUTTON_C_RIGHT;
        }
        if (normalizedModelId.find("c_down") != std::string::npos) {
            return OOTXMM_DRAW_MM_OCARINA_BUTTON_C_DOWN;
        }
        if (normalizedModelId.find("_a") != std::string::npos || normalizedModelId.find(".a") != std::string::npos) {
            return OOTXMM_DRAW_MM_OCARINA_BUTTON_A;
        }
    }

    return std::nullopt;
}

static Gfx* LoadOoTxMmForeignGfxByName(const OoTxMmForeignDrawInfo& drawInfo, const char* path) {
    if (path == nullptr) {
        return nullptr;
    }

    if (!drawInfo.loadFromMmO2r) {
        return ResourceMgr_LoadGfxByName(path);
    }

    const auto mmArchive = SohCrossover::GetMmO2rArchive();
    if (mmArchive == nullptr) {
        SPDLOG_INFO("[OoTxMM][VisualDraw] mm.o2r archive is unavailable for foreign path={}", path);
        return nullptr;
    }

    auto resourceManager = Ship::Context::GetInstance()->GetResourceManager();
    if (resourceManager == nullptr) {
        return nullptr;
    }

    auto resource = std::static_pointer_cast<Fast::DisplayList>(
        resourceManager->LoadResource(Ship::ResourceIdentifier(path, reinterpret_cast<uintptr_t>(mmArchive.get()), mmArchive),
                                      true));
    if (resource == nullptr || resource->Instructions.empty()) {
        SPDLOG_INFO("[OoTxMM][VisualDraw] exact mm.o2r display list load failed path={}", path);
        return nullptr;
    }

    return reinterpret_cast<Gfx*>(&resource->Instructions[0]);
}

static GetItemEntry GetOoTxMmFallbackOpenChestEntry() {
    return Rando::StaticData::RetrieveItem(RG_OPEN_CHEST).GetGIEntry_Copy();
}

static void DrawOoTxMmNothing(PlayState* play, GetItemEntry* getItemEntry) {
}

static void DrawOoTxMmFallbackOpenChest(PlayState* play);

static void DrawOoTxMmSilverRupeeVisual(PlayState* play, GetItemEntry* getItemEntry) {
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx* innerDl = ResourceMgr_LoadGfxByName(gGiRupeeInnerDL);
    Gfx* outerDl = ResourceMgr_LoadGfxByName(gGiRupeeOuterDL);
    if (innerDl == nullptr || outerDl == nullptr) {
        DrawOoTxMmFallbackOpenChest(play);
        return;
    }

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, 192, 192, 192, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 56, 56, 64, 255);
    gSPDisplayList(POLY_OPA_DISP++, innerDl);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 200, 200, 208, 255);
    gSPDisplayList(POLY_XLU_DISP++, outerDl);
}

static void DrawOoTxMmFallbackOpenChest(PlayState* play) {
    auto fallbackEntry = GetOoTxMmFallbackOpenChestEntry();
    Randomizer_DrawOpenChest(play, &fallbackEntry);
}

static uint32_t StableOoTxMmHash(const std::string& value) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : value) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

static RandomizerGet GetOoTxMmNativeJunkModel(RandomizerCheck rc, const std::string& nativeCheck) {
    static constexpr std::array<RandomizerGet, 13> junkModels = {
        RG_BOMBS_5,  RG_BOMBS_10,  RG_BOMBS_20,  RG_DEKU_NUTS_5, RG_DEKU_STICK_1, RG_DEKU_SEEDS_30, RG_RECOVERY_HEART,
        RG_ARROWS_5, RG_ARROWS_10, RG_ARROWS_30, RG_BLUE_RUPEE,  RG_RED_RUPEE,    RG_DEKU_NUTS_10,
    };
    const std::string key = nativeCheck.empty() ? std::to_string(static_cast<int32_t>(rc)) : nativeCheck;
    return junkModels[StableOoTxMmHash(key) % junkModels.size()];
}

static void Randomizer_DrawOoTxMmForeignModel(PlayState* play, GetItemEntry* getItemEntry) {
    const auto drawInfo = sOoTxMmForeignDraws.find(getItemEntry->drawItemId);
    if (drawInfo == sOoTxMmForeignDraws.end()) {
        SPDLOG_INFO("[OoTxMM][VisualDraw] missing draw info drawItemId={} objectId={} gi={} getItemId={} itemId={}",
                    getItemEntry->drawItemId, getItemEntry->objectId, getItemEntry->gi, getItemEntry->getItemId,
                    getItemEntry->itemId);
        DrawOoTxMmFallbackOpenChest(play);
        return;
    }

    Gfx* dlist = LoadOoTxMmForeignGfxByName(drawInfo->second, drawInfo->second.path);
    if (dlist == nullptr) {
        SPDLOG_INFO("[OoTxMM][VisualDraw] failed to load primary path={}", drawInfo->second.path);
        DrawOoTxMmFallbackOpenChest(play);
        return;
    }
    Gfx* dlist2 = nullptr;
    if (drawInfo->second.path2 != nullptr) {
        dlist2 = LoadOoTxMmForeignGfxByName(drawInfo->second, drawInfo->second.path2);
        if (dlist2 == nullptr) {
            SPDLOG_INFO("[OoTxMM][VisualDraw] failed to load secondary path={}", drawInfo->second.path2);
        }
    }
    Gfx* dlist3 = nullptr;
    if (drawInfo->second.path3 != nullptr) {
        dlist3 = LoadOoTxMmForeignGfxByName(drawInfo->second, drawInfo->second.path3);
        if (dlist3 == nullptr) {
            SPDLOG_INFO("[OoTxMM][VisualDraw] failed to load tertiary path={}", drawInfo->second.path3);
        }
    }
    Gfx* dlist4 = nullptr;
    if (drawInfo->second.path4 != nullptr) {
        dlist4 = LoadOoTxMmForeignGfxByName(drawInfo->second, drawInfo->second.path4);
        if (dlist4 == nullptr) {
            SPDLOG_INFO("[OoTxMM][VisualDraw] failed to load quaternary path={}", drawInfo->second.path4);
        }
    }

    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_OWL_STATUE) {
        Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
        Matrix_Translate(0.0f, -3000.0f, 0.0f, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        gSPDisplayList(POLY_OPA_DISP++, dlist);
        return;
    }
    if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_SOUL_GENERIC) {
        Matrix_Scale(1.0f, 1.0f, 1.0f, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        gSPDisplayList(POLY_OPA_DISP++, dlist);
        return;
    }
    if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_REMAINS_ODOLWA ||
        getItemEntry->drawItemId == OOTXMM_DRAW_MM_REMAINS_GOHT ||
        getItemEntry->drawItemId == OOTXMM_DRAW_MM_REMAINS_GYORG ||
        getItemEntry->drawItemId == OOTXMM_DRAW_MM_REMAINS_TWINMOLD) {
        Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        gSPDisplayList(POLY_OPA_DISP++, dlist);
        return;
    }
    if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_TIME_DAY ||
        getItemEntry->drawItemId == OOTXMM_DRAW_MM_TIME_NIGHT) {
        Matrix_Translate(0.0f, 0.0f, 0.0f, MTXMODE_APPLY);
        Matrix_Scale(0.015f, 0.015f, 0.015f, MTXMODE_APPLY);
        if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_TIME_DAY) {
            Matrix_RotateZ(BINANG_TO_RAD(0xC000), MTXMODE_APPLY);
        }
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        gSPDisplayList(POLY_OPA_DISP++, dlist);
        if (dlist2 != nullptr) {
            gSPDisplayList(POLY_OPA_DISP++, dlist2);
        }
        if (dlist3 != nullptr) {
            gSPDisplayList(POLY_OPA_DISP++, dlist3);
        }
        return;
    }
    if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_SONG_EPONA ||
        getItemEntry->drawItemId == OOTXMM_DRAW_MM_SONG_NOVA ||
        getItemEntry->drawItemId == OOTXMM_DRAW_MM_SONG_SONATA ||
        getItemEntry->drawItemId == OOTXMM_DRAW_MM_SONG_SOARING ||
        getItemEntry->drawItemId == OOTXMM_DRAW_MM_SONG_ELEGY ||
        getItemEntry->drawItemId == OOTXMM_DRAW_MM_SONG_LULLABY_INTRO ||
        getItemEntry->drawItemId == OOTXMM_DRAW_MM_SONG_LULLABY ||
        getItemEntry->drawItemId == OOTXMM_DRAW_MM_SONG_OATH) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        Matrix_Translate(0.0f, drawInfo->second.baseTranslateY + kOoTxMmForeignModelHeightUnit, 0.0f, MTXMODE_APPLY);
        const float scale = drawInfo->second.scale * kOoTxMmForeignModelScaleMultiplier;
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_SONG_EPONA) {
            gDPSetEnvColor(POLY_XLU_DISP++, 146, 87, 49, 255);
        } else if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_SONG_NOVA) {
            gDPSetEnvColor(POLY_XLU_DISP++, 20, 20, 255, 255);
        } else if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_SONG_SONATA) {
            gDPSetEnvColor(POLY_XLU_DISP++, 98, 255, 98, 255);
        } else if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_SONG_SOARING) {
            gDPSetEnvColor(POLY_XLU_DISP++, 200, 160, 255, 255);
        } else if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_SONG_ELEGY) {
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 98, 0, 255);
        } else if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_SONG_LULLABY_INTRO) {
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 100, 100, 255);
        } else if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_SONG_LULLABY) {
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 20, 20, 255);
        } else if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_SONG_OATH) {
            gDPSetEnvColor(POLY_XLU_DISP++, 98, 0, 98, 255);
        }
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        gSPDisplayList(POLY_XLU_DISP++, dlist);
        return;
    }

    Matrix_Translate(0.0f, drawInfo->second.baseTranslateY + kOoTxMmForeignModelHeightUnit, 0.0f, MTXMODE_APPLY);
    const float scale = drawInfo->second.scale * kOoTxMmForeignModelScaleMultiplier;
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_FROG_BLUE) {
        gDPSetEnvColor(POLY_OPA_DISP++, 120, 130, 230, 255);
    } else if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_FROG_CYAN) {
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 170, 200, 255);
    } else if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_FROG_PINK) {
        gDPSetEnvColor(POLY_OPA_DISP++, 210, 120, 100, 255);
    } else if (getItemEntry->drawItemId == OOTXMM_DRAW_MM_FROG_WHITE) {
        gDPSetEnvColor(POLY_OPA_DISP++, 190, 190, 190, 255);
    }
    if (drawInfo->second.rotateX != 0.0f) {
        Matrix_RotateX(drawInfo->second.rotateX, MTXMODE_APPLY);
    }
    if (drawInfo->second.rotateY != 0.0f) {
        Matrix_RotateY(drawInfo->second.rotateY, MTXMODE_APPLY);
    }
    if (drawInfo->second.rotateZ != 0.0f) {
        Matrix_RotateZ(drawInfo->second.rotateZ, MTXMODE_APPLY);
    }
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, dlist);
    if (dlist2 != nullptr) {
        gSPDisplayList(POLY_OPA_DISP++, dlist2);
    }
    if (dlist3 != nullptr) {
        gSPDisplayList(POLY_OPA_DISP++, dlist3);
    }
    if (dlist4 != nullptr) {
        gSPDisplayList(POLY_OPA_DISP++, dlist4);
    }
}

static GetItemEntry BuildCrossoverVisualOnlyItemEntry(RandomizerCheck rc, GetItemEntry getItemEntry) {
    getItemEntry.textId = TEXT_RANDOMIZER_CUSTOM_ITEM;
    if (getItemEntry.modIndex == MOD_NONE) {
        const auto fallbackEntry = GetOoTxMmFallbackOpenChestEntry();
        getItemEntry.modIndex = MOD_RANDOMIZER;
        getItemEntry.tableId = TABLE_RANDOMIZER;
        getItemEntry.getItemId = RG_OPEN_CHEST;
        getItemEntry.itemId = RG_OPEN_CHEST;
        getItemEntry.objectId = fallbackEntry.objectId;
        getItemEntry.gi = fallbackEntry.gi;
        getItemEntry.drawItemId = fallbackEntry.drawItemId;
        getItemEntry.drawModIndex = fallbackEntry.drawModIndex;
        getItemEntry.gid = fallbackEntry.gid;
        getItemEntry.drawFunc = fallbackEntry.drawFunc;
    }

    const auto location = Rando::StaticData::GetLocation(rc);
    if (location != nullptr) {
        const auto placement = SohCrossover::GetSharedPlacementByNativeCheck(location->GetName());
        const auto modelId = SohCrossover::GetModelIdByNativeCheck(location->GetName());
        if (placement.has_value()) {
            SPDLOG_INFO(
                "[OoTxMM][VisualBuild] rc={} location={} check={} item={} sourceGame={} model={} baseMod={} "
                "baseDraw={} baseObject={} baseGi={} baseGetItem={} baseItem={}",
                static_cast<int32_t>(rc), location->GetName(), placement->check, placement->item,
                placement->itemSourceGame, modelId.value_or("(none)"), getItemEntry.modIndex, getItemEntry.drawItemId,
                getItemEntry.objectId, getItemEntry.gi, getItemEntry.getItemId, getItemEntry.itemId);
        } else {
            SPDLOG_INFO(
                "[OoTxMM][VisualBuild] rc={} location={} no placement model={} baseMod={} baseDraw={} baseObject={} "
                "baseGi={} baseGetItem={} baseItem={}",
                static_cast<int32_t>(rc), location->GetName(), modelId.value_or("(none)"), getItemEntry.modIndex,
                getItemEntry.drawItemId, getItemEntry.objectId, getItemEntry.gi, getItemEntry.getItemId,
                getItemEntry.itemId);
        }
        std::string normalizedModelId = modelId.value_or("");
        std::replace(normalizedModelId.begin(), normalizedModelId.end(), ':', '.');
        std::transform(normalizedModelId.begin(), normalizedModelId.end(), normalizedModelId.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        const bool isMmNone =
            (placement.has_value() && placement->item == "mm:RI_NONE") || normalizedModelId == "xmm.none";
        if (isMmNone) {
            getItemEntry.drawFunc = DrawOoTxMmNothing;
            getItemEntry.gid = GID_MAXIMUM;
            return getItemEntry;
        }

        const bool isMmJunk =
            (placement.has_value() && placement->item == "mm:RI_JUNK") || normalizedModelId == "xmm.junk";
        if (isMmJunk) {
            const auto junkEntry = Rando::StaticData::RetrieveItem(
                GetOoTxMmNativeJunkModel(rc, location->GetName())).GetGIEntry_Copy();
            getItemEntry.objectId = junkEntry.objectId;
            getItemEntry.gi = junkEntry.gi;
            getItemEntry.drawItemId = junkEntry.drawItemId;
            getItemEntry.drawModIndex = junkEntry.drawModIndex;
            getItemEntry.gid = junkEntry.gid;
            getItemEntry.drawFunc = junkEntry.drawFunc;
            return getItemEntry;
        }

        if (normalizedModelId == "xmm.trap" || (placement.has_value() && placement->item == "mm:RI_TRAP")) {
            const auto trapEntry = Rando::StaticData::RetrieveItem(RG_ICE_TRAP).GetGIEntry_Copy();
            getItemEntry.objectId = trapEntry.objectId;
            getItemEntry.gi = trapEntry.gi;
            getItemEntry.drawItemId = trapEntry.drawItemId;
            getItemEntry.drawModIndex = trapEntry.drawModIndex;
            getItemEntry.gid = trapEntry.gid;
            getItemEntry.drawFunc = trapEntry.drawFunc;
            return getItemEntry;
        }

        const bool isMmEponaSong =
            normalizedModelId.find("song.epona") != std::string::npos ||
            normalizedModelId.find("song_epona") != std::string::npos ||
            (placement.has_value() && (placement->item.find("SONG_EPONA") != std::string::npos ||
                                       placement->item.find("EPONAS_SONG") != std::string::npos));
        if (isMmEponaSong) {
            const auto songEntry = Rando::StaticData::RetrieveItem(RG_EPONAS_SONG).GetGIEntry_Copy();
            getItemEntry.objectId = songEntry.objectId;
            getItemEntry.gi = songEntry.gi;
            getItemEntry.drawItemId = songEntry.drawItemId;
            getItemEntry.drawModIndex = songEntry.drawModIndex;
            getItemEntry.gid = songEntry.gid;
            getItemEntry.drawFunc = songEntry.drawFunc;
            return getItemEntry;
        }

        const bool isMmStrayFairy =
            normalizedModelId.find("stray_fairy") != std::string::npos ||
            (placement.has_value() && placement->item.find("STRAY_FAIRY") != std::string::npos);
        if (isMmStrayFairy) {
            getItemEntry.objectId = OBJECT_GI_SOUL;
            getItemEntry.gi = GID_FAIRY + 1;
            getItemEntry.drawItemId = GID_FAIRY;
            getItemEntry.drawModIndex = MOD_NONE;
            getItemEntry.gid = GID_FAIRY;
            getItemEntry.drawFunc = nullptr;
            return getItemEntry;
        }

        const bool isMmProgressiveWallet =
            normalizedModelId.find("progressive_wallet") != std::string::npos ||
            normalizedModelId.find("equipment.wallet") != std::string::npos ||
            (placement.has_value() && placement->item.find("PROGRESSIVE_WALLET") != std::string::npos);
        if (isMmProgressiveWallet) {
            const auto walletEntry = Rando::StaticData::RetrieveItem(RG_ADULT_WALLET).GetGIEntry_Copy();
            getItemEntry.objectId = walletEntry.objectId;
            getItemEntry.gi = walletEntry.gi;
            getItemEntry.drawItemId = walletEntry.drawItemId;
            getItemEntry.drawModIndex = walletEntry.drawModIndex;
            getItemEntry.gid = walletEntry.gid;
            getItemEntry.drawFunc = walletEntry.drawFunc;
            return getItemEntry;
        }

        const bool isMmSkulltulaToken =
            normalizedModelId.find("skulltula") != std::string::npos || normalizedModelId.find("gs_token") != std::string::npos ||
            (placement.has_value() && (placement->item.find("GS_TOKEN") != std::string::npos ||
                                       placement->item.find("SKULLTULA") != std::string::npos));
        if (isMmSkulltulaToken) {
            const auto tokenEntry = Rando::StaticData::RetrieveItem(RG_GOLD_SKULLTULA_TOKEN).GetGIEntry_Copy();
            getItemEntry.objectId = tokenEntry.objectId;
            getItemEntry.gi = tokenEntry.gi;
            getItemEntry.drawItemId = tokenEntry.drawItemId;
            getItemEntry.drawModIndex = tokenEntry.drawModIndex;
            getItemEntry.gid = tokenEntry.gid;
            getItemEntry.drawFunc = tokenEntry.drawFunc;
            return getItemEntry;
        }

        const auto applyNativeVisualEntry = [&](RandomizerGet itemId) {
            const auto nativeEntry = Rando::StaticData::RetrieveItem(itemId).GetGIEntry_Copy();
            getItemEntry.objectId = nativeEntry.objectId;
            getItemEntry.gi = nativeEntry.gi;
            getItemEntry.drawItemId = nativeEntry.drawItemId;
            getItemEntry.drawModIndex = nativeEntry.drawModIndex;
            getItemEntry.gid = nativeEntry.gid;
            getItemEntry.drawFunc = nativeEntry.drawFunc;
        };

        if (normalizedModelId == "xshared.heart_piece" || (placement.has_value() && placement->item == "mm:RI_HEART_PIECE")) {
            applyNativeVisualEntry(RG_PIECE_OF_HEART);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.heart_container" || normalizedModelId == "xshared.double_defense" ||
            (placement.has_value() && placement->item == "mm:RI_HEART_CONTAINER")) {
            applyNativeVisualEntry(RG_HEART_CONTAINER);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.recovery_heart") {
            applyNativeVisualEntry(RG_RECOVERY_HEART);
            return getItemEntry;
        }

        const bool isMmSilverRupee =
            normalizedModelId == "xshared.rupee.silver" ||
            normalizedModelId.find("rupee_silver") != std::string::npos ||
            (placement.has_value() && placement->item.find("RUPEE_SILVER") != std::string::npos);
        if (isMmSilverRupee) {
            getItemEntry.objectId = OBJECT_GI_RUPY;
            getItemEntry.gi = GI_RUPEE_GOLD;
            getItemEntry.drawItemId = GID_RUPEE_GOLD;
            getItemEntry.drawModIndex = MOD_NONE;
            getItemEntry.gid = GID_MAXIMUM;
            getItemEntry.drawFunc = DrawOoTxMmSilverRupeeVisual;
            return getItemEntry;
        }

        const bool isMmHugeRupee =
            normalizedModelId == "xshared.rupee.huge" ||
            normalizedModelId.find("rupee_huge") != std::string::npos ||
            normalizedModelId.find("huge_rupee") != std::string::npos ||
            (placement.has_value() && placement->item.find("HUGE_RUPEE") != std::string::npos);
        if (isMmHugeRupee) {
            applyNativeVisualEntry(RG_HUGE_RUPEE);
            return getItemEntry;
        }

        const bool isMmPurpleRupee =
            normalizedModelId == "xshared.rupee.purple" ||
            normalizedModelId.find("rupee_purple") != std::string::npos ||
            (placement.has_value() && placement->item.find("RUPEE_PURPLE") != std::string::npos);
        if (isMmPurpleRupee) {
            applyNativeVisualEntry(RG_PURPLE_RUPEE);
            return getItemEntry;
        }

        const bool isMmRedRupee =
            normalizedModelId == "xshared.rupee.red" ||
            normalizedModelId.find("rupee_red") != std::string::npos ||
            (placement.has_value() && placement->item.find("RUPEE_RED") != std::string::npos);
        if (isMmRedRupee) {
            applyNativeVisualEntry(RG_RED_RUPEE);
            return getItemEntry;
        }

        const bool isMmBlueRupee =
            normalizedModelId == "xshared.rupee.blue" ||
            normalizedModelId.find("rupee_blue") != std::string::npos ||
            (placement.has_value() && placement->item.find("RUPEE_BLUE") != std::string::npos);
        if (isMmBlueRupee) {
            applyNativeVisualEntry(RG_BLUE_RUPEE);
            return getItemEntry;
        }

        const bool isMmGreenRupee =
            normalizedModelId == "xshared.rupee.green" ||
            normalizedModelId.find("rupee_green") != std::string::npos ||
            (placement.has_value() && placement->item.find("RUPEE_GREEN") != std::string::npos);
        if (isMmGreenRupee || normalizedModelId == "xshared.rupee") {
            applyNativeVisualEntry(RG_GREEN_RUPEE);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.arrows") {
            applyNativeVisualEntry(RG_ARROWS_30);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.arrows.fire") {
            applyNativeVisualEntry(RG_FIRE_ARROWS);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.arrows.ice") {
            applyNativeVisualEntry(RG_ICE_ARROWS);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.arrows.light") {
            applyNativeVisualEntry(RG_LIGHT_ARROWS);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.bombs") {
            applyNativeVisualEntry(RG_BOMBS_10);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.bombchu") {
            applyNativeVisualEntry(RG_BOMBCHU_10);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.magic_jar_small") {
            applyNativeVisualEntry(RG_MAGIC_SINGLE);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.magic_jar_large") {
            applyNativeVisualEntry(RG_MAGIC_DOUBLE);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.deku_seeds") {
            applyNativeVisualEntry(RG_DEKU_SEEDS_30);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.deku_nuts") {
            applyNativeVisualEntry(RG_DEKU_NUTS_5);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.deku_sticks") {
            applyNativeVisualEntry(RG_DEKU_STICK_1);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.bomb_bag") {
            applyNativeVisualEntry(RG_PROGRESSIVE_BOMB_BAG);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.magic_beans") {
            applyNativeVisualEntry(RG_MAGIC_BEAN);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.lens") {
            applyNativeVisualEntry(RG_LENS_OF_TRUTH);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.wallet") {
            applyNativeVisualEntry(RG_PROGRESSIVE_WALLET);
            return getItemEntry;
        }

        if (normalizedModelId.find("_refill") != std::string::npos) {
            applyNativeVisualEntry(RG_BOTTLE_WITH_RED_POTION);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.bottle.empty") {
            applyNativeVisualEntry(RG_EMPTY_BOTTLE);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.bottle.fairy") {
            applyNativeVisualEntry(RG_BOTTLE_WITH_RED_POTION);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.bottle.fish") {
            applyNativeVisualEntry(RG_BOTTLE_WITH_RED_POTION);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.bottle.bug") {
            applyNativeVisualEntry(RG_BOTTLE_WITH_RED_POTION);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.bottle.milk" || normalizedModelId == "xshared.bottle.half_milk") {
            applyNativeVisualEntry(RG_BOTTLE_WITH_RED_POTION);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.bottle.red_potion") {
            applyNativeVisualEntry(RG_BOTTLE_WITH_RED_POTION);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.bottle.blue_potion") {
            applyNativeVisualEntry(RG_BOTTLE_WITH_RED_POTION);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.bottle.small_poe") {
            applyNativeVisualEntry(RG_BOTTLE_WITH_RED_POTION);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.bottle.big_poe") {
            applyNativeVisualEntry(RG_BOTTLE_WITH_RED_POTION);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.mask.goron") {
            applyNativeVisualEntry(RG_GORON_MASK);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.mask.zora") {
            applyNativeVisualEntry(RG_ZORA_MASK);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.mask.keaton") {
            applyNativeVisualEntry(RG_KEATON_MASK);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.mask.truth") {
            applyNativeVisualEntry(RG_MASK_OF_TRUTH);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.mask.bunny") {
            applyNativeVisualEntry(RG_BUNNY_HOOD);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.ocarina_of_time") {
            applyNativeVisualEntry(RG_OCARINA_OF_TIME);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.song.time") {
            applyNativeVisualEntry(RG_SONG_OF_TIME);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.song.sun") {
            applyNativeVisualEntry(RG_SUNS_SONG);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.song.saria") {
            applyNativeVisualEntry(RG_SARIAS_SONG);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.song.epona") {
            applyNativeVisualEntry(RG_EPONAS_SONG);
            return getItemEntry;
        }

        if (normalizedModelId == "xshared.song.storms") {
            applyNativeVisualEntry(RG_SONG_OF_STORMS);
            return getItemEntry;
        }

        if (modelId.has_value()) {
            const bool isMmUnsafeQuestModel =
                normalizedModelId.find("moon") != std::string::npos ||
                normalizedModelId.find("tear") != std::string::npos ||
                (placement.has_value() && (placement->item.find("MOON") != std::string::npos ||
                                           placement->item.find("TEAR") != std::string::npos));
            if (isMmUnsafeQuestModel) {
                RandomizerGet placeholder = RG_STONE_OF_AGONY;
                const auto questEntry = Rando::StaticData::RetrieveItem(placeholder).GetGIEntry_Copy();
                getItemEntry.objectId = questEntry.objectId;
                getItemEntry.gi = questEntry.gi;
                getItemEntry.drawItemId = questEntry.drawItemId;
                getItemEntry.drawModIndex = questEntry.drawModIndex;
                getItemEntry.gid = questEntry.gid;
                getItemEntry.drawFunc = questEntry.drawFunc;
                return getItemEntry;
            }

            const bool isMmProgressiveSword =
                normalizedModelId.find("progressive_sword") != std::string::npos ||
                (placement.has_value() && placement->item.find("PROGRESSIVE_SWORD") != std::string::npos);
            if (isMmProgressiveSword) {
                getItemEntry.drawItemId = SohCrossover::CountMmProgressiveSwordGrantsForCurrentSave() > 0
                                              ? OOTXMM_DRAW_MM_GILDED_SWORD
                                              : OOTXMM_DRAW_MM_RAZOR_SWORD;
                getItemEntry.drawFunc = Randomizer_DrawOoTxMmForeignModel;
                return getItemEntry;
            }

            const auto foreignDrawId = ResolveOoTxMmForeignDrawId(*modelId);
            if (foreignDrawId.has_value()) {
                getItemEntry.drawItemId = *foreignDrawId;
                getItemEntry.drawFunc = Randomizer_DrawOoTxMmForeignModel;
            } else {
                const auto fallbackEntry = GetOoTxMmFallbackOpenChestEntry();
                getItemEntry.objectId = fallbackEntry.objectId;
                getItemEntry.gi = fallbackEntry.gi;
                getItemEntry.drawItemId = fallbackEntry.drawItemId;
                getItemEntry.drawModIndex = fallbackEntry.drawModIndex;
                getItemEntry.gid = fallbackEntry.gid;
                getItemEntry.drawFunc = fallbackEntry.drawFunc;
            }
        }
    }

    return getItemEntry;
}

GetItemEntry Randomizer_ApplyCrossoverVisualOnlyItemEntry(RandomizerCheck rc, GetItemEntry getItemEntry) {
    if (SohCrossover::IsCrossGameMmCheck(rc) &&
        (!SohCrossover::ShouldGiveNativeItemForCrossGameMmCheck(rc) ||
         SohCrossover::ShouldUseCrossoverVisualForCrossGameMmCheck(rc))) {
        return BuildCrossoverVisualOnlyItemEntry(rc, getItemEntry);
    }

    return getItemEntry;
}

extern "C" GetItemEntry Randomizer_ApplyCrossoverVisualOnlyItemEntryForC(RandomizerCheck rc, GetItemEntry getItemEntry) {
    return Randomizer_ApplyCrossoverVisualOnlyItemEntry(rc, getItemEntry);
}

static GetItemEntry ApplyCrossoverVisualOnlyItemEntry(RandomizerCheck rc, GetItemEntry getItemEntry) {
    return Randomizer_ApplyCrossoverVisualOnlyItemEntry(rc, getItemEntry);
}

static bool Randomizer_ShouldSkipGetItemAnimation(RandomizerCheck rc, GetItemEntry getItemEntry,
                                                  GetItemCategory getItemCategory) {
    return rc != RC_HF_OCARINA_OF_TIME_ITEM && rc != RC_SPIRIT_TEMPLE_SILVER_GAUNTLETS_CHEST &&
           rc != RC_MARKET_BOMBCHU_BOWLING_FIRST_PRIZE && rc != RC_MARKET_BOMBCHU_BOWLING_SECOND_PRIZE &&
           !(getItemEntry.modIndex == MOD_RANDOMIZER && getItemEntry.getItemId == RG_ICE_TRAP) && IS_RANDO &&
           (CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("TimeSavers.SkipGetItemAnimation"), SGIA_JUNK) == SGIA_ALL ||
            (CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("TimeSavers.SkipGetItemAnimation"), SGIA_JUNK) == SGIA_JUNK &&
             !(getItemEntry.getItemId >= RG_DEKU_TREE_MAP && getItemEntry.getItemId <= RG_ICE_CAVERN_MAP &&
               getItemEntry.modIndex == MOD_RANDOMIZER) &&
             (getItemCategory == ITEM_CATEGORY_JUNK || getItemCategory == ITEM_CATEGORY_SKULLTULA_TOKEN ||
              getItemCategory == ITEM_CATEGORY_HEALTH || getItemCategory == ITEM_CATEGORY_LESSER)));
}

void RandomizerOnFlagSetHandler(int16_t flagType, int16_t flag) {
    // Consume adult trade items
    if (RAND_GET_OPTION(RSK_SHUFFLE_ADULT_TRADE) && flagType == FLAG_RANDOMIZER_INF) {
        switch (flag) {
            case RAND_INF_ADULT_TRADES_DMT_TRADE_BROKEN_SWORD:
                Flags_UnsetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_SWORD_BROKEN);
                Inventory_ReplaceItem(gPlayState, ITEM_SWORD_BROKEN, Randomizer_GetNextAdultTradeItem());
                break;
            case RAND_INF_ADULT_TRADES_DMT_TRADE_EYEDROPS:
                Flags_UnsetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_EYEDROPS);
                Inventory_ReplaceItem(gPlayState, ITEM_EYEDROPS, Randomizer_GetNextAdultTradeItem());
                break;
        }
    }

    if (flagType == FLAG_EVENT_CHECK_INF && flag == EVENTCHKINF_TALON_WOKEN_IN_CASTLE) {
        // remove chicken as this is the only use for it
        Flags_UnsetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_CHICKEN);
    }

    if (flagType == FLAG_EVENT_CHECK_INF && flag == EVENTCHKINF_OBTAINED_ZELDAS_LETTER) {
        Flags_SetRandomizerInf(RAND_INF_ZELDAS_LETTER);
    }

    if (flagType == FLAG_EVENT_CHECK_INF && flag == EVENTCHKINF_TALON_RETURNED_FROM_CASTLE) {
        if (Flags_GetEventChkInf(EVENTCHKINF_OBTAINED_POCKET_EGG)) {
            Flags_SetRandomizerInf(RAND_INF_TALON_SENT_MALON_HOME);
        }
    }

    RandomizerCheck rc = GetRandomizerCheckFromFlag(flagType, flag);
    if (rc == RC_UNKNOWN_CHECK)
        return;

    if (flagType == FLAG_GS_TOKEN &&
        Rando::Context::GetInstance()->GetOption(RSK_SHUFFLE_TOKENS).Is(RO_TOKENSANITY_OFF)) {
        Rando::Context::GetInstance()->GetItemLocation(rc)->SetCheckStatus(RCSHOW_COLLECTED);
        return;
    }
    auto loc = Rando::Context::GetInstance()->GetItemLocation(rc);
    if (loc == nullptr || loc->HasObtained() || loc->GetPlacedRandomizerGet() == RG_NONE) {
        Rando::Context::GetInstance()->GetItemLocation(rc)->SetCheckStatus(RCSHOW_COLLECTED);
        return;
    }

    SPDLOG_INFO("Queuing RC: {}", static_cast<uint32_t>(rc));
    randomizerQueuedChecks.push(rc);
}

void RandomizerOnSceneFlagSetHandler(int16_t sceneNum, int16_t flagType, int16_t flag) {
    if (flagType == FLAG_SCENE_SWITCH) {
        auto dungeonInfo = Rando::Context::GetInstance()->GetDungeons()->GetDungeonFromScene(sceneNum);
        bool isVanilla = dungeonInfo == nullptr || dungeonInfo->IsVanilla();

        switch (sceneNum) {
            case SCENE_GERUDOS_FORTRESS:
                if (RAND_GET_OPTION(RSK_SHUFFLE_DUNGEON_ENTRANCES).IsNot(RO_DUNGEON_ENTRANCE_SHUFFLE_OFF) &&
                    flag == 0x3A) {
                    Flags_SetRandomizerInf(RAND_INF_GF_GTG_GATE_PERMANENTLY_OPEN);
                }
                break;
            case SCENE_DEKU_TREE:
                if (!isVanilla && flag == 0x27) {
                    Flags_SetRandomizerInf(RAND_INF_DEKU_TREE_MQ_TORCH_SWITCH);
                }
                break;
            case SCENE_DODONGOS_CAVERN:
                if (!isVanilla && flag == 0x25) {
                    Flags_SetRandomizerInf(RAND_INF_DODONGOS_CAVERN_MQ_SILVER_RUPEES);
                }
                break;
            case SCENE_JABU_JABU:
                if (isVanilla && flag == 0x3b) {
                    Flags_SetRandomizerInf(RAND_INF_JABU_JABUS_BELLY_FIRST_SWITCH);
                }
                break;
            case SCENE_FOREST_TEMPLE:
                if (flag == 0x26) {
                    Flags_SetRandomizerInf(RAND_INF_FOREST_DRAINED_WELL);
                } else if (flag == 0x25) {
                    Flags_SetRandomizerInf(RAND_INF_FOREST_LOBBY_EYES);
                    if (!isVanilla) {
                        Flags_SetSwitch(gPlayState, 0x2a);
                    }
                } else if (!isVanilla && flag == 0x2a) {
                    Flags_SetRandomizerInf(RAND_INF_FOREST_LOBBY_EYES);
                    Flags_SetSwitch(gPlayState, 0x25);
                } else if (!isVanilla && flag == 0x21) {
                    Flags_SetRandomizerInf(RAND_INF_FOREST_MQ_COURTYARD_WEB_BURNT);
                }
                break;
            case SCENE_FIRE_TEMPLE:
                if (!isVanilla && flag == 0x28) {
                    Flags_SetRandomizerInf(RAND_INF_FIRE_MQ_LOBBY_TORCHES);
                }
                break;
            case SCENE_SPIRIT_TEMPLE:
                if (isVanilla && flag == 0x23) {
                    Flags_SetRandomizerInf(RAND_INF_SPIRIT_SUN_ON_FLOOR_ON);
                } else if (!isVanilla && flag == 0x37) {
                    Flags_SetRandomizerInf(RAND_INF_SPIRIT_MQ_LOBBY_SILVER_RUPEES);
                }
                break;
        }
    }

    RandomizerCheck rc = GetRandomizerCheckFromSceneFlag(sceneNum, flagType, flag);
    if (rc == RC_UNKNOWN_CHECK)
        return;

    auto loc = Rando::Context::GetInstance()->GetItemLocation(rc);
    if (loc == nullptr || loc->HasObtained() || loc->GetPlacedRandomizerGet() == RG_NONE)
        return;

    SPDLOG_INFO("Queuing RC: {}", static_cast<uint32_t>(rc));
    randomizerQueuedChecks.push(rc);
}

static Vec3f spawnPos = { 0.0f, -999.0f, 0.0f };

void RandomizerOnPlayerUpdateForRCQueueHandler() {
    // If we're already queued, don't queue again
    if (randomizerQueuedCheck != RC_UNKNOWN_CHECK)
        return;

    // If there's nothing to queue, don't queue
    if (randomizerQueuedChecks.size() < 1)
        return;

    // If we're in a cutscene, don't queue
    Player* player = GET_PLAYER(gPlayState);
    if (Player_InBlockingCsMode(gPlayState, player) || player->stateFlags1 & PLAYER_STATE1_IN_ITEM_CS ||
        player->stateFlags1 & PLAYER_STATE1_GETTING_ITEM || player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
        return;
    }

    RandomizerCheck rc = randomizerQueuedChecks.front();
    auto loc = Rando::Context::GetInstance()->GetItemLocation(rc);
    RandomizerGet vanillaRandomizerGet = Rando::StaticData::GetLocation(rc)->GetVanillaItem();
    GetItemID vanillaItem = (GetItemID)Rando::StaticData::RetrieveItem(vanillaRandomizerGet).GetItemID();
    GetItemEntry getItemEntry =
        Rando::Context::GetInstance()->GetFinalGIEntry(rc, true, (GetItemID)vanillaRandomizerGet);
    GetItemCategory getItemCategory = Randomizer_AdjustItemCategoryForCheck(getItemEntry, rc);
    const bool isVisualOnlyCrossoverMmItem =
        SohCrossover::IsCrossGameMmCheck(rc) && !SohCrossover::ShouldGiveNativeItemForCrossGameMmCheck(rc);

    if (loc->HasObtained()) {
        SPDLOG_INFO("RC {} already obtained, skipping", static_cast<uint32_t>(rc));
    } else {
        iceTrapScale = 0.0f;
        randomizerQueuedCheck = rc;
        randomizerQueuedItemEntry = ApplyCrossoverVisualOnlyItemEntry(rc, getItemEntry);
        randomizerQueuedItemGiveStarted = false;
        randomizerQueuedEmitWindowMessage = false;
        SPDLOG_INFO("Queuing Item mod {} item {} from RC {}", getItemEntry.modIndex, getItemEntry.itemId,
                    static_cast<uint32_t>(rc));
        if (isVisualOnlyCrossoverMmItem) {
            SohCrossover::QueueCrossGameMmPickupTextbox(rc);
        }
        if (Randomizer_ShouldSkipGetItemAnimation(rc, getItemEntry, getItemCategory)) {
            Item_DropCollectible(gPlayState, &spawnPos, static_cast<int16_t>(ITEM00_SOH_GIVE_ITEM_ENTRY | 0x8000));
        }
    }

    randomizerQueuedChecks.pop();
}

void RandomizerOnPlayerUpdateForItemQueueHandler() {
    if (randomizerQueuedCheck == RC_UNKNOWN_CHECK)
        return;

    Player* player = GET_PLAYER(gPlayState);
    if (player == NULL) {
        return;
    }

    if (randomizerQueuedItemGiveStarted) {
        if (!(Player_InBlockingCsMode(gPlayState, player) || player->stateFlags1 & PLAYER_STATE1_IN_ITEM_CS ||
              player->stateFlags1 & PLAYER_STATE1_GETTING_ITEM || player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR)) {
            if (++randomizerQueuedItemGiveSettledFrames >= 2) {
                SPDLOG_INFO("Queued item give settled without callback, completing RC {}", static_cast<uint32_t>(randomizerQueuedCheck));
                const RandomizerCheck completedCheck = randomizerQueuedCheck;
                const GetItemEntry completedItemEntry = randomizerQueuedItemEntry;
                const bool emitWindowMessage = randomizerQueuedEmitWindowMessage;
                RandomizerCompleteQueuedCheckAfterNativeGive();
                if (emitWindowMessage) {
                    EmitWindowMessageForRandomizerCheck(completedCheck, completedItemEntry);
                }
            }
        } else {
            randomizerQueuedItemGiveSettledFrames = 0;
        }
        return;
    }

    if (Player_InBlockingCsMode(gPlayState, player) ||
        player->stateFlags1 & PLAYER_STATE1_IN_ITEM_CS || player->stateFlags1 & PLAYER_STATE1_GETTING_ITEM ||
        player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
        return;
    }

    SPDLOG_INFO("Attempting to give Item mod {} item {} from RC {}", randomizerQueuedItemEntry.modIndex,
                randomizerQueuedItemEntry.itemId, static_cast<uint32_t>(randomizerQueuedCheck));
    if (GiveItemEntryWithoutActor(gPlayState, randomizerQueuedItemEntry)) {
        randomizerQueuedItemGiveStarted = true;
        randomizerQueuedItemGiveSettledFrames = 0;
    }
    if (player->stateFlags1 & PLAYER_STATE1_IN_WATER) {
        // Allow the player to receive the item while swimming
        player->stateFlags2 |= PLAYER_STATE2_UNDERWATER;
        Player_ActionHandler_2(player, gPlayState);
    }
}

void RandomizerOnItemReceiveHandler(GetItemEntry receivedItemEntry) {
    if (randomizerQueuedCheck == RC_UNKNOWN_CHECK)
        return;

    auto loc = Rando::Context::GetInstance()->GetItemLocation(randomizerQueuedCheck);
    if (randomizerQueuedCheck != RC_UNKNOWN_CHECK) {
        SPDLOG_INFO("Queued receive compare: queued mod {} item {} gi {} | received mod {} item {} gi {} from RC {}",
                    randomizerQueuedItemEntry.modIndex, randomizerQueuedItemEntry.itemId, randomizerQueuedItemEntry.getItemId,
                    receivedItemEntry.modIndex, receivedItemEntry.itemId, receivedItemEntry.getItemId,
                    static_cast<uint32_t>(randomizerQueuedCheck));
    }

    if (randomizerQueuedItemEntry.modIndex == receivedItemEntry.modIndex &&
        (randomizerQueuedItemEntry.itemId == receivedItemEntry.itemId ||
         randomizerQueuedItemEntry.getItemId == receivedItemEntry.getItemId ||
         randomizerQueuedItemEntry.itemId == receivedItemEntry.getItemId ||
         randomizerQueuedItemEntry.getItemId == receivedItemEntry.itemId)) {
        const RandomizerCheck completedCheck = randomizerQueuedCheck;
        const GetItemEntry completedItemEntry = randomizerQueuedItemEntry;
        const bool emitWindowMessage = randomizerQueuedEmitWindowMessage;
        SPDLOG_INFO("Item received mod {} item {} from RC {}", receivedItemEntry.modIndex, receivedItemEntry.itemId,
                    static_cast<uint32_t>(randomizerQueuedCheck));
        loc->SetCheckStatus(RCSHOW_COLLECTED);
        CheckTracker::SpoilAreaFromCheck(randomizerQueuedCheck);
        CheckTracker::RecalculateAllAreaTotals();
        CheckTracker::RecalculateAvailableChecks();
        SaveManager::Instance->SaveSection(gSaveContext.fileNum, SECTION_ID_TRACKER_DATA, true);
        randomizerQueuedCheck = RC_UNKNOWN_CHECK;
        randomizerQueuedItemEntry = GET_ITEM_NONE;
        randomizerQueuedItemGiveStarted = false;
        randomizerQueuedItemGiveSettledFrames = 0;
        randomizerQueuedEmitWindowMessage = false;
        if (emitWindowMessage) {
            EmitWindowMessageForRandomizerCheck(completedCheck, completedItemEntry);
        }
    }

    if (receivedItemEntry.modIndex == MOD_RANDOMIZER && receivedItemEntry.getItemId == RG_MAGIC_BEAN_PACK) {
        if (OTRGlobals::Instance->gRandomizer->GetRandoSettingValue(RSK_SKIP_PLANTING_BEANS)) {
            gSaveContext.sceneFlags[SCENE_DEATH_MOUNTAIN_CRATER].swch |= (1 << 3);
            if (gPlayState->sceneNum == SCENE_DEATH_MOUNTAIN_CRATER) {
                Flags_SetSwitch(gPlayState, 3);
            }
            gSaveContext.sceneFlags[SCENE_DEATH_MOUNTAIN_TRAIL].swch |= (1 << 6);
            if (gPlayState->sceneNum == SCENE_DEATH_MOUNTAIN_TRAIL) {
                Flags_SetSwitch(gPlayState, 6);
            }
            gSaveContext.sceneFlags[SCENE_DESERT_COLOSSUS].swch |= (1 << 24);
            if (gPlayState->sceneNum == SCENE_DESERT_COLOSSUS) {
                Flags_SetSwitch(gPlayState, 24);
            }
            gSaveContext.sceneFlags[SCENE_GERUDO_VALLEY].swch |= (1 << 3);
            if (gPlayState->sceneNum == SCENE_GERUDO_VALLEY) {
                Flags_SetSwitch(gPlayState, 3);
            }
            gSaveContext.sceneFlags[SCENE_GRAVEYARD].swch |= (1 << 3);
            if (gPlayState->sceneNum == SCENE_GRAVEYARD) {
                Flags_SetSwitch(gPlayState, 3);
            }
            gSaveContext.sceneFlags[SCENE_KOKIRI_FOREST].swch |= (1 << 9);
            if (gPlayState->sceneNum == SCENE_KOKIRI_FOREST) {
                Flags_SetSwitch(gPlayState, 9);
            }
            gSaveContext.sceneFlags[SCENE_LAKE_HYLIA].swch |= (1 << 1);
            if (gPlayState->sceneNum == SCENE_LAKE_HYLIA) {
                Flags_SetSwitch(gPlayState, 1);
            }
            gSaveContext.sceneFlags[SCENE_LOST_WOODS].swch |= (1 << 4) | (1 << 18);
            if (gPlayState->sceneNum == SCENE_LOST_WOODS) {
                Flags_SetSwitch(gPlayState, 4);
                Flags_SetSwitch(gPlayState, 18);
            }
            gSaveContext.sceneFlags[SCENE_ZORAS_RIVER].swch |= (1 << 3);
            if (gPlayState->sceneNum == SCENE_ZORAS_RIVER) {
                Flags_SetSwitch(gPlayState, 3);
            }
            ObjBean* bean = (ObjBean*)Actor_Find(&gPlayState->actorCtx, ACTOR_OBJ_BEAN, ACTORCAT_BG);
            if (bean != nullptr) {
                Flags_SetSwitch(gPlayState, bean->dyna.actor.params & 0x3F);
                func_80B8FE00(bean);
            }
            AMMO(ITEM_BEAN) = 0;
        }
    }

    if (receivedItemEntry.modIndex == MOD_NONE &&
        (receivedItemEntry.itemId == ITEM_HEART_PIECE || receivedItemEntry.itemId == ITEM_HEART_PIECE_2 ||
         receivedItemEntry.itemId == ITEM_HEART_CONTAINER)) {
        gSaveContext.healthAccumulator = MAX_HEALTH; // Refill 20 hearts
        if ((s32)(gSaveContext.inventory.questItems & 0xF0000000) == 0x40000000) {
            gSaveContext.inventory.questItems ^= 0x40000000;
            gSaveContext.healthCapacity += FULL_HEART_HEALTH;
            gSaveContext.health += FULL_HEART_HEALTH;
        }
    }

    if (loc->GetRandomizerCheck() == RC_SPIRIT_TEMPLE_SILVER_GAUNTLETS_CHEST &&
        !CVarGetInteger(CVAR_ENHANCEMENT("TimeSavers.SkipCutscene.Story"), IS_RANDO)) {
        static uint32_t updateHook;
        updateHook = GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>([]() {
            Player* player = GET_PLAYER(gPlayState);
            if (player == NULL || Player_InBlockingCsMode(gPlayState, player) ||
                player->stateFlags1 & PLAYER_STATE1_IN_ITEM_CS || player->stateFlags1 & PLAYER_STATE1_GETTING_ITEM ||
                player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
                return;
            }

            gPlayState->nextEntranceIndex = ENTR_DESERT_COLOSSUS_EAST_EXIT;
            gPlayState->transitionTrigger = TRANS_TRIGGER_START;
            gSaveContext.nextCutsceneIndex = 0xFFF1;
            gPlayState->transitionType = TRANS_TYPE_SANDSTORM_END;
            GET_PLAYER(gPlayState)->stateFlags1 &= ~PLAYER_STATE1_IN_CUTSCENE;
            Player_TryCsAction(gPlayState, NULL, 8);
            GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnPlayerUpdate>(updateHook);
        });
    }
}

void EnExItem_DrawRandomizedItem(EnExItem* enExItem, PlayState* play) {
    GetItemEntry randoGetItem = enExItem->sohItemEntry;
    if (CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"), 0)) {
        randoGetItem = GET_ITEM_MYSTERY;
    }
    func_8002EBCC(&enExItem->actor, play, 0);
    func_8002ED80(&enExItem->actor, play, 0);
    EnItem00_CustomItemsParticles(&enExItem->actor, play, randoGetItem);
    GetItemEntry_Draw(play, randoGetItem);
}

void EnExItem_WaitForObjectRandomized(EnExItem* enExItem, PlayState* play) {
    EnExItem_WaitForObject(enExItem, play);
    if (Object_IsLoaded(&play->objectCtx, enExItem->objectIdx)) {
        enExItem->actor.draw = (ActorFunc)EnExItem_DrawRandomizedItem;
        Actor_SetScale(&enExItem->actor, enExItem->scale);

        // for now we're just using this to not have items float
        // below the bowling counter, but it would be nice to use
        // this to not draw gigantic skull tokens etc.
        switch (enExItem->type) {
            case EXITEM_BOMB_BAG_COUNTER: {
                enExItem->actor.shape.yOffset = -10.0f;
                break;
            }
        }
    }
}

void EnItem00_DrawRandomizedItem(EnItem00* enItem00, PlayState* play) {
    f32 mtxScale = CVarGetFloat(CVAR_RANDOMIZER_ENHANCEMENT("TimeSavers.SkipGetItemAnimationScale"), 10.0f);
    Matrix_Scale(mtxScale, mtxScale, mtxScale, MTXMODE_APPLY);
    GetItemEntry randoItem = enItem00->itemEntry;
    if (CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"), 0) &&
        enItem00->actor.params != ITEM00_SOH_GIVE_ITEM_ENTRY) {
        randoItem = GET_ITEM_MYSTERY;
    }
    func_8002EBCC(&enItem00->actor, play, 0);
    func_8002ED80(&enItem00->actor, play, 0);
    EnItem00_CustomItemsParticles(&enItem00->actor, play, randoItem);
    GetItemEntry_Draw(play, randoItem);
}

void ItemBHeart_DrawRandomizedItem(ItemBHeart* itemBHeart, PlayState* play) {
    GetItemEntry randoItem = itemBHeart->sohItemEntry;
    if (CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"), 0)) {
        randoItem = GET_ITEM_MYSTERY;
    }
    func_8002EBCC(&itemBHeart->actor, play, 0);
    func_8002ED80(&itemBHeart->actor, play, 0);
    EnItem00_CustomItemsParticles(&itemBHeart->actor, play, randoItem);
    GetItemEntry_Draw(play, randoItem);
}

void ItemBHeart_UpdateRandomizedItem(Actor* actor, PlayState* play) {
    ItemBHeart* itemBHeart = (ItemBHeart*)actor;

    func_80B85264(itemBHeart, play);
    Actor_UpdateBgCheckInfo(play, &itemBHeart->actor, 0.0f, 0.0f, 0.0f, 4);
    if ((itemBHeart->actor.xzDistToPlayer < 30.0f) && (fabsf(itemBHeart->actor.yDistToPlayer) < 40.0f)) {
        Flags_SetCollectible(play, 0x1F);
        Actor_Kill(&itemBHeart->actor);
    }
}

void ItemEtcetera_DrawRandomizedItem(ItemEtcetera* itemEtcetera, PlayState* play) {
    GetItemEntry randoItem = itemEtcetera->sohItemEntry;
    if (CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"), 0)) {
        randoItem = GET_ITEM_MYSTERY;
    }
    EnItem00_CustomItemsParticles(&itemEtcetera->actor, play, randoItem);
    func_8002EBCC(&itemEtcetera->actor, play, 0);
    func_8002ED80(&itemEtcetera->actor, play, 0);
    GetItemEntry_Draw(play, randoItem);
}

void ItemEtcetera_DrawRandomizedItemThroughLens(ItemEtcetera* itemEtcetera, PlayState* play) {
    if (play->actorCtx.lensActive) { // todo [Rando] mysterious shuffle for chest minigame key shuffle
        ItemEtcetera_DrawRandomizedItem(itemEtcetera, play);
    }
}

void ItemEtcetera_func_80B858B4_Randomized(ItemEtcetera* itemEtcetera, PlayState* play) {
    if (itemEtcetera->actor.xzDistToPlayer < 30.0f && fabsf(itemEtcetera->actor.yDistToPlayer) < 50.0f) {
        if ((itemEtcetera->actor.params & 0xFF) == 1) {
            Flags_SetEventChkInf(EVENTCHKINF_OBTAINED_RUTOS_LETTER);
            Flags_SetSwitch(play, 0xB);
        }

        Actor_Kill(&itemEtcetera->actor);
    } else {
        if ((play->gameplayFrames & 0xD) == 0) {
            EffectSsBubble_Spawn(play, &itemEtcetera->actor.world.pos, 0.0f, 0.0f, 10.0f, 0.13f);
        }
    }
}

void ItemEtcetera_func_80B85824_Randomized(ItemEtcetera* itemEtcetera, PlayState* play) {
    if ((itemEtcetera->actor.params & 0xFF) != 7) {
        return;
    }

    if (itemEtcetera->actor.xzDistToPlayer < 30.0f && fabsf(itemEtcetera->actor.yDistToPlayer) < 50.0f) {

        Flags_SetTreasure(play, 0x1F);
        Actor_Kill(&itemEtcetera->actor);
    }
}

void ItemEtcetera_MoveRandomizedFireArrowDown(ItemEtcetera* itemEtcetera, PlayState* play) {
    Actor_UpdateBgCheckInfo(play, &itemEtcetera->actor, 10.0f, 10.0f, 0.0f, 5);
    Actor_MoveXZGravity(&itemEtcetera->actor);
    if (!(itemEtcetera->actor.bgCheckFlags & 1)) {
        ItemEtcetera_SpawnSparkles(itemEtcetera, play);
    }
    itemEtcetera->actor.shape.rot.y += 0x400;
    ItemEtcetera_func_80B85824_Randomized(itemEtcetera, play);
}

void ItemEtcetera_UpdateRandomizedFireArrow(ItemEtcetera* itemEtcetera, PlayState* play) {
    if ((play->csCtx.state != CS_STATE_IDLE) && (play->csCtx.npcActions[0] != NULL)) {
        if (play->csCtx.npcActions[0]->action == 2) {
            itemEtcetera->actor.draw = (ActorFunc)ItemEtcetera_DrawRandomizedItem;
            itemEtcetera->actor.gravity = -0.1f;
            itemEtcetera->actor.minVelocityY = -4.0f;
            itemEtcetera->actionFunc = ItemEtcetera_MoveRandomizedFireArrowDown;
        }
    } else {
        itemEtcetera->actor.gravity = -0.1f;
        itemEtcetera->actor.minVelocityY = -4.0f;
        itemEtcetera->actionFunc = ItemEtcetera_MoveRandomizedFireArrowDown;
    }
}

u8 EnDs_RandoCanGetGrannyItem() {
    return (RAND_GET_OPTION(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_ALL_BUT_BEANS) ||
            RAND_GET_OPTION(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_ALL)) &&
           !Flags_GetRandomizerInf(RAND_INF_MERCHANTS_GRANNYS_SHOP) &&
           // Traded odd mushroom when adult trade is on
           ((RAND_GET_OPTION(RSK_SHUFFLE_ADULT_TRADE) && Flags_GetItemGetInf(ITEMGETINF_30)) ||
            // Found claim check when adult trade is off
            (!RAND_GET_OPTION(RSK_SHUFFLE_ADULT_TRADE) && INV_CONTENT(ITEM_CLAIM_CHECK) == ITEM_CLAIM_CHECK));
}

u8 EnJs_RandoCanGetCarpetMerchantItem() {
    return (RAND_GET_OPTION(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_ALL) ||
            RAND_GET_OPTION(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_ALL_BUT_BEANS)) &&
           // If the rando check has already been awarded, use vanilla behavior.
           !Flags_GetRandomizerInf(RAND_INF_MERCHANTS_CARPET_SALESMAN);
}

u8 EnGm_RandoCanGetMedigoronItem() {
    return (RAND_GET_OPTION(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_ALL) ||
            RAND_GET_OPTION(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_ALL_BUT_BEANS)) &&
           // If the rando check has already been awarded, use vanilla behavior.
           !Flags_GetRandomizerInf(RAND_INF_MERCHANTS_MEDIGORON);
}

void RandomizerSetChestGameRandomizerInf(RandomizerCheck rc) {
    switch (rc) {
        case RC_MARKET_TREASURE_CHEST_GAME_ITEM_1:
            Flags_SetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_ITEM_1);
            break;
        case RC_MARKET_TREASURE_CHEST_GAME_ITEM_2:
            Flags_SetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_ITEM_2);
            break;
        case RC_MARKET_TREASURE_CHEST_GAME_ITEM_3:
            Flags_SetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_ITEM_3);
            break;
        case RC_MARKET_TREASURE_CHEST_GAME_ITEM_4:
            Flags_SetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_ITEM_4);
            break;
        case RC_MARKET_TREASURE_CHEST_GAME_ITEM_5:
            Flags_SetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_ITEM_5);
            break;
        case RC_MARKET_TREASURE_CHEST_GAME_KEY_1:
            Flags_SetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_KEY_1);
            break;
        case RC_MARKET_TREASURE_CHEST_GAME_KEY_2:
            Flags_SetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_KEY_2);
            break;
        case RC_MARKET_TREASURE_CHEST_GAME_KEY_3:
            Flags_SetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_KEY_3);
            break;
        case RC_MARKET_TREASURE_CHEST_GAME_KEY_4:
            Flags_SetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_KEY_4);
            break;
        case RC_MARKET_TREASURE_CHEST_GAME_KEY_5:
            Flags_SetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_KEY_5);
            break;
        default:
            break;
    }
}

void Player_Action_8084E6D4_override(Player* player, PlayState* play) {
    if (LinkAnimation_Update(play, &player->skelAnime)) {
        func_8084DFAC(play, player);
    }
}

void func_8083A434_override(PlayState* play, Player* player) {
    Player_SetupActionPreserveAnimMovement(play, player, Player_Action_8084E6D4_override, 0);
    player->stateFlags1 |= PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_IN_CUTSCENE;
}

bool ShouldGiveFishingPrize(f32 sFishOnHandLength) {
    // RANDOTODO: update the enhancement sliders to not allow
    // values above rando fish weight values when rando'd
    if (LINK_IS_CHILD) {
        int32_t weight = CVarGetInteger(CVAR_ENHANCEMENT("CustomizeFishing"), 0)
                             ? CVarGetInteger(CVAR_ENHANCEMENT("MinimumFishWeightChild"), 10)
                             : 10;
        f32 score = sqrt(((f32)weight - 0.5f) / 0.0036f);
        return sFishOnHandLength >= score && (IS_RANDO ? !Flags_GetRandomizerInf(RAND_INF_CHILD_FISHING)
                                                       : !(HIGH_SCORE(HS_FISHING) & HS_FISH_PRIZE_CHILD));
    } else {
        int32_t weight = CVarGetInteger(CVAR_ENHANCEMENT("CustomizeFishing"), 0)
                             ? CVarGetInteger(CVAR_ENHANCEMENT("MinimumFishWeightAdult"), 13)
                             : 13;
        f32 score = sqrt(((f32)weight - 0.5f) / 0.0036f);
        return sFishOnHandLength >= score && (IS_RANDO ? !Flags_GetRandomizerInf(RAND_INF_ADULT_FISHING)
                                                       : !(HIGH_SCORE(HS_FISHING) & HS_FISH_PRIZE_ADULT));
    }
}

void RandomizerOnDialogMessageHandler() {
    MessageContext* msgCtx = &gPlayState->msgCtx;
    Actor* actor = msgCtx->talkActor;
    auto ctx = Rando::Context::GetInstance();
    bool revealMerchant = ctx->GetOption(RSK_MERCHANT_TEXT_HINT).Get() != RO_GENERIC_OFF;
    bool nonBeanMerchants = ctx->GetOption(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_ALL_BUT_BEANS) ||
                            ctx->GetOption(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_ALL);

    RandomizerCheck reveal = RC_UNKNOWN_CHECK;
    if (ctx->GetOption(RSK_CHICKENS_HINT) &&
        (msgCtx->textId >= TEXT_ANJU_PLEASE_BRING_MY_CUCCOS_BACK && msgCtx->textId <= TEXT_ANJU_PLEASE_BRING_1_CUCCO)) {
        reveal = RC_KAK_ANJU_AS_CHILD;
    } else {
        switch (msgCtx->textId) {
            case TEXT_SKULLTULA_PEOPLE_IM_CURSED:
                if (actor->params == 1 && ctx->GetOption(RSK_KAK_10_SKULLS_HINT)) {
                    reveal = RC_KAK_10_GOLD_SKULLTULA_REWARD;
                } else if (actor->params == 2 && ctx->GetOption(RSK_KAK_20_SKULLS_HINT)) {
                    reveal = RC_KAK_20_GOLD_SKULLTULA_REWARD;
                } else if (actor->params == 3 && ctx->GetOption(RSK_KAK_30_SKULLS_HINT)) {
                    reveal = RC_KAK_30_GOLD_SKULLTULA_REWARD;
                } else if (actor->params == 4 && ctx->GetOption(RSK_KAK_40_SKULLS_HINT)) {
                    reveal = RC_KAK_40_GOLD_SKULLTULA_REWARD;
                } else if (ctx->GetOption(RSK_KAK_50_SKULLS_HINT)) {
                    reveal = RC_KAK_50_GOLD_SKULLTULA_REWARD;
                }
                break;
            case TEXT_SKULLTULA_PEOPLE_MAKE_YOU_VERY_RICH:
                if (ctx->GetOption(RSK_KAK_100_SKULLS_HINT)) {
                    reveal = RC_KAK_100_GOLD_SKULLTULA_REWARD;
                }
                break;
            case TEXT_MASK_SHOP_SIGN:
                if (ctx->GetOption(RSK_MASK_SHOP_HINT)) {
                    auto itemSkull_loc = ctx->GetItemLocation(RC_DEKU_THEATER_SKULL_MASK);
                    if (itemSkull_loc->GetCheckStatus() == RCSHOW_UNCHECKED) {
                        itemSkull_loc->SetCheckStatus(RCSHOW_IDENTIFIED);
                    }
                    reveal = RC_DEKU_THEATER_MASK_OF_TRUTH;
                }
                break;
            case TEXT_GHOST_SHOP_EXPLAINATION:
            case TEXT_GHOST_SHOP_CARD_HAS_POINTS:
                if (ctx->GetOption(RSK_BIG_POES_HINT)) {
                    reveal = RC_MARKET_10_BIG_POES;
                }
                break;
            case TEXT_MALON_EVERYONE_TURNING_EVIL:
            case TEXT_MALON_I_SING_THIS_SONG:
            case TEXT_MALON_HOW_IS_EPONA_DOING:
            case TEXT_MALON_OBSTICLE_COURSE:
            case TEXT_MALON_INGO_MUST_HAVE_BEEN_TEMPTED:
                if (ctx->GetOption(RSK_MALON_HINT)) {
                    reveal = RC_KF_LINKS_HOUSE_COW;
                }
                break;
            case TEXT_FROGS_UNDERWATER:
                if (ctx->GetOption(RSK_FROGS_HINT)) {
                    reveal = RC_ZR_FROGS_OCARINA_GAME;
                }
                break;
            case TEXT_GF_HBA_SIGN:
            case TEXT_HBA_NOT_ON_HORSE:
            case TEXT_HBA_INITIAL_EXPLAINATION:
            case TEXT_HBA_ALREADY_HAVE_1000:
                if (ctx->GetOption(RSK_HBA_HINT)) {
                    auto item1000_loc = ctx->GetItemLocation(RC_GF_HBA_1000_POINTS);
                    if (item1000_loc->GetCheckStatus() == RCSHOW_UNCHECKED) {
                        item1000_loc->SetCheckStatus(RCSHOW_IDENTIFIED);
                    }
                    reveal = RC_GF_HBA_1500_POINTS;
                }
                break;
            case TEXT_SCRUB_RANDOM:
                if (ctx->GetOption(RSK_SCRUB_TEXT_HINT).Get() != RO_GENERIC_OFF) {
                    EnDns* enDns = (EnDns*)actor;
                    auto checkIdentity = ObjectExtension::GetInstance().Get<ScrubIdentity>(actor);
                    if (checkIdentity != nullptr) {
                        reveal = OTRGlobals::Instance->gRandomizer->GetCheckFromRandomizerInf(
                            checkIdentity->identity.randomizerInf);
                    }
                }
                break;
            case TEXT_BEAN_SALESMAN_BUY_FOR_10:
                if (revealMerchant && (ctx->GetOption(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_BEANS_ONLY) ||
                                       ctx->GetOption(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_ALL))) {
                    reveal = RC_ZR_MAGIC_BEAN_SALESMAN;
                }
                break;
            case TEXT_GRANNYS_SHOP:
                if (revealMerchant && nonBeanMerchants &&
                    (ctx->GetOption(RSK_SHUFFLE_ADULT_TRADE) || INV_CONTENT(ITEM_CLAIM_CHECK) == ITEM_CLAIM_CHECK)) {
                    reveal = RC_KAK_GRANNYS_SHOP;
                }
                break;
            case TEXT_MEDIGORON:
                if (revealMerchant && nonBeanMerchants) {
                    reveal = RC_GC_MEDIGORON;
                }
                break;
            case TEXT_CARPET_SALESMAN_1:
                if (revealMerchant && nonBeanMerchants) {
                    reveal = RC_WASTELAND_BOMBCHU_SALESMAN;
                }
                break;
            case TEXT_BIGGORON_BETTER_AT_SMITHING:
            case TEXT_BIGGORON_WAITING_FOR_YOU:
            case TEXT_BIGGORON_RETURN_AFTER_A_FEW_DAYS:
            case TEXT_BIGGORON_I_MAAAADE_THISSSS:
                if (ctx->GetOption(RSK_BIGGORON_HINT)) {
                    reveal = RC_DMT_TRADE_CLAIM_CHECK;
                }
                break;
            case TEXT_SHEIK_NEED_HOOK:
            case TEXT_SHEIK_HAVE_HOOK:
                if (ctx->GetOption(RSK_OOT_HINT) && gPlayState->sceneNum == SCENE_TEMPLE_OF_TIME &&
                    !ctx->GetItemLocation(RC_SONG_FROM_OCARINA_OF_TIME)->HasObtained()) {
                    auto itemoot_loc = ctx->GetItemLocation(RC_HF_OCARINA_OF_TIME_ITEM);
                    if (itemoot_loc->GetCheckStatus() == RCSHOW_UNCHECKED) {
                        itemoot_loc->SetCheckStatus(RCSHOW_IDENTIFIED);
                    }
                    reveal = RC_SONG_FROM_OCARINA_OF_TIME;
                }
                break;
            case TEXT_FISHING_CLOUDY:
            case TEXT_FISHING_TRY_ANOTHER_LURE:
            case TEXT_FISHING_SECRETS:
            case TEXT_FISHING_GOOD_FISHERMAN:
            case TEXT_FISHING_DIFFERENT_POND:
            case TEXT_FISHING_SCRATCHING:
            case TEXT_FISHING_TRY_ANOTHER_LURE_WITH_SINKING_LURE:
                if (ctx->GetOption(RSK_LOACH_HINT)) {
                    reveal = RC_LH_HYRULE_LOACH;
                }
                break;
        }
    }

    if (reveal != RC_UNKNOWN_CHECK) {
        auto item_loc = ctx->GetItemLocation(reveal);
        if (item_loc->GetCheckStatus() == RCSHOW_UNCHECKED) {
            item_loc->SetCheckStatus(RCSHOW_IDENTIFIED);
        }
    }
}

void RandomizerCompleteQueuedCheckWithoutNativeItem() {
    if (randomizerQueuedCheck == RC_UNKNOWN_CHECK) {
        return;
    }

    auto loc = Rando::Context::GetInstance()->GetItemLocation(randomizerQueuedCheck);
    SPDLOG_INFO("Completing visual-only Item mod {} item {} from RC {}", randomizerQueuedItemEntry.modIndex,
                randomizerQueuedItemEntry.itemId, static_cast<uint32_t>(randomizerQueuedCheck));
    loc->SetCheckStatus(RCSHOW_COLLECTED);
    CheckTracker::SpoilAreaFromCheck(randomizerQueuedCheck);
    CheckTracker::RecalculateAllAreaTotals();
    CheckTracker::RecalculateAvailableChecks();
    SaveManager::Instance->SaveSection(gSaveContext.fileNum, SECTION_ID_TRACKER_DATA, true);
    randomizerQueuedCheck = RC_UNKNOWN_CHECK;
    randomizerQueuedItemEntry = GET_ITEM_NONE;
    randomizerQueuedItemGiveStarted = false;
    randomizerQueuedItemGiveSettledFrames = 0;
    randomizerQueuedEmitWindowMessage = false;
}

static void RandomizerCompleteQueuedCheckAfterNativeGive() {
    if (randomizerQueuedCheck == RC_UNKNOWN_CHECK) {
        return;
    }

    auto loc = Rando::Context::GetInstance()->GetItemLocation(randomizerQueuedCheck);
    SPDLOG_INFO("Completing native queued Item mod {} item {} from RC {}", randomizerQueuedItemEntry.modIndex,
                randomizerQueuedItemEntry.itemId, static_cast<uint32_t>(randomizerQueuedCheck));
    loc->SetCheckStatus(RCSHOW_COLLECTED);
    CheckTracker::SpoilAreaFromCheck(randomizerQueuedCheck);
    CheckTracker::RecalculateAllAreaTotals();
    CheckTracker::RecalculateAvailableChecks();
    SaveManager::Instance->SaveSection(gSaveContext.fileNum, SECTION_ID_TRACKER_DATA, true);
    randomizerQueuedCheck = RC_UNKNOWN_CHECK;
    randomizerQueuedItemEntry = GET_ITEM_NONE;
    randomizerQueuedItemGiveStarted = false;
    randomizerQueuedItemGiveSettledFrames = 0;
    randomizerQueuedEmitWindowMessage = false;
}

extern "C" int Randomizer_HandleSkullTokenPickup(EnSi* enSi, PlayState* play) {
    RandomizerCheck rc =
        OTRGlobals::Instance->gRandomizer->GetCheckFromActor(enSi->actor.id, play->sceneNum, enSi->actor.params);
    if (rc == RC_UNKNOWN_CHECK || !OTRGlobals::Instance->gRandoContext->IsLocationShuffled(rc)) {
        return false;
    }

    auto* loc = Rando::Context::GetInstance()->GetItemLocation(rc);
    if (loc == nullptr || loc->HasObtained() || loc->GetPlacedRandomizerGet() == RG_NONE) {
        return true;
    }

    GetItemEntry getItemEntry = Rando::Context::GetInstance()->GetFinalGIEntry(
        rc, true, (GetItemID)Rando::StaticData::GetLocation(rc)->GetVanillaItem());
    const bool isVisualOnlyCrossoverMmItem =
        SohCrossover::IsCrossGameMmCheck(rc) && !SohCrossover::ShouldGiveNativeItemForCrossGameMmCheck(rc);
    const bool giveNativeItem =
        !isVisualOnlyCrossoverMmItem || SohCrossover::ShouldGiveNativeItemForCrossGameMmCheck(rc);
    GetItemCategory getItemCategory = Randomizer_AdjustItemCategoryForCheck(getItemEntry, rc);
    const bool shouldSkipAnimation = Randomizer_ShouldSkipGetItemAnimation(rc, getItemEntry, getItemCategory);

    SPDLOG_INFO("Handling shuffled skull token pickup RC {} mod {} item {} skip {}", static_cast<uint32_t>(rc),
                getItemEntry.modIndex, getItemEntry.itemId, shouldSkipAnimation);

    if (!shouldSkipAnimation) {
        randomizerQueuedCheck = rc;
        randomizerQueuedItemEntry = ApplyCrossoverVisualOnlyItemEntry(rc, getItemEntry);
        randomizerQueuedItemGiveStarted = false;
        randomizerQueuedItemGiveSettledFrames = 0;
        if (isVisualOnlyCrossoverMmItem) {
            SohCrossover::QueueCrossGameMmPickupTextbox(rc);
        }
        return true;
    }

    if (giveNativeItem && !SohCrossover::TryGiveNativeItemForCrossGameMmCheck(rc, play)) {
        if (getItemEntry.modIndex == MOD_NONE) {
            if (getItemEntry.getItemId == GI_SWORD_BGS) {
                gSaveContext.bgsFlag = true;
            }
            Item_Give(play, static_cast<uint8_t>(getItemEntry.itemId));
        } else if (getItemEntry.modIndex == MOD_RANDOMIZER) {
            if (getItemEntry.getItemId == RG_ICE_TRAP) {
                gSaveContext.ship.pendingIceTrapCount++;
            } else {
                Randomizer_Item_Give(play, getItemEntry);
            }
        }
    }

    loc->SetCheckStatus(RCSHOW_COLLECTED);
    CheckTracker::SpoilAreaFromCheck(rc);
    CheckTracker::RecalculateAllAreaTotals();
    CheckTracker::RecalculateAvailableChecks();
    SaveManager::Instance->SaveSection(gSaveContext.fileNum, SECTION_ID_TRACKER_DATA, true);
    EmitWindowMessageForRandomizerCheck(rc, getItemEntry);
    return true;
}

static void RandomizerCompleteImmediateItem00Check(EnItem00* item00) {
    if (item00->randoCheck != RC_UNKNOWN_CHECK) {
        auto loc = Rando::Context::GetInstance()->GetItemLocation(item00->randoCheck);
        if (loc != nullptr) {
            loc->SetCheckStatus(RCSHOW_COLLECTED);
        }
    }

    if (item00->randoInf != RAND_INF_MAX) {
        Flags_SetRandomizerInf(item00->randoInf);
    } else {
        Flags_SetCollectible(gPlayState, item00->collectibleFlag);
    }
}

static void RandomizerMarkItem00SourceCollected(EnItem00* item00) {
    if (item00->randoInf != RAND_INF_MAX) {
        Flags_SetRandomizerInf(item00->randoInf);
    } else {
        Flags_SetCollectible(gPlayState, item00->collectibleFlag);
    }
}

extern "C" void func_80A5475C(EnHeishi2* CastleGuard, PlayState* play);

void RandomizerOnVanillaBehaviorHandler(GIVanillaBehavior id, bool* should, va_list originalArgs) {
    va_list args;
    va_copy(args, originalArgs);

    switch (id) {
        case VB_CLIMB:
            if (RAND_GET_OPTION(RSK_SHUFFLE_CLIMB) && !Flags_GetRandomizerInf(RAND_INF_CAN_CLIMB)) {
                s32* x = va_arg(args, s32*);
                s32* y = va_arg(args, s32*);

                *x = 0;
                if (*y > 0) {
                    *y = 0;
                }
            }
            break;
        case VB_CRAWL:
            *should = *should && Flags_GetRandomizerInf(RAND_INF_CAN_CRAWL);
            break;
        case VB_ALLOW_ENTRANCE_CS_FOR_EITHER_AGE: {
            s32 entranceIndex = va_arg(args, s32);

            // Allow Nabooru fight cutscene to play for child in rando
            if (entranceIndex == ENTR_SPIRIT_TEMPLE_BOSS_ENTRANCE) {
                *should = true;
            }
            break;
        }
        case VB_PLAY_SLOW_CHEST_CS: {
            // We force fast chests if SkipGetItemAnimation is enabled because the camera in the CS looks pretty wonky
            // otherwise
            if (CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("TimeSavers.SkipGetItemAnimation"), SGIA_JUNK)) {
                *should = false;
            }
            break;
        }
        case VB_GIVE_ITEM_FROM_CHEST: {
            EnBox* chest = va_arg(args, EnBox*);
            RandomizerCheck rc = OTRGlobals::Instance->gRandomizer->GetCheckFromActor(
                chest->dyna.actor.id, gPlayState->sceneNum, chest->dyna.actor.params);
            if (!OTRGlobals::Instance->gRandoContext->IsLocationShuffled(rc)) {
                break;
            }

            // if this is a treasure chest game chest then set the appropriate rando inf
            RandomizerSetChestGameRandomizerInf(rc);

            Player* player = GET_PLAYER(gPlayState);
            Player_SetupWaitForPutAway(gPlayState, player, func_8083A434_override);

            *should = false;
            break;
        }
        case VB_SPAWN_FIRE_ARROW:
            *should = !Flags_GetTreasure(gPlayState, 0x1F);
            break;
        case VB_PLAY_NABOORU_CAPTURED_CS:
            // This behavior is replicated for randomizer in RandomizerOnItemReceiveHandler
            *should = false;
            break;
        case VB_SHIEK_PREPARE_TO_GIVE_SERENADE_OF_WATER: {
            *should =
                !Flags_GetEventChkInf(EVENTCHKINF_LEARNED_SERENADE_OF_WATER) && !Flags_GetTreasure(gPlayState, 0x2);
            break;
        }
        case VB_BE_ELIGIBLE_FOR_SERENADE_OF_WATER:
            *should =
                !Flags_GetEventChkInf(EVENTCHKINF_LEARNED_SERENADE_OF_WATER) && Flags_GetTreasure(gPlayState, 0x2);
            break;
        case VB_BE_ELIGIBLE_FOR_PRELUDE_OF_LIGHT:
            *should =
                !Flags_GetEventChkInf(EVENTCHKINF_LEARNED_PRELUDE_OF_LIGHT) && CHECK_QUEST_ITEM(QUEST_MEDALLION_FOREST);
            break;
        case VB_MIDO_SPAWN:
            if (RAND_GET_OPTION(RSK_FOREST).IsNot(RO_CLOSED_FOREST_OFF) &&
                !Flags_GetEventChkInf(EVENTCHKINF_SHOWED_MIDO_SWORD_SHIELD)) {
                *should = true;
            }
            break;
        case VB_MOVE_MIDO_IN_KOKIRI_FOREST:
            if (RAND_GET_OPTION(RSK_FOREST).Is(RO_CLOSED_FOREST_OFF) && gSaveContext.cutsceneIndex == 0) {
                *should = true;
            }
            break;
        case VB_MALON_RETURN_FROM_CASTLE:
            *should = Flags_GetEventChkInf(EVENTCHKINF_TALON_RETURNED_FROM_CASTLE) &&
                      Flags_GetEventChkInf(EVENTCHKINF_OBTAINED_POCKET_EGG);
            break;
        case VB_SEND_MALON_HOME:
            *should = Flags_GetRandomizerInf(RAND_INF_TALON_SENT_MALON_HOME);
            break;
        case VB_MIDO_CONSIDER_DEKU_TREE_DEAD:
            *should = Flags_GetEventChkInf(EVENTCHKINF_OBTAINED_KOKIRI_EMERALD_DEKU_TREE_DEAD);
            break;
        case VB_OPEN_CHEST:
            *should = *should && Flags_GetRandomizerInf(RAND_INF_CAN_OPEN_CHEST);
            break;
        case VB_OPEN_KOKIRI_FOREST:
            *should = Flags_GetEventChkInf(EVENTCHKINF_OBTAINED_KOKIRI_EMERALD_DEKU_TREE_DEAD) ||
                      RAND_GET_OPTION(RSK_FOREST).IsNot(RO_CLOSED_FOREST_ON);
            break;
        case VB_BE_ELIGIBLE_FOR_DARUNIAS_JOY_REWARD:
            *should = !Flags_GetRandomizerInf(RAND_INF_DARUNIAS_JOY);
            break;
        case VB_BE_ELIGIBLE_FOR_LIGHT_ARROWS:
            *should = LINK_IS_ADULT && (gEntranceTable[gSaveContext.entranceIndex].scene == SCENE_TEMPLE_OF_TIME) &&
                      !Flags_GetEventChkInf(EVENTCHKINF_RETURNED_TO_TEMPLE_OF_TIME_WITH_ALL_MEDALLIONS) &&
                      MeetsLACSRequirements();
            break;
        case VB_BE_ELIGIBLE_FOR_NOCTURNE_OF_SHADOW:
            *should = !Flags_GetEventChkInf(EVENTCHKINF_BONGO_BONGO_ESCAPED_FROM_WELL) && LINK_IS_ADULT &&
                      gEntranceTable[((void)0, gSaveContext.entranceIndex)].scene == SCENE_KAKARIKO_VILLAGE &&
                      CHECK_QUEST_ITEM(QUEST_MEDALLION_FOREST) && CHECK_QUEST_ITEM(QUEST_MEDALLION_FIRE) &&
                      CHECK_QUEST_ITEM(QUEST_MEDALLION_WATER) && gSaveContext.cutsceneIndex < 0xFFF0;
            break;
        case VB_BE_ELIGIBLE_FOR_CHILD_ROLLING_GORON_REWARD: {
            // Don't require a bomb bag to get prize in rando
            *should = true;
            break;
        }
        case VB_BE_ELIGIBLE_FOR_MAGIC_BEANS_PURCHASE: {
            if (RAND_GET_OPTION(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_BEANS_ONLY) ||
                RAND_GET_OPTION(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_ALL)) {
                *should = gSaveContext.rupees >=
                          OTRGlobals::Instance->gRandoContext->GetItemLocation(RC_ZR_MAGIC_BEAN_SALESMAN)->GetPrice();
            } else if (RAND_GET_OPTION(RSK_SKIP_PLANTING_BEANS)) {
                *should = gSaveContext.rupees >= 60;
            } else if (BEANS_BOUGHT == 9) {
                *should = gSaveContext.rupees >= 99;
            }
            break;
        }
        case VB_MAGIC_BEAN_SALESMAN_TAKE_MONEY: {
            if (BEANS_BOUGHT == 9) {
                Rupees_ChangeBy(-99);
                *should = false;
            }
            break;
        }
        case VB_CAN_BRIBE_HEISHI2: {
            EnHeishi2* guard = va_arg(args, EnHeishi2*);
            guard->actor.textId = 0x7072;
            guard->unk_300 = TEXT_STATE_CHOICE;
            guard->unk_30E = 1;
            guard->actionFunc = func_80A5475C;
            *should = false;
            break;
        }
        case VB_GIVE_ITEM_MASTER_SWORD:
            if (RAND_GET_OPTION(RSK_SHUFFLE_MASTER_SWORD) || RAND_GET_OPTION(RSK_STARTING_MASTER_SWORD)) {
                *should = false;
            } else {
                *should = true;
                Rando::Context::GetInstance()->GetItemLocation(RC_TOT_MASTER_SWORD)->SetCheckStatus(RCSHOW_COLLECTED);
                CheckTracker::RecalculateAllAreaTotals();
            }
            break;
        case VB_ITEM00_DESPAWN: {
            EnItem00* item00 = va_arg(args, EnItem00*);
            if (item00->actor.params == ITEM00_SOH_GIVE_ITEM_ENTRY ||
                item00->actor.params == ITEM00_SOH_GIVE_ITEM_ENTRY_GI) {
                GetItemEntry itemEntry = randomizerQueuedItemEntry;
                item00->itemEntry = itemEntry;
                item00->actor.draw = (ActorFunc)EnItem00_DrawRandomizedItem;
            } else {
                RandomizerCheck rc = OTRGlobals::Instance->gRandomizer->GetCheckFromActor(
                    item00->actor.id, gPlayState->sceneNum, item00->ogParams);
                if (rc != RC_UNKNOWN_CHECK) {
                    item00->randoInf = RAND_INF_MAX;
                    item00->actor.params = ITEM00_SOH_DUMMY;
                    item00->itemEntry = ApplyCrossoverVisualOnlyItemEntry(
                        rc, Rando::Context::GetInstance()->GetFinalGIEntry(
                                rc, true, (GetItemID)Rando::StaticData::GetLocation(rc)->GetVanillaItem()));
                    item00->actor.draw = (ActorFunc)EnItem00_DrawRandomizedItem;
                    *should = Rando::Context::GetInstance()->GetItemLocation(rc)->HasObtained();
                }
            }
            break;
        }
        case VB_ITEM_B_HEART_DESPAWN: {
            ItemBHeart* itemBHeart = va_arg(args, ItemBHeart*);
            RandomizerCheck rc = OTRGlobals::Instance->gRandomizer->GetCheckFromActor(
                itemBHeart->actor.id, gPlayState->sceneNum, itemBHeart->actor.params);
            if (rc != RC_UNKNOWN_CHECK) {
                itemBHeart->sohItemEntry = ApplyCrossoverVisualOnlyItemEntry(
                    rc, Rando::Context::GetInstance()->GetFinalGIEntry(
                            rc, true, (GetItemID)Rando::StaticData::GetLocation(rc)->GetVanillaItem()));
                itemBHeart->actor.draw = (ActorFunc)ItemBHeart_DrawRandomizedItem;
                itemBHeart->actor.update = (ActorFunc)ItemBHeart_UpdateRandomizedItem;
                *should = Rando::Context::GetInstance()->GetItemLocation(rc)->HasObtained();
            }
            break;
        }
        case VB_MALON_ALREADY_TAUGHT_EPONAS_SONG: {
            *should = Flags_GetRandomizerInf(RAND_INF_LEARNED_EPONA_SONG);
            break;
        }
        case VB_KING_ZORA_THANK_CHILD: {
            // Allow turning in Ruto's letter even if you have already rescued her
            if (!Flags_GetEventChkInf(EVENTCHKINF_KING_ZORA_MOVED)) {
                GET_PLAYER(gPlayState)->exchangeItemId = EXCH_ITEM_LETTER_RUTO;
            }
            *should = Flags_GetEventChkInf(EVENTCHKINF_USED_JABU_JABUS_BELLY_BLUE_WARP);
            break;
        }
        case VB_BE_ABLE_TO_EXCHANGE_RUTOS_LETTER: {
            *should = LINK_IS_CHILD;
            break;
        }
        case VB_KING_ZORA_BE_MOVED: {
            *should = false;
            switch (RAND_GET_OPTION(RSK_ZORAS_FOUNTAIN).Get()) {
                case RO_ZF_CLOSED:
                    if (Flags_GetEventChkInf(EVENTCHKINF_KING_ZORA_MOVED)) {
                        *should = true;
                    }
                    break;
                case RO_ZF_CLOSED_CHILD:
                    if (LINK_IS_ADULT) {
                        *should = true;
                    } else if (Flags_GetEventChkInf(EVENTCHKINF_KING_ZORA_MOVED)) {
                        *should = true;
                    }
                    break;
                case RO_ZF_OPEN:
                    *should = true;
                    break;
            }
            break;
        }
        case VB_KING_ZORA_TUNIC_CHECK: {
            if (!Flags_GetRandomizerInf(RAND_INF_KING_ZORA_THAWED)) {
                *should = false;
            }
            break;
        }
        case VB_BIGGORON_CONSIDER_SWORD_COLLECTED: {
            *should = Flags_GetRandomizerInf(RAND_INF_ADULT_TRADES_DMT_TRADE_CLAIM_CHECK);
            break;
        }
        case VB_BIGGORON_CONSIDER_TRADE_COMPLETE: {
            // This being true will prevent other biggoron trades, there are already safeguards in place to prevent
            // claim check from being traded multiple times, so we don't really need the quest to ever be considered
            // "complete"
            *should = false;
            break;
        }
        case VB_PREVENT_STRENGTH: {
            if (!Flags_GetRandomizerInf(RAND_INF_CAN_GRAB)) {
                GET_PLAYER(gPlayState)->stateFlags2 &= ~PLAYER_STATE2_MOVING_DYNAPOLY;
                *should = true;
            }
            break;
        }
        case VB_GORONS_CONSIDER_FIRE_TEMPLE_FINISHED: {
            *should = Flags_GetEventChkInf(EVENTCHKINF_USED_FIRE_TEMPLE_BLUE_WARP);
            break;
        }
        case VB_GORONS_CONSIDER_DODONGOS_CAVERN_FINISHED: {
            *should = Flags_GetEventChkInf(EVENTCHKINF_USED_DODONGOS_CAVERN_BLUE_WARP);
            break;
        }
        case VB_GORONS_CONSIDER_TUNIC_COLLECTED: {
            *should = Flags_GetInfTable(INFTABLE_GORON_CITY_DOORS_UNLOCKED);
            break;
        }
        case VB_GIVE_ITEM_FROM_ITEM_00: {
            EnItem00* item00 = va_arg(args, EnItem00*);
            if (item00->actor.params == ITEM00_SOH_DUMMY) {
                const bool isCrossoverMmItem =
                    item00->randoCheck != RC_UNKNOWN_CHECK && SohCrossover::IsCrossGameMmCheck(item00->randoCheck);
                const bool giveNativeItem =
                    !isCrossoverMmItem || SohCrossover::ShouldGiveNativeItemForCrossGameMmCheck(item00->randoCheck);
                const auto itemCategory = Randomizer_AdjustItemCategoryForCheck(item00->itemEntry, item00->randoCheck);
                Player* player = GET_PLAYER(gPlayState);
                const bool isUnderwaterPickup = player != nullptr && (player->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0;
                const bool shouldSkipAnimation =
                    isUnderwaterPickup || item00->randoCheck == RC_UNKNOWN_CHECK
                        ? true
                        : Randomizer_ShouldSkipGetItemAnimation(item00->randoCheck, item00->itemEntry, itemCategory);

                if (!shouldSkipAnimation) {
                    randomizerQueuedCheck = item00->randoCheck;
                    randomizerQueuedItemEntry = item00->itemEntry;
                    randomizerQueuedItemGiveStarted = false;
                    randomizerQueuedItemGiveSettledFrames = 0;
                    randomizerQueuedEmitWindowMessage = true;
                    if (isCrossoverMmItem && !giveNativeItem) {
                        SohCrossover::QueueCrossGameMmPickupTextbox(item00->randoCheck);
                    }
                    RandomizerMarkItem00SourceCollected(item00);
                    Actor_Kill(&item00->actor);
                    *should = false;
                    break;
                }

                if (giveNativeItem && !SohCrossover::TryGiveNativeItemForCrossGameMmCheck(item00->randoCheck, gPlayState)) {
                    if (item00->itemEntry.modIndex == MOD_NONE) {
                        if (item00->itemEntry.getItemId == GI_SWORD_BGS) {
                            gSaveContext.bgsFlag = true;
                        }
                        Item_Give(gPlayState, static_cast<uint8_t>(item00->itemEntry.itemId));
                    } else if (item00->itemEntry.modIndex == MOD_RANDOMIZER) {
                        if (item00->itemEntry.getItemId == RG_ICE_TRAP) {
                            gSaveContext.ship.pendingIceTrapCount++;
                        } else {
                            Randomizer_Item_Give(gPlayState, item00->itemEntry);
                        }
                    }
                }

                RandomizerCompleteImmediateItem00Check(item00);
                EmitWindowMessageForRandomizerCheck(item00->randoCheck, item00->itemEntry);
                Actor_Kill(&item00->actor);
                *should = false;
            } else if (item00->actor.params == ITEM00_SOH_GIVE_ITEM_ENTRY) {
                const bool isCrossoverMmItem = SohCrossover::IsCrossGameMmCheck(randomizerQueuedCheck);
                const bool giveNativeItem =
                    !isCrossoverMmItem || SohCrossover::ShouldGiveNativeItemForCrossGameMmCheck(randomizerQueuedCheck);
                Audio_PlaySoundGeneral(NA_SE_SY_GET_ITEM, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                if (giveNativeItem && !SohCrossover::TryGiveNativeItemForCrossGameMmCheck(randomizerQueuedCheck, gPlayState)) {
                    if (item00->itemEntry.modIndex == MOD_NONE) {
                        if (item00->itemEntry.getItemId == GI_SWORD_BGS) {
                            gSaveContext.bgsFlag = true;
                        }
                        Item_Give(gPlayState, static_cast<uint8_t>(item00->itemEntry.itemId));
                    } else if (item00->itemEntry.modIndex == MOD_RANDOMIZER) {
                        if (item00->itemEntry.getItemId == RG_ICE_TRAP) {
                            gSaveContext.ship.pendingIceTrapCount++;
                        } else {
                            Randomizer_Item_Give(gPlayState, item00->itemEntry);
                        }
                    }
                    RandomizerCompleteQueuedCheckAfterNativeGive();
                } else {
                    RandomizerCompleteQueuedCheckWithoutNativeItem();
                }

                if (!isCrossoverMmItem) {
                    EmitWindowMessageForGetItemEntry(item00->itemEntry);
                }

                // This is typically called when you close the text box after getting an item, in case a previous
                // function hid the interface.
                gSaveContext.unk_13EA = 0;
                Interface_ChangeAlpha(0x32);
                // EnItem00_SetupAction(item00, func_8001E5C8);
                // *should = false;
            } else if (item00->actor.params == ITEM00_SOH_GIVE_ITEM_ENTRY_GI) {
                if (!Actor_HasParent(&item00->actor, gPlayState)) {
                    GiveItemEntryFromActorWithFixedRange(&item00->actor, gPlayState, item00->itemEntry);
                }
                EnItem00_SetupAction(item00, func_8001E5C8);
                *should = false;
            }
            break;
        }
        case VB_BE_ELIGIBLE_FOR_SARIAS_SONG: {
            *should = !Flags_GetEventChkInf(EVENTCHKINF_LEARNED_SARIAS_SONG);
            break;
        }
        case VB_GIVE_ITEM_FROM_DEKU_THEATER: {
            EnDntJiji* enDntJiji = va_arg(args, EnDntJiji*);
            enDntJiji->actionFunc = EnDntJiji_GivePrize;
            *should = false;
            break;
        }
        case VB_GIVE_ITEM_FROM_GRANNYS_SHOP: {
            if (!EnDs_RandoCanGetGrannyItem()) {
                break;
            }
            EnDs* granny = va_arg(args, EnDs*);
            // Only setting the inf if we've actually gotten the rando item and not the vanilla blue potion
            Flags_SetRandomizerInf(RAND_INF_MERCHANTS_GRANNYS_SHOP);
            granny->actor.parent = NULL;
            granny->actionFunc = EnDs_Talk;
            *should = false;
            break;
        }
        case VB_GIVE_ITEM_FROM_ANJU_AS_CHILD: {
            Flags_SetItemGetInf(ITEMGETINF_0C);
            *should = false;
            break;
        }
        case VB_GIVE_ITEM_FROM_ANJU_AS_ADULT: {
            EnNiwLady* enNiwLady = va_arg(args, EnNiwLady*);
            Flags_SetItemGetInf(ITEMGETINF_2C);
            enNiwLady->actionFunc = func_80ABA778;
            *should = false;
            break;
        }
        case VB_CHECK_RANDO_PRICE_OF_CARPET_SALESMAN: {
            if (EnJs_RandoCanGetCarpetMerchantItem()) {
                *should =
                    gSaveContext.rupees <
                    OTRGlobals::Instance->gRandoContext->GetItemLocation(RC_WASTELAND_BOMBCHU_SALESMAN)->GetPrice();
            }
            break;
        }
        case VB_GIVE_ITEM_FROM_CARPET_SALESMAN: {
            EnJs* enJs = va_arg(args, EnJs*);
            if (EnJs_RandoCanGetCarpetMerchantItem()) {
                Rupees_ChangeBy(
                    OTRGlobals::Instance->gRandoContext->GetItemLocation(RC_WASTELAND_BOMBCHU_SALESMAN)->GetPrice() *
                    -1);
                enJs->actor.parent = NULL;
                enJs->actor.textId = TEXT_CARPET_SALESMAN_ARMS_DEALER;
                enJs->actionFunc = (EnJsActionFunc)func_80A890C0;
                enJs->actor.flags |= ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
                Flags_SetRandomizerInf(RAND_INF_MERCHANTS_CARPET_SALESMAN);
                *should = true;
            }
            break;
        }
        case VB_GIVE_BOMBCHUS_FROM_CARPET_SALESMAN: {
            *should =
                RAND_GET_OPTION(RSK_BOMBCHU_BAG).Is(RO_BOMBCHU_BAG_NONE) || INV_CONTENT(ITEM_BOMBCHU) == ITEM_BOMBCHU;
            break;
        }
        case VB_CHECK_RANDO_PRICE_OF_MEDIGORON: {
            if (EnGm_RandoCanGetMedigoronItem()) {
                *should = gSaveContext.rupees <
                          OTRGlobals::Instance->gRandoContext->GetItemLocation(RC_GC_MEDIGORON)->GetPrice();
            }
            break;
        }

        case VB_GIVE_ITEM_FROM_MEDIGORON:
        case VB_BE_ELIGIBLE_FOR_GIANTS_KNIFE_PURCHASE: {
            if (EnGm_RandoCanGetMedigoronItem()) {
                if (id == VB_GIVE_ITEM_FROM_MEDIGORON) {
                    EnGm* enGm = va_arg(args, EnGm*);
                    Flags_SetInfTable(INFTABLE_B1);
                    Flags_SetRandomizerInf(RAND_INF_MERCHANTS_MEDIGORON);
                    enGm->actor.parent = NULL;
                    enGm->actionFunc = (EnGmActionFunc)func_80A3DC44;
                    Rupees_ChangeBy(OTRGlobals::Instance->gRandoContext->GetItemLocation(RC_GC_MEDIGORON)->GetPrice() *
                                    -1);
                    *should = false;
                } else {
                    // Resets "Talked to Medigoron" flag in infTable to restore initial conversation state
                    Flags_UnsetInfTable(INFTABLE_B1);
                    *should = true;
                }
            }
            break;
        }
        case VB_GIVE_ITEM_FROM_MAGIC_BEAN_SALESMAN: {
            EnMs* enMs = va_arg(args, EnMs*);
            if (RAND_GET_OPTION(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_BEANS_ONLY) ||
                RAND_GET_OPTION(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_ALL)) {
                Rupees_ChangeBy(
                    OTRGlobals::Instance->gRandoContext->GetItemLocation(RC_ZR_MAGIC_BEAN_SALESMAN)->GetPrice() * -1);
                BEANS_BOUGHT = 10;
                // Only set inf for buying rando check
                Flags_SetRandomizerInf(RAND_INF_MERCHANTS_MAGIC_BEAN_SALESMAN);
                enMs->actionFunc = (EnMsActionFunc)EnMs_Wait;
                *should = false;
            } else if (RAND_GET_OPTION(RSK_SKIP_PLANTING_BEANS)) {
                Rupees_ChangeBy(-60);
                Item_Give(NULL, ITEM_BEAN);
                BEANS_BOUGHT = 10;
                AMMO(ITEM_BEAN) = 0;
                gSaveContext.sceneFlags[SCENE_DEATH_MOUNTAIN_CRATER].swch |= (1 << 3);
                gSaveContext.sceneFlags[SCENE_DEATH_MOUNTAIN_TRAIL].swch |= (1 << 6);
                gSaveContext.sceneFlags[SCENE_DESERT_COLOSSUS].swch |= (1 << 24);
                gSaveContext.sceneFlags[SCENE_GERUDO_VALLEY].swch |= (1 << 3);
                gSaveContext.sceneFlags[SCENE_GRAVEYARD].swch |= (1 << 3);
                gSaveContext.sceneFlags[SCENE_KOKIRI_FOREST].swch |= (1 << 9);
                gSaveContext.sceneFlags[SCENE_LAKE_HYLIA].swch |= (1 << 1);
                gSaveContext.sceneFlags[SCENE_LOST_WOODS].swch |= (1 << 4) | (1 << 18);
                gSaveContext.sceneFlags[SCENE_ZORAS_RIVER].swch |= (1 << 3);
                ObjBean* bean = (ObjBean*)Actor_Find(&gPlayState->actorCtx, ACTOR_OBJ_BEAN, ACTORCAT_BG);
                if (bean != nullptr) {
                    Flags_SetSwitch(gPlayState, bean->dyna.actor.params & 0x3F);
                    func_80B8FE00(bean);
                }
                enMs->actionFunc = (EnMsActionFunc)EnMs_Wait;
                *should = false;
            }
            break;
        }
        case VB_DEKU_THEATER_FINISH_GIVING_PRIZE:
            *should = true;
            break;
        case VB_FROGS_GO_TO_IDLE: {
            EnFr* enFr = va_arg(args, EnFr*);

            if ((enFr->songIndex >= FROG_STORMS && enFr->reward == GI_HEART_PIECE) ||
                (enFr->songIndex < FROG_STORMS && enFr->reward == GI_RUPEE_PURPLE)) {
                *should = true;
            }
            break;
        }
        case VB_TRADE_POCKET_CUCCO: {
            EnNiwLady* enNiwLady = va_arg(args, EnNiwLady*);
            Flags_UnsetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_POCKET_CUCCO);
            Inventory_ReplaceItem(gPlayState, ITEM_POCKET_CUCCO, Randomizer_GetNextAdultTradeItem());
            // Trigger the reward now
            Flags_SetItemGetInf(ITEMGETINF_2E);
            enNiwLady->actionFunc = func_80ABA778;

            *should = false;
            break;
        }
        case VB_TRADE_COJIRO: {
            Flags_UnsetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_COJIRO);
            Inventory_ReplaceItem(gPlayState, ITEM_COJIRO, Randomizer_GetNextAdultTradeItem());
            *should = false;
            break;
        }
        case VB_TRADE_ODD_MUSHROOM: {
            EnDs* granny = va_arg(args, EnDs*);
            Flags_UnsetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_ODD_MUSHROOM);
            Inventory_ReplaceItem(gPlayState, ITEM_ODD_MUSHROOM, Randomizer_GetNextAdultTradeItem());
            // Trigger the reward now
            Flags_SetItemGetInf(ITEMGETINF_30);
            granny->actor.textId = 0x504F;
            granny->actionFunc = (EnDsActionFunc)EnDs_TalkAfterGiveOddPotion;
            granny->actor.flags &= ~ACTOR_FLAG_TALK;
            *should = false;
            break;
        }
        case VB_TRADE_ODD_POTION: {
            EnKo* enKo = va_arg(args, EnKo*);
            Flags_UnsetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_ODD_POTION);
            Inventory_ReplaceItem(gPlayState, ITEM_ODD_POTION, Randomizer_GetNextAdultTradeItem());
            // Trigger the reward now
            Flags_SetItemGetInf(ITEMGETINF_31);
            *should = false;
            break;
        }
        case VB_TRADE_SAW: {
            Flags_UnsetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_SAW);
            Inventory_ReplaceItem(gPlayState, ITEM_SAW, Randomizer_GetNextAdultTradeItem());
            *should = false;
            break;
        }
        case VB_ADULT_KING_ZORA_ITEM_GIVE: {
            EnKz* enKz = va_arg(args, EnKz*);
            Input input = gPlayState->state.input[0];

            if (CVarGetInteger(CVAR_ENHANCEMENT("EarlyEyeballFrog"), 0)) {
                // For early eyeball frog hook override, simulate collection delay behavior by just checking for the R
                // button being held while wearing a shield, and a trade item lower than frog in inventory
                bool hasShieldHoldingR = (CHECK_BTN_ANY(input.cur.button, BTN_R) &&
                                          CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) > EQUIP_VALUE_SHIELD_NONE);

                if (func_8002F368(gPlayState) == EXCH_ITEM_PRESCRIPTION ||
                    (hasShieldHoldingR && INV_CONTENT(ITEM_TRADE_ADULT) < ITEM_FROG)) {
                    Flags_SetRandomizerInf(RAND_INF_ADULT_TRADES_ZD_TRADE_PRESCRIPTION);
                    Flags_UnsetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_PRESCRIPTION);
                    Inventory_ReplaceItem(gPlayState, ITEM_PRESCRIPTION, Randomizer_GetNextAdultTradeItem());
                } else {
                    Flags_SetRandomizerInf(RAND_INF_KING_ZORA_THAWED);
                }
            } else {
                if (enKz->isTrading) {
                    Flags_SetRandomizerInf(RAND_INF_ADULT_TRADES_ZD_TRADE_PRESCRIPTION);
                    Flags_UnsetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_PRESCRIPTION);
                    Inventory_ReplaceItem(gPlayState, ITEM_PRESCRIPTION, Randomizer_GetNextAdultTradeItem());
                } else {
                    Flags_SetRandomizerInf(RAND_INF_KING_ZORA_THAWED);
                }
            }
            *should = false;
            break;
        }
        case VB_TRADE_FROG: {
            Flags_UnsetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_FROG);
            Inventory_ReplaceItem(gPlayState, ITEM_FROG, Randomizer_GetNextAdultTradeItem());
            *should = false;
            break;
        }
        case VB_BUSINESS_SCRUB_DESPAWN: {
            EnShopnuts* enShopnuts = va_arg(args, EnShopnuts*);
            s16 respawnData = gSaveContext.respawn[RESPAWN_MODE_RETURN].data & ((1 << 8) - 1);
            ScrubIdentity scrubIdentity = OTRGlobals::Instance->gRandomizer->IdentifyScrub(
                gPlayState->sceneNum, enShopnuts->actor.params, respawnData);

            if (scrubIdentity.identity.randomizerCheck != RC_UNKNOWN_CHECK) {
                *should = Flags_GetRandomizerInf(scrubIdentity.identity.randomizerInf);
            }
            break;
        }
        case VB_GIVE_ITEM_FROM_BUSINESS_SCRUB: {
            EnDns* enDns = va_arg(args, EnDns*);
            *should = !ObjectExtension::GetInstance().Has<ScrubIdentity>(enDns);
            break;
        }
        // To explain the logic because Fado and Grog are linked:
        // - If you have Cojiro, then spawn Grog and not Fado.
        // - If you don't have Cojiro but do have Odd Potion, spawn Fado and not Grog.
        // - If you don't have either, spawn Grog if you haven't traded the Odd Mushroom.
        // - If you don't have either but have traded the mushroom, don't spawn either.
        case VB_DESPAWN_GROG: {
            if (!RAND_GET_OPTION(RSK_SHUFFLE_ADULT_TRADE)) {
                break;
            }
            if (Flags_GetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_COJIRO)) {
                *should = false;
            } else if (Flags_GetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_ODD_POTION)) {
                *should = true;
            } else {
                *should = Flags_GetItemGetInf(ITEMGETINF_30); // Traded odd mushroom
            }
            break;
        }
        case VB_SPAWN_LW_FADO: {
            if (!RAND_GET_OPTION(RSK_SHUFFLE_ADULT_TRADE)) {
                break;
            }

            if (Flags_GetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_COJIRO)) {
                *should = false;
            } else {
                *should = Flags_GetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_ODD_POTION);
            }

            break;
        }
        case VB_USE_EYEDROP_DIALOGUE: {
            // Skip eye drop text on rando if Link went in the water, so you can still receive the dive check
            EnMk* enMk = va_arg(args, EnMk*);
            *should &= enMk->swimFlag == 0;
            break;
        }
        case VB_OFFER_BLUE_POTION: {
            *should |= RAND_GET_OPTION(RSK_SHUFFLE_ADULT_TRADE).Is(RO_GENERIC_OFF) &&
                       INV_CONTENT(ITEM_CLAIM_CHECK) == ITEM_CLAIM_CHECK;
            break;
        }
        case VB_OKARINA_TAG_COMPLETE: {
            if (gPlayState->sceneNum == SCENE_BOTTOM_OF_THE_WELL) {
                auto dungeon =
                    OTRGlobals::Instance->gRandoContext->GetDungeons()->GetDungeonFromScene(SCENE_BOTTOM_OF_THE_WELL);
                if (dungeon->IsVanilla()) {
                    EnOkarinaTag* enOkarinaTag = va_arg(args, EnOkarinaTag*);
                    if (enOkarinaTag->switchFlag >= 0 && Flags_GetSwitch(gPlayState, enOkarinaTag->switchFlag)) {
                        Flags_UnsetSwitch(gPlayState, enOkarinaTag->switchFlag);
                        *should = false;
                    }
                }
            }
            break;
        }
        case VB_OKARINA_TAG_COMPLETED: {
            if (gPlayState->sceneNum == SCENE_BOTTOM_OF_THE_WELL) {
                auto dungeon =
                    OTRGlobals::Instance->gRandoContext->GetDungeons()->GetDungeonFromScene(SCENE_BOTTOM_OF_THE_WELL);
                if (dungeon->IsVanilla()) {
                    *should = false;
                }
            }
            break;
        }
        case VB_GRANNY_SAY_INSUFFICIENT_RUPEES: {
            if (EnDs_RandoCanGetGrannyItem()) {
                *should = gSaveContext.rupees <
                          OTRGlobals::Instance->gRandoContext->GetItemLocation(RC_KAK_GRANNYS_SHOP)->GetPrice();
            }
            break;
        }
        case VB_GRANNY_TAKE_MONEY: {
            if (EnDs_RandoCanGetGrannyItem()) {
                *should = false;
                Rupees_ChangeBy(OTRGlobals::Instance->gRandoContext->GetItemLocation(RC_KAK_GRANNYS_SHOP)->GetPrice() *
                                -1);
            }
            break;
        }
        case VB_NEED_BOTTLE_FOR_GRANNYS_ITEM: {
            // Allow buying the rando item regardless of having a bottle
            *should &= !EnDs_RandoCanGetGrannyItem();
            break;
        }
        case VB_GIVE_ITEM_FROM_SHOOTING_GALLERY: {
            EnSyatekiMan* enSyatekiMan = va_arg(args, EnSyatekiMan*);
            enSyatekiMan->getItemId = GI_RUPEE_PURPLE;
            if (LINK_IS_ADULT) {
                // Give purple rupee if we've already obtained the reward OR we don't have a bow
                *should = Flags_GetItemGetInf(ITEMGETINF_0E) || CUR_UPG_VALUE(UPG_QUIVER) == 0;
            } else {
                // Give purple rupee if we've already obtained the reward
                *should = Flags_GetItemGetInf(ITEMGETINF_0D);
            }
            break;
        }
        case VB_BE_ELIGIBLE_FOR_ADULT_SHOOTING_GAME_REWARD: {
            *should = CUR_UPG_VALUE(UPG_QUIVER) > 0;
            if (!*should) {
                // In Rando without a quiver, display a message reminding the player to come back with a bow
                Message_StartTextbox(gPlayState, TEXT_SHOOTING_GALLERY_MAN_COME_BACK_WITH_BOW, NULL);
            }
            break;
        }
        case VB_BE_ELIGIBLE_TO_OPEN_DOT: {
            bool eligible =
                RAND_GET_OPTION(RSK_DOOR_OF_TIME).IsNot(RO_DOOROFTIME_CLOSED) ||
                (INV_CONTENT(ITEM_OCARINA_FAIRY) == ITEM_OCARINA_TIME && CHECK_QUEST_ITEM(QUEST_KOKIRI_EMERALD) &&
                 CHECK_QUEST_ITEM(QUEST_GORON_RUBY) && CHECK_QUEST_ITEM(QUEST_ZORA_SAPPHIRE));
            *should = eligible;
            break;
        }
        case VB_GIVE_ITEM_FROM_HORSEBACK_ARCHERY: {
            EnGe1* enGe1 = va_arg(args, EnGe1*);
            // give both rewards at the same time
            if (gSaveContext.minigameScore >= 1000) {
                Flags_SetInfTable(INFTABLE_190);
            }
            if (gSaveContext.minigameScore >= 1500) {
                Flags_SetItemGetInf(ITEMGETINF_0F);
            }
            // move gerudo actor onto her wait loop
            enGe1->actionFunc = EnGe1_Wait_Archery;
            EnGe1_SetAnimationIdle(enGe1);
            // skip the vanilla gives.
            *should = false;
            break;
        }
        case VB_GIVE_ITEM_FROM_SKULLTULA_REWARD: {
            // In z_en_sth.c the rewards are stored in sGetItemIds, the first entry
            // in that array is GI_RUPEE_GOLD, and the reward is picked in EnSth_GivePlayerItem
            // via sGetItemIds[this->actor.params]. This means if actor.params == 0 we're looking
            // at the 100 GS reward
            EnSth* enSth = va_arg(args, EnSth*);
            if (enSth->actor.params == 0) {
                // if nothing is shuffled onto 100 GS,
                // or we already got the 100 GS reward,
                // let the player farm
                if (!RAND_GET_OPTION(RSK_SHUFFLE_100_GS_REWARD) ||
                    Flags_GetRandomizerInf(RAND_INF_KAK_100_GOLD_SKULLTULA_REWARD)) {
                    *should = true;
                    break;
                }

                // we're giving the 100 GS rando reward! set the rando inf
                Flags_SetRandomizerInf(RAND_INF_KAK_100_GOLD_SKULLTULA_REWARD);

                // also set the actionfunc so this doesn't immediately get
                // called again (and lead to a vanilla+rando item give
                // because the flag check will pass next time)
                enSth->actionFunc = (EnSthActionFunc)EnSth_RewardObtainedTalk;
            }
            *should = false;
            break;
        }
        case VB_GIVE_ITEM_FROM_OCARINA_MEMORY_GAME: {
            EnSkj* enSkj = va_arg(args, EnSkj*);
            Flags_SetItemGetInf(ITEMGETINF_17);
            enSkj->actionFunc = (EnSkjActionFunc)EnSkj_CleanupOcarinaGame;
            *should = false;
            break;
        }
        case VB_GIVE_ITEM_FROM_LOST_DOG: {
            EnHy* enHy = va_arg(args, EnHy*);
            Flags_SetInfTable(INFTABLE_191);
            gSaveContext.dogParams = 0;
            gSaveContext.dogIsLost = false;
            enHy->actionFunc = func_80A7127C;
            *should = false;
            break;
        }
        case VB_GIVE_ITEM_FROM_BOMBCHU_BOWLING: {
            EnBomBowlPit* enBomBowlPit = va_arg(args, EnBomBowlPit*);
            if (enBomBowlPit->prizeIndex == EXITEM_BOMB_BAG_BOWLING ||
                enBomBowlPit->prizeIndex == EXITEM_HEART_PIECE_BOWLING) {
                *should = false;
            }
            break;
        }
        case VB_GERUDO_GUARD_SET_ACTION_AFTER_TALK:
            if (gPlayState->msgCtx.choiceIndex == 0 && gPlayState->sceneNum == SCENE_GERUDOS_FORTRESS) {
                EnGe2* enGe2 = va_arg(args, EnGe2*);
                EnGe2_SetupCapturePlayer(enGe2, gPlayState);
                *should = false;
            }
            break;
        case VB_GERUDOS_BE_FRIENDLY: {
            *should = CHECK_QUEST_ITEM(QUEST_GERUDO_CARD);
            break;
        }
        case VB_GTG_GATE_BE_OPEN: {
            if (Flags_GetRandomizerInf(RAND_INF_GF_GTG_GATE_PERMANENTLY_OPEN)) {
                *should = true;
            }
            break;
        }
        case VB_GIVE_ITEM_GERUDO_MEMBERSHIP_CARD: {
            Flags_SetRandomizerInf(RAND_INF_TH_ITEM_FROM_LEADER_OF_FORTRESS);
            *should = false;
            break;
        }
        case VB_BE_ELIGIBLE_FOR_RAINBOW_BRIDGE: {
            *should = MeetsRainbowBridgeRequirements();
            break;
        }
        case VB_PLAY_BLUE_WARP_CS: {
            // We need to override just these two temples because they check medallions instead of flags
            if (gPlayState->sceneNum == SCENE_SPIRIT_TEMPLE_BOSS) {
                *should = !Flags_GetRandomizerInf(RAND_INF_DUNGEONS_DONE_SPIRIT_TEMPLE);
            } else if (gPlayState->sceneNum == SCENE_SHADOW_TEMPLE_BOSS) {
                *should = !Flags_GetRandomizerInf(RAND_INF_DUNGEONS_DONE_SHADOW_TEMPLE);
            }
            break;
        }
        case VB_DRAW_AMMO_COUNT: {
            s16 item = *va_arg(args, s16*);
            // don't draw ammo count if you have the infinite upgrade
            if ((item == ITEM_NUT && Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_NUT_UPGRADE)) ||
                (item == ITEM_STICK && Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_STICK_UPGRADE)) ||
                (item == ITEM_BOMB && Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_BOMB_BAG)) ||
                ((item == ITEM_BOW || item == ITEM_BOW_ARROW_FIRE || item == ITEM_BOW_ARROW_ICE ||
                  item == ITEM_BOW_ARROW_LIGHT) &&
                 Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_QUIVER) && gPlayState->shootingGalleryStatus < 2 &&
                 gSaveContext.minigameState != 1) ||
                (item == ITEM_SLINGSHOT && Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_BULLET_BAG) &&
                 gPlayState->shootingGalleryStatus < 2) ||
                (item == ITEM_BOMBCHU && Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_BOMBCHUS) &&
                 gPlayState->bombchuBowlingStatus < 1)) {
                *should = false;
            }
            break;
        }
        case VB_HAVE_OCARINA_NOTE_A4: {
            if (!Flags_GetRandomizerInf(RAND_INF_HAS_OCARINA_C_RIGHT)) {
                *should = false;
            }
            break;
        }
        case VB_HAVE_OCARINA_NOTE_B4: {
            if (!Flags_GetRandomizerInf(RAND_INF_HAS_OCARINA_C_LEFT)) {
                *should = false;
            }
            break;
        }
        case VB_HAVE_OCARINA_NOTE_D4: {
            if (!Flags_GetRandomizerInf(RAND_INF_HAS_OCARINA_A)) {
                *should = false;
            }
            break;
        }
        case VB_HAVE_OCARINA_NOTE_D5: {
            if (!Flags_GetRandomizerInf(RAND_INF_HAS_OCARINA_C_UP)) {
                *should = false;
            }
            break;
        }
        case VB_HAVE_OCARINA_NOTE_F4: {
            if (!Flags_GetRandomizerInf(RAND_INF_HAS_OCARINA_C_DOWN)) {
                *should = false;
            }
            break;
        }
        case VB_SKIP_SCARECROWS_SONG: {
            int ocarinaButtonCount = 0;
            for (int i = VB_HAVE_OCARINA_NOTE_A4; i <= VB_HAVE_OCARINA_NOTE_F4; i++) {
                if (GameInteractor_Should((GIVanillaBehavior)i, true)) {
                    ocarinaButtonCount++;
                }
            }

            if (ocarinaButtonCount < 2) {
                *should = false;
                break;
            }

            if (gPlayState->msgCtx.msgMode == MSGMODE_OCARINA_PLAYING && RAND_GET_OPTION(RSK_SKIP_SCARECROWS_SONG)) {
                *should = true;
                break;
            }
            break;
        }
        case VB_RENDER_RUPEE_COUNTER: {
            if (!Flags_GetRandomizerInf(RAND_INF_HAS_WALLET) || Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_MONEY)) {
                *should = false;
            }
            break;
        }
        case VB_BE_ABLE_TO_PLAY_BOMBCHU_BOWLING: {
            // Only check for bomb bag when bombchus aren't in logic
            // and only check for bombchus when bombchus are in logic
            *should = INV_CONTENT((RAND_GET_OPTION(RSK_BOMBCHU_BAG) ? ITEM_BOMBCHU : ITEM_BOMB)) != ITEM_NONE;
            break;
        }
        case VB_SHOULD_CHECK_FOR_FISHING_RECORD: {
            f32 sFishOnHandLength = *va_arg(args, f32*);
            *should = *should || ShouldGiveFishingPrize(sFishOnHandLength);
            break;
        }
        case VB_SHOULD_SET_FISHING_RECORD: {
            VBFishingData* fishData = va_arg(args, VBFishingData*);
            *should = (s16)fishData->sFishingRecordLength < (s16)fishData->fishWeight;
            if (!*should) {
                *fishData->sFishOnHandLength = 0.0f;
            }
            break;
        }
        case VB_SHOULD_GIVE_VANILLA_FISHING_PRIZE: {
            VBFishingData* fishData = va_arg(args, VBFishingData*);
            *should = !IS_RANDO && ShouldGiveFishingPrize(fishData->fishWeight);
            break;
        }
        case VB_GIVE_RANDO_FISHING_PRIZE: {
            if (IS_RANDO) {
                VBFishingData* fishData = va_arg(args, VBFishingData*);
                if (*fishData->sFishOnHandIsLoach) {
                    if (!Flags_GetRandomizerInf(RAND_INF_CAUGHT_LOACH) &&
                        OTRGlobals::Instance->gRandomizer->GetRandoSettingValue(RSK_FISHSANITY) ==
                            RO_FISHSANITY_HYRULE_LOACH) {
                        Flags_SetRandomizerInf(RAND_INF_CAUGHT_LOACH);
                        Message_StartTextbox(gPlayState, TEXT_FISHING_RELEASE_THIS_ONE, NULL);
                        *should = true;
                        fishData->actor->stateAndTimer = 20;
                    }
                } else {
                    if (ShouldGiveFishingPrize(fishData->fishWeight)) {
                        if (LINK_IS_CHILD) {
                            Flags_SetRandomizerInf(RAND_INF_CHILD_FISHING);
                            HIGH_SCORE(HS_FISHING) |= HS_FISH_PRIZE_CHILD;
                        } else {
                            Flags_SetRandomizerInf(RAND_INF_ADULT_FISHING);
                            HIGH_SCORE(HS_FISHING) |= HS_FISH_PRIZE_ADULT;
                        }
                        *should = true;
                        *fishData->sSinkingLureLocation = (u8)Rand_ZeroFloat(3.999f) + 1;
                        fishData->actor->stateAndTimer = 0;
                    }
                }
            }
            break;
        }
        case VB_GIVE_RANDO_GLITCH_FISHING_PRIZE: {
            if (IS_RANDO) {
                Fishing* fishing = va_arg(args, Fishing*);
                if (!Flags_GetRandomizerInf(RAND_INF_ADULT_FISHING)) {
                    Flags_SetRandomizerInf(RAND_INF_ADULT_FISHING);
                }
                *should = true;
                fishing->stateAndTimer = 0;
            }
            break;
        }
        case VB_TRADE_TIMER_EYEDROPS: {
            EnMk* enMk = va_arg(args, EnMk*);
            Flags_SetRandomizerInf(RAND_INF_ADULT_TRADES_LH_TRADE_FROG);
            enMk->actor.flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
            enMk->actionFunc = EnMk_Wait;
            enMk->flags |= 1;
            *should = false;
            break;
        }
        // We need to override the vanilla behavior here because the player might sequence break and get Ruto kidnapped
        // before accessing other checks that require Ruto. So if she's kidnapped we allow her to spawn again
        case VB_RUTO_BE_CONSIDERED_NOT_KIDNAPPED: {
            *should = !Flags_GetInfTable(INFTABLE_145) || Flags_GetInfTable(INFTABLE_146);
            break;
        }
        case VB_SET_VOIDOUT_FROM_SURFACE: {
            // ENTRTODO: Move all entrance rando handling to a dedicated file
            std::vector<s16> entrPersistTempFlags = {
                ENTR_DEKU_TREE_BOSS_ENTRANCE,     ENTR_DEKU_TREE_BOSS_DOOR,        ENTR_DODONGOS_CAVERN_BOSS_ENTRANCE,
                ENTR_DODONGOS_CAVERN_BOSS_DOOR,   ENTR_JABU_JABU_BOSS_ENTRANCE,    ENTR_JABU_JABU_BOSS_DOOR,
                ENTR_FOREST_TEMPLE_BOSS_ENTRANCE, ENTR_FOREST_TEMPLE_BOSS_DOOR,    ENTR_FIRE_TEMPLE_BOSS_ENTRANCE,
                ENTR_FIRE_TEMPLE_BOSS_DOOR,       ENTR_WATER_TEMPLE_BOSS_ENTRANCE, ENTR_WATER_TEMPLE_BOSS_DOOR,
                ENTR_SPIRIT_TEMPLE_BOSS_ENTRANCE, ENTR_SPIRIT_TEMPLE_BOSS_DOOR,    ENTR_SHADOW_TEMPLE_BOSS_ENTRANCE,
                ENTR_SHADOW_TEMPLE_BOSS_DOOR,     ENTR_SPIRIT_TEMPLE_ENTRANCE,
            };

            s16 originalEntrance = (s16)va_arg(args, int);

            // In Entrance rando, if our respawnFlag is set for a grotto return, we don't want the void out to happen
            if (*should == true && RAND_GET_OPTION(RSK_SHUFFLE_ENTRANCES)) {
                // Check for dungeon special entrances that are randomized to a new location
                if (std::find(entrPersistTempFlags.begin(), entrPersistTempFlags.end(), originalEntrance) !=
                        entrPersistTempFlags.end() &&
                    originalEntrance != gPlayState->nextEntranceIndex) {
                    // Normally dungeons use a special voidout between scenes so that entering/exiting a boss room,
                    // or leaving via Spirit Hands and going back in persist temp flags across scenes.
                    // For ER, the temp flags should be wiped out so that they aren't transferred to the new location.
                    gPlayState->actorCtx.flags.tempSwch = 0;
                    gPlayState->actorCtx.flags.tempCollect = 0;

                    // If the respawnFlag is set for a grotto return, we don't want the void out to happen.
                    // Set the data flag to one to prevent the respawn point from being overridden by dungeon doors.
                    if (gSaveContext.respawnFlag == 2) {
                        gSaveContext.respawn[RESPAWN_MODE_DOWN].data = 1;
                        *should = false;
                    }
                }
            }
            break;
        }
        case VB_HEALTH_METER_BE_CRITICAL: {
            if (gSaveContext.health == gSaveContext.healthCapacity) {
                *should = false;
            }
            break;
        }
        case VB_HEISHI2_ACCEPT_ITEM_AS_ZELDAS_LETTER: {
            if (*should) {
                // remove zelda's letter as this is the only use for it
                Flags_UnsetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_LETTER_ZELDA);
            }
            break;
        }
        case VB_FREEZE_ON_SKULL_TOKEN:
        case VB_TRADE_TIMER_ODD_MUSHROOM:
        case VB_TRADE_TIMER_FROG:
        case VB_GIVE_ITEM_FROM_TARGET_IN_WOODS:
        case VB_GIVE_ITEM_FROM_TALONS_CHICKENS:
        case VB_GIVE_ITEM_FROM_DIVING_MINIGAME:
        case VB_GIVE_ITEM_FROM_GORON:
        case VB_GIVE_ITEM_FROM_LAB_DIVE:
        case VB_GIVE_ITEM_FROM_SKULL_KID_SARIAS_SONG:
        case VB_GIVE_ITEM_FROM_MAN_ON_ROOF:
        case VB_GIVE_ITEM_FROM_BLUE_WARP:
        case VB_GIVE_ITEM_FAIRY_OCARINA:
        case VB_GIVE_ITEM_WEIRD_EGG:
        case VB_GIVE_ITEM_LIGHT_ARROW:
        case VB_GIVE_ITEM_STRENGTH_1:
        case VB_GIVE_ITEM_ZELDAS_LETTER:
        case VB_GIVE_ITEM_OCARINA_OF_TIME:
        case VB_GIVE_ITEM_LIGHT_MEDALLION:
        case VB_GIVE_ITEM_FOREST_MEDALLION:
        case VB_GIVE_ITEM_FIRE_MEDALLION:
        case VB_GIVE_ITEM_WATER_MEDALLION:
        case VB_GIVE_ITEM_SPIRIT_MEDALLION:
        case VB_GIVE_ITEM_SHADOW_MEDALLION:
        case VB_CHEST_USE_ICE_EFFECT:
            *should = false;
            break;
        case VB_GIVE_ITEM_SKULL_TOKEN:
            *should = (Rando::Context::GetInstance()->GetOption(RSK_SHUFFLE_TOKENS).Is(RO_TOKENSANITY_OFF));
            break;
        default:
            break;
    }

    va_end(args);
}

void RandomizerOnSceneInitHandler(int16_t sceneNum) {
    // Treasure Chest Game
    // todo: for now we're just unsetting all of them, we will
    //       probably need to do something different when we implement shuffle
    if (sceneNum == SCENE_TREASURE_BOX_SHOP) {
        Flags_UnsetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_ITEM_1);
        Rando::Context::GetInstance()
            ->GetItemLocation(RC_MARKET_TREASURE_CHEST_GAME_ITEM_1)
            ->SetCheckStatus(RCSHOW_UNCHECKED);
        Flags_UnsetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_ITEM_2);
        Rando::Context::GetInstance()
            ->GetItemLocation(RC_MARKET_TREASURE_CHEST_GAME_ITEM_2)
            ->SetCheckStatus(RCSHOW_UNCHECKED);
        Flags_UnsetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_ITEM_3);
        Rando::Context::GetInstance()
            ->GetItemLocation(RC_MARKET_TREASURE_CHEST_GAME_ITEM_3)
            ->SetCheckStatus(RCSHOW_UNCHECKED);
        Flags_UnsetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_ITEM_4);
        Rando::Context::GetInstance()
            ->GetItemLocation(RC_MARKET_TREASURE_CHEST_GAME_ITEM_4)
            ->SetCheckStatus(RCSHOW_UNCHECKED);
        Flags_UnsetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_ITEM_5);
        Rando::Context::GetInstance()
            ->GetItemLocation(RC_MARKET_TREASURE_CHEST_GAME_ITEM_5)
            ->SetCheckStatus(RCSHOW_UNCHECKED);
        Flags_UnsetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_KEY_1);
        Rando::Context::GetInstance()
            ->GetItemLocation(RC_MARKET_TREASURE_CHEST_GAME_KEY_1)
            ->SetCheckStatus(RCSHOW_UNCHECKED);
        Flags_UnsetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_KEY_2);
        Rando::Context::GetInstance()
            ->GetItemLocation(RC_MARKET_TREASURE_CHEST_GAME_KEY_2)
            ->SetCheckStatus(RCSHOW_UNCHECKED);
        Flags_UnsetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_KEY_3);
        Rando::Context::GetInstance()
            ->GetItemLocation(RC_MARKET_TREASURE_CHEST_GAME_KEY_3)
            ->SetCheckStatus(RCSHOW_UNCHECKED);
        Flags_UnsetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_KEY_4);
        Rando::Context::GetInstance()
            ->GetItemLocation(RC_MARKET_TREASURE_CHEST_GAME_KEY_4)
            ->SetCheckStatus(RCSHOW_UNCHECKED);
        Flags_UnsetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_KEY_5);
        Rando::Context::GetInstance()
            ->GetItemLocation(RC_MARKET_TREASURE_CHEST_GAME_KEY_5)
            ->SetCheckStatus(RCSHOW_UNCHECKED);
        CheckTracker::RecalculateAllAreaTotals();
    }

    // ENTRTODO: Move all entrance rando handling to a dedicated file
    if (RAND_GET_OPTION(RSK_SHUFFLE_ENTRANCES)) {
        // In ER, override roomNum to load based on scene and spawn during scene init
        if (gSaveContext.respawnFlag <= 0) {
            s8 origRoom = gPlayState->roomCtx.curRoom.num;
            s8 replacedRoom = Entrance_OverrideSpawnSceneRoom(gPlayState->sceneNum, gPlayState->curSpawn, origRoom);

            if (origRoom != replacedRoom) {
                // Reset room ctx back to prev room and then load the new room
                gPlayState->roomCtx.status = 0;
                gPlayState->roomCtx.curRoom = gPlayState->roomCtx.prevRoom;
                func_8009728C(gPlayState, &gPlayState->roomCtx, replacedRoom);
            }
        }

        // Handle updated link spawn positions
        Entrance_OverrideSpawnScene(sceneNum, gPlayState->curSpawn);
    }

    // LACS & Prelude checks
    static uint32_t updateHook = 0;

    if (updateHook) {
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnPlayerUpdate>(updateHook);
        updateHook = 0;
    }

    // If we're not in the Temple of Time or we've already learned the Prelude of Light and received LACs, we don't need
    // to do anything
    if (sceneNum != SCENE_TEMPLE_OF_TIME ||
        (Flags_GetEventChkInf(EVENTCHKINF_LEARNED_PRELUDE_OF_LIGHT) &&
         Flags_GetEventChkInf(EVENTCHKINF_RETURNED_TO_TEMPLE_OF_TIME_WITH_ALL_MEDALLIONS)))
        return;

    updateHook = GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>([]() {
        if (!Flags_GetEventChkInf(EVENTCHKINF_LEARNED_PRELUDE_OF_LIGHT) && LINK_IS_ADULT &&
            CHECK_QUEST_ITEM(QUEST_MEDALLION_FOREST) && gPlayState->roomCtx.curRoom.num == 0) {
            Flags_SetEventChkInf(EVENTCHKINF_LEARNED_PRELUDE_OF_LIGHT);
        }

        // We're always in rando here, and rando always overrides this should so we can just pass false
        if (GameInteractor_Should(VB_BE_ELIGIBLE_FOR_LIGHT_ARROWS, false)) {
            Flags_SetEventChkInf(EVENTCHKINF_RETURNED_TO_TEMPLE_OF_TIME_WITH_ALL_MEDALLIONS);
        }

        // If both awards have been given, we can unregister the hook, otherwise it will get unregistered when the
        // player leaves the area
        if (Flags_GetEventChkInf(EVENTCHKINF_LEARNED_PRELUDE_OF_LIGHT) &&
            Flags_GetEventChkInf(EVENTCHKINF_RETURNED_TO_TEMPLE_OF_TIME_WITH_ALL_MEDALLIONS)) {
            GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnPlayerUpdate>(updateHook);
            updateHook = 0;
        }
    });
}

void RandomizerAfterSceneCommandsHandler(int16_t sceneNum) {
    // ENTRTODO: Move all entrance rando handling to a dedicated file
    if (RAND_GET_OPTION(RSK_SHUFFLE_ENTRANCES)) {
        Entrance_OverrideWeatherState();
    }
}

void EnSi_DrawRandomizedItem(EnSi* enSi, PlayState* play) {
    GetItemEntry randoItem = enSi->sohGetItemEntry;
    if (CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"), 0)) {
        randoItem = GET_ITEM_MYSTERY;
    }
    func_8002ED80(&enSi->actor, play, 0);
    func_8002EBCC(&enSi->actor, play, 0);
    EnItem00_CustomItemsParticles(&enSi->actor, play, randoItem);
    GetItemEntry_Draw(play, randoItem);
}

u32 EnDns_RandomizerPurchaseableCheck(EnDns* enDns) {
    auto checkIdentity = ObjectExtension::GetInstance().Get<ScrubIdentity>(enDns);
    if (checkIdentity != nullptr && Flags_GetRandomizerInf(checkIdentity->identity.randomizerInf)) {
        return DNS_CANBUY_RESULT_CANT_GET_NOW;
    }
    if (gSaveContext.rupees < enDns->dnsItemEntry->itemPrice) {
        return DNS_CANBUY_RESULT_NEED_RUPEES;
    }
    return DNS_CANBUY_RESULT_SUCCESS;
}

void EnDns_RandomizerPurchase(EnDns* enDns) {
    Rupees_ChangeBy(-enDns->dnsItemEntry->itemPrice);
    auto checkIdentity = ObjectExtension::GetInstance().Get<ScrubIdentity>(enDns);
    if (checkIdentity != nullptr) {
        Flags_SetRandomizerInf(checkIdentity->identity.randomizerInf);
    }
}

void RandomizerOnActorInitHandler(void* actorRef) {
    Actor* actor = static_cast<Actor*>(actorRef);

    if (actor->id == ACTOR_PLAYER) {
        auto dungeonInfo = Rando::Context::GetInstance()->GetDungeons()->GetDungeonFromScene(gPlayState->sceneNum);
        bool isVanilla = dungeonInfo == nullptr || dungeonInfo->IsVanilla();
        switch (gPlayState->sceneNum) {
            case SCENE_DEKU_TREE:
                if (!isVanilla && Flags_GetRandomizerInf(RAND_INF_DEKU_TREE_MQ_TORCH_SWITCH)) {
                    Flags_SetSwitch(gPlayState, 0x27);
                }
                if (isVanilla) { // make falling platform respawn
                    Flags_UnsetSwitch(gPlayState, 0x14);
                }
                break;
            case SCENE_DODONGOS_CAVERN:
                if (!isVanilla && Flags_GetRandomizerInf(RAND_INF_DODONGOS_CAVERN_MQ_SILVER_RUPEES)) {
                    Flags_SetSwitch(gPlayState, 0x25);
                }
                if (isVanilla) { // make gossip stone fairy temp flag
                    Flags_UnsetSwitch(gPlayState, 0x11);
                }
                break;
            case SCENE_JABU_JABU:
                if (isVanilla && Flags_GetRandomizerInf(RAND_INF_JABU_JABUS_BELLY_FIRST_SWITCH)) {
                    Flags_SetSwitch(gPlayState, 0x3b);
                }
                break;
            case SCENE_FOREST_TEMPLE:
                if (Flags_GetRandomizerInf(RAND_INF_FOREST_DRAINED_WELL)) {
                    Flags_SetSwitch(gPlayState, 0x26);
                }
                if (Flags_GetRandomizerInf(RAND_INF_FOREST_LOBBY_EYES)) {
                    Flags_SetSwitch(gPlayState, 0x25);
                    if (!isVanilla) {
                        Flags_SetSwitch(gPlayState, 0x2a);
                    }
                }
                if (!isVanilla && Flags_GetRandomizerInf(RAND_INF_FOREST_MQ_COURTYARD_WEB_BURNT)) {
                    Flags_SetSwitch(gPlayState, 0x21);
                }
                break;
            case SCENE_FIRE_TEMPLE:
                if (!isVanilla && Flags_GetRandomizerInf(RAND_INF_FIRE_MQ_LOBBY_TORCHES)) {
                    Flags_SetSwitch(gPlayState, 0x28);
                }
                break;
            case SCENE_SPIRIT_TEMPLE:
                if (isVanilla && Flags_GetRandomizerInf(RAND_INF_SPIRIT_SUN_ON_FLOOR_ON)) {
                    Flags_SetSwitch(gPlayState, 0x23);
                }
                if (!isVanilla && Flags_GetRandomizerInf(RAND_INF_SPIRIT_MQ_LOBBY_SILVER_RUPEES)) {
                    Flags_SetSwitch(gPlayState, 0x37);
                }
                break;
        }
    }

    if (actor->id == ACTOR_EN_SI) {
        RandomizerCheck rc =
            OTRGlobals::Instance->gRandomizer->GetCheckFromActor(actor->id, gPlayState->sceneNum, actor->params);
        if (rc != RC_UNKNOWN_CHECK) {
            EnSi* enSi = static_cast<EnSi*>(actorRef);
            enSi->sohGetItemEntry = ApplyCrossoverVisualOnlyItemEntry(
                rc, Rando::Context::GetInstance()->GetFinalGIEntry(
                        rc, true, (GetItemID)Rando::StaticData::GetLocation(rc)->GetVanillaItem()));
            actor->draw = (ActorFunc)EnSi_DrawRandomizedItem;
        }
    }

    if (actor->id == ACTOR_EN_DNS) {
        EnDns* enDns = static_cast<EnDns*>(actorRef);
        s16 respawnData = gSaveContext.respawn[RESPAWN_MODE_RETURN].data & ((1 << 8) - 1);
        auto scrubIdentity =
            OTRGlobals::Instance->gRandomizer->IdentifyScrub(gPlayState->sceneNum, enDns->actor.params, respawnData);

        if (scrubIdentity.identity.randomizerCheck != RC_UNKNOWN_CHECK) {
            // DNS uses pointers so we're creating our own entry instead of modifying the original
            ObjectExtension::GetInstance().Set<DnsItemEntry>(actorRef, std::move(DnsItemEntry{
                                                                           enDns->dnsItemEntry->itemPrice,
                                                                           1,
                                                                           scrubIdentity.getItemId,
                                                                           EnDns_RandomizerPurchaseableCheck,
                                                                           EnDns_RandomizerPurchase,
                                                                       }));
            enDns->dnsItemEntry = ObjectExtension::GetInstance().Get<DnsItemEntry>(actorRef);

            if (scrubIdentity.itemPrice != -1) {
                enDns->dnsItemEntry->itemPrice = scrubIdentity.itemPrice;
            }

            ObjectExtension::GetInstance().Set<ScrubIdentity>(actorRef, std::move(scrubIdentity));
            enDns->actor.textId = TEXT_SCRUB_RANDOM;

            static uint32_t enDnsUpdateHook = 0;
            static uint32_t enDnsKillHook = 0;
            if (!enDnsUpdateHook) {
                enDnsUpdateHook =
                    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>([](void* innerActorRef) {
                        Actor* innerActor = static_cast<Actor*>(innerActorRef);
                        if (innerActor->id == ACTOR_EN_DNS) {
                            if (ObjectExtension::GetInstance().Has<ScrubIdentity>(innerActor)) {
                                innerActor->textId = TEXT_SCRUB_RANDOM;
                            }
                        }
                    });
                enDnsKillHook =
                    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t sceneNum) {
                        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnActorUpdate>(enDnsUpdateHook);
                        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnSceneInit>(enDnsKillHook);
                        enDnsUpdateHook = 0;
                        enDnsKillHook = 0;
                    });
            }
        }
    }

    if (actor->id == ACTOR_ITEM_ETCETERA) {
        ItemEtcetera* itemEtcetera = static_cast<ItemEtcetera*>(actorRef);
        RandomizerCheck rc = OTRGlobals::Instance->gRandomizer->GetCheckFromActor(
            itemEtcetera->actor.id, gPlayState->sceneNum, itemEtcetera->actor.params);
        if (rc != RC_UNKNOWN_CHECK) {
            itemEtcetera->sohItemEntry = ApplyCrossoverVisualOnlyItemEntry(
                rc, Rando::Context::GetInstance()->GetFinalGIEntry(
                        rc, true, (GetItemID)Rando::StaticData::GetLocation(rc)->GetVanillaItem()));
            itemEtcetera->drawFunc = (ActorFunc)ItemEtcetera_DrawRandomizedItem;
        }

        int32_t type = itemEtcetera->actor.params & 0xFF;
        switch (type) {
            case ITEM_ETC_LETTER: {
                itemEtcetera->futureActionFunc = (ItemEtceteraActionFunc)ItemEtcetera_func_80B858B4_Randomized;
                break;
            }
            case ITEM_ETC_ARROW_FIRE: {
                itemEtcetera->futureActionFunc = (ItemEtceteraActionFunc)ItemEtcetera_UpdateRandomizedFireArrow;
                break;
            }
            case ITEM_ETC_RUPEE_GREEN_CHEST_GAME:
            case ITEM_ETC_RUPEE_BLUE_CHEST_GAME:
            case ITEM_ETC_RUPEE_RED_CHEST_GAME:
            case ITEM_ETC_RUPEE_PURPLE_CHEST_GAME:
            case ITEM_ETC_HEART_PIECE_CHEST_GAME:
            case ITEM_ETC_KEY_SMALL_CHEST_GAME: {
                if (rc != RC_UNKNOWN_CHECK) {
                    itemEtcetera->drawFunc = (ActorFunc)ItemEtcetera_DrawRandomizedItemThroughLens;
                }
                break;
            }
        }
    }

    if (actor->id == ACTOR_EN_EX_ITEM) {
        EnExItem* enExItem = static_cast<EnExItem*>(actorRef);

        RandomizerCheck rc = RC_UNKNOWN_CHECK;
        switch (enExItem->type) {
            case EXITEM_BOMB_BAG_COUNTER:
            case EXITEM_BOMB_BAG_BOWLING:
                rc = RC_MARKET_BOMBCHU_BOWLING_FIRST_PRIZE;
                break;
            case EXITEM_HEART_PIECE_COUNTER:
            case EXITEM_HEART_PIECE_BOWLING:
                rc = RC_MARKET_BOMBCHU_BOWLING_SECOND_PRIZE;
                break;
            case EXITEM_BOMBCHUS_COUNTER:
            case EXITEM_BOMBCHUS_BOWLING:
                // RC_MARKET_BOMBCHU_BOWLING_BOMBCHUS was removed as a 3DS holdover not in anyones near term plans due
                // to being pretty useless.
                break;
            case EXITEM_BULLET_BAG:
                rc = RC_LW_TARGET_IN_WOODS;
                break;
        }
        if (rc != RC_UNKNOWN_CHECK) {
            enExItem->sohItemEntry = ApplyCrossoverVisualOnlyItemEntry(
                rc, Rando::Context::GetInstance()->GetFinalGIEntry(
                        rc, true, (GetItemID)Rando::StaticData::GetLocation(rc)->GetVanillaItem()));
            enExItem->actionFunc = (EnExItemActionFunc)EnExItem_WaitForObjectRandomized;
        }
    }

    if (actor->id == ACTOR_EN_GE1) {
        EnGe1* enGe1 = static_cast<EnGe1*>(actorRef);
        auto ge1Type = enGe1->actor.params & 0xFF;
        if (ge1Type == GE1_TYPE_TRAINING_GROUND_GUARD &&
            Flags_GetRandomizerInf(RAND_INF_GF_GTG_GATE_PERMANENTLY_OPEN)) {
            enGe1->actionFunc = (EnGe1ActionFunc)EnGe1_SetNormalText;
        } else if (ge1Type == GE1_TYPE_GATE_OPERATOR && enGe1->actor.world.pos.x != -1358.0f) {
            // When spawning the gate operator, also spawn an extra gate operator on the wasteland side
            Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_GE1, -1358.0f, 88.0f, -3018.0f, 0, 0x95B0, 0,
                        0x0300 | GE1_TYPE_GATE_OPERATOR);
        }
    }

    if (actor->id == ACTOR_BG_JYA_BIGMIRROR && Flags_GetRandomizerInf(RAND_INF_SPIRIT_BIG_MIRROR_STATUE_TURNED)) {
        Flags_SetSwitch(gPlayState, 0x29); // destroy wall
        auto jyaBigMirror = static_cast<BgJyaBigmirror*>(actorRef);
        jyaBigMirror->puzzleFlags |=
            BIGMIR_PUZZLE_COBRA1_SOLVED | BIGMIR_PUZZLE_COBRA2_SOLVED | BIGMIR_PUZZLE_BOMBIWA_DESTROYED;
        jyaBigMirror->cobraInfo[0].rotY = 0x4000;
        jyaBigMirror->cobraInfo[1].rotY = 0x8000;
    }

    if (actor->id == ACTOR_DEMO_KEKKAI && actor->params == 0) { // 0 == KEKKAI_TOWER
        if (CompletedAllTrials()) {
            Actor_Kill(actor);
        }
    }

    if (actor->id == ACTOR_EN_OSSAN && actor->params == OSSAN_TYPE_MASK &&
        RAND_GET_OPTION(RSK_MASK_QUEST).Is(RO_MASK_QUEST_SHUFFLE)) {
        Actor_Kill(actor);
    }

    if (actor->id == ACTOR_BG_TREEMOUTH && LINK_IS_ADULT &&
        RAND_GET_OPTION(RSK_SHUFFLE_DUNGEON_ENTRANCES).IsNot(RO_DUNGEON_ENTRANCE_SHUFFLE_OFF) &&
        (RAND_GET_OPTION(RSK_FOREST).Is(RO_CLOSED_FOREST_OFF) ||
         Flags_GetEventChkInf(EVENTCHKINF_SHOWED_MIDO_SWORD_SHIELD))) {
        BgTreemouth* bgTreemouth = static_cast<BgTreemouth*>(actorRef);
        bgTreemouth->unk_168 = 1.0f;
    }

    // consumable bags
    if (actor->id == ACTOR_EN_ITEM00 &&
        ((RAND_GET_OPTION(RSK_SHUFFLE_DEKU_STICK_BAG) && CUR_UPG_VALUE(UPG_STICKS) == 0 &&
          actor->params == ITEM00_STICK) ||
         (RAND_GET_OPTION(RSK_SHUFFLE_DEKU_NUT_BAG) && CUR_UPG_VALUE(UPG_NUTS) == 0 && actor->params == ITEM00_NUTS))) {
        Actor_Kill(actor);
    }

    if (RAND_GET_OPTION(RSK_SHUFFLE_BOSS_SOULS)) {
        // Boss souls require an additional item (represented by a RAND_INF) to spawn a boss in a particular lair
        RandomizerInf currentBossSoulRandInf = RAND_INF_MAX;
        switch (gPlayState->sceneNum) {
            case SCENE_DEKU_TREE_BOSS:
                currentBossSoulRandInf = RAND_INF_GOHMA_SOUL;
                break;
            case SCENE_DODONGOS_CAVERN_BOSS:
                currentBossSoulRandInf = RAND_INF_KING_DODONGO_SOUL;
                break;
            case SCENE_JABU_JABU_BOSS:
                currentBossSoulRandInf = RAND_INF_BARINADE_SOUL;
                break;
            case SCENE_FOREST_TEMPLE_BOSS:
                currentBossSoulRandInf = RAND_INF_PHANTOM_GANON_SOUL;
                break;
            case SCENE_FIRE_TEMPLE_BOSS:
                currentBossSoulRandInf = RAND_INF_VOLVAGIA_SOUL;
                break;
            case SCENE_WATER_TEMPLE_BOSS:
                currentBossSoulRandInf = RAND_INF_MORPHA_SOUL;
                break;
            case SCENE_SHADOW_TEMPLE_BOSS:
                currentBossSoulRandInf = RAND_INF_BONGO_BONGO_SOUL;
                break;
            case SCENE_SPIRIT_TEMPLE_BOSS:
                currentBossSoulRandInf = RAND_INF_TWINROVA_SOUL;
                break;
            case SCENE_GANONDORF_BOSS:
            case SCENE_GANON_BOSS:
                if (RAND_GET_OPTION(RSK_SHUFFLE_BOSS_SOULS).Is(RO_BOSS_SOULS_ON_PLUS_GANON)) {
                    currentBossSoulRandInf = RAND_INF_GANON_SOUL;
                }
                break;
            default:
                break;
        }

        // Deletes all actors in the boss category if the soul isn't found.
        // Some actors, like Dark Link, Arwings, and Zora's Sapphire...?, are in this category despite not being actual
        // bosses, so ignore any "boss" if `currentBossSoulRandInf` doesn't change from RAND_INF_MAX. Iron Knuckle
        // (Nabooru) in Twinrova's room is a special exception, so exclude knuckles too.
        if (currentBossSoulRandInf != RAND_INF_MAX) {
            if (!Flags_GetRandomizerInf(currentBossSoulRandInf) && actor->category == ACTORCAT_BOSS &&
                actor->id != ACTOR_EN_IK) {
                Actor_Delete(&gPlayState->actorCtx, actor, gPlayState);
            }
            // Special case for Phantom Ganon's horse (and fake), as they're considered "background actors",
            // but still control the boss fight flow.
            if (!Flags_GetRandomizerInf(RAND_INF_PHANTOM_GANON_SOUL) && actor->id == ACTOR_EN_FHG) {
                Actor_Delete(&gPlayState->actorCtx, actor, gPlayState);
            }
        }
    }

    // In MQ Spirit, remove the large silver block in the hole as child so the chest in the silver block hallway
    // can be guaranteed accessible
    if (actor->id == ACTOR_OBJ_OSHIHIKI && LINK_IS_CHILD && ResourceMgr_IsGameMasterQuest() &&
        gPlayState->sceneNum == SCENE_SPIRIT_TEMPLE && actor->room == 6 && // Spirit Temple silver block hallway
        actor->params == 0x9C7                                             // Silver block that is marked as in the hole
    ) {
        Actor_Kill(actor);
        return;
    }

    if (RAND_GET_OPTION(RSK_SHUFFLE_BEAN_SOULS)) {
        RandomizerInf currentBeanSoulRandInf = RAND_INF_MAX;
        if (actor->id == ACTOR_OBJ_BEAN) {
            switch (gPlayState->sceneNum) {
                case SCENE_DEATH_MOUNTAIN_CRATER:
                    currentBeanSoulRandInf = RAND_INF_DEATH_MOUNTAIN_CRATER_BEAN_SOUL;
                    break;
                case SCENE_DEATH_MOUNTAIN_TRAIL:
                    currentBeanSoulRandInf = RAND_INF_DEATH_MOUNTAIN_TRAIL_BEAN_SOUL;
                    break;
                case SCENE_DESERT_COLOSSUS:
                    currentBeanSoulRandInf = RAND_INF_DESERT_COLOSSUS_BEAN_SOUL;
                    break;
                case SCENE_GERUDO_VALLEY:
                    currentBeanSoulRandInf = RAND_INF_GERUDO_VALLEY_BEAN_SOUL;
                    break;
                case SCENE_GRAVEYARD:
                    currentBeanSoulRandInf = RAND_INF_GRAVEYARD_BEAN_SOUL;
                    break;
                case SCENE_KOKIRI_FOREST:
                    currentBeanSoulRandInf = RAND_INF_KOKIRI_FOREST_BEAN_SOUL;
                    break;
                case SCENE_LAKE_HYLIA:
                    currentBeanSoulRandInf = RAND_INF_LAKE_HYLIA_BEAN_SOUL;
                    break;
                case SCENE_LOST_WOODS:
                    if ((actor->params & 0x3F) == 4) {
                        currentBeanSoulRandInf = RAND_INF_LOST_WOODS_BRIDGE_BEAN_SOUL;
                    } else {
                        currentBeanSoulRandInf = RAND_INF_LOST_WOODS_BEAN_SOUL;
                    }
                    break;
                case SCENE_ZORAS_RIVER:
                    currentBeanSoulRandInf = RAND_INF_ZORAS_RIVER_BEAN_SOUL;
                    break;
            }
        } else if (actor->id == ACTOR_OBJ_MAKEKINSUTA) {
            switch (gPlayState->sceneNum) {
                case SCENE_DEATH_MOUNTAIN_CRATER:
                    currentBeanSoulRandInf = RAND_INF_DEATH_MOUNTAIN_CRATER_BEAN_SOUL;
                    break;
                case SCENE_DEATH_MOUNTAIN_TRAIL:
                    currentBeanSoulRandInf = RAND_INF_DEATH_MOUNTAIN_TRAIL_BEAN_SOUL;
                    break;
                case SCENE_DESERT_COLOSSUS:
                    currentBeanSoulRandInf = RAND_INF_DESERT_COLOSSUS_BEAN_SOUL;
                    break;
                case SCENE_GERUDO_VALLEY:
                    currentBeanSoulRandInf = RAND_INF_GERUDO_VALLEY_BEAN_SOUL;
                    break;
                case SCENE_GRAVEYARD:
                    currentBeanSoulRandInf = RAND_INF_GRAVEYARD_BEAN_SOUL;
                    break;
                case SCENE_KOKIRI_FOREST:
                    currentBeanSoulRandInf = RAND_INF_KOKIRI_FOREST_BEAN_SOUL;
                    break;
                case SCENE_LAKE_HYLIA:
                    currentBeanSoulRandInf = RAND_INF_LAKE_HYLIA_BEAN_SOUL;
                    break;
                case SCENE_LOST_WOODS:
                    if (actor->params == 0x4e01) {
                        currentBeanSoulRandInf = RAND_INF_LOST_WOODS_BRIDGE_BEAN_SOUL;
                    } else {
                        currentBeanSoulRandInf = RAND_INF_LOST_WOODS_BEAN_SOUL;
                    }
                    break;
                case SCENE_ZORAS_RIVER:
                    currentBeanSoulRandInf = RAND_INF_ZORAS_RIVER_BEAN_SOUL;
                    break;
            }
        }
        if (currentBeanSoulRandInf != RAND_INF_MAX && !Flags_GetRandomizerInf(currentBeanSoulRandInf)) {
            Actor_Kill(actor);
            return;
        }
    }

    // If child is in the adult shooting gallery or adult in the child shooting gallery, then despawn the shooting
    // gallery man
    if (actor->id == ACTOR_EN_SYATEKI_MAN && RAND_GET_OPTION(RSK_SHUFFLE_INTERIOR_ENTRANCES) &&
        ((LINK_IS_CHILD &&
          // Kakariko Village -> Adult Shooting Gallery, index 003B in the entrance table
          Entrance_SceneAndSpawnAre(SCENE_SHOOTING_GALLERY, 0x00)) ||
         (LINK_IS_ADULT &&
          // Market -> Child Shooting Gallery,           index 016D in the entrance table
          Entrance_SceneAndSpawnAre(SCENE_SHOOTING_GALLERY, 0x01)))) {
        Actor_Kill(actor);
        return;
    }

    if (actor->id == ACTOR_EN_NB && (actor->params & 0xFF) == NB_TYPE_CRAWLSPACE &&
        !RAND_GET_OPTION(RSK_SHUFFLE_SPEAK)) {
        Actor_Kill(actor);
    }

    // Turn MQ switch into toggle
    if (actor->id == ACTOR_OBJ_SWITCH && gPlayState->sceneNum == SCENE_BOTTOM_OF_THE_WELL &&
        (actor->params & 0x3f07) == 0x303) {
        auto dungeon =
            OTRGlobals::Instance->gRandoContext->GetDungeons()->GetDungeonFromScene(SCENE_BOTTOM_OF_THE_WELL);
        if (dungeon->IsMQ()) {
            actor->params |= 0x10;
        }
    }

    // In ER, once Link has spawned we know the scene has loaded, so we can sanitize the last known entrance type
    if (actor->id == ACTOR_PLAYER && RAND_GET_OPTION(RSK_SHUFFLE_ENTRANCES)) {
        Grotto_SanitizeEntranceType();
    }
}

void RandomizerOnGameFrameUpdateHandler() {
    if (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_QUIVER)) {
        AMMO(ITEM_BOW) = static_cast<int8_t>(CUR_CAPACITY(UPG_QUIVER));
    }

    if (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_BOMB_BAG)) {
        AMMO(ITEM_BOMB) = static_cast<int8_t>(CUR_CAPACITY(UPG_BOMB_BAG));
    }

    if (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_BULLET_BAG)) {
        AMMO(ITEM_SLINGSHOT) = static_cast<int8_t>(CUR_CAPACITY(UPG_BULLET_BAG));
    }

    if (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_STICK_UPGRADE)) {
        AMMO(ITEM_STICK) = static_cast<int8_t>(CUR_CAPACITY(UPG_STICKS));
    }

    if (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_NUT_UPGRADE)) {
        AMMO(ITEM_NUT) = static_cast<int8_t>(CUR_CAPACITY(UPG_NUTS));
    }

    if (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER)) {
        gSaveContext.magic = static_cast<int8_t>(gSaveContext.magicCapacity);
    }

    if (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_BOMBCHUS)) {
        AMMO(ITEM_BOMBCHU) = 50;
    }

    if (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_MONEY)) {
        gSaveContext.rupees = static_cast<s16>(CUR_CAPACITY(UPG_WALLET));
    }

    if (!Flags_GetRandomizerInf(RAND_INF_HAS_WALLET)) {
        gSaveContext.rupees = 0;
    }
}

extern "C" void func_8099485C(DoorGerudo* gerudoDoor, PlayState* play);

void RandomizerOnActorUpdateHandler(void* refActor) {
    Actor* actor = static_cast<Actor*>(refActor);

    if (Flags_GetRandomizerInf(RAND_INF_HAS_SKELETON_KEY)) {
        if (actor->id == ACTOR_EN_DOOR) {
            EnDoor* door = reinterpret_cast<EnDoor*>(actor);
            door->lockTimer = 0;
        } else if (actor->id == ACTOR_DOOR_SHUTTER) {
            DoorShutter* shutterDoor = reinterpret_cast<DoorShutter*>(actor);
            if (shutterDoor->doorType == SHUTTER_KEY_LOCKED) {
                shutterDoor->unk_16E = 0;
            }
        } else if (actor->id == ACTOR_DOOR_GERUDO) {
            DoorGerudo* gerudoDoor = reinterpret_cast<DoorGerudo*>(actor);
            gerudoDoor->actionFunc = func_8099485C;
            gerudoDoor->dyna.actor.world.pos.y = gerudoDoor->dyna.actor.home.pos.y + 200.0f;
        }
    }

    if (actor->id == ACTOR_BG_JYA_BIGMIRROR) {
        auto jyaBigMirror = reinterpret_cast<BgJyaBigmirror*>(actor);
        if ((jyaBigMirror->puzzleFlags & (BIGMIR_PUZZLE_COBRA1_SOLVED | BIGMIR_PUZZLE_COBRA2_SOLVED)) ==
            (BIGMIR_PUZZLE_COBRA1_SOLVED | BIGMIR_PUZZLE_COBRA2_SOLVED)) {
            Flags_SetRandomizerInf(RAND_INF_SPIRIT_BIG_MIRROR_STATUE_TURNED);
        } else {
            Flags_UnsetRandomizerInf(RAND_INF_SPIRIT_BIG_MIRROR_STATUE_TURNED);
        }
    }

    // In ER, override the warp song locations. Also removes the warp song cutscene
    if (RAND_GET_OPTION(RSK_SHUFFLE_ENTRANCES) && actor->id == ACTOR_DEMO_KANKYO &&
        actor->params == 0x000F) { // Warp Song particles
        Entrance_SetWarpSongEntrance();
    }
}

// from z_player.c
typedef struct {
    /* 0x00 */ Vec3f pos;
    /* 0x0C */ s16 yaw;
} SpecialRespawnInfo; // size = 0x10

// special respawns used when voided out without swim to prevent infinite loops
std::unordered_map<s32, SpecialRespawnInfo> swimSpecialRespawnInfo = {
    { ENTR_ZORAS_RIVER_3, // hf to zr in water
      { { -1455.443f, -20.0f, 1384.826f }, 28761 } },
    { ENTR_HYRULE_FIELD_14, // zr to hf in water
      { { 5730.209f, -20.0f, 3725.911f }, -20025 } },
    { ENTR_LOST_WOODS_UNDERWATER_SHORTCUT, // zr to lw
      { { 1978.718f, -36.908f, -855.0f }, -16384 } },
    { ENTR_ZORAS_RIVER_UNDERWATER_SHORTCUT, // lw to zr
      { { 4082.366f, 860.442f, -1018.949f }, -32768 } },
    { ENTR_LAKE_HYLIA_RIVER_EXIT, // gv to lh
      { { -3276.416f, -1033.0f, 2908.421f }, 11228 } },
    { ENTR_WATER_TEMPLE_ENTRANCE, // lh to water temple
      { { -182.0f, 780.0f, 759.5f }, -32768 } },
    { ENTR_LAKE_HYLIA_OUTSIDE_TEMPLE, // water temple to lh
      { { -955.028f, -1306.9f, 6768.954f }, -32768 } },
    { ENTR_ZORAS_DOMAIN_UNDERWATER_SHORTCUT, // lh to zd
      { { -109.86f, 11.396f, -9.933f }, -29131 } },
    { ENTR_LAKE_HYLIA_UNDERWATER_SHORTCUT, // zd to lh
      { { -912.0f, -1326.967f, 3391.0f }, 0 } },
    { ENTR_GERUDO_VALLEY_1, // caught by gerudos as child
      { { -424.0f, -2051.0f, -74.0f }, 16384 } },
    { ENTR_HYRULE_FIELD_ON_BRIDGE_SPAWN, // mk to hf (can be a problem when it then turns night)
      { { 0.0f, 0.0f, 1100.0f }, 0 } },
    { ENTR_ZORAS_FOUNTAIN_JABU_JABU_BLUE_WARP, // jabu blue warp to zf
      { { -1580.0f, 150.0f, 1670.0f }, 8000 } },
};

f32 triforcePieceScale;

void RandomizerOnPlayerUpdateHandler() {
    if ((GET_PLAYER(gPlayState)->stateFlags1 & PLAYER_STATE1_IN_WATER) && !Flags_GetRandomizerInf(RAND_INF_CAN_SWIM) &&
        CUR_EQUIP_VALUE(EQUIP_TYPE_BOOTS) != EQUIP_VALUE_BOOTS_IRON) {
        // if you void out in water temple without swim you get instantly kicked out to prevent softlocks
        if (gPlayState->sceneNum == SCENE_WATER_TEMPLE) {
            GameInteractor::RawAction::TeleportPlayer(
                Entrance_OverrideNextIndex(ENTR_LAKE_HYLIA_OUTSIDE_TEMPLE)); // lake hylia from water temple
        } else {
            auto respawn = swimSpecialRespawnInfo.find(gSaveContext.entranceIndex);
            if (respawn != swimSpecialRespawnInfo.end()) {
                Play_SetupRespawnPoint(gPlayState, RESPAWN_MODE_DOWN, 0xDFF);
                if (gPlayState->sceneNum == gEntranceTable[gSaveContext.entranceIndex].scene) {
                    gSaveContext.respawn[RESPAWN_MODE_DOWN].roomIndex =
                        gPlayState->setupEntranceList[gEntranceTable[gSaveContext.entranceIndex].spawn].room;
                }
                gSaveContext.respawn[RESPAWN_MODE_DOWN].pos = respawn->second.pos;
                gSaveContext.respawn[RESPAWN_MODE_DOWN].yaw = respawn->second.yaw;
            }

            Play_TriggerVoidOut(gPlayState);
        }
    }

    // Triforce Hunt needs the check if the player isn't being teleported to the credits scene.
    if (!GameInteractor::IsGameplayPaused() && Flags_GetRandomizerInf(RAND_INF_GRANT_GANONS_BOSSKEY) &&
        gPlayState->transitionTrigger != TRANS_TRIGGER_START &&
        (1 << 0 & gSaveContext.inventory.dungeonItems[SCENE_GANONS_TOWER]) == 0) {
        GiveItemEntryWithoutActor(gPlayState,
                                  *Rando::StaticData::GetItemTable().at(RG_GANONS_CASTLE_BOSS_KEY).GetGIEntry());
    }

    if (!GameInteractor::IsGameplayPaused() && RAND_GET_OPTION(RSK_TRIFORCE_HUNT).IsNot(RO_TRIFORCE_HUNT_OFF)) {
        // Warp to credits
        if (GameInteractor::State::TriforceHuntCreditsWarpActive) {
            gPlayState->nextEntranceIndex = ENTR_CHAMBER_OF_THE_SAGES_0;
            gSaveContext.nextCutsceneIndex = 0xFFF2;
            gPlayState->transitionTrigger = TRANS_TRIGGER_START;
            gPlayState->transitionType = TRANS_TYPE_FADE_WHITE;
            GameInteractor::State::TriforceHuntCreditsWarpActive = 0;
        }

        // Reset Triforce Piece scale for GI animation. Triforce Hunt allows for multiple triforce models,
        // and cycles through them based on the amount of triforce pieces collected. It takes a little while
        // for the count to increase during the GI animation, so the model is entirely hidden until that piece
        // has been added. That scale has to be reset after the textbox is closed, and this is the best way
        // to ensure it's done at that point in time specifically.
        if (GameInteractor::State::TriforceHuntPieceGiven) {
            triforcePieceScale = 0.0f;
            GameInteractor::State::TriforceHuntPieceGiven = 0;
        }
    }
}

void RandomizerOnSceneSpawnActorsHandler() {
    if (LINK_IS_ADULT && RAND_GET_OPTION(RSK_SHEIK_LA_HINT)) {
        switch (gPlayState->sceneNum) {
            case SCENE_TEMPLE_OF_TIME:
                if (gPlayState->roomCtx.curRoom.num == 1) {
                    Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_XC, -104, -40, 2382, 0,
                                static_cast<int16_t>(0x8000), 0, SHEIK_TYPE_RANDO);
                }
                break;
            case SCENE_INSIDE_GANONS_CASTLE:
                if (gPlayState->roomCtx.curRoom.num == 1) {
                    Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_XC, 101, 150, 137, 0, 0, 0,
                                SHEIK_TYPE_RANDO);
                }
                break;
            default:
                break;
        }
    }
}

void RandomizerOnPlayDestroyHandler() {
    // In ER, remove link from epona when entering somewhere that doesn't support epona
    if (RAND_GET_OPTION(RSK_SHUFFLE_OVERWORLD_ENTRANCES)) {
        Entrance_HandleEponaState();
    }
}

void RandomizerOnExitGameHandler(int32_t fileNum) {
    // When going from a rando save to a vanilla save within the same game instance
    // we need to reset the entrance table back to its vanilla state
    Entrance_ResetEntranceTable();
}

void RandomizerOnKaleidoscopeUpdateHandler(int16_t inDungeonScene) {
    static uint16_t prevKaleidoState = 0;

    // In ER, handle overriding the game over respawn entrance and dealing with death warp to from grottos
    if (RAND_GET_OPTION(RSK_SHUFFLE_ENTRANCES)) {
        if (prevKaleidoState == 0x10 && gPlayState->pauseCtx.state == 0x11 && gPlayState->pauseCtx.promptChoice == 0) {
            // Needs to be called before Play_TriggerRespawn when transitioning from state 0x10 to 0x11
            Entrance_SetGameOverEntrance();
        }
        if (prevKaleidoState == 0x11 && gPlayState->pauseCtx.state == 0 && gPlayState->pauseCtx.promptChoice == 0) {
            // Needs to be called after Play_TriggerRespawn when transitioning from state 0x11 to 0
            Grotto_ForceGrottoReturn();
        }
    }

    prevKaleidoState = gPlayState->pauseCtx.state;
}

void RandomizerOnCuccoOrChickenHatch() {
    if (LINK_IS_CHILD) {
        Flags_UnsetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_WEIRD_EGG);
        Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_CHICKEN);
    }
}

static void RandomizerRegisterHooks() {
    static uint32_t onFlagSetHook = 0;
    static uint32_t onSceneFlagSetHook = 0;
    static uint32_t onPlayerUpdateForRCQueueHook = 0;
    static uint32_t onPlayerUpdateForItemQueueHook = 0;
    static uint32_t onItemReceiveHook = 0;
    static uint32_t onDialogMessageHook = 0;
    static uint32_t onVanillaBehaviorHook = 0;
    static uint32_t onSceneInitHook = 0;
    static uint32_t afterSceneCommandsHook = 0;
    static uint32_t onActorInitHook = 0;
    static uint32_t onActorUpdateHook = 0;
    static uint32_t onPlayerUpdateHook = 0;
    static uint32_t onGameFrameUpdateHook = 0;
    static uint32_t onSceneSpawnActorsHook = 0;
    static uint32_t onPlayDestroyHook = 0;
    static uint32_t onExitGameHook = 0;
    static uint32_t onKaleidoUpdateHook = 0;
    static uint32_t onCuccoOrChickenHatchHook = 0;

    // register this outside OnLoadGame as VB is invoked before OnLoadGame
    COND_VB_SHOULD(VB_REVERT_SPOILING_ITEMS, true, {
        if (IS_RANDO && RAND_GET_OPTION(RSK_SHUFFLE_ADULT_TRADE)) {
            *should = false;
        }
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t fileNum) {
        ShipInit::Init("IS_RANDO");

        randomizerQueuedChecks = std::queue<RandomizerCheck>();
        randomizerQueuedCheck = RC_UNKNOWN_CHECK;
        randomizerQueuedItemEntry = GET_ITEM_NONE;
        randomizerQueuedItemGiveStarted = false;
        randomizerQueuedEmitWindowMessage = false;

        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnFlagSet>(onFlagSetHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnSceneFlagSet>(onSceneFlagSetHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnPlayerUpdate>(onPlayerUpdateForRCQueueHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnPlayerUpdate>(onPlayerUpdateForItemQueueHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnItemReceive>(onItemReceiveHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnItemReceive>(onDialogMessageHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnVanillaBehavior>(onVanillaBehaviorHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnSceneInit>(onSceneInitHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::AfterSceneCommands>(afterSceneCommandsHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnActorInit>(onActorInitHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnActorUpdate>(onActorUpdateHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnPlayerUpdate>(onPlayerUpdateHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnGameFrameUpdate>(onGameFrameUpdateHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnSceneSpawnActors>(onSceneSpawnActorsHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnPlayDestroy>(onPlayDestroyHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnExitGame>(onExitGameHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnKaleidoscopeUpdate>(onKaleidoUpdateHook);
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnCuccoOrChickenHatch>(onCuccoOrChickenHatchHook);

        onFlagSetHook = 0;
        onSceneFlagSetHook = 0;
        onPlayerUpdateForRCQueueHook = 0;
        onPlayerUpdateForItemQueueHook = 0;
        onItemReceiveHook = 0;
        onDialogMessageHook = 0;
        onVanillaBehaviorHook = 0;
        onSceneInitHook = 0;
        afterSceneCommandsHook = 0;
        onActorInitHook = 0;
        onActorUpdateHook = 0;
        onPlayerUpdateHook = 0;
        onGameFrameUpdateHook = 0;
        onSceneSpawnActorsHook = 0;
        onPlayDestroyHook = 0;
        onExitGameHook = 0;
        onKaleidoUpdateHook = 0;
        onCuccoOrChickenHatchHook = 0;

        if (!IS_RANDO)
            return;

        // ENTRTODO: Move all entrance rando handling to a dedicated file
        // Setup the modified entrance table and entrance shuffle table for rando
        Entrance_Init();
        if (CVarGetInteger("gRandomizer.Combo.Logic.DungeonEntrances", 0) != 0) {
            CVarSetInteger(CVAR_RANDOMIZER_SETTING("ShuffleDungeonsEntrances"), RO_DUNGEON_ENTRANCE_SHUFFLE_OFF);
            Entrance_ResetEntranceTable();
        }

        // Handle randomized spawn positions after the save context has been setup from load
        if (RAND_GET_OPTION(RSK_SHUFFLE_ENTRANCES)) {
            Entrance_SetSavewarpEntrance();
        }

        onFlagSetHook =
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnFlagSet>(RandomizerOnFlagSetHandler);
        onSceneFlagSetHook =
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneFlagSet>(RandomizerOnSceneFlagSetHandler);
        onPlayerUpdateForRCQueueHook = GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(
            RandomizerOnPlayerUpdateForRCQueueHandler);
        onPlayerUpdateForItemQueueHook = GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(
            RandomizerOnPlayerUpdateForItemQueueHandler);
        onItemReceiveHook =
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnItemReceive>(RandomizerOnItemReceiveHandler);
        onDialogMessageHook = GameInteractor::Instance->RegisterGameHook<GameInteractor::OnDialogMessage>(
            RandomizerOnDialogMessageHandler);
        onVanillaBehaviorHook = GameInteractor::Instance->RegisterGameHook<GameInteractor::OnVanillaBehavior>(
            RandomizerOnVanillaBehaviorHandler);
        onSceneInitHook =
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(RandomizerOnSceneInitHandler);
        afterSceneCommandsHook = GameInteractor::Instance->RegisterGameHook<GameInteractor::AfterSceneCommands>(
            RandomizerAfterSceneCommandsHandler);
        onActorInitHook =
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorInit>(RandomizerOnActorInitHandler);
        onActorUpdateHook =
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>(RandomizerOnActorUpdateHandler);
        onPlayerUpdateHook =
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(RandomizerOnPlayerUpdateHandler);
        onGameFrameUpdateHook = GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(
            RandomizerOnGameFrameUpdateHandler);
        onSceneSpawnActorsHook = GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneSpawnActors>(
            RandomizerOnSceneSpawnActorsHandler);
        onPlayDestroyHook =
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>(RandomizerOnPlayDestroyHandler);
        onExitGameHook =
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>(RandomizerOnExitGameHandler);
        onKaleidoUpdateHook = GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoscopeUpdate>(
            RandomizerOnKaleidoscopeUpdateHandler);
        onCuccoOrChickenHatchHook = GameInteractor::Instance->RegisterGameHook<GameInteractor::OnCuccoOrChickenHatch>(
            RandomizerOnCuccoOrChickenHatch);

        if (RAND_GET_OPTION(RSK_FISHSANITY).IsNot(RO_FISHSANITY_OFF)) {
            OTRGlobals::Instance->gRandoContext->GetFishsanity()->InitializeFromSave();
        }
    });
}

static RegisterShipInitFunc initFunc_RegisterHooks(RandomizerRegisterHooks);
