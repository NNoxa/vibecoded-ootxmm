#include "TransformMasks.h"

#include "global.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_En_Boom/z_en_boom.h"
#include "soh/Crossover/Crossover.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/SohGui/ImGuiUtils.h"
#include "soh/resource/type/AudioSoundFont.h"
#include "soh/resource/type/PlayerAnimation.h"
#include "soh/resource/type/Skeleton.h"
#include <fast/resource/type/DisplayList.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/window/Window.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <spdlog/spdlog.h>

namespace {

extern "C" void Player_Action_Roll(Player* player, PlayState* play);
extern "C" void Player_SetupRoll(Player* player, PlayState* play);
extern "C" s16 func_8084ABD8(PlayState* play, Player* player, s32 arg2, s16 arg3);
extern "C" void Player_SetBootData(PlayState* play, Player* player);
extern "C" s32 Player_GetMovementSpeedAndYaw(Player* player, f32* outSpeedTarget, s16* outYawTarget, f32 speedMode,
                                             PlayState* play);

bool IsUsingOcarina(const Player* player) {
    if (player == nullptr) {
        return false;
    }

    const bool actionIsOcarina =
        player->itemAction == PLAYER_IA_OCARINA_FAIRY || player->itemAction == PLAYER_IA_OCARINA_OF_TIME ||
        player->heldItemAction == PLAYER_IA_OCARINA_FAIRY || player->heldItemAction == PLAYER_IA_OCARINA_OF_TIME;
    const bool itemIsOcarina = player->heldItemId == ITEM_OCARINA_FAIRY || player->heldItemId == ITEM_OCARINA_TIME;
    const bool stateIsOcarina = (player->stateFlags2 & PLAYER_STATE2_OCARINA_PLAYING) != 0;

    return actionIsOcarina || itemIsOcarina || stateIsOcarina;
}

constexpr std::array<s16, 10> kMaskCycleItems = {
    ITEM_MASK_KEATON, ITEM_MASK_SKULL,         ITEM_MASK_SPOOKY, ITEM_MASK_BUNNY, ITEM_MASK_GORON,
    ITEM_MASK_ZORA,   ITEM_MASK_GERUDO,        ITEM_MASK_TRUTH,  ITEM_MASK_DEKU,  ITEM_MASK_FIERCE_DEITY,
};

constexpr s32 LINK_DEKU_LIMB_HEAD = 10;
constexpr s32 LINK_GORON_LIMB_TORSO = 20;
constexpr s32 LINK_ZORA_LIMB_LEFT_HAND = 15;
constexpr size_t kMaxOcarinaDisplayLists = 8;
constexpr const char* kDekuMaskIconPath = "icon_item_static_yar/gItemIconDekuMaskTex";
constexpr const char* kDekuMaskIconPathOtr = "__OTR__icon_item_static_yar/gItemIconDekuMaskTex";
constexpr const char* kFierceDeityMaskIconPath = "icon_item_static_yar/gItemIconFierceDeityMaskTex";
constexpr const char* kFierceDeityMaskIconPathOtr = "__OTR__icon_item_static_yar/gItemIconFierceDeityMaskTex";
constexpr const char* kDekuMaskItemNamePath = "item_name_static/gItemNameDekuMaskENGTex";
constexpr const char* kDekuMaskItemNamePathOtr = "__OTR__item_name_static/gItemNameDekuMaskENGTex";
constexpr const char* kFierceDeityMaskItemNamePath = "item_name_static/gItemNameFierceDeitysMaskENGTex";
constexpr const char* kFierceDeityMaskItemNamePathOtr = "__OTR__item_name_static/gItemNameFierceDeitysMaskENGTex";
constexpr f32 kZoraSurfaceDepth = 36.0f;
constexpr f32 kZoraDeepThreshold = 68.0f;
constexpr f32 kZoraSwimReentryThreshold = 30.0f;
constexpr f32 kDekuBubbleMinScale = 3.5f;
constexpr f32 kDekuBubbleMaxCharge = 16.0f;
constexpr f32 kDekuWaterHopBaseSpeed = 8.0f;
constexpr f32 kDekuWaterHopSurfaceYOffset = 34.0f;
constexpr f32 kDekuSpinInitialSpeed = 20000.0f;
constexpr f32 kDekuSpinSpeedDecay = 800.0f;
constexpr f32 kDekuSpinInitialTimer = 196608.0f;
constexpr f32 kDekuSpinMovementScale = 6.0f;
constexpr f32 kDekuSpinMovementMinMultiplier = 0.78f;
constexpr f32 kDekuSpinDrawYOffset = -5.5f;
constexpr f32 kTransformSpeedModeCurved = 0.018f;

struct OcarinaInstrumentTransform {
    f32 translateX;
    f32 translateY;
    f32 translateZ;
    f32 rotateX;
    f32 rotateY;
    f32 rotateZ;
};

constexpr OcarinaInstrumentTransform kDekuInstrumentTransform = {
    4.0f, 22.0f, -18.0f, 0.0f, M_PI / 2.0f, M_PI / 2.0f,
};

constexpr OcarinaInstrumentTransform kGoronInstrumentTransform = {
    0.0f, -4.0f, 12.0f, -M_PI / 2.0f, M_PI, 0.0f,
};

constexpr OcarinaInstrumentTransform kZoraInstrumentTransform = {
    -6.0f, 0.0f, 12.0f, 0.0f, M_PI / 2.0f, -M_PI / 2.0f,
};

constexpr f32 kGoronRollingShapeYOffset = 1800.0f;
constexpr f32 kGoronRollBodyScaleX = 0.94f;
constexpr f32 kGoronRollBodyScaleY = 1.34f;
constexpr f32 kGoronRollBodyScaleZ = 0.94f;
constexpr f32 kGoronHeatSpeedThreshold = 11.5f;
constexpr f32 kGoronSpikeSpeedThreshold = 15.75f;
constexpr s16 kGoronHeatVelocityFrames = 5;
constexpr s16 kGoronSpikeVelocityFrames = 0x36;
constexpr s16 kGoronSpikeSpinThreshold = 0x36B0;
constexpr s8 kGoronRollStickDeadzone = 12;
constexpr s8 kGoronRollStartStickYThreshold = 20;
constexpr u16 kMmSfxGoronToBall = 0x8E6;
constexpr u16 kMmSfxBallToGoron = 0x8E7;
constexpr u16 kMmSfxGoronBallCharge = 0x8EB;
constexpr u16 kMmSfxGoronChargedRoll = 0x980;
constexpr u16 kMmSfxGoronRoll = 0x990;
constexpr u16 kMmSfxGoronBallChargeDash = 0x9A3;
constexpr u16 kMmSfxGoronSpikeTransition = 0x9A2;
constexpr u16 kMmSfxDekuBubbleBreath = 0x9A1;
constexpr u16 kMmSfxDekuSpinAttack = 0x9A9;
constexpr u16 kMmSfxDekuJumpBase = 0x9B0;
constexpr u16 kMmSfxDekuBubbleFire = 0x08E0;
constexpr u16 kMmSfxDekuBubbleMove = 0x185A;
constexpr u16 kMmSfxTransformMaskFlash = 0x484F;
constexpr u16 kMmCarrierSfxGoronToBall = 0x8E6;
constexpr u16 kMmCarrierSfxBallToGoron = 0x8E7;
constexpr u16 kMmCarrierSfxGoronRollA = 0x8E8;
constexpr u16 kMmCarrierSfxGoronRollB = 0x8E9;
constexpr u16 kMmCarrierSfxGoronHeat = 0x8EA;
constexpr u16 kMmCarrierSfxGoronSpikeA = 0x8EB;
constexpr u16 kMmCarrierSfxGoronSpikeB = 0x8EC;
constexpr u16 kMmCarrierSfxGoronHeatStart = 0x8ED;
constexpr u16 kMmCarrierSfxGoronSpikeStart = 0x8EE;
constexpr u16 kMmSfxIndexMask = 0x3FF;
constexpr s32 kMmDirectGoronAudioFontId = 0;

struct MmGoronCarrierPatchEntry {
    u16 carrierSfxId;
    u8 instrumentIndex;
    u8 semitone;
    const char* debugName;
};

constexpr std::array<MmGoronCarrierPatchEntry, 9> kMmGoronCarrierPatchEntries = {{
    { kMmCarrierSfxGoronToBall, 47, 35, "to-ball" },
    { kMmCarrierSfxBallToGoron, 47, 31, "ball-to-goron" },
    { kMmCarrierSfxGoronRollA, 77, 58, "roll-a" },
    { kMmCarrierSfxGoronRollB, 47, 41, "roll-b" },
    { kMmCarrierSfxGoronHeat, 111, 60, "heat-loop" },
    { kMmCarrierSfxGoronSpikeA, 35, 65, "spike-a" },
    { kMmCarrierSfxGoronSpikeB, 75, 64, "spike-b" },
    { kMmCarrierSfxGoronHeatStart, 47, 67, "heat-start" },
    { kMmCarrierSfxGoronSpikeStart, 75, 36, "spike-start" },
}};

bool sMmGoronSfxPatchAttempted = false;
bool sMmGoronSfxPatchApplied = false;
struct MmGoronSfxState {
    bool patchApplied = false;
    s32 patchRetryCount = 0;
    s16 activeLoopStage = 0;
    f32 rollFreqA = 1.0f;
    f32 rollFreqB = 1.0f;
    f32 heatFreq = 1.0f;
    f32 spikeFreqA = 1.0f;
    f32 spikeFreqB = 1.0f;
    f32 oneShotFreq = 1.0f;
    f32 rollVolume = 0.85f;
    f32 heatVolume = 0.62f;
    f32 spikeVolume = 0.85f;
    f32 oneShotVolume = 0.85f;
};

MmGoronSfxState sMmGoronSfxState;

enum MmGoronVoiceSlot : size_t {
    MM_GORON_VOICE_TO_BALL = 0,
    MM_GORON_VOICE_TO_GORON,
    MM_GORON_VOICE_ROLL_A,
    MM_GORON_VOICE_ROLL_B,
    MM_GORON_VOICE_HEAT_LOOP,
    MM_GORON_VOICE_SPIKE_A,
    MM_GORON_VOICE_SPIKE_B,
    MM_GORON_VOICE_HEAT_START,
    MM_GORON_VOICE_SPIKE_START,
    MM_GORON_VOICE_ATTACK_N,
    MM_GORON_VOICE_ATTACK_L,
    MM_GORON_VOICE_AUTO_JUMP,
    MM_GORON_VOICE_CLIMB_END,
    MM_GORON_VOICE_FOOTSTEP,
    MM_GORON_VOICE_COUNT,
};

struct MmGoronVoiceDefinition {
    const char* debugName;
    u16 mmSfxId;
    u16 instrumentIndex;
    u8 note;
    bool looped;
    bool useSoundEffect;
    f32 pitchMultiplier;
    f32 velocity;
    s32 oneShotFrames;
};

constexpr std::array<MmGoronVoiceDefinition, MM_GORON_VOICE_COUNT> kMmGoronVoiceDefinitions = {{
    { "to-ball", kMmSfxGoronToBall, 47, 35, false, false, 1.0f, 0.95f, 12 },
    { "to-goron", kMmSfxBallToGoron, 47, 31, false, false, 1.0f, 0.95f, 12 },
    { "roll-a", kMmSfxGoronRoll, 77, 58, true, false, 0.891f, 0.85f, 0 },
    { "roll-b", kMmSfxGoronRoll, 47, 41, true, false, 0.334f, 0.75f, 0 },
    { "heat-loop", kMmSfxGoronBallChargeDash, 111, 60, true, false, 1.0f, 0.62f, 0 },
    { "spike-a", kMmSfxGoronBallCharge, 35, 65, true, false, 21.357f, 0.85f, 0 },
    { "spike-b", kMmSfxGoronBallCharge, 75, 64, true, false, 1.414f, 0.85f, 0 },
    { "heat-start", kMmSfxGoronChargedRoll, 47, 67, false, false, 1.0f, 0.9f, 14 },
    { "spike-start", kMmSfxGoronSpikeTransition, 75, 36, false, false, 1.0f, 0.9f, 14 },
    { "voice-attack-n", 0x68C0, 192, 60, false, true, 1.0f, 2.0f, 28 },
    { "voice-attack-l", 0x68C1, 193, 60, false, true, 1.0f, 2.0f, 30 },
    { "voice-auto-jump", 0x68D4, 212, 60, false, true, 1.0f, 2.0f, 30 },
    { "voice-climb-end", 0x68C4, 196, 60, false, true, 1.0f, 1.8f, 28 },
    { "footstep", 0x0812, 2, 54, false, false, 1.0f, 0.72f, 8 },
}};

struct MmGoronDirectVoice {
    SequenceChannel channel;
    SequenceLayer layer;
    bool initialized = false;
    bool active = false;
    bool looped = false;
    s32 oneShotFramesRemaining = 0;
    u16 instrumentIndex = 0;
    u8 note = 0;
    const char* debugName = nullptr;
};

struct MmGoronDirectAudioState {
    bool initialized = false;
    bool fontReady = false;
    s32 initAttempts = 0;
    SOH::SoundFont* mmFont = nullptr;
    std::array<MmGoronDirectVoice, MM_GORON_VOICE_COUNT> voices;
};

MmGoronDirectAudioState sMmGoronDirectAudioState;

constexpr s32 kMmVoicePcmMaxSounds = 16;
constexpr s32 kVadpcmFrameAdpcm = 9;
constexpr s32 kVadpcmFrameSmallAdpcm = 5;
constexpr s32 kVadpcmSamplesPerFrame = 16;
constexpr f32 kMmGoronVoicePcmVolumeScale = 0.36f;
constexpr u16 kMmVoicePcmKeyNone = 0xFFFF;
constexpr u16 kMmVoicePcmKeyGoronBase = 0x6C00;
constexpr u16 kMmVoicePcmKeyZoraBase = 0x6A00;
constexpr u16 kMmVoicePcmKeyZoraSwimDashBase = 0x6A40;
constexpr u16 kMmVoicePcmKeyZoraBarrierBase = 0x6A50;
constexpr u16 kMmVoicePcmKeyZoraStepBase = 0x6A60;
constexpr u16 kMmVoicePcmKeyDekuSpinBase = 0x6B00;
constexpr u16 kMmVoicePcmKeyDekuHopBase = 0x6B10;
constexpr u16 kMmVoicePcmKeyDekuBubbleBase = 0x6B20;
constexpr u16 kMmVoicePcmKeyDekuVoiceBase = 0x6B40;
constexpr u16 kMmVoicePcmKeyDekuStepBase = 0x6B70;
constexpr u16 kMmVoicePcmKeyTransformFlash = 0x6BF0;

struct MmVoicePcmSound {
    s16* pcmData = nullptr;
    u32 pcmLength = 0;
    f32 pcmPosition = 0.0f;
    f32 advance = 1.0f;
    f32 volume = 0.85f;
    f32 envVolume = 0.0f;
    f32 envReleaseAt = 0.0f;
    bool release = false;
    bool loop = false;
    bool active = false;
    u16 key = kMmVoicePcmKeyNone;
};

std::array<MmVoicePcmSound, kMmVoicePcmMaxSounds> sMmVoicePcmSounds;

bool EnsureMmGoronDirectAudioReady();
void EnsureMmGoronSfxPatched();
::SoundFontSound* GetMmGoronDirectSound(MmGoronVoiceSlot slot);
bool PlayMmInstrumentPcm(u16 key, const char* debugName, u16 mmSfxId, u8 instrumentIndex, u8 note, s8 transpose,
                         f32 volume, bool loop);
bool PlayMmTrianglePcm(u16 key, const char* debugName, u16 mmSfxId, u8 note, s8 transpose, f32 volume, bool loop);
bool PlayMmSfxPcmById(u16 key, const char* debugName, u16 mmSfxId, f32 volume, bool loop = false,
                      f32 pitchScale = 1.0f);
bool PlayMmDekuVoiceOneShot(u16 mmSfxId, f32 pitchScale = 1.0f);
void StopMmPcmByKey(u16 key);
void StopMmPcmKeyRange(u16 baseKey, s32 count);

extern "C" {
extern char** fontMap;
extern size_t fontMapSize;
}

const OcarinaInstrumentTransform* GetOcarinaInstrumentTransform(u8 playerMask) {
    switch (playerMask) {
        case PLAYER_MASK_DEKU:
            return &kDekuInstrumentTransform;
        case PLAYER_MASK_GORON:
            return &kGoronInstrumentTransform;
        case PLAYER_MASK_ZORA:
            return &kZoraInstrumentTransform;
        default:
            return nullptr;
    }
}


struct TransformMaskFormDefinition {
    u8 playerMask;
    f32 drawScale;
    f32 drawYOffset;
    f32 ocarinaDrawYOffset;
    f32 ledgeReachBonus;
    f32 ledgeCheckHeightBonus;
    f32 colliderHeight;
    f32 colliderRadius;
    f32 colliderYShift;
    f32 colliderHeightScale;
    f32 colliderRadiusScale;
    f32 colliderMinHeight;
    f32 colliderMinRadius;
    f32 colliderYShiftOffset;
    const char* skeletonPath;
    const char* skeletonPathOtr;
    const char* eyeTexturePath;
    const char* eyeTexturePathOtr;
    const char* mouthTexturePath;
    const char* mouthTexturePathOtr;
    const char* idleAnimPath;
    const char* idleAnimPathOtr;
    s16 idleFrameCount;
    const char* walkAnimPath;
    const char* walkAnimPathOtr;
    s16 walkFrameCount;
    const char* runAnimPath;
    const char* runAnimPathOtr;
    s16 runFrameCount;
    const char* attackAnimPath;
    const char* attackAnimPathOtr;
    s16 attackFrameCount;
    const char* attackAnimPath2;
    const char* attackAnimPath2Otr;
    s16 attackFrameCount2;
    const char* attackAnimPath3;
    const char* attackAnimPath3Otr;
    s16 attackFrameCount3;
    const char* spinAnimPath;
    const char* spinAnimPathOtr;
    s16 spinFrameCount;
    const char* jumpAnimPath;
    const char* jumpAnimPathOtr;
    s16 jumpFrameCount;
    const char* hopAnimPath;
    const char* hopAnimPathOtr;
    s16 hopFrameCount;
    const char* climbStartAnimPath;
    const char* climbStartAnimPathOtr;
    s16 climbStartFrameCount;
    const char* climbLoopAnimPath;
    const char* climbLoopAnimPathOtr;
    s16 climbLoopFrameCount;
    const char* defendAnimPath;
    const char* defendAnimPathOtr;
    s16 defendFrameCount;
    const char* defendWaitAnimPath;
    const char* defendWaitAnimPathOtr;
    s16 defendWaitFrameCount;
    const char* defendEndAnimPath;
    const char* defendEndAnimPathOtr;
    s16 defendEndFrameCount;
    const char* ocarinaStartAnimPath;
    const char* ocarinaStartAnimPathOtr;
    s16 ocarinaStartFrameCount;
    const char* ocarinaPlayAnimPath;
    const char* ocarinaPlayAnimPathOtr;
    s16 ocarinaPlayFrameCount;
    const char* doorAnimPath;
    const char* doorAnimPathOtr;
    s16 doorFrameCount;
    const char* chestAnimPath;
    const char* chestAnimPathOtr;
    s16 chestFrameCount;
    const char* swimAnimPath;
    const char* swimAnimPathOtr;
    s16 swimFrameCount;
    const char* swimIdleAnimPath;
    const char* swimIdleAnimPathOtr;
    s16 swimIdleFrameCount;
};

void* TryLoadMmRawPointer(const char* primaryPath, const char* otrPath);
void* TryLoadNativeRawPointer(const char* primaryPath, const char* otrPath);
std::shared_ptr<Fast::DisplayList> TryLoadMmDisplayListResource(const char* primaryPath, const char* otrPath);
void PatchDisplayListSegmentRefsToNoOp(std::shared_ptr<Fast::DisplayList> displayList, u8 segment,
                                       const char* debugName);
void LogDisplayListKeyCommands(const std::shared_ptr<Fast::DisplayList>& displayList, const char* debugName);
void ResetRuntimeState();
bool CanUseDekuActionButtons(const Player* player);

constexpr std::array<TransformMaskFormDefinition, 4> kTransformForms = {
    TransformMaskFormDefinition{ PLAYER_MASK_DEKU, 1.0f, -2550.0f, 600.0f, 0.0f, 0.0f, 42.0f, 12.0f, 0.0f, 0.85f, 1.00f, 42.0f, 12.0f, 8.0f,
                                 "objects/object_link_nuts/gLinkDekuSkel",
                                 "__OTR__objects/object_link_nuts/gLinkDekuSkel",
                                 "objects/object_link_nuts/object_link_nuts_Tex_0048B0",
                                 "__OTR__objects/object_link_nuts/object_link_nuts_Tex_0048B0",
                                 "objects/object_link_nuts/object_link_nuts_Tex_0044B0",
                                 "__OTR__objects/object_link_nuts/object_link_nuts_Tex_0044B0",
                                 "misc/link_animetion/gPlayerAnim_link_normal_wait_free_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_wait_free_Data",
                                 72,
                                 "misc/link_animetion/gPlayerAnim_link_normal_walk_free_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_walk_free_Data",
                                 17,
                                 "misc/link_animetion/gPlayerAnim_link_normal_run_free_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_run_free_Data",
                                 17,
                                 "misc/link_animetion/gPlayerAnim_pn_tamahaki_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pn_tamahaki_Data",
                                 8,
                                 nullptr,
                                 nullptr,
                                 0,
                                 nullptr,
                                 nullptr,
                                 0,
                                 "misc/link_animetion/gPlayerAnim_pn_attack_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pn_attack_Data",
                                 2,
                                 "misc/link_animetion/gPlayerAnim_link_normal_jump_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_jump_Data",
                                 8,
                                 "misc/link_animetion/gPlayerAnim_link_normal_fall_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_fall_Data",
                                 20,
                                 "misc/link_animetion/gPlayerAnim_link_normal_jump_climb_hold_free_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_jump_climb_hold_free_Data",
                                 10,
                                 "misc/link_animetion/gPlayerAnim_link_normal_jump_climb_up_free_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_jump_climb_up_free_Data",
                                 29,
                                 "misc/link_animetion/gPlayerAnim_pn_gurd_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pn_gurd_Data",
                                 4,
                                 "misc/link_animetion/gPlayerAnim_pn_gurd_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pn_gurd_Data",
                                 4,
                                 nullptr,
                                 nullptr,
                                 0,
                                 "misc/link_animetion/gPlayerAnim_pn_gakkistart_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pn_gakkistart_Data",
                                 12,
                                 "misc/link_animetion/gPlayerAnim_pn_gakkiplay_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pn_gakkiplay_Data",
                                 8,
                                 "misc/link_animetion/gPlayerAnim_pn_doorA_open_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pn_doorA_open_Data",
                                 100,
                                 "misc/link_animetion/gPlayerAnim_pn_Tbox_open_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pn_Tbox_open_Data",
                                 133,
                                 nullptr,
                                 nullptr,
                                 0,
                                 nullptr,
                                 nullptr,
                                 0 },
    TransformMaskFormDefinition{ PLAYER_MASK_GORON, 0.88f, -760.0f, 0.0f, 24.0f, 24.0f, 62.0f, 24.0f, 0.0f, 1.05f, 1.28f, 62.0f, 24.0f, 0.0f,
                                 "objects/object_link_goron/gLinkGoronSkel",
                                 "__OTR__objects/object_link_goron/gLinkGoronSkel",
                                 "objects/object_link_goron/gLinkGoronEyesOpenTex",
                                 "__OTR__objects/object_link_goron/gLinkGoronEyesOpenTex",
                                 "objects/object_link_goron/object_link_goron_Tex_003A40",
                                 "__OTR__objects/object_link_goron/object_link_goron_Tex_003A40",
                                 "misc/link_animetion/gPlayerAnim_pg_wait_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pg_wait_Data",
                                 79,
                                 "misc/link_animetion/gPlayerAnim_link_normal_walk_free_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_walk_free_Data",
                                 17,
                                 "misc/link_animetion/gPlayerAnim_link_normal_run_free_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_run_free_Data",
                                 17,
                                 "misc/link_animetion/gPlayerAnim_pg_punchA_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pg_punchA_Data",
                                 12,
                                 "misc/link_animetion/gPlayerAnim_pg_punchB_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pg_punchB_Data",
                                 19,
                                 "misc/link_animetion/gPlayerAnim_pg_punchC_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pg_punchC_Data",
                                 20,
                                 "misc/link_animetion/gPlayerAnim_pg_maru_change_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pg_maru_change_Data",
                                 11,
                                 "misc/link_animetion/gPlayerAnim_link_normal_jump_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_jump_Data",
                                 8,
                                 "misc/link_animetion/gPlayerAnim_link_fighter_backturn_jump_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_fighter_backturn_jump_Data",
                                 15,
                                 "misc/link_animetion/gPlayerAnim_pg_climb_startA_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pg_climb_startA_Data",
                                 20,
                                 "misc/link_animetion/gPlayerAnim_pg_climb_upL_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pg_climb_upL_Data",
                                 10,
                                 "misc/link_animetion/gPlayerAnim_link_normal_defense_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_defense_Data",
                                 1,
                                 "misc/link_animetion/gPlayerAnim_link_normal_defense_wait_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_defense_wait_Data",
                                 4,
                                 "misc/link_animetion/gPlayerAnim_link_normal_defense_end_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_defense_end_Data",
                                 5,
                                 "misc/link_animetion/gPlayerAnim_pg_gakkistart_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pg_gakkistart_Data",
                                 20,
                                 "misc/link_animetion/gPlayerAnim_pg_gakkiplay_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pg_gakkiplay_Data",
                                 4,
                                 "misc/link_animetion/gPlayerAnim_pg_doorA_open_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pg_doorA_open_Data",
                                 30,
                                 "misc/link_animetion/gPlayerAnim_pg_Tbox_open_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pg_Tbox_open_Data",
                                 30,
                                 nullptr,
                                 nullptr,
                                 0,
                                 nullptr,
                                 nullptr,
                                 0 },
    TransformMaskFormDefinition{ PLAYER_MASK_ZORA, 1.0f, 0.0f, 0.0f, 18.0f, 18.0f, 122.0f, 20.0f, 0.0f, 1.95f, 1.28f, 122.0f, 20.0f, 16.0f,
                                 "objects/object_link_zora/gLinkZoraSkel",
                                 "__OTR__objects/object_link_zora/gLinkZoraSkel",
                                 "objects/object_link_zora/gLinkZoraEyesOpenTex",
                                 "__OTR__objects/object_link_zora/gLinkZoraEyesOpenTex",
                                 "objects/object_link_zora/gLinkZoraMouthClosedTex",
                                 "__OTR__objects/object_link_zora/gLinkZoraMouthClosedTex",
                                 "misc/link_animetion/gPlayerAnim_pz_wait_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pz_wait_Data",
                                 59,
                                 "misc/link_animetion/gPlayerAnim_link_normal_walk_free_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_walk_free_Data",
                                 17,
                                 "misc/link_animetion/gPlayerAnim_link_normal_run_free_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_run_free_Data",
                                 17,
                                 "misc/link_animetion/gPlayerAnim_pz_attackA_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pz_attackA_Data",
                                 7,
                                 "misc/link_animetion/gPlayerAnim_pz_attackB_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pz_attackB_Data",
                                 10,
                                 "misc/link_animetion/gPlayerAnim_pz_attackC_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pz_attackC_Data",
                                 20,
                                 "misc/link_animetion/gPlayerAnim_link_normal_landing_roll_free_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_landing_roll_free_Data",
                                 25,
                                 "misc/link_animetion/gPlayerAnim_pz_jumpAT_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pz_jumpAT_Data",
                                 13,
                                 "misc/link_animetion/gPlayerAnim_link_fighter_backturn_jump_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_fighter_backturn_jump_Data",
                                 15,
                                 "misc/link_animetion/gPlayerAnim_pz_climb_startA_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pz_climb_startA_Data",
                                 30,
                                 "misc/link_animetion/gPlayerAnim_pz_climb_upL_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pz_climb_upL_Data",
                                 21,
                                 "misc/link_animetion/gPlayerAnim_link_normal_defense_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_defense_Data",
                                 1,
                                 "misc/link_animetion/gPlayerAnim_link_normal_defense_wait_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_defense_wait_Data",
                                 10,
                                 "misc/link_animetion/gPlayerAnim_link_normal_defense_end_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_defense_end_Data",
                                 3,
                                 "misc/link_animetion/gPlayerAnim_pz_gakkistart_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pz_gakkistart_Data",
                                 15,
                                 "misc/link_animetion/gPlayerAnim_pz_gakkiplay_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pz_gakkiplay_Data",
                                 7,
                                 "misc/link_animetion/gPlayerAnim_pz_doorA_open_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pz_doorA_open_Data",
                                 100,
                                 "misc/link_animetion/gPlayerAnim_pz_Tbox_open_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_pz_Tbox_open_Data",
                                 133,
                                 "misc/link_animetion/gPlayerAnim_link_swimer_swim_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_swimer_swim_Data",
                                 24,
                                 "misc/link_animetion/gPlayerAnim_link_swimer_swim_wait_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_swimer_swim_wait_Data",
                                 4 },
    TransformMaskFormDefinition{ PLAYER_MASK_FIERCE_DEITY, 1.625f, 0.0f, 0.0f, 12.0f, 12.0f, 132.0f, 21.0f, 0.0f, 1.75f, 1.20f, 132.0f, 21.0f, 12.0f,
                                 "objects/object_link_boy/gLinkFierceDeitySkel",
                                 "__OTR__objects/object_link_boy/gLinkFierceDeitySkel",
                                 "objects/object_link_boy/gLinkFierceDeityEyesTex",
                                 "__OTR__objects/object_link_boy/gLinkFierceDeityEyesTex",
                                 "objects/object_link_boy/gLinkFierceDeityMouthTex",
                                 "__OTR__objects/object_link_boy/gLinkFierceDeityMouthTex",
                                 "misc/link_animetion/gPlayerAnim_link_fighter_wait_long_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_fighter_wait_long_Data",
                                 32,
                                 "misc/link_animetion/gPlayerAnim_link_fighter_walk_long_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_fighter_walk_long_Data",
                                 17,
                                 "misc/link_animetion/gPlayerAnim_link_fighter_run_long_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_fighter_run_long_Data",
                                 16,
                                 "misc/link_animetion/gPlayerAnim_link_fighter_normal_kiru_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_fighter_normal_kiru_Data",
                                 5,
                                 nullptr,
                                 nullptr,
                                 0,
                                 nullptr,
                                 nullptr,
                                 0,
                                 "misc/link_animetion/gPlayerAnim_link_fighter_landing_roll_long_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_fighter_landing_roll_long_Data",
                                 20,
                                 "misc/link_animetion/gPlayerAnim_link_normal_jump_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_jump_Data",
                                 8,
                                 "misc/link_animetion/gPlayerAnim_link_fighter_backturn_jump_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_fighter_backturn_jump_Data",
                                 15,
                                 "misc/link_animetion/gPlayerAnim_link_normal_jump_climb_startA_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_jump_climb_startA_Data",
                                 30,
                                 "misc/link_animetion/gPlayerAnim_link_normal_jump_climb_up_free_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_jump_climb_up_free_Data",
                                 21,
                                 "misc/link_animetion/gPlayerAnim_link_fighter_defense_long_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_fighter_defense_long_Data",
                                 4,
                                 "misc/link_animetion/gPlayerAnim_link_fighter_defense_long_wait_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_fighter_defense_long_wait_Data",
                                 10,
                                 "misc/link_animetion/gPlayerAnim_link_normal_defense_end_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_defense_end_Data",
                                 5,
                                 "misc/link_animetion/gPlayerAnim_link_normal_okarina_start_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_okarina_start_Data",
                                 10,
                                 "misc/link_animetion/gPlayerAnim_link_normal_okarina_swing_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_normal_okarina_swing_Data",
                                 10,
                                 "misc/link_animetion/gPlayerAnim_link_demo_doorA_link_free_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_demo_doorA_link_free_Data",
                                 30,
                                 nullptr,
                                 nullptr,
                                 0,
                                 "misc/link_animetion/gPlayerAnim_link_swimer_swim_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_swimer_swim_Data",
                                 24,
                                 "misc/link_animetion/gPlayerAnim_link_swimer_swim_wait_Data",
                                 "__OTR__misc/link_animetion/gPlayerAnim_link_swimer_swim_wait_Data",
                                 4 },
};

enum TransformMaskMotionState {
    TRANSFORM_MASK_MOTION_UNSET = -1,
    TRANSFORM_MASK_MOTION_IDLE,
    TRANSFORM_MASK_MOTION_WALK,
    TRANSFORM_MASK_MOTION_RUN,
    TRANSFORM_MASK_MOTION_DOOR,
    TRANSFORM_MASK_MOTION_GET_ITEM,
    TRANSFORM_MASK_MOTION_SWIM,
    TRANSFORM_MASK_MOTION_ATTACK,
    TRANSFORM_MASK_MOTION_DEFEND,
    TRANSFORM_MASK_MOTION_OCARINA,
    TRANSFORM_MASK_MOTION_SPIN,
    TRANSFORM_MASK_MOTION_JUMP,
    TRANSFORM_MASK_MOTION_FALL,
    TRANSFORM_MASK_MOTION_HOP,
    TRANSFORM_MASK_MOTION_CLIMB,
    TRANSFORM_MASK_MOTION_ZTARGET_IDLE,
    TRANSFORM_MASK_MOTION_ZTARGET_SIDEWALK_LEFT,
    TRANSFORM_MASK_MOTION_ZTARGET_SIDEWALK_RIGHT,
    TRANSFORM_MASK_MOTION_ZTARGET_BACKWALK,
    TRANSFORM_MASK_MOTION_ZORA_BOOMERANG,
    TRANSFORM_MASK_MOTION_ZORA_FAST_SWIM,
    TRANSFORM_MASK_MOTION_OOT_ACTION,
};

enum ZoraBoomerangState : u8 {
    ZORA_BOOMERANG_IDLE = 0,
    ZORA_BOOMERANG_AIM,
    ZORA_BOOMERANG_THROW,
    ZORA_BOOMERANG_FLIGHT,
    ZORA_BOOMERANG_CATCH,
};

enum ZoraSwimState : u8 {
    ZORA_SWIM_IDLE = 0,
    ZORA_SWIM_FAST_START,
    ZORA_SWIM_FAST,
    ZORA_SWIM_FAST_EXIT,
    ZORA_SWIM_DOLPHIN_JUMP,
};

enum GoronCustomActionState {
    GORON_ACTION_NONE = 0,
    GORON_ACTION_PUNCH,
    GORON_ACTION_ROLL_START,
    GORON_ACTION_ROLL,
    GORON_ACTION_SLAM_JUMP,
    GORON_ACTION_SLAM_POUND,
};

enum DekuCustomActionState : u8 {
    DEKU_ACTION_NONE = 0,
    DEKU_ACTION_SPIN,
    DEKU_ACTION_BUBBLE_AIM,
    DEKU_ACTION_BUBBLE_FIRE,
};

struct DekuBubbleState {
    bool active = false;
    Vec3f pos = {};
    Vec3f prevPos = {};
    Vec3f spawnPos = {};
    s16 rotX = 0;
    s16 rotY = 0;
    s16 wobbleAccX = 0;
    s16 wobbleAccY = 0;
    f32 hSpeed = 0.0f;
    f32 velY = 0.0f;
    f32 scale = kDekuBubbleMinScale;
    s16 timer = 0;
    s8 state = 0;
};

struct TransformMaskRuntimeState {
    bool initialized = false;
    bool assetsReady = false;
    PlayState* ownerPlay = nullptr;
    Player* ownerPlayer = nullptr;
    s16 ownerSceneNum = -1;
    u8 activeMask = PLAYER_MASK_NONE;
    s8 savedTunic = PLAYER_TUNIC_KOKIRI;
    bool zoraTunicForced = false;
    TransformMaskMotionState motionState = TRANSFORM_MASK_MOTION_UNSET;
    SkelAnime skelAnime = {};
    SkelAnime goronShieldSkelAnime = {};
    bool goronShieldSkelReady = false;
    std::shared_ptr<SOH::PlayerAnimation> idleAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> walkAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> runAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> attackAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> attackAnimResource2 = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> attackAnimResource3 = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> spinAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> jumpAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> fallAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> hopAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> climbStartAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> climbLoopAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> defendAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> defendWaitAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> defendEndAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronPunchEndAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronPunchEndAnimResource2 = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronPunchEndAnimResource3 = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronPunchEndRAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronPunchEndRAnimResource2 = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronPunchEndRAnimResource3 = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronSidehopLeftAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronSidehopRightAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronBackflipAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronSidehopLeftEndAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronSidehopRightEndAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronBackflipEndAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronClimbStartBAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronClimbUpRAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronClimbEndALAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronClimbEndARAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronClimbEndBLAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronClimbEndBRAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> dekuClimbUpLAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> dekuClimbUpRAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronDoorBAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> goronMaskOffStartAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> ztargetIdleRightAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> ztargetIdleLeftAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> ztargetSideWalkLeftAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> ztargetSideWalkRightAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> ztargetBackWalkAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> ocarinaStartAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> ocarinaPlayAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> doorAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> chestAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> swimAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> swimIdleAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> zoraFishSwimAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> zoraWaterRollAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> zoraSwimToWaitAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> zoraSwimDownAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> zoraSwimDeepEndAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> zoraWaitToSwimWaitAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> zoraLandToSwimWaitAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> zoraCutterAttackAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> zoraCutterCatchAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> zoraCutterWaitAnimResource = nullptr;
    std::shared_ptr<SOH::PlayerAnimation> zoraJumpKickEndAnimResource = nullptr;
    LinkAnimationHeader idleAnimHeader = {};
    LinkAnimationHeader walkAnimHeader = {};
    LinkAnimationHeader runAnimHeader = {};
    LinkAnimationHeader doorAnimHeader = {};
    LinkAnimationHeader chestAnimHeader = {};
    LinkAnimationHeader swimAnimHeader = {};
    LinkAnimationHeader swimIdleAnimHeader = {};
    LinkAnimationHeader zoraFishSwimAnimHeader = {};
    LinkAnimationHeader zoraWaterRollAnimHeader = {};
    LinkAnimationHeader zoraSwimToWaitAnimHeader = {};
    LinkAnimationHeader zoraSwimDownAnimHeader = {};
    LinkAnimationHeader zoraSwimDeepEndAnimHeader = {};
    LinkAnimationHeader zoraWaitToSwimWaitAnimHeader = {};
    LinkAnimationHeader zoraLandToSwimWaitAnimHeader = {};
    LinkAnimationHeader zoraCutterAttackAnimHeader = {};
    LinkAnimationHeader zoraCutterCatchAnimHeader = {};
    LinkAnimationHeader zoraCutterWaitAnimHeader = {};
    LinkAnimationHeader zoraJumpKickEndAnimHeader = {};
    LinkAnimationHeader attackAnimHeader = {};
    LinkAnimationHeader attackAnimHeader2 = {};
    LinkAnimationHeader attackAnimHeader3 = {};
    LinkAnimationHeader spinAnimHeader = {};
    LinkAnimationHeader jumpAnimHeader = {};
    LinkAnimationHeader fallAnimHeader = {};
    LinkAnimationHeader hopAnimHeader = {};
    LinkAnimationHeader climbStartAnimHeader = {};
    LinkAnimationHeader climbLoopAnimHeader = {};
    LinkAnimationHeader defendAnimHeader = {};
    LinkAnimationHeader defendWaitAnimHeader = {};
    LinkAnimationHeader defendEndAnimHeader = {};
    LinkAnimationHeader goronPunchEndAnimHeader = {};
    LinkAnimationHeader goronPunchEndAnimHeader2 = {};
    LinkAnimationHeader goronPunchEndAnimHeader3 = {};
    LinkAnimationHeader goronPunchEndRAnimHeader = {};
    LinkAnimationHeader goronPunchEndRAnimHeader2 = {};
    LinkAnimationHeader goronPunchEndRAnimHeader3 = {};
    LinkAnimationHeader goronSidehopLeftAnimHeader = {};
    LinkAnimationHeader goronSidehopRightAnimHeader = {};
    LinkAnimationHeader goronBackflipAnimHeader = {};
    LinkAnimationHeader goronSidehopLeftEndAnimHeader = {};
    LinkAnimationHeader goronSidehopRightEndAnimHeader = {};
    LinkAnimationHeader goronBackflipEndAnimHeader = {};
    LinkAnimationHeader goronClimbStartBAnimHeader = {};
    LinkAnimationHeader goronClimbUpRAnimHeader = {};
    LinkAnimationHeader goronClimbEndALAnimHeader = {};
    LinkAnimationHeader goronClimbEndARAnimHeader = {};
    LinkAnimationHeader goronClimbEndBLAnimHeader = {};
    LinkAnimationHeader goronClimbEndBRAnimHeader = {};
    LinkAnimationHeader dekuClimbUpLAnimHeader = {};
    LinkAnimationHeader dekuClimbUpRAnimHeader = {};
    LinkAnimationHeader goronDoorBAnimHeader = {};
    LinkAnimationHeader goronMaskOffStartAnimHeader = {};
    LinkAnimationHeader ztargetIdleRightAnimHeader = {};
    LinkAnimationHeader ztargetIdleLeftAnimHeader = {};
    LinkAnimationHeader ztargetSideWalkLeftAnimHeader = {};
    LinkAnimationHeader ztargetSideWalkRightAnimHeader = {};
    LinkAnimationHeader ztargetBackWalkAnimHeader = {};
    LinkAnimationHeader ocarinaStartAnimHeader = {};
    LinkAnimationHeader ocarinaPlayAnimHeader = {};
    std::vector<s16> swimAnimCopy = {};
    std::vector<s16> swimIdleAnimCopy = {};
    std::vector<s16> zoraFishSwimAnimCopy = {};
    std::vector<s16> zoraWaterRollAnimCopy = {};
    std::vector<s16> zoraSwimToWaitAnimCopy = {};
    std::vector<s16> zoraSwimDownAnimCopy = {};
    std::vector<s16> zoraSwimDeepEndAnimCopy = {};
    std::vector<s16> zoraWaitToSwimWaitAnimCopy = {};
    std::vector<s16> zoraLandToSwimWaitAnimCopy = {};
    void* eyeTexture = nullptr;
    void* mouthTexture = nullptr;
    std::array<void*, kMaxOcarinaDisplayLists> ocarinaDLs = {};
    size_t ocarinaDLCount = 0;
    s32 ocarinaLimbIndex = -1;
    u8 attackComboIndex = 0xFF;
    std::shared_ptr<Fast::DisplayList> goronRolledUpDLResource = nullptr;
    std::shared_ptr<Fast::DisplayList> goronPoweredRollDLResource = nullptr;
    std::shared_ptr<Fast::DisplayList> goronHeatEffectDLResource1 = nullptr;
    std::shared_ptr<Fast::DisplayList> goronHeatEffectDLResource2 = nullptr;
    std::shared_ptr<Fast::DisplayList> goronPunchEffectDLResource = nullptr;
    std::shared_ptr<Fast::DisplayList> zoraLeftFinDLResource = nullptr;
    std::shared_ptr<Fast::DisplayList> zoraRightFinDLResource = nullptr;
    std::shared_ptr<Fast::DisplayList> zoraBarrierDLResource = nullptr;
    std::shared_ptr<Fast::DisplayList> dekuBubbleStillDLResource = nullptr;
    std::shared_ptr<Fast::DisplayList> dekuBubbleMoveDLResource = nullptr;
    std::shared_ptr<Fast::DisplayList> dekuBubbleSetupDLResource = nullptr;
    std::shared_ptr<Fast::DisplayList> dekuSpinTrailDLResource = nullptr;
    std::shared_ptr<Fast::DisplayList> dekuShieldDLResource = nullptr;
    std::shared_ptr<Fast::DisplayList> fdLeftHandSwordDLResource = nullptr;
    std::shared_ptr<Fast::DisplayList> fdLeftHandEmptyDLResource = nullptr;
    std::shared_ptr<Fast::DisplayList> fdLeftHandBottleDLResource = nullptr;
    std::shared_ptr<Fast::DisplayList> fdRightHandEmptyDLResource = nullptr;
    std::shared_ptr<Fast::DisplayList> fdSwordBeamDLResource = nullptr;
    void* goronRolledUpDL = nullptr;
    void* goronSpikeShellDL = nullptr;
    void* goronSpikeRockBodyDL = nullptr;
    void* goronSpikeMetalBodyDL = nullptr;
    void* goronSpikeEffectDL1 = nullptr;
    void* goronSpikeEffectDL2 = nullptr;
    void* goronPunchEffectDL = nullptr;
    void* goronSpikeMatAnim1 = nullptr;
    void* goronSpikeMatAnim2 = nullptr;
    void* zoraLeftFinDL = nullptr;
    void* zoraRightFinDL = nullptr;
    void* zoraBarrierDL = nullptr;
    void* dekuBubbleStillDL = nullptr;
    void* dekuBubbleMoveDL = nullptr;
    void* dekuBubbleSetupDL = nullptr;
    void* dekuSpinTrailDL = nullptr;
    void* dekuShieldDL = nullptr;
    void* fdLeftHandSwordDL = nullptr;
    void* fdLeftHandEmptyDL = nullptr;
    void* fdLeftHandBottleDL = nullptr;
    void* fdRightHandEmptyDL = nullptr;
    void* fdSwordBeamDL = nullptr;
    u8 goronAction = GORON_ACTION_NONE;
    u8 goronPunchIndex = 0;
    u8 goronPunchChainCount = 0;
    bool goronQueuedPunch = false;
    bool goronPunchHitSfxPlayed = false;
    bool goronPunchAnchorValid = false;
    Vec3f goronPunchAnchorPos = {};
    s16 goronActionTimer = 0;
    s16 goronRollChargeTimer = 0;
    s16 goronSpikePulseTimer = 0;
    s16 goronPowerStage = 0;
    s16 goronWallReboundTimer = 0;
    s16 goronSlamImpactTimer = 0;
    bool goronSlamImpacted = false;
    f32 goronRollSpeed = 0.0f;
    f32 goronRollBallSpeed = 0.0f;
    f32 goronRollBounce = 0.0f;
    f32 goronRollSquash = 0.0f;
    f32 goronRollTilt = 0.0f;
    f32 goronRollColorLerp = 0.0f;
    f32 goronRollVisualAngle = 0.0f;
    f32 goronRollBouncePhase = 0.0f;
    s16 goronRollHomeYaw = 0;
    s16 goronActionLockedYaw = 0;
    s16 goronRollSpinRate = 0;
    s16 goronRollSfxCounter = 0;
    s16 goronRollDriftYaw = 0;
    s16 goronMagicDrainTimer = 0;
    bool goronSpikeMode = false;
    bool goronClimbUseRight = false;
    ZoraBoomerangState zoraBoomerangState = ZORA_BOOMERANG_IDLE;
    s16 zoraBoomerangTimer = 0;
    s16 zoraBoomerangAimYaw = 0;
    s16 zoraBoomerangAimPitch = 0;
    s16 zoraBoomerangLockedYaw = 0;
    Actor* zoraBoomerangActorL = nullptr;
    Actor* zoraBoomerangActorR = nullptr;
    Actor zoraBoomerangCameraTarget = {};
    ZoraSwimState zoraSwimState = ZORA_SWIM_IDLE;
    s16 zoraSwimPitch = 0;
    s16 zoraSwimRoll = 0;
    s16 zoraSwimRollSmoothed = 0;
    s16 zoraSwimYawRate = 0;
    s16 zoraSwimPhaseCounter = 0;
    u16 zoraSwimActionTimer = 0;
    s16 zoraDolphinYaw = 0;
    f32 zoraSwimSpeed = 0.0f;
    f32 zoraDolphinLinearVelocity = 0.0f;
    bool zoraFastSwimActive = false;
    bool zoraDolphinReentryLocked = false;
    u8 zoraDolphinDebugCooldown = 0;
    bool zoraBootsActive = false;
    bool zoraBarrierActive = false;
    s16 zoraBarrierIntensity = 0;
    s16 zoraBarrierMagicDrainTimer = 0;
    DekuCustomActionState dekuAction = DEKU_ACTION_NONE;
    DekuBubbleState dekuBubble = {};
    f32 dekuBubbleCharge = 0.0f;
    s16 dekuBubbleChargeTimer = 0;
    s16 dekuBubbleAimPitch = 0;
    s16 dekuBubbleAimYaw = 0;
    s16 visualTurnTilt = 0;
    s16 visualTurnTiltLastYaw = 0;
    bool visualTurnTiltYawValid = false;
    f32 dekuSpinTimer = 0.0f;
    f32 dekuSpinSpeed = 0.0f;
    s32 dekuSpinRotAccum = 0;
    s32 dekuSpinTracerIndex = -1;
    bool dekuSpinTracerActive = false;
    s16 dekuWaterHopsRemaining = 5;
    s16 dekuWaterHopAnimTimer = 0;
    bool dekuStableRootYValid = false;
    s16 dekuStableRootY = 0;
    TransformMaskMotionState goronLastMotionSfx = TRANSFORM_MASK_MOTION_UNSET;
    s16 transformFlashTimer = 0;
};

TransformMaskRuntimeState sRuntimeState;

bool IsTransformAudioActiveForMask(Player* player, u8 playerMask) {
    if (player == nullptr) {
        return false;
    }

    if (player->currentMask == playerMask) {
        return true;
    }

    return sRuntimeState.initialized && sRuntimeState.assetsReady && sRuntimeState.ownerPlayer == player &&
           sRuntimeState.activeMask == playerMask;
}

ColliderCylinder sGoronAttackCollider = {};
bool sGoronAttackColliderInitialized = false;
Actor* sGoronAttackColliderActor = nullptr;
ColliderCylinder sZoraBarrierCollider = {};
bool sZoraBarrierColliderInitialized = false;
Actor* sZoraBarrierColliderActor = nullptr;
ColliderCylinder sDekuBubbleCollider = {};
bool sDekuBubbleColliderInitialized = false;
Actor* sDekuBubbleColliderActor = nullptr;

void TransformMaskVirtualActorUpdate(Actor* actor, PlayState* play) {
}

void ResetTransformMaskCameraEffects() {
    PlayState* play = sRuntimeState.ownerPlay;
    if (play == nullptr || !sRuntimeState.initialized) {
        return;
    }

    Camera* camera = Play_GetCamera(play, MAIN_CAM);
    if (camera == nullptr) {
        return;
    }

    Camera_ChangeMode(camera, CAM_MODE_NORMAL);
    Camera_ChangeSetting(camera, CAM_SET_NORMAL0);
}

void StartTransformMaskPutOnEffect(Player* player) {
    sRuntimeState.transformFlashTimer = 5;

    const bool playedMmFlash =
        PlayMmSfxPcmById(kMmVoicePcmKeyTransformFlash, "transform-mask-flash", kMmSfxTransformMaskFlash, 0.7f, false);
    if (!playedMmFlash) {
        Audio_PlaySoundGeneral(NA_SE_SY_CAMERA_ZOOM_UP, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }

    if (player != nullptr) {
        player->linearVelocity = 0.0f;
    }
}

static ColliderCylinderInit sGoronAttackColliderInit = {
    {
        COLTYPE_HIT0,
        AT_ON | AT_TYPE_ALL,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x40000048, 0x00, 0x08 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 36, 60, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sZoraBarrierColliderInit = {
    {
        COLTYPE_HIT0,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK2,
        { DMG_SLASH_KOKIRI | DMG_SPIN_KOKIRI | DMG_JUMP_KOKIRI | DMG_HOOKSHOT, 0x00, 0x02 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 50, 80, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sDekuBubbleColliderInit = {
    {
        COLTYPE_HIT0,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { DMG_SPIN_KOKIRI, 0x00, 0x02 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 30, 30, 0, { 0, 0, 0 } },
};

bool IsGoronSpikeShellVisible() {
    return sRuntimeState.goronSpikeMode && sRuntimeState.goronPowerStage >= 2;
}

bool IsGoronRollInputIntent(const Input* input) {
    return input != nullptr && CHECK_BTN_ALL(input->cur.button, BTN_A);
}

bool IsGoronVanillaHopInputIntent(const Player* player, const Input* input) {
    if (player == nullptr || input == nullptr || !CHECK_BTN_ALL(input->cur.button, BTN_A)) {
        return false;
    }

    const bool targeting = (player->stateFlags1 & (PLAYER_STATE1_Z_TARGETING | PLAYER_STATE1_HOSTILE_LOCK_ON)) != 0;
    if (!targeting) {
        return false;
    }

    return input->rel.stick_y < -kGoronRollStickDeadzone || abs(input->rel.stick_x) > kGoronRollStickDeadzone;
}

bool IsZTargeting(const Player* player) {
    return player != nullptr &&
           (player->stateFlags1 & (PLAYER_STATE1_Z_TARGETING | PLAYER_STATE1_HOSTILE_LOCK_ON)) != 0;
}

TransformMaskMotionState GetZTargetMotion(Player* player) {
    if (player == nullptr || !IsZTargeting(player)) {
        return TRANSFORM_MASK_MOTION_UNSET;
    }

    if (fabsf(player->actor.speedXZ) <= 0.1f && fabsf(player->linearVelocity) <= 0.1f) {
        return TRANSFORM_MASK_MOTION_ZTARGET_IDLE;
    }

    const s32 stickDir = player->controlStickDirections[player->controlStickDataIndex];
    switch (stickDir) {
        case PLAYER_STICK_DIR_LEFT:
            return sRuntimeState.ztargetSideWalkLeftAnimHeader.segment != nullptr
                       ? TRANSFORM_MASK_MOTION_ZTARGET_SIDEWALK_LEFT
                       : TRANSFORM_MASK_MOTION_WALK;
        case PLAYER_STICK_DIR_RIGHT:
            return sRuntimeState.ztargetSideWalkRightAnimHeader.segment != nullptr
                       ? TRANSFORM_MASK_MOTION_ZTARGET_SIDEWALK_RIGHT
                       : TRANSFORM_MASK_MOTION_WALK;
        case PLAYER_STICK_DIR_BACKWARD:
            return sRuntimeState.ztargetBackWalkAnimHeader.segment != nullptr
                       ? TRANSFORM_MASK_MOTION_ZTARGET_BACKWALK
                       : TRANSFORM_MASK_MOTION_WALK;
        case PLAYER_STICK_DIR_FORWARD:
        default:
            return TRANSFORM_MASK_MOTION_WALK;
    }
}

bool IsDekuSidehopAnimationActive(Player* player) {
    return player != nullptr && player->currentMask == PLAYER_MASK_DEKU &&
           (sRuntimeState.skelAnime.animation == &sRuntimeState.goronSidehopLeftAnimHeader ||
            sRuntimeState.skelAnime.animation == &sRuntimeState.goronSidehopRightAnimHeader);
}

bool IsGettingItemPose(const Player* player);
bool IsPlayerVanillaRollAnimation(const Player* player);

bool ShouldFierceDeityUseOotActionPose(Player* player) {
    if (player == nullptr || player->currentMask != PLAYER_MASK_FIERCE_DEITY) {
        return false;
    }

    const u32 stateFlags1 = player->stateFlags1;
    const u32 stateFlags2 = player->stateFlags2;

    return player->meleeWeaponState != 0 || (stateFlags2 & (PLAYER_STATE2_HOPPING | PLAYER_STATE2_SPIN_ATTACKING)) != 0 ||
           (stateFlags1 & (PLAYER_STATE1_SHIELDING | PLAYER_STATE1_CLIMBING_LADDER |
                            PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE |
                            PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_TALKING | PLAYER_STATE1_IN_CUTSCENE |
                            PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_WATER |
                            PLAYER_STATE1_CHARGING_SPIN_ATTACK)) != 0 ||
           player->heldActor != nullptr || IsGettingItemPose(player) || IsPlayerVanillaRollAnimation(player);
}

void NeutralizeDekuSidehopRoot(Player* player) {
    if (!IsDekuSidehopAnimationActive(player)) {
        return;
    }

    sRuntimeState.skelAnime.jointTable[0].x = 0;
    if (sRuntimeState.dekuStableRootYValid) {
        sRuntimeState.skelAnime.jointTable[0].y = sRuntimeState.dekuStableRootY;
    }
    sRuntimeState.skelAnime.jointTable[0].z = 0;
    sRuntimeState.skelAnime.prevTransl = sRuntimeState.skelAnime.jointTable[0];
}

void CaptureDekuStableRootY(Player* player) {
    if (player == nullptr || player->currentMask != PLAYER_MASK_DEKU || IsDekuSidehopAnimationActive(player)) {
        return;
    }

    switch (sRuntimeState.motionState) {
        case TRANSFORM_MASK_MOTION_IDLE:
        case TRANSFORM_MASK_MOTION_WALK:
        case TRANSFORM_MASK_MOTION_RUN:
        case TRANSFORM_MASK_MOTION_ZTARGET_IDLE:
        case TRANSFORM_MASK_MOTION_ZTARGET_SIDEWALK_LEFT:
        case TRANSFORM_MASK_MOTION_ZTARGET_SIDEWALK_RIGHT:
        case TRANSFORM_MASK_MOTION_ZTARGET_BACKWALK:
        case TRANSFORM_MASK_MOTION_DEFEND:
            sRuntimeState.dekuStableRootY = sRuntimeState.skelAnime.jointTable[0].y;
            sRuntimeState.dekuStableRootYValid = true;
            break;
        default:
            break;
    }
}

f32 GetTransformLocomotionSpeed(const Player* player) {
    if (player == nullptr) {
        return 0.0f;
    }

    return fabsf(player->linearVelocity);
}

f32 GetTransformLocomotionAnimSpeed(const Player* player, TransformMaskMotionState motionState) {
    const f32 speed = GetTransformLocomotionSpeed(player);

    switch (motionState) {
        case TRANSFORM_MASK_MOTION_RUN:
            return CLAMP((speed * 0.15f) + 1.0f, 1.0f, 2.5f);
        case TRANSFORM_MASK_MOTION_WALK:
            if (player != nullptr && player->currentMask == PLAYER_MASK_GORON) {
                return CLAMP((speed * 0.22f + 0.85f) * 0.67f, 0.57f, 1.04f);
            }
            return CLAMP((speed * 0.3f) + 1.0f, 0.75f, 2.5f);
        case TRANSFORM_MASK_MOTION_ZTARGET_SIDEWALK_LEFT:
        case TRANSFORM_MASK_MOTION_ZTARGET_SIDEWALK_RIGHT:
        case TRANSFORM_MASK_MOTION_ZTARGET_BACKWALK:
            return CLAMP((speed * 0.3f) + 0.75f, 0.75f, 2.0f);
        default:
            return 1.0f;
    }
}

bool IsTransformLocomotionMotion(TransformMaskMotionState motionState) {
    return motionState == TRANSFORM_MASK_MOTION_WALK || motionState == TRANSFORM_MASK_MOTION_RUN ||
           motionState == TRANSFORM_MASK_MOTION_ZTARGET_SIDEWALK_LEFT ||
           motionState == TRANSFORM_MASK_MOTION_ZTARGET_SIDEWALK_RIGHT ||
           motionState == TRANSFORM_MASK_MOTION_ZTARGET_BACKWALK;
}

void RefreshTransformLocomotionAnimSpeed(Player* player, TransformMaskMotionState motionState) {
    if (IsTransformLocomotionMotion(motionState)) {
        sRuntimeState.skelAnime.playSpeed = GetTransformLocomotionAnimSpeed(player, motionState);
    }
}

void PlayGoronActionSfx(Player* player, u16 sfxId) {
    if (player == nullptr || sfxId == 0) {
        return;
    }

    Audio_PlayActorSound2(&player->actor, sfxId);
}

void LockGoronPlayerMotion(Player* player) {
    if (player == nullptr) {
        return;
    }

    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;
    player->pushedSpeed = 0.0f;
    player->yaw = sRuntimeState.goronActionLockedYaw;
    player->actor.world.rot.y = sRuntimeState.goronActionLockedYaw;
    player->actor.shape.rot.y = sRuntimeState.goronActionLockedYaw;
    player->meleeWeaponState = 0;
}

void RegisterGoronBombDamageCollider(PlayState* play, Player* player, bool rolling) {
    if (play == nullptr || player == nullptr) {
        return;
    }

    if (!sGoronAttackColliderInitialized || sGoronAttackColliderActor != &player->actor) {
        if (sGoronAttackColliderInitialized) {
            Collider_DestroyCylinder(play, &sGoronAttackCollider);
        }
        Collider_InitCylinder(play, &sGoronAttackCollider);
        Collider_SetCylinder(play, &sGoronAttackCollider, &player->actor, &sGoronAttackColliderInit);
        sGoronAttackColliderInitialized = true;
        sGoronAttackColliderActor = &player->actor;
    }

    Collider_UpdateCylinder(&player->actor, &sGoronAttackCollider);
    sGoronAttackCollider.dim.radius = rolling ? 46 : 38;
    sGoronAttackCollider.dim.height = rolling ? 52 : 64;
    sGoronAttackCollider.dim.yShift = rolling ? 0 : 18;
    sGoronAttackCollider.info.toucher.damage = rolling ? 4 : 2;
    sGoronAttackCollider.info.toucher.dmgFlags = 0x40000048;
    sGoronAttackCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
    CollisionCheck_SetAT(play, &play->colChkCtx, &sGoronAttackCollider.base);
}

bool IsZoraPunchStrikeActive() {
    static constexpr std::array<std::array<f32, 2>, 3> kZoraPunchFrames = {{
        { 2.0f, 5.0f },
        { 3.0f, 8.0f },
        { 3.0f, 10.0f },
    }};

    if (sRuntimeState.skelAnime.animation != &sRuntimeState.attackAnimHeader &&
        sRuntimeState.skelAnime.animation != &sRuntimeState.attackAnimHeader2 &&
        sRuntimeState.skelAnime.animation != &sRuntimeState.attackAnimHeader3) {
        return false;
    }

    const u8 punchIndex = std::min<u8>(sRuntimeState.goronPunchIndex, 2);
    const f32 curFrame = sRuntimeState.skelAnime.curFrame;
    return curFrame >= kZoraPunchFrames[punchIndex][0] && curFrame <= kZoraPunchFrames[punchIndex][1];
}

void RegisterZoraPunchDamageCollider(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || !IsZoraPunchStrikeActive()) {
        return;
    }

    if (!sGoronAttackColliderInitialized || sGoronAttackColliderActor != &player->actor) {
        if (sGoronAttackColliderInitialized) {
            Collider_DestroyCylinder(play, &sGoronAttackCollider);
        }
        Collider_InitCylinder(play, &sGoronAttackCollider);
        Collider_SetCylinder(play, &sGoronAttackCollider, &player->actor, &sGoronAttackColliderInit);
        sGoronAttackColliderInitialized = true;
        sGoronAttackColliderActor = &player->actor;
    }

    Collider_UpdateCylinder(&player->actor, &sGoronAttackCollider);
    sGoronAttackCollider.dim.radius = sRuntimeState.goronPunchIndex == 2 ? 44 : 36;
    sGoronAttackCollider.dim.height = 64;
    sGoronAttackCollider.dim.yShift = 18;
    sGoronAttackCollider.info.toucher.damage = 1;
    sGoronAttackCollider.info.toucher.dmgFlags = DMG_JUMP_MASTER;
    sGoronAttackCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
    CollisionCheck_SetAT(play, &play->colChkCtx, &sGoronAttackCollider.base);
}

void ApplyZoraJumpSlashDamage(Player* player) {
    if (player == nullptr || player->currentMask != PLAYER_MASK_ZORA || player->meleeWeaponState == 0) {
        return;
    }

    for (s32 i = 0; i < 2; i++) {
        player->meleeWeaponQuads[i].info.toucher.damage = 2;
        player->meleeWeaponQuads[i].info.toucher.dmgFlags = DMG_JUMP_MASTER;
        player->meleeWeaponQuads[i].info.toucherFlags = TOUCH_ON | TOUCH_NEAREST;
    }
}

void RegisterGoronGroundPoundCollider(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr) {
        return;
    }

    if (!sGoronAttackColliderInitialized || sGoronAttackColliderActor != &player->actor) {
        if (sGoronAttackColliderInitialized) {
            Collider_DestroyCylinder(play, &sGoronAttackCollider);
        }
        Collider_InitCylinder(play, &sGoronAttackCollider);
        Collider_SetCylinder(play, &sGoronAttackCollider, &player->actor, &sGoronAttackColliderInit);
        sGoronAttackColliderInitialized = true;
        sGoronAttackColliderActor = &player->actor;
    }

    Collider_UpdateCylinder(&player->actor, &sGoronAttackCollider);
    sGoronAttackCollider.dim.radius = 60;
    sGoronAttackCollider.dim.height = 52;
    sGoronAttackCollider.dim.yShift = 0;
    sGoronAttackCollider.info.toucher.damage = 4;
    sGoronAttackCollider.info.toucher.dmgFlags = DMG_HAMMER_SWING;
    sGoronAttackCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
    CollisionCheck_SetAT(play, &play->colChkCtx, &sGoronAttackCollider.base);
}

bool IsDekuSpinActive() {
    return sRuntimeState.dekuAction == DEKU_ACTION_SPIN && sRuntimeState.dekuSpinTimer > 0;
}

void EnsureDekuSpinTracer(PlayState* play) {
    if (play == nullptr || sRuntimeState.dekuSpinTracerActive) {
        return;
    }

    EffectBlureInit1 blure = {};
    blure.p1StartColor[0] = 255;
    blure.p1StartColor[1] = 245;
    blure.p1StartColor[2] = 110;
    blure.p1StartColor[3] = 190;
    blure.p2StartColor[0] = 255;
    blure.p2StartColor[1] = 180;
    blure.p2StartColor[2] = 50;
    blure.p2StartColor[3] = 75;
    blure.p1EndColor[0] = 255;
    blure.p1EndColor[1] = 245;
    blure.p1EndColor[2] = 110;
    blure.p1EndColor[3] = 0;
    blure.p2EndColor[0] = 255;
    blure.p2EndColor[1] = 180;
    blure.p2EndColor[2] = 50;
    blure.p2EndColor[3] = 0;
    blure.elemDuration = 5;
    blure.unkFlag = 0;
    blure.calcMode = 0;
    blure.trailType = TRAIL_TYPE_BOOMERANG;

    Effect_Add(play, &sRuntimeState.dekuSpinTracerIndex, EFFECT_BLURE1, 0, 0, &blure);
    sRuntimeState.dekuSpinTracerActive = sRuntimeState.dekuSpinTracerIndex >= 0;
}

void AddDekuSpinTracerVertex(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || !IsDekuSpinActive()) {
        return;
    }

    EnsureDekuSpinTracer(play);
    if (!sRuntimeState.dekuSpinTracerActive) {
        return;
    }

    EffectBlure* blure = static_cast<EffectBlure*>(Effect_GetByIndex(sRuntimeState.dekuSpinTracerIndex));
    if (blure == nullptr) {
        sRuntimeState.dekuSpinTracerActive = false;
        sRuntimeState.dekuSpinTracerIndex = -1;
        return;
    }

    const s16 trailYaw = player->yaw + static_cast<s16>(sRuntimeState.dekuSpinRotAccum) + 0x8000;
    Vec3f p1 = player->actor.world.pos;
    Vec3f p2 = player->actor.world.pos;
    p1.x += Math_SinS(trailYaw) * 38.0f;
    p1.y += 9.0f;
    p1.z += Math_CosS(trailYaw) * 38.0f;
    p2.x += Math_SinS(trailYaw) * 48.0f;
    p2.y += 31.0f;
    p2.z += Math_CosS(trailYaw) * 48.0f;
    EffectBlure_AddVertex(blure, &p1, &p2);
}

void StopDekuSpinTracer() {
    if (!sRuntimeState.dekuSpinTracerActive) {
        return;
    }

    EffectBlure* blure = static_cast<EffectBlure*>(Effect_GetByIndex(sRuntimeState.dekuSpinTracerIndex));
    if (blure != nullptr) {
        EffectBlure_AddSpace(blure);
    }
    sRuntimeState.dekuSpinTracerActive = false;
    sRuntimeState.dekuSpinTracerIndex = -1;
}

void RegisterDekuSpinDamageCollider(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || !IsDekuSpinActive()) {
        return;
    }

    if (!sGoronAttackColliderInitialized || sGoronAttackColliderActor != &player->actor) {
        if (sGoronAttackColliderInitialized) {
            Collider_DestroyCylinder(play, &sGoronAttackCollider);
        }
        Collider_InitCylinder(play, &sGoronAttackCollider);
        Collider_SetCylinder(play, &sGoronAttackCollider, &player->actor, &sGoronAttackColliderInit);
        sGoronAttackColliderInitialized = true;
        sGoronAttackColliderActor = &player->actor;
    }

    Collider_UpdateCylinder(&player->actor, &sGoronAttackCollider);
    sGoronAttackCollider.dim.radius = 34;
    sGoronAttackCollider.dim.height = 42;
    sGoronAttackCollider.dim.yShift = 8;
    sGoronAttackCollider.info.toucher.damage = 1;
    sGoronAttackCollider.info.toucher.dmgFlags = DMG_SPIN_KOKIRI;
    sGoronAttackCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
    CollisionCheck_SetAT(play, &play->colChkCtx, &sGoronAttackCollider.base);
}

bool PlayMmDekuSpinSfx(Player* player) {
    (void)player;
    const bool layer0 =
        PlayMmInstrumentPcm(kMmVoicePcmKeyDekuSpinBase + 0, "deku-spin-0", kMmSfxDekuSpinAttack, 46, 21, 48, 0.34f,
                            false);
    const bool layer1 =
        PlayMmInstrumentPcm(kMmVoicePcmKeyDekuSpinBase + 1, "deku-spin-1", kMmSfxDekuSpinAttack, 27, 33, 0, 0.42f,
                            false);
    return layer0 || layer1;
}

bool PlayMmDekuHopSfx(Player* player, s32 hopsRemainingBeforeHop) {
    const s32 hopIndex = CLAMP(5 - hopsRemainingBeforeHop, 0, 4);
    (void)player;
    const u16 sfxId = static_cast<u16>(kMmSfxDekuJumpBase + hopIndex);
    const u8 targetNote = static_cast<u8>(60 + (hopIndex == 0 ? 0 : hopIndex == 1 ? 2 : hopIndex == 2 ? 4 : hopIndex == 3 ? 5 : 7));
    const u8 portaNote = static_cast<u8>(36 + (hopIndex == 0 ? 0 : hopIndex == 1 ? 2 : hopIndex == 2 ? 4 : hopIndex == 3 ? 5 : 7));
    const bool layer0 =
        PlayMmTrianglePcm(static_cast<u16>(kMmVoicePcmKeyDekuHopBase + (hopIndex * 2)), "deku-hop-triangle", sfxId,
                          portaNote, 0, 0.28f, false);
    const bool layer1 = PlayMmInstrumentPcm(static_cast<u16>(kMmVoicePcmKeyDekuHopBase + (hopIndex * 2) + 1),
                                            "deku-hop-layer", sfxId, 4, 57, 0, 0.30f, false);
    (void)targetNote;
    return layer0 || layer1;
}

bool PlayMmDekuBubbleChargeSfx() {
    const bool layer0 = PlayMmInstrumentPcm(kMmVoicePcmKeyDekuBubbleBase + 0, "deku-bubble-breath-0",
                                            kMmSfxDekuBubbleBreath, 74, 44, 0, 0.24f, true);
    const bool layer1 = PlayMmTrianglePcm(kMmVoicePcmKeyDekuBubbleBase + 1, "deku-bubble-breath-1",
                                          kMmSfxDekuBubbleBreath, 53, 0, 0.15f, true);
    return layer0 || layer1;
}

bool PlayMmDekuBubbleFireSfx(bool hasMagic) {
    StopMmPcmKeyRange(kMmVoicePcmKeyDekuBubbleBase, 4);
    (void)hasMagic;
    return PlayMmInstrumentPcm(kMmVoicePcmKeyDekuBubbleBase + 2, "deku-bubble-fire", kMmSfxDekuBubbleFire, 74, 52, 6,
                               0.42f, false);
}

void PlayMmDekuStepSfx(Player* player, f32 pitchAdjustment) {
    if (player == nullptr) {
        return;
    }

    (void)pitchAdjustment;
}

void StartDekuSpin(PlayState* play, Player* player, bool waterHopFinish) {
    if (play == nullptr || player == nullptr) {
        return;
    }

    sRuntimeState.dekuAction = DEKU_ACTION_SPIN;
    sRuntimeState.dekuSpinTimer = kDekuSpinInitialTimer;
    sRuntimeState.dekuSpinSpeed = kDekuSpinInitialSpeed;
    sRuntimeState.dekuSpinRotAccum = 0;
    StopDekuSpinTracer();
    EnsureDekuSpinTracer(play);
    sRuntimeState.dekuBubbleCharge = 0.0f;
    sRuntimeState.dekuBubbleChargeTimer = 0;
    sRuntimeState.dekuBubbleAimPitch = 0;
    sRuntimeState.dekuBubbleAimYaw = player->yaw;
    player->stateFlags1 &= ~PLAYER_STATE1_FIRST_PERSON;
    player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS | PLAYER_STATE2_SPIN_ATTACKING;
    player->actor.speedXZ = waterHopFinish ? std::max(player->actor.speedXZ, 4.5f) : std::max(player->actor.speedXZ, 3.0f);
    player->linearVelocity = player->actor.speedXZ;
    PlayMmDekuSpinSfx(player);

    if (sRuntimeState.spinAnimHeader.segment != nullptr) {
        LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.spinAnimHeader, 1.0f, 0.0f,
                             Animation_GetLastFrame(&sRuntimeState.spinAnimHeader), ANIMMODE_ONCE, -2.0f);
        sRuntimeState.motionState = TRANSFORM_MASK_MOTION_SPIN;
    }
}

void StopDekuAction(Player* player) {
    sRuntimeState.dekuAction = DEKU_ACTION_NONE;
    sRuntimeState.dekuBubbleCharge = 0.0f;
    sRuntimeState.dekuBubbleChargeTimer = 0;
    sRuntimeState.dekuSpinTimer = 0;
    sRuntimeState.dekuSpinSpeed = 0.0f;
    sRuntimeState.dekuSpinRotAccum = 0;
    StopDekuSpinTracer();
    if (player != nullptr) {
        player->actor.shape.rot.y = player->yaw;
        player->stateFlags1 &= ~PLAYER_STATE1_FIRST_PERSON;
        player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
        player->stateFlags2 &= ~(PLAYER_STATE2_DISABLE_ROTATION_ALWAYS | PLAYER_STATE2_SPIN_ATTACKING);
    }
}

void StopDekuSpin(Player* player) {
    if (sRuntimeState.dekuAction == DEKU_ACTION_SPIN) {
        sRuntimeState.dekuAction = DEKU_ACTION_NONE;
    }
    sRuntimeState.dekuSpinTimer = 0;
    sRuntimeState.dekuSpinSpeed = 0.0f;
    sRuntimeState.dekuSpinRotAccum = 0;
    StopDekuSpinTracer();
    if (player != nullptr) {
        player->actor.shape.rot.y = player->yaw;
        player->stateFlags2 &= ~(PLAYER_STATE2_DISABLE_ROTATION_ALWAYS | PLAYER_STATE2_SPIN_ATTACKING);
    }
}

void ExitDekuBubbleAimCamera(PlayState* play, Player* player) {
    StopMmPcmKeyRange(kMmVoicePcmKeyDekuBubbleBase, 4);

    if (player == nullptr) {
        return;
    }

    player->stateFlags1 &= ~PLAYER_STATE1_FIRST_PERSON;
    player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;

    if (play != nullptr && !IsZTargeting(player)) {
        Camera* camera = Play_GetCamera(play, MAIN_CAM);
        if (camera != nullptr && camera->mode == CAM_MODE_FIRSTPERSON) {
            Camera_ChangeMode(camera, CAM_MODE_NORMAL);
        }
    }
}

void UpdateDekuBubbleAimFromCamera(PlayState* play, Player* player) {
    if (player == nullptr) {
        return;
    }

    if (IsZTargeting(player)) {
        sRuntimeState.dekuBubbleAimYaw = player->yaw;
        sRuntimeState.dekuBubbleAimPitch = 0;
        return;
    }

    Input* input = play != nullptr ? &play->state.input[0] : nullptr;
    Camera* camera = play != nullptr ? Play_GetCamera(play, MAIN_CAM) : nullptr;
    if (camera != nullptr && (player->stateFlags1 & PLAYER_STATE1_FIRST_PERSON) != 0) {
        if (input != nullptr) {
            constexpr s16 kAimRate = 0x12;
            constexpr s8 kAimDeadzone = 4;
            if (ABS(input->rel.stick_x) > kAimDeadzone) {
                sRuntimeState.dekuBubbleAimYaw -= static_cast<s16>(input->rel.stick_x * kAimRate);
            }
            if (ABS(input->rel.stick_y) > kAimDeadzone) {
                sRuntimeState.dekuBubbleAimPitch =
                    CLAMP(static_cast<s16>(sRuntimeState.dekuBubbleAimPitch - (input->rel.stick_y * kAimRate)),
                          -0x2A00, 0x2A00);
            }
        }

        camera->inputDir.x = sRuntimeState.dekuBubbleAimPitch;
        camera->inputDir.y = sRuntimeState.dekuBubbleAimYaw;
        camera->inputDir.z = 0;
        camera->camDir = camera->inputDir;
        player->actor.focus.rot.x = sRuntimeState.dekuBubbleAimPitch;
        player->actor.focus.rot.y = sRuntimeState.dekuBubbleAimYaw;
        return;
    }

    if (camera != nullptr) {
        const f32 dx = camera->at.x - camera->eye.x;
        const f32 dy = camera->at.y - camera->eye.y;
        const f32 dz = camera->at.z - camera->eye.z;
        const f32 xzDist = sqrtf(SQ(dx) + SQ(dz));

        if (xzDist > 0.1f || fabsf(dy) > 0.1f) {
            sRuntimeState.dekuBubbleAimYaw = Math_Atan2S(dx, dz);
            sRuntimeState.dekuBubbleAimPitch =
                CLAMP(static_cast<s16>(Math_Atan2S(xzDist, dy) - 0x4000), -0x2A00, 0x2A00);
            return;
        }
    }

    sRuntimeState.dekuBubbleAimYaw = player->actor.focus.rot.y != 0 ? player->actor.focus.rot.y : player->yaw;
    sRuntimeState.dekuBubbleAimPitch = player->actor.focus.rot.x;
}

Vec3f GetDekuBubbleMouthPos(PlayState* play, Player* player, s16 yaw, f32 pitch = 0, f32 charge = 0.0f) {
    (void)play;
    (void)yaw;
    (void)pitch;

    Vec3f pos = player->actor.focus.pos;
    if (pos.y < player->actor.world.pos.y + 24.0f) {
        pos = player->actor.world.pos;
        pos.y += 46.0f;
    }

    const f32 forward = 40.0f + (charge * 0.08f);
    pos.x += Math_SinS(player->yaw) * forward;
    pos.y += -8.0f;
    pos.z += Math_CosS(player->yaw) * forward;
    return pos;
}

Vec3f GetDekuBubbleAimPreviewPos(PlayState* play, Player* player) {
    Camera* camera = play != nullptr ? Play_GetCamera(play, MAIN_CAM) : nullptr;
    if (camera == nullptr) {
        return GetDekuBubbleMouthPos(play, player, sRuntimeState.dekuBubbleAimYaw, sRuntimeState.dekuBubbleAimPitch,
                                     sRuntimeState.dekuBubbleCharge);
    }

    Vec3f forward = {
        camera->at.x - camera->eye.x,
        camera->at.y - camera->eye.y,
        camera->at.z - camera->eye.z,
    };
    f32 forwardLen = sqrtf(SQ(forward.x) + SQ(forward.y) + SQ(forward.z));
    if (forwardLen < 0.01f) {
        forward.x = Math_SinS(sRuntimeState.dekuBubbleAimYaw) * Math_CosS(sRuntimeState.dekuBubbleAimPitch);
        forward.y = -Math_SinS(sRuntimeState.dekuBubbleAimPitch);
        forward.z = Math_CosS(sRuntimeState.dekuBubbleAimYaw) * Math_CosS(sRuntimeState.dekuBubbleAimPitch);
        forwardLen = 1.0f;
    }
    forward.x /= forwardLen;
    forward.y /= forwardLen;
    forward.z /= forwardLen;

    Vec3f right = { forward.z, 0.0f, -forward.x };
    f32 rightLen = sqrtf(SQ(right.x) + SQ(right.z));
    if (rightLen < 0.01f) {
        right = { 1.0f, 0.0f, 0.0f };
    } else {
        right.x /= rightLen;
        right.z /= rightLen;
    }

    Vec3f up = {
        (forward.y * right.z) - (forward.z * right.y),
        (forward.z * right.x) - (forward.x * right.z),
        (forward.x * right.y) - (forward.y * right.x),
    };
    f32 upLen = sqrtf(SQ(up.x) + SQ(up.y) + SQ(up.z));
    if (upLen < 0.01f) {
        up = { 0.0f, 1.0f, 0.0f };
    } else {
        up.x /= upLen;
        up.y /= upLen;
        up.z /= upLen;
    }

    constexpr f32 kPreviewForward = 160.0f;
    constexpr f32 kPreviewDown = 22.0f;
    return {
        camera->eye.x + (forward.x * kPreviewForward) - (up.x * kPreviewDown),
        camera->eye.y + (forward.y * kPreviewForward) - (up.y * kPreviewDown),
        camera->eye.z + (forward.z * kPreviewForward) - (up.z * kPreviewDown),
    };
}

void FireDekuBubble(PlayState* play, Player* player) {
    if (player == nullptr) {
        return;
    }

    const bool hasMagic = gSaveContext.magic >= 2;
    f32 charge = hasMagic ? sRuntimeState.dekuBubbleCharge : 1.0f;
    if (hasMagic) {
        gSaveContext.magic -= 2;
    }

    charge = CLAMP(charge, 1.0f, kDekuBubbleMaxCharge);
    DekuBubbleState& bubble = sRuntimeState.dekuBubble;
    bubble.active = true;
    bubble.scale = std::max(charge, kDekuBubbleMinScale);
    bubble.timer = hasMagic ? 99 : 10;
    bubble.state = 0;
    bubble.wobbleAccX = 0;
    bubble.wobbleAccY = 0;
    bubble.rotY = sRuntimeState.dekuBubbleAimYaw;
    bubble.rotX = sRuntimeState.dekuBubbleAimPitch;

    f32 speed = CLAMP(16.0f - bubble.scale, 1.0f, 80.0f);
    bubble.hSpeed = Math_CosS(bubble.rotX) * speed;
    bubble.velY = -Math_SinS(bubble.rotX) * speed;
    bubble.pos = GetDekuBubbleMouthPos(play, player, bubble.rotY, bubble.rotX);
    bubble.spawnPos = bubble.pos;
    bubble.prevPos = bubble.pos;
    player->yaw = bubble.rotY;
    player->actor.world.rot.y = bubble.rotY;
    player->actor.shape.rot.y = bubble.rotY;
    PlayMmDekuBubbleFireSfx(hasMagic);
}

void RegisterDekuBubbleDamageCollider(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || !sRuntimeState.dekuBubble.active) {
        return;
    }

    if (!sDekuBubbleColliderInitialized || sDekuBubbleColliderActor != &player->actor) {
        if (sDekuBubbleColliderInitialized) {
            Collider_DestroyCylinder(play, &sDekuBubbleCollider);
        }
        Collider_InitCylinder(play, &sDekuBubbleCollider);
        Collider_SetCylinder(play, &sDekuBubbleCollider, &player->actor, &sDekuBubbleColliderInit);
        sDekuBubbleColliderInitialized = true;
        sDekuBubbleColliderActor = &player->actor;
    }

    const DekuBubbleState& bubble = sRuntimeState.dekuBubble;
    sDekuBubbleCollider.dim.radius = static_cast<s16>(CLAMP(bubble.scale * 3.0f, 18.0f, 56.0f));
    sDekuBubbleCollider.dim.height = static_cast<s16>(CLAMP(bubble.scale * 3.0f, 18.0f, 56.0f));
    sDekuBubbleCollider.dim.yShift = -sDekuBubbleCollider.dim.height / 2;
    sDekuBubbleCollider.dim.pos.x = static_cast<s16>(bubble.pos.x);
    sDekuBubbleCollider.dim.pos.y = static_cast<s16>(bubble.pos.y);
    sDekuBubbleCollider.dim.pos.z = static_cast<s16>(bubble.pos.z);
    sDekuBubbleCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
    CollisionCheck_SetAT(play, &play->colChkCtx, &sDekuBubbleCollider.base);
}

void UpdateDekuBubbleProjectile(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || !sRuntimeState.dekuBubble.active) {
        return;
    }

    DekuBubbleState& bubble = sRuntimeState.dekuBubble;
    if (sDekuBubbleColliderInitialized && (sDekuBubbleCollider.base.atFlags & (AT_HIT | AT_BOUNCED))) {
        EffectSsBubble_Spawn(play, &bubble.pos, 0.0f, 0.15625f, 0.3125f, 0.0040625f);
        sDekuBubbleCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
        bubble.active = false;
        return;
    }

    if (--bubble.timer <= 0) {
        EffectSsBubble_Spawn(play, &bubble.pos, 0.0f, 0.15625f, 0.3125f, 0.0040625f);
        bubble.active = false;
        return;
    }

    bubble.prevPos = bubble.pos;
    if (bubble.state == 0) {
        const f32 velX = Math_SinS(bubble.rotY) * bubble.hSpeed;
        const f32 velZ = Math_CosS(bubble.rotY) * bubble.hSpeed;
        const f32 totalVelMag = sqrtf(SQ(bubble.hSpeed) + SQ(bubble.velY));

        if (totalVelMag > 0.001f) {
            const f32 ratio = 10.0f / totalVelMag;
            bubble.prevPos.x = bubble.pos.x - (velX * ratio);
            bubble.prevPos.y = bubble.pos.y - (bubble.velY * ratio);
            bubble.prevPos.z = bubble.pos.z - (velZ * ratio);
        }
        bubble.state = 1;
    } else {
        if (Math_StepToF(&bubble.scale, 1.0f, 0.4f)) {
            EffectSsBubble_Spawn(play, &bubble.pos, 0.0f, 0.15625f, 0.3125f, 0.0040625f);
            bubble.active = false;
            return;
        }

        bubble.wobbleAccX += static_cast<s16>(bubble.scale * (500.0f + Rand_ZeroFloat(1400.0f)));
        bubble.rotX += static_cast<s16>(500.0f * Math_SinS(bubble.wobbleAccX));
        bubble.wobbleAccY += static_cast<s16>(bubble.scale * (500.0f + Rand_ZeroFloat(1400.0f)));
        bubble.rotY += static_cast<s16>(500.0f * Math_SinS(bubble.wobbleAccY));

        const f32 speed = CLAMP(16.0f - bubble.scale, 1.0f, 80.0f);
        bubble.hSpeed = Math_CosS(bubble.rotX) * speed;
        bubble.velY = -Math_SinS(bubble.rotX) * speed;
    }

    bubble.pos.x += Math_SinS(bubble.rotY) * bubble.hSpeed;
    bubble.pos.y += bubble.velY;
    bubble.pos.z += Math_CosS(bubble.rotY) * bubble.hSpeed;
    RegisterDekuBubbleDamageCollider(play, player);

    Vec3f hitPos;
    CollisionPoly* hitPoly = nullptr;
    s32 bgId = 0;
    if (BgCheck_EntityLineTest1(&play->colCtx, &bubble.prevPos, &bubble.pos, &hitPos, &hitPoly, true, true, true, true,
                                &bgId)) {
        EffectSsBubble_Spawn(play, &hitPos, 0.0f, 0.15625f, 0.3125f, 0.0040625f);
        bubble.active = false;
    }
}

void TriggerDekuWaterHop(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr) {
        return;
    }

    f32 speed = kDekuWaterHopBaseSpeed * (0.3f + ((5 - sRuntimeState.dekuWaterHopsRemaining) * 0.18f));
    speed = std::max(speed, 4.0f);
    if (player->actor.yDistToWater > -20.0f && player->actor.yDistToWater < 80.0f) {
        player->actor.world.pos.y += player->actor.yDistToWater + kDekuWaterHopSurfaceYOffset;
    }
    player->actor.velocity.y = speed;
    player->actor.bgCheckFlags &= ~1;
    player->stateFlags1 &= ~PLAYER_STATE1_IN_WATER;
    player->stateFlags2 &= ~PLAYER_STATE2_UNDERWATER;
    player->linearVelocity = std::max(player->linearVelocity, 3.5f);
    player->actor.speedXZ = std::max(player->actor.speedXZ, 3.5f);
    Vec3f splashPos = player->actor.world.pos;
    splashPos.y -= kDekuWaterHopSurfaceYOffset;
    EffectSsGSplash_Spawn(play, &splashPos, nullptr, nullptr, speed <= 10.0f ? 0 : 1, static_cast<s16>(speed * 50.0f));
    PlayMmDekuHopSfx(player, sRuntimeState.dekuWaterHopsRemaining);
    OoTxMm_PlayTransformMaskVoiceSfx(player, NA_SE_VO_LI_AUTO_JUMP);

    sRuntimeState.dekuWaterHopsRemaining--;
    if (sRuntimeState.dekuWaterHopsRemaining <= 0) {
        StartDekuSpin(play, player, true);
    } else {
        sRuntimeState.dekuWaterHopAnimTimer = 10;
        if (sRuntimeState.jumpAnimHeader.segment != nullptr) {
            LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.jumpAnimHeader, 1.0f, 0.0f,
                                 Animation_GetLastFrame(&sRuntimeState.jumpAnimHeader), ANIMMODE_ONCE, -2.0f);
            sRuntimeState.motionState = TRANSFORM_MASK_MOTION_JUMP;
        }
    }
}

void UpdateDekuMechanics(PlayState* play, Player* player, const TransformMaskFormDefinition* form) {
    (void)form;
    if (play == nullptr || player == nullptr) {
        return;
    }

    UpdateDekuBubbleProjectile(play, player);
    if ((player->stateFlags2 & PLAYER_STATE2_HOPPING) != 0 && sRuntimeState.dekuAction == DEKU_ACTION_SPIN) {
        StopDekuSpin(player);
    }

    if ((player->actor.bgCheckFlags & 1) != 0 && (player->stateFlags1 & PLAYER_STATE1_IN_WATER) == 0) {
        sRuntimeState.dekuWaterHopsRemaining = 5;
        sRuntimeState.dekuWaterHopAnimTimer = 0;
    } else if (sRuntimeState.dekuWaterHopAnimTimer > 0) {
        sRuntimeState.dekuWaterHopAnimTimer--;
    }

    if (sRuntimeState.dekuAction == DEKU_ACTION_SPIN && sRuntimeState.dekuWaterHopsRemaining > 0 &&
        player->actor.yDistToWater > -8.0f && player->actor.yDistToWater < 48.0f) {
        player->stateFlags1 &= ~PLAYER_STATE1_IN_WATER;
        player->stateFlags2 &= ~PLAYER_STATE2_UNDERWATER;
        TriggerDekuWaterHop(play, player);
        return;
    }

    if ((player->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0 && sRuntimeState.dekuAction != DEKU_ACTION_BUBBLE_AIM) {
        if (sRuntimeState.dekuAction == DEKU_ACTION_SPIN && sRuntimeState.dekuWaterHopsRemaining <= 0 &&
            player->actor.velocity.y > 0.0f) {
            player->stateFlags1 &= ~PLAYER_STATE1_IN_WATER;
        } else if (sRuntimeState.dekuAction == DEKU_ACTION_SPIN && sRuntimeState.dekuWaterHopsRemaining <= 0 &&
                   play->transitionTrigger == TRANS_TRIGGER_OFF && play->transitionMode == TRANS_MODE_OFF) {
            Vec3f splashPos = player->actor.world.pos;
            splashPos.y += player->actor.yDistToWater;
            EffectSsGSplash_Spawn(play, &splashPos, nullptr, nullptr, 1, 500);
            StopDekuAction(player);
            player->currentMask = PLAYER_MASK_NONE;
            gSaveContext.ship.maskMemory = PLAYER_MASK_NONE;
            sRuntimeState.dekuWaterHopsRemaining = 5;
            sRuntimeState.dekuWaterHopAnimTimer = 0;
            Play_TriggerVoidOut(play);
            return;
        } else if (sRuntimeState.dekuAction == DEKU_ACTION_SPIN) {
            return;
        }

        if (sRuntimeState.dekuWaterHopsRemaining > 0) {
            TriggerDekuWaterHop(play, player);
        } else if (play->transitionTrigger == TRANS_TRIGGER_OFF && play->transitionMode == TRANS_MODE_OFF) {
            StopDekuAction(player);
            player->currentMask = PLAYER_MASK_NONE;
            gSaveContext.ship.maskMemory = PLAYER_MASK_NONE;
            sRuntimeState.dekuWaterHopsRemaining = 5;
            sRuntimeState.dekuWaterHopAnimTimer = 0;
            Play_TriggerVoidOut(play);
        }
        return;
    }

    Input* input = &play->state.input[0];
    const bool canUseButtons = CanUseDekuActionButtons(player);
    if (!canUseButtons && sRuntimeState.dekuAction != DEKU_ACTION_BUBBLE_AIM) {
        StopDekuAction(player);
        return;
    }

    if (sRuntimeState.dekuAction == DEKU_ACTION_SPIN) {
        RegisterDekuSpinDamageCollider(play, player);
        player->stateFlags2 |= PLAYER_STATE2_SPIN_ATTACKING | PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;

        const s16 prevYaw = player->yaw;
        f32 speedTarget = 0.0f;
        s16 yawTarget = player->yaw;
        if (Player_GetMovementSpeedAndYaw(player, &speedTarget, &yawTarget, kTransformSpeedModeCurved, play)) {
            const f32 spinMovementMultiplier =
                std::max(kDekuSpinMovementMinMultiplier,
                         1.0f - (0.9f * ((11100.0f - sRuntimeState.dekuSpinSpeed) / 11100.0f)));
            speedTarget *= spinMovementMultiplier * kDekuSpinMovementScale;
            const s16 yawDiff = player->yaw - yawTarget;
            if (ABS(yawDiff) > 0x6000) {
                Math_StepToF(&player->linearVelocity, 0.0f, 1.5f);
            } else {
                Math_StepToF(&player->linearVelocity, speedTarget, 1.5f);
                Math_SmoothStepToS(&player->yaw, yawTarget, 2, 0x320, 0x14);
            }
        }

        sRuntimeState.dekuSpinSpeed -= kDekuSpinSpeedDecay;
        sRuntimeState.dekuSpinRotAccum += static_cast<s16>(sRuntimeState.dekuSpinSpeed) + static_cast<s16>(player->yaw - prevYaw);
        player->actor.shape.rot.y = player->yaw;
        player->actor.focus.rot.y = player->yaw;
        player->actor.focus.rot.x = 0;
        player->actor.focus.rot.z = 0;
        player->actor.speedXZ = player->linearVelocity;
        AddDekuSpinTracerVertex(play, player);

        const f32 absSpeed = fabsf(sRuntimeState.dekuSpinSpeed);
        if (Math_StepToF(&sRuntimeState.dekuSpinTimer, 0.0f, absSpeed)) {
            if (player->actor.yDistToWater > 0.0f && sRuntimeState.dekuWaterHopsRemaining <= 0 &&
                play->transitionTrigger == TRANS_TRIGGER_OFF && play->transitionMode == TRANS_MODE_OFF) {
                Vec3f splashPos = player->actor.world.pos;
                splashPos.y += player->actor.yDistToWater;
                EffectSsGSplash_Spawn(play, &splashPos, nullptr, nullptr, 1, 500);
                player->currentMask = PLAYER_MASK_NONE;
                gSaveContext.ship.maskMemory = PLAYER_MASK_NONE;
                StopDekuAction(player);
                sRuntimeState.dekuWaterHopsRemaining = 5;
                sRuntimeState.dekuWaterHopAnimTimer = 0;
                Play_TriggerVoidOut(play);
                return;
            }
            StopDekuAction(player);
        }
        return;
    }

    if (sRuntimeState.dekuAction == DEKU_ACTION_BUBBLE_AIM) {
        player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
        player->actor.speedXZ = 0.0f;
        player->linearVelocity = 0.0f;

        if (IsZTargeting(player)) {
            player->stateFlags1 &= ~PLAYER_STATE1_FIRST_PERSON;
            player->actor.focus.rot.y = player->yaw;
            player->actor.focus.rot.x = 0;
        } else {
            player->stateFlags1 |= PLAYER_STATE1_FIRST_PERSON;
            Camera* camera = Play_GetCamera(play, MAIN_CAM);
            if (camera != nullptr && camera->mode != CAM_MODE_FIRSTPERSON) {
                Camera_ChangeMode(camera, CAM_MODE_FIRSTPERSON);
            }
        }
        UpdateDekuBubbleAimFromCamera(play, player);

        Math_SmoothStepToF(&sRuntimeState.dekuBubbleCharge, kDekuBubbleMaxCharge, 0.07f, 1.8f, 0.01f);
        sRuntimeState.dekuBubbleChargeTimer++;
        if ((sRuntimeState.dekuBubbleChargeTimer % 8) == 0) {
            PlayMmDekuBubbleChargeSfx();
        }
        if (!CHECK_BTN_ALL(input->cur.button, BTN_B)) {
            FireDekuBubble(play, player);
            sRuntimeState.dekuAction = DEKU_ACTION_BUBBLE_FIRE;
            sRuntimeState.dekuBubbleCharge = 0.0f;
            sRuntimeState.dekuBubbleChargeTimer = 0;
            ExitDekuBubbleAimCamera(play, player);
            if (sRuntimeState.attackAnimHeader.segment != nullptr) {
                LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.attackAnimHeader, 1.0f, 0.0f,
                                     Animation_GetLastFrame(&sRuntimeState.attackAnimHeader), ANIMMODE_ONCE, -2.0f);
                sRuntimeState.motionState = TRANSFORM_MASK_MOTION_ATTACK;
            }
        }
        return;
    }

    if (sRuntimeState.dekuAction == DEKU_ACTION_BUBBLE_FIRE) {
        if (sRuntimeState.skelAnime.animation == &sRuntimeState.attackAnimHeader &&
            !Animation_OnFrame(&sRuntimeState.skelAnime, Animation_GetLastFrame(&sRuntimeState.attackAnimHeader))) {
            return;
        }
        sRuntimeState.dekuAction = DEKU_ACTION_NONE;
    }

    if (!canUseButtons) {
        return;
    }

    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        sRuntimeState.dekuAction = DEKU_ACTION_BUBBLE_AIM;
        sRuntimeState.dekuBubbleCharge = 0.0f;
        sRuntimeState.dekuBubbleChargeTimer = 0;
        sRuntimeState.dekuBubbleAimPitch = 0;
        sRuntimeState.dekuBubbleAimYaw = player->yaw;
        if (sRuntimeState.attackAnimHeader.segment != nullptr) {
            LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.attackAnimHeader, 1.0f, 0.0f,
                                 Animation_GetLastFrame(&sRuntimeState.attackAnimHeader), ANIMMODE_LOOP, -2.0f);
            sRuntimeState.motionState = TRANSFORM_MASK_MOTION_ATTACK;
        }
        return;
    }

    if (!IsZTargeting(player) && CHECK_BTN_ALL(input->press.button, BTN_A) && player->interactRangeActor == nullptr &&
        player->talkActor == nullptr && player->doorType == PLAYER_DOORTYPE_NONE) {
        StartDekuSpin(play, player, false);
    }
}

struct SpecialItemUiState {
    void* dekuMaskIcon = nullptr;
    void* fierceDeityMaskIcon = nullptr;
};

SpecialItemUiState sSpecialItemUiState;

void EnsureSpecialItemUiResourcesLoaded() {
    if (sSpecialItemUiState.dekuMaskIcon == nullptr) {
        sSpecialItemUiState.dekuMaskIcon = TryLoadMmRawPointer(kDekuMaskIconPath, kDekuMaskIconPathOtr);
    }
    if (sSpecialItemUiState.fierceDeityMaskIcon == nullptr) {
        sSpecialItemUiState.fierceDeityMaskIcon =
            TryLoadMmRawPointer(kFierceDeityMaskIconPath, kFierceDeityMaskIconPathOtr);
    }
}

const TransformMaskFormDefinition* GetFormDefinition(u8 playerMask) {
    for (const auto& form : kTransformForms) {
        if (form.playerMask == playerMask) {
            return &form;
        }
    }

    return nullptr;
}

template <typename T>
std::shared_ptr<T> LoadMmResource(const char* path) {
    if (path == nullptr) {
        return nullptr;
    }

    const auto mmArchive = SohCrossover::GetMmO2rArchive();
    if (mmArchive == nullptr) {
        return nullptr;
    }

    auto resourceManager = Ship::Context::GetInstance()->GetResourceManager();
    if (resourceManager == nullptr) {
        return nullptr;
    }

    return std::static_pointer_cast<T>(
        resourceManager->LoadResource(Ship::ResourceIdentifier(path, reinterpret_cast<uintptr_t>(mmArchive.get()), mmArchive), true));
}

FlexSkeletonHeader* TryLoadMmSkeleton(const char* primaryPath, const char* otrPath) {
    if (auto skeleton = LoadMmResource<SOH::Skeleton>(primaryPath)) {
        return reinterpret_cast<FlexSkeletonHeader*>(skeleton->GetPointer());
    }

    if (auto skeleton = LoadMmResource<SOH::Skeleton>(otrPath)) {
        return reinterpret_cast<FlexSkeletonHeader*>(skeleton->GetPointer());
    }

    return nullptr;
}

std::shared_ptr<SOH::PlayerAnimation> TryLoadMmPlayerAnimData(const char* primaryPath, const char* otrPath) {
    if (auto animation = LoadMmResource<SOH::PlayerAnimation>(primaryPath)) {
        return animation;
    }

    if (auto animation = LoadMmResource<SOH::PlayerAnimation>(otrPath)) {
        return animation;
    }

    return nullptr;
}

void* TryLoadMmRawPointer(const char* primaryPath, const char* otrPath) {
    const auto mmArchive = SohCrossover::GetMmO2rArchive();
    if (mmArchive == nullptr) {
        return nullptr;
    }

    auto resourceManager = Ship::Context::GetInstance()->GetResourceManager();
    if (resourceManager == nullptr) {
        return nullptr;
    }

    auto tryLoad = [&](const char* path) -> void* {
        if (path == nullptr) {
            return nullptr;
        }

        auto resource =
            resourceManager->LoadResource(Ship::ResourceIdentifier(path, reinterpret_cast<uintptr_t>(mmArchive.get()), mmArchive), true);
        return resource != nullptr ? resource->GetRawPointer() : nullptr;
    };

    if (void* pointer = tryLoad(primaryPath)) {
        return pointer;
    }

    return tryLoad(otrPath);
}

std::shared_ptr<Fast::DisplayList> TryLoadMmDisplayListResource(const char* primaryPath, const char* otrPath) {
    if (auto displayList = LoadMmResource<Fast::DisplayList>(primaryPath)) {
        return displayList;
    }

    return LoadMmResource<Fast::DisplayList>(otrPath);
}

std::shared_ptr<SOH::AudioSoundFont> TryLoadMmAudioSoundFont(const char* primaryPath, const char* otrPath) {
    if (auto font = LoadMmResource<SOH::AudioSoundFont>(primaryPath)) {
        return font;
    }

    return LoadMmResource<SOH::AudioSoundFont>(otrPath);
}

void DecodeVadpcmFrame(const u8* frame, s32 frameSize, s16* out, const s16* book, s32 order, s16* hist) {
    const s32 shift = frame[0] >> 4;
    const s32 pred = frame[0] & 0xF;
    const s16* tbl0 = &book[pred * order * 8];
    const s16* tbl1 = &book[pred * order * 8 + 8];
    const u8* data = &frame[1];

    for (s32 half = 0; half < 2; ++half) {
        s16 ins[8];
        const s16 prev1 = hist[1];
        const s16 prev2 = hist[0];

        if (frameSize == kVadpcmFrameSmallAdpcm) {
            const u8* p = &data[half * 2];
            for (s32 j = 0; j < 2; ++j) {
                ins[j * 4 + 0] = static_cast<s16>((static_cast<s32>(p[j] >> 6) << 30) >> 30) << shift;
                ins[j * 4 + 1] = static_cast<s16>(((static_cast<s32>(p[j] >> 4) & 0x3) << 30) >> 30) << shift;
                ins[j * 4 + 2] = static_cast<s16>(((static_cast<s32>(p[j] >> 2) & 0x3) << 30) >> 30) << shift;
                ins[j * 4 + 3] = static_cast<s16>(((static_cast<s32>(p[j]) & 0x3) << 30) >> 30) << shift;
            }
        } else {
            const u8* p = &data[half * 4];
            for (s32 j = 0; j < 4; ++j) {
                ins[j * 2 + 0] = static_cast<s16>((static_cast<s32>(p[j] >> 4) << 28) >> 28) << shift;
                ins[j * 2 + 1] = static_cast<s16>(((static_cast<s32>(p[j]) & 0xF) << 28) >> 28) << shift;
            }
        }

        for (s32 j = 0; j < 8; ++j) {
            s32 acc = tbl0[j] * prev2 + tbl1[j] * prev1 + (static_cast<s32>(ins[j]) << 11);
            for (s32 k = 0; k < j; ++k) {
                acc += tbl1[(j - k) - 1] * ins[k];
            }
            acc >>= 11;
            out[half * 8 + j] = static_cast<s16>(CLAMP(acc, -32768, 32767));
        }

        hist[0] = out[half * 8 + 6];
        hist[1] = out[half * 8 + 7];
    }
}

s16* DecodeVadpcmSample(SoundFontSample* sample, u32* outLength) {
    if (sample == nullptr || sample->sampleAddr == nullptr || sample->size == 0 || outLength == nullptr) {
        return nullptr;
    }

    s32 frameSize;
    switch (sample->codec) {
        case CODEC_ADPCM:
            frameSize = kVadpcmFrameAdpcm;
            break;
        case CODEC_SMALL_ADPCM:
            frameSize = kVadpcmFrameSmallAdpcm;
            break;
        default:
            SPDLOG_WARN("[OoTxMM Transform] Unsupported MM voice sample codec {}", static_cast<int>(sample->codec));
            return nullptr;
    }

    if (sample->book == nullptr || sample->book->book == nullptr) {
        SPDLOG_WARN("[OoTxMM Transform] MM voice sample missing ADPCM book");
        return nullptr;
    }

    u32 numFrames = sample->size / frameSize;
    u32 totalSamples = numFrames * kVadpcmSamplesPerFrame;
    if (sample->loop != nullptr && sample->loop->loopEnd > 0 && sample->loop->loopEnd < totalSamples) {
        totalSamples = sample->loop->loopEnd;
        numFrames = (totalSamples + kVadpcmSamplesPerFrame - 1) / kVadpcmSamplesPerFrame;
    }

    s16* pcm = static_cast<s16*>(std::malloc(totalSamples * sizeof(s16)));
    if (pcm == nullptr) {
        return nullptr;
    }

    s16 hist[2] = { 0, 0 };
    const u8* src = reinterpret_cast<const u8*>(sample->sampleAddr);
    for (u32 frame = 0; frame < numFrames; ++frame) {
        s16 framePcm[kVadpcmSamplesPerFrame];
        DecodeVadpcmFrame(&src[frame * frameSize], frameSize, framePcm, sample->book->book, sample->book->order, hist);

        const u32 dstOffset = frame * kVadpcmSamplesPerFrame;
        const u32 sampleCount = std::min<u32>(kVadpcmSamplesPerFrame, totalSamples - dstOffset);
        std::memcpy(&pcm[dstOffset], framePcm, sampleCount * sizeof(s16));
    }

    *outLength = totalSamples;
    return pcm;
}

void StopMmVoicePcmSlot(MmVoicePcmSound& sound) {
    sound.active = false;
    if (sound.pcmData != nullptr) {
        std::free(sound.pcmData);
        sound.pcmData = nullptr;
    }
    sound.pcmLength = 0;
    sound.pcmPosition = 0.0f;
    sound.loop = false;
    sound.key = kMmVoicePcmKeyNone;
}

bool StartMmVoicePcm(u16 key, const char* debugName, u16 mmSfxId, ::SoundFontSound* sound, f32 pitchScale,
                     f32 pitchMultiplier, f32 velocity, bool loop = false) {
    if (sound == nullptr || sound->sample == nullptr || sound->sample->sampleAddr == nullptr) {
        SPDLOG_WARN("[OoTxMM Transform] MM voice {} has no decodable sample", debugName != nullptr ? debugName : "<null>");
        return false;
    }

    u32 pcmLength = 0;
    s16* pcm = DecodeVadpcmSample(sound->sample, &pcmLength);
    if (pcm == nullptr || pcmLength == 0) {
        if (pcm != nullptr) {
            std::free(pcm);
        }
        SPDLOG_WARN("[OoTxMM Transform] MM voice {} ADPCM decode failed", debugName != nullptr ? debugName : "<null>");
        return false;
    }

    MmVoicePcmSound* target = nullptr;
    for (auto& existing : sMmVoicePcmSounds) {
        if (existing.active && existing.key == key) {
            if (loop && existing.loop) {
                existing.volume = CLAMP(velocity, 0.0f, 1.0f);
                existing.release = false;
                return true;
            }
            StopMmVoicePcmSlot(existing);
            target = &existing;
            break;
        }
    }

    if (target == nullptr) {
        for (auto& existing : sMmVoicePcmSounds) {
            if (!existing.active) {
                target = &existing;
                break;
            }
        }
    }

    if (target == nullptr) {
        target = &sMmVoicePcmSounds[0];
        StopMmVoicePcmSlot(*target);
    }

    target->pcmData = pcm;
    target->pcmLength = pcmLength;
    target->pcmPosition = 0.0f;
    target->advance = sound->tuning * pitchMultiplier * pitchScale;
    target->volume = CLAMP(velocity, 0.0f, 1.0f);
    target->envVolume = 0.0f;
    target->envReleaseAt =
        loop ? 0.0f : std::max<f32>(0.0f, (pcmLength / std::max(target->advance, 0.01f)) - 320.0f);
    target->release = false;
    target->loop = loop;
    target->key = key;
    target->active = true;
    SPDLOG_INFO("[OoTxMM Transform] Playing MM voice PCM {} (sfx 0x{:04X}, len {}, advance {:.3f})",
                debugName != nullptr ? debugName : "<null>", mmSfxId, pcmLength, target->advance);
    return true;
}

::SoundFontSound* GetMmInstrumentSound(u8 instrumentIndex, u8 note, const char* debugName) {
    if (!EnsureMmGoronDirectAudioReady() || instrumentIndex >= sMmGoronDirectAudioState.mmFont->numInstruments) {
        SPDLOG_WARN("[OoTxMM Transform] MM SFX {} invalid instrument {}", debugName, instrumentIndex);
        return nullptr;
    }

    Instrument* instrument =
        reinterpret_cast<Instrument*>(sMmGoronDirectAudioState.mmFont->instruments[instrumentIndex]);
    ::SoundFontSound* sound = instrument != nullptr ? Audio_InstrumentGetSound(instrument, note) : nullptr;
    if (sound == nullptr || sound->sample == nullptr || sound->sample->sampleAddr == nullptr) {
        SPDLOG_WARN("[OoTxMM Transform] MM SFX {} missing instrument {} note {}", debugName, instrumentIndex, note);
        return nullptr;
    }
    return sound;
}

bool PlayMmInstrumentPcm(u16 key, const char* debugName, u16 mmSfxId, u8 instrumentIndex, u8 note, s8 transpose,
                         f32 volume, bool loop) {
    ::SoundFontSound* sound = GetMmInstrumentSound(instrumentIndex, note, debugName);
    const f32 notePitch = powf(2.0f, (static_cast<f32>(note) + transpose - 60.0f) / 12.0f);
    return StartMmVoicePcm(key, debugName, mmSfxId, sound, 1.0f, notePitch, volume, loop);
}

bool StartMmOwnedRawPcm(u16 key, const char* debugName, u16 mmSfxId, s16* pcm, u32 pcmLength, f32 advance, f32 volume,
                        bool loop = false) {
    if (pcm == nullptr || pcmLength == 0) {
        return false;
    }

    MmVoicePcmSound* target = nullptr;
    for (auto& existing : sMmVoicePcmSounds) {
        if (existing.active && existing.key == key) {
            if (loop && existing.loop) {
                existing.volume = CLAMP(volume, 0.0f, 1.0f);
                existing.release = false;
                std::free(pcm);
                return true;
            }
            StopMmVoicePcmSlot(existing);
            target = &existing;
            break;
        }
    }

    if (target == nullptr) {
        for (auto& existing : sMmVoicePcmSounds) {
            if (!existing.active) {
                target = &existing;
                break;
            }
        }
    }

    if (target == nullptr) {
        target = &sMmVoicePcmSounds[0];
        StopMmVoicePcmSlot(*target);
    }

    target->pcmData = pcm;
    target->pcmLength = pcmLength;
    target->pcmPosition = 0.0f;
    target->advance = advance;
    target->volume = CLAMP(volume, 0.0f, 1.0f);
    target->envVolume = 0.0f;
    target->envReleaseAt =
        loop ? 0.0f : std::max<f32>(0.0f, (pcmLength / std::max(advance, 0.01f)) - 320.0f);
    target->release = false;
    target->loop = loop;
    target->key = key;
    target->active = true;
    SPDLOG_INFO("[OoTxMM Transform] Playing MM raw PCM {} (sfx 0x{:04X}, len {}, advance {:.3f})",
                debugName != nullptr ? debugName : "<null>", mmSfxId, pcmLength, target->advance);
    return true;
}

bool PlayMmTrianglePcm(u16 key, const char* debugName, u16 mmSfxId, u8 note, s8 transpose, f32 volume, bool loop) {
    constexpr s32 kTriangleCycleLen = 64;
    constexpr s32 kTriangleNumCycles = 200;
    constexpr u32 kTriangleTotalSamples = kTriangleCycleLen * kTriangleNumCycles;
    constexpr f32 kTriangleSampleRate = 16744.0f;
    constexpr s16 kTriangleAmplitude = 10000;

    s16* pcm = static_cast<s16*>(std::malloc(kTriangleTotalSamples * sizeof(s16)));
    if (pcm == nullptr) {
        return false;
    }

    for (u32 i = 0; i < kTriangleTotalSamples; ++i) {
        const s32 phase = static_cast<s32>(i % kTriangleCycleLen);
        constexpr s32 half = kTriangleCycleLen / 2;
        if (phase < half) {
            pcm[i] = static_cast<s16>(-kTriangleAmplitude + ((2 * kTriangleAmplitude * phase) / half));
        } else {
            pcm[i] =
                static_cast<s16>(kTriangleAmplitude - ((2 * kTriangleAmplitude * (phase - half)) / half));
        }
    }

    const f32 advance = (kTriangleSampleRate / 32000.0f) *
                        powf(2.0f, (static_cast<f32>(note) + transpose - 60.0f) / 12.0f);
    return StartMmOwnedRawPcm(key, debugName, mmSfxId, pcm, kTriangleTotalSamples, advance, volume, loop);
}

void StopMmPcmByKey(u16 key) {
    for (auto& sound : sMmVoicePcmSounds) {
        if (sound.active && sound.key == key) {
            StopMmVoicePcmSlot(sound);
        }
    }
}

void StopMmPcmKeyRange(u16 baseKey, s32 count) {
    for (s32 i = 0; i < count; ++i) {
        StopMmPcmByKey(static_cast<u16>(baseKey + i));
    }
}

void PlayMmZoraSwimDashSfx() {
    PlayMmInstrumentPcm(kMmVoicePcmKeyZoraSwimDashBase + 0, "zora-swim-dash-0", 0x08EC, 72, 53, 0, 0.48f, false);
    PlayMmInstrumentPcm(kMmVoicePcmKeyZoraSwimDashBase + 1, "zora-swim-dash-1", 0x08EC, 27, 41, -12, 0.38f, false);
    PlayMmInstrumentPcm(kMmVoicePcmKeyZoraSwimDashBase + 2, "zora-swim-dash-2", 0x08EC, 20, 73, 0, 0.38f, false);
}

void UpdateMmZoraBarrierSfx(bool active) {
    if (active) {
        PlayMmInstrumentPcm(kMmVoicePcmKeyZoraBarrierBase + 0, "zora-barrier-0", 0x09AF, 46, 64, 48, 0.34f, true);
        PlayMmInstrumentPcm(kMmVoicePcmKeyZoraBarrierBase + 1, "zora-barrier-1", 0x09AF, 46, 55, 0, 0.28f, true);
    } else {
        StopMmPcmByKey(kMmVoicePcmKeyZoraBarrierBase + 0);
        StopMmPcmByKey(kMmVoicePcmKeyZoraBarrierBase + 1);
    }
}

void PlayMmZoraStepSfx(Player* player, f32 pitchAdjustment) {
    if (player == nullptr) {
        return;
    }

    PlayMmInstrumentPcm(kMmVoicePcmKeyZoraStepBase, "zora-step", 0x08F0, 31, 23, 0, 0.22f, false);
    const u16 floorSfx =
        player->currentBoots == PLAYER_BOOTS_IRON ? NA_SE_PL_WALK_HEAVYBOOTS : NA_SE_PL_WALK_GROUND + player->floorSfxOffset;
    func_800F4010(&player->actor.projectedPos, floorSfx, pitchAdjustment);
}

bool PlayMmGoronVoicePcm(MmGoronVoiceSlot slot, f32 pitchScale) {
    if (slot >= MM_GORON_VOICE_COUNT || !EnsureMmGoronDirectAudioReady()) {
        return false;
    }

    const auto& def = kMmGoronVoiceDefinitions[slot];
    if (!def.useSoundEffect) {
        return false;
    }

    ::SoundFontSound* sound = GetMmGoronDirectSound(slot);
    return StartMmVoicePcm(static_cast<u16>(kMmVoicePcmKeyGoronBase + slot), def.debugName, def.mmSfxId, sound,
                           pitchScale, def.pitchMultiplier, def.velocity);
}

void ResetMmGoronDirectVoice(MmGoronDirectVoice& voice) {
    if (voice.layer.note != nullptr) {
        Audio_SeqLayerNoteDecay(&voice.layer);
        voice.layer.note = nullptr;
    }

    std::memset(&voice.channel, 0, sizeof(voice.channel));
    std::memset(&voice.layer, 0, sizeof(voice.layer));
    voice.initialized = false;
    voice.active = false;
    voice.looped = false;
    voice.oneShotFramesRemaining = 0;
    voice.instrumentIndex = 0;
    voice.note = 0;
    voice.debugName = nullptr;
}

bool EnsureMmGoronDirectAudioReady() {
    if (sMmGoronDirectAudioState.fontReady && sMmGoronDirectAudioState.mmFont != nullptr) {
        return true;
    }

    sMmGoronDirectAudioState.initAttempts++;
    auto mmSoundFont = TryLoadMmAudioSoundFont("audio/fonts/Soundfont_0", "__OTR__audio/fonts/Soundfont_0");
    if (mmSoundFont == nullptr) {
        if (sMmGoronDirectAudioState.initAttempts <= 3 || (sMmGoronDirectAudioState.initAttempts % 60) == 0) {
            SPDLOG_WARN("[OoTxMM Transform] MM direct Goron audio: failed to load Soundfont_0 (attempt {})",
                        sMmGoronDirectAudioState.initAttempts);
        }
        return false;
    }

    sMmGoronDirectAudioState.mmFont = mmSoundFont->GetPointer();
    sMmGoronDirectAudioState.fontReady =
        sMmGoronDirectAudioState.mmFont != nullptr && sMmGoronDirectAudioState.mmFont->instruments != nullptr;
    if (!sMmGoronDirectAudioState.fontReady) {
        SPDLOG_WARN("[OoTxMM Transform] MM direct Goron audio: Soundfont_0 missing instruments");
        sMmGoronDirectAudioState.mmFont = nullptr;
        return false;
    }

    SPDLOG_INFO("[OoTxMM Transform] MM direct Goron audio ready ({} instruments, {} sfx)",
                sMmGoronDirectAudioState.mmFont->numInstruments, sMmGoronDirectAudioState.mmFont->numSfx);
    return true;
}

bool EnsureMmGoronDirectVoiceInitialized(MmGoronVoiceSlot slot) {
    if (!EnsureMmGoronDirectAudioReady()) {
        return false;
    }

    auto& voice = sMmGoronDirectAudioState.voices[slot];
    if (voice.initialized) {
        return true;
    }

    const auto& def = kMmGoronVoiceDefinitions[slot];
    SequencePlayer* seqPlayer = &gAudioContext.seqPlayers[SEQ_PLAYER_SFX];

    std::memset(&voice.channel, 0, sizeof(voice.channel));
    std::memset(&voice.layer, 0, sizeof(voice.layer));
    voice.channel.enabled = true;
    voice.channel.finished = false;
    voice.channel.stopScript = false;
    voice.channel.largeNotes = false;
    voice.channel.noteAllocPolicy = 8;
    voice.channel.muteBehavior = 0;
    voice.channel.reverb = 0;
    voice.channel.notePriority = 10;
    voice.channel.someOtherPriority = 10;
    voice.channel.fontId = kMmDirectGoronAudioFontId;
    voice.channel.reverbIndex = 0;
    voice.channel.bookOffset = 0;
    voice.channel.pan = 64;
    voice.channel.newPan = 64;
    voice.channel.panChannelWeight = 128;
    voice.channel.velocityRandomVariance = 0;
    voice.channel.gateTimeRandomVariance = 0;
    voice.channel.volume = 1.0f;
    voice.channel.volumeScale = 1.0f;
    voice.channel.freqScale = 1.0f;
    voice.channel.changes.asByte = 0xFF;
    voice.channel.scriptState.depth = 0;
    voice.channel.adsr.envelope = gDefaultEnvelope;
    voice.channel.adsr.releaseRate = 0xF0;
    voice.channel.adsr.sustain = 0;
    voice.channel.vibratoRateTarget = 0x800;
    voice.channel.vibratoRateStart = 0x800;
    voice.channel.vibratoExtentTarget = 0;
    voice.channel.vibratoExtentStart = 0;
    voice.channel.vibratoRateChangeDelay = 0;
    voice.channel.vibratoExtentChangeDelay = 0;
    voice.channel.vibratoDelay = 0;
    voice.channel.filter = nullptr;
    voice.channel.unk_20 = 0;
    voice.channel.unk_0F = 0;
    voice.channel.seqPlayer = seqPlayer;
    voice.channel.layers[0] = &voice.layer;
    Audio_InitNoteLists(&voice.channel.notePool);

    voice.layer.channel = &voice.channel;
    voice.layer.adsr = voice.channel.adsr;
    voice.layer.adsr.releaseRate = 0;
    voice.layer.enabled = true;
    voice.layer.finished = false;
    voice.layer.stopSomething = false;
    voice.layer.continuousNotes = def.looped;
    voice.layer.bit3 = false;
    voice.layer.ignoreDrumPan = false;
    voice.layer.bit1 = false;
    voice.layer.notePropertiesNeedInit = false;
    voice.layer.stereo.asByte = 0;
    voice.layer.pan = 64;
    voice.layer.notePan = 64;
    voice.layer.portamento.mode = 0;
    voice.layer.scriptState.depth = 0;
    voice.layer.gateTime = 0x80;
    voice.layer.transposition = 0;
    voice.layer.delay = 0;
    voice.layer.gateDelay = 0;
    voice.layer.delay2 = 0;
    voice.layer.note = nullptr;
    voice.layer.instrument = nullptr;
    voice.layer.freqScale = 1.0f;
    voice.layer.unk_34 = 1.0f;
    voice.layer.velocitySquare = 1.0f;
    voice.layer.velocitySquare2 = 1.0f;
    voice.layer.noteVelocity = def.velocity;
    voice.layer.noteFreqScale = 1.0f;
    voice.layer.sound = nullptr;
    voice.layer.instOrWave = 0;
    voice.layer.semitone = def.note;
    voice.layer.portamento.extent = 0.0f;
    voice.layer.portamento.speed = 0.0f;

    voice.debugName = def.debugName;
    voice.instrumentIndex = def.instrumentIndex;
    voice.note = def.note;
    voice.looped = def.looped;
    voice.oneShotFramesRemaining = 0;
    voice.active = false;
    voice.initialized = true;
    SPDLOG_INFO("[OoTxMM Transform] Initialized MM direct Goron voice {}", def.debugName);
    return true;
}

u16 PickMmGoronVoiceEffectIndex(u16 action) {
    struct VoiceEffectAlternatives {
        std::array<u16, 4> effects;
        u8 count;
    };

    constexpr std::array<VoiceEffectAlternatives, 32> kMmGoronVoiceEffectIndices = {{
        { { 256, 257, 258, 259 }, 4 }, { { 260, 261, 0, 0 }, 2 },     { { 266, 267, 0, 0 }, 2 },
        { { 262, 281, 0, 0 }, 2 },     { { 263, 264, 0, 0 }, 2 },     { { 265, 266, 267, 0 }, 3 },
        { { 268, 269, 270, 0 }, 3 },   { { 273, 274, 0, 0 }, 2 },     { { 271, 272, 0, 0 }, 2 },
        { { 275, 279, 0, 0 }, 2 },     { { 279, 0, 0, 0 }, 1 },       { { 276, 0, 0, 0 }, 1 },
        { { 271, 272, 0, 0 }, 2 },     { { 265, 266, 267, 0 }, 3 },   { { 256, 0, 0, 0 }, 1 },
        { { 259, 0, 0, 0 }, 1 },       { { 283, 0, 0, 0 }, 1 },       { { 261, 0, 0, 0 }, 1 },
        { { 265, 266, 267, 0 }, 3 },   { { 260, 0, 0, 0 }, 1 },       { { 262, 0, 0, 0 }, 1 },
        { { 262, 0, 0, 0 }, 1 },       { { 268, 0, 0, 0 }, 1 },       { { 256, 0, 0, 0 }, 1 },
        { { 256, 0, 0, 0 }, 1 },       { { 271, 272, 0, 0 }, 2 },     { { 256, 0, 0, 0 }, 1 },
        { { 256, 0, 0, 0 }, 1 },       { { 256, 0, 0, 0 }, 1 },       { { 256, 0, 0, 0 }, 1 },
        { { 256, 0, 0, 0 }, 1 },       { { 256, 0, 0, 0 }, 1 },
    }};

    const auto& alternatives =
        action < kMmGoronVoiceEffectIndices.size() ? kMmGoronVoiceEffectIndices[action]
                                                       : kMmGoronVoiceEffectIndices[0];
    const u32 randomIndex = alternatives.count > 1 ? (gAudioContext.audioRandom % alternatives.count) : 0;
    return alternatives.effects[randomIndex];
}

u16 PickMmZoraVoiceEffectIndex(u16 action) {
    struct VoiceEffectAlternatives {
        std::array<u16, 4> effects;
        u8 count;
    };

    constexpr std::array<VoiceEffectAlternatives, 32> kMmZoraVoiceEffectIndices = {{
        { { 192, 193, 194, 195 }, 4 }, { { 196, 197, 0, 0 }, 2 },     { { 202, 203, 0, 0 }, 2 },
        { { 198, 217, 0, 0 }, 2 },     { { 199, 200, 0, 0 }, 2 },     { { 201, 202, 203, 0 }, 3 },
        { { 204, 205, 206, 0 }, 3 },   { { 209, 210, 0, 0 }, 2 },     { { 207, 208, 0, 0 }, 2 },
        { { 211, 215, 0, 0 }, 2 },     { { 192, 0, 0, 0 }, 1 },       { { 192, 0, 0, 0 }, 1 },
        { { 207, 208, 0, 0 }, 2 },     { { 201, 202, 203, 0 }, 3 },   { { 192, 0, 0, 0 }, 1 },
        { { 192, 0, 0, 0 }, 1 },       { { 192, 0, 0, 0 }, 1 },       { { 192, 0, 0, 0 }, 1 },
        { { 192, 0, 0, 0 }, 1 },       { { 192, 0, 0, 0 }, 1 },       { { 192, 0, 0, 0 }, 1 },
        { { 192, 0, 0, 0 }, 1 },       { { 199, 200, 0, 0 }, 2 },     { { 192, 0, 0, 0 }, 1 },
        { { 204, 205, 206, 0 }, 3 },   { { 192, 0, 0, 0 }, 1 },       { { 192, 0, 0, 0 }, 1 },
        { { 192, 0, 0, 0 }, 1 },       { { 192, 0, 0, 0 }, 1 },       { { 192, 0, 0, 0 }, 1 },
        { { 192, 0, 0, 0 }, 1 },       { { 192, 0, 0, 0 }, 1 },
    }};

    const auto& alternatives =
        action < kMmZoraVoiceEffectIndices.size() ? kMmZoraVoiceEffectIndices[action]
                                                      : kMmZoraVoiceEffectIndices[0];
    const u32 randomIndex = alternatives.count > 1 ? (gAudioContext.audioRandom % alternatives.count) : 0;
    return alternatives.effects[randomIndex];
}

u16 PickMmDekuVoiceEffectIndex(u16 action) {
    struct VoiceEffectAlternatives {
        std::array<u16, 4> effects;
        u8 count;
    };

    constexpr std::array<VoiceEffectAlternatives, 32> kMmDekuVoiceEffectIndices = {{
        { { 128, 129, 130, 131 }, 4 }, { { 132, 133, 0, 0 }, 2 },     { { 138, 139, 0, 0 }, 2 },
        { { 134, 153, 0, 0 }, 2 },     { { 135, 136, 0, 0 }, 2 },     { { 137, 138, 139, 0 }, 3 },
        { { 140, 141, 142, 0 }, 3 },   { { 145, 146, 0, 0 }, 2 },     { { 143, 144, 0, 0 }, 2 },
        { { 147, 151, 0, 0 }, 2 },     { { 128, 0, 0, 0 }, 1 },       { { 128, 0, 0, 0 }, 1 },
        { { 143, 144, 0, 0 }, 2 },     { { 137, 138, 139, 0 }, 3 },   { { 128, 0, 0, 0 }, 1 },
        { { 128, 0, 0, 0 }, 1 },       { { 128, 0, 0, 0 }, 1 },       { { 128, 0, 0, 0 }, 1 },
        { { 128, 0, 0, 0 }, 1 },       { { 128, 0, 0, 0 }, 1 },       { { 128, 0, 0, 0 }, 1 },
        { { 128, 0, 0, 0 }, 1 },       { { 135, 136, 0, 0 }, 2 },     { { 128, 0, 0, 0 }, 1 },
        { { 140, 141, 142, 0 }, 3 },   { { 128, 0, 0, 0 }, 1 },       { { 128, 0, 0, 0 }, 1 },
        { { 128, 0, 0, 0 }, 1 },       { { 128, 0, 0, 0 }, 1 },       { { 128, 0, 0, 0 }, 1 },
        { { 128, 0, 0, 0 }, 1 },       { { 128, 0, 0, 0 }, 1 },
    }};

    const auto& alternatives =
        action < kMmDekuVoiceEffectIndices.size() ? kMmDekuVoiceEffectIndices[action]
                                                      : kMmDekuVoiceEffectIndices[0];
    const u32 randomIndex = alternatives.count > 1 ? (gAudioContext.audioRandom % alternatives.count) : 0;
    return alternatives.effects[randomIndex];
}

::SoundFontSound* GetMmVoiceEffectSound(u16 candidateIndex, const char* debugName) {
    if (!EnsureMmGoronDirectAudioReady()) {
        return nullptr;
    }
    if (sMmGoronDirectAudioState.mmFont->soundEffects == nullptr) {
        SPDLOG_WARN("[OoTxMM Transform] MM voice {} has no sfx table", debugName != nullptr ? debugName : "<null>");
        return nullptr;
    }
    if (candidateIndex >= sMmGoronDirectAudioState.mmFont->numSfx) {
        SPDLOG_WARN("[OoTxMM Transform] MM voice {} invalid sfx index {}", debugName != nullptr ? debugName : "<null>",
                    candidateIndex);
        return nullptr;
    }

    ::SoundFontSound* sound =
        reinterpret_cast<::SoundFontSound*>(&sMmGoronDirectAudioState.mmFont->soundEffects[candidateIndex]);
    if (sound == nullptr || sound->sample == nullptr || sound->sample->sampleAddr == nullptr) {
        SPDLOG_WARN("[OoTxMM Transform] MM voice {} missing sample for sfx index {}",
                    debugName != nullptr ? debugName : "<null>", candidateIndex);
        return nullptr;
    }
    return sound;
}

bool PlayMmSfxPcmById(u16 key, const char* debugName, u16 mmSfxId, f32 volume, bool loop, f32 pitchScale) {
    EnsureMmGoronSfxPatched();
    const u16 effectIndex = static_cast<u16>(mmSfxId & kMmSfxIndexMask);
    ::SoundFontSound* sound = GetMmVoiceEffectSound(effectIndex, debugName);
    return StartMmVoicePcm(key, debugName, mmSfxId, sound, pitchScale, 1.0f, volume, loop);
}

bool PlayMmDekuVoiceOneShot(u16 mmSfxId, f32 pitchScale) {
    EnsureMmGoronSfxPatched();
    if (!sMmGoronSfxState.patchApplied || mmSfxId < 0x6880 || mmSfxId > 0x689F) {
        return false;
    }

    const u16 action = static_cast<u16>(mmSfxId - 0x6880);
    const u16 effectIndex = PickMmDekuVoiceEffectIndex(action);
    ::SoundFontSound* sound = GetMmVoiceEffectSound(effectIndex, "deku-voice");
    if (sound == nullptr && effectIndex != static_cast<u16>(128 + action)) {
        sound = GetMmVoiceEffectSound(static_cast<u16>(128 + action), "deku-voice-fallback");
    }
    return StartMmVoicePcm(static_cast<u16>(kMmVoicePcmKeyDekuVoiceBase + action), "deku-voice", mmSfxId, sound,
                           pitchScale, 1.0f, 0.70f);
}

bool PlayMmZoraVoiceOneShot(u16 mmSfxId, f32 pitchScale = 1.0f) {
    EnsureMmGoronSfxPatched();
    if (!sMmGoronSfxState.patchApplied || mmSfxId < 0x68A0 || mmSfxId > 0x68BF) {
        return false;
    }

    const u16 action = static_cast<u16>(mmSfxId - 0x68A0);
    const u16 effectIndex = PickMmZoraVoiceEffectIndex(action);
    ::SoundFontSound* sound = GetMmVoiceEffectSound(effectIndex, "zora-voice");
    if (sound == nullptr && effectIndex != static_cast<u16>(192 + action)) {
        sound = GetMmVoiceEffectSound(static_cast<u16>(192 + action), "zora-voice-fallback");
    }
    return StartMmVoicePcm(static_cast<u16>(kMmVoicePcmKeyZoraBase + action), "zora-voice", mmSfxId, sound,
                           pitchScale, 1.0f, 0.72f);
}

::SoundFontSound* GetMmGoronDirectSound(MmGoronVoiceSlot slot) {
    if (!EnsureMmGoronDirectAudioReady()) {
        return nullptr;
    }

    const auto& def = kMmGoronVoiceDefinitions[slot];
    if (def.useSoundEffect) {
        if (sMmGoronDirectAudioState.mmFont->soundEffects == nullptr) {
            SPDLOG_WARN("[OoTxMM Transform] MM direct Goron voice {} has no sfx table", def.debugName);
            return nullptr;
        }

        const u16 action = def.mmSfxId >= 0x68C0 ? static_cast<u16>(def.mmSfxId - 0x68C0) : 0;
        const u16 mmVoiceIndex = PickMmGoronVoiceEffectIndex(action);
        const std::array<u16, 3> candidateIndices = { mmVoiceIndex, def.instrumentIndex, static_cast<u16>(192 + action) };

        for (u16 candidateIndex : candidateIndices) {
            if (candidateIndex >= sMmGoronDirectAudioState.mmFont->numSfx) {
                continue;
            }

            ::SoundFontSound* sound =
                reinterpret_cast<::SoundFontSound*>(&sMmGoronDirectAudioState.mmFont->soundEffects[candidateIndex]);
            if (sound != nullptr && sound->sample != nullptr && sound->sample->sampleAddr != nullptr) {
                return sound;
            }
        }

        SPDLOG_WARN("[OoTxMM Transform] MM direct Goron voice {} has no sample for sfx indices {}, {}, {}",
                    def.debugName, candidateIndices[0], candidateIndices[1], candidateIndices[2]);
        return nullptr;
    }

    if (def.instrumentIndex >= sMmGoronDirectAudioState.mmFont->numInstruments) {
        SPDLOG_WARN("[OoTxMM Transform] MM direct Goron voice {} invalid instrument {}", def.debugName,
                    def.instrumentIndex);
        return nullptr;
    }

    Instrument* instrument = reinterpret_cast<Instrument*>(sMmGoronDirectAudioState.mmFont->instruments[def.instrumentIndex]);
    if (instrument == nullptr) {
        SPDLOG_WARN("[OoTxMM Transform] MM direct Goron voice {} missing instrument {}", def.debugName,
                    def.instrumentIndex);
        return nullptr;
    }

    ::SoundFontSound* sound = Audio_InstrumentGetSound(instrument, def.note);
    if (sound == nullptr || sound->sample == nullptr || sound->sample->sampleAddr == nullptr) {
        SPDLOG_WARN("[OoTxMM Transform] MM direct Goron voice {} has no sample for note {}", def.debugName,
                    def.note);
        return nullptr;
    }
    return sound;
}

void StopMmGoronDirectVoice(MmGoronVoiceSlot slot) {
    auto& voice = sMmGoronDirectAudioState.voices[slot];
    if (!voice.initialized || !voice.active) {
        return;
    }

    if (voice.layer.note != nullptr) {
        Audio_SeqLayerNoteDecay(&voice.layer);
        voice.layer.note = nullptr;
    }
    voice.active = false;
    voice.oneShotFramesRemaining = 0;
    voice.layer.enabled = false;
    SPDLOG_INFO("[OoTxMM Transform] Stopped MM direct Goron voice {}", voice.debugName != nullptr ? voice.debugName : "<null>");
}

void StartOrUpdateMmGoronDirectVoice(MmGoronVoiceSlot slot, f32 freqScale, f32 velocity) {
    if (!EnsureMmGoronDirectVoiceInitialized(slot)) {
        return;
    }

    auto& voice = sMmGoronDirectAudioState.voices[slot];
    auto& def = kMmGoronVoiceDefinitions[slot];
    ::SoundFontSound* sound = GetMmGoronDirectSound(slot);
    if (sound == nullptr) {
        return;
    }

    voice.layer.enabled = true;
    voice.layer.finished = false;
    voice.layer.continuousNotes = def.looped;
    voice.layer.bit3 = true;
    voice.layer.notePropertiesNeedInit = true;
    voice.layer.sound = sound;
    voice.layer.noteFreqScale = sound->tuning * def.pitchMultiplier * freqScale;
    voice.layer.noteVelocity = velocity;
    voice.layer.notePan = 64;

    if (voice.layer.note == nullptr || voice.layer.note->playbackState.parentLayer != &voice.layer) {
        voice.layer.note = Audio_AllocNote(&voice.layer);
        if (voice.layer.note == nullptr || voice.layer.note->playbackState.parentLayer != &voice.layer) {
            SPDLOG_WARN("[OoTxMM Transform] Failed to allocate MM direct Goron voice {} note", def.debugName);
            voice.layer.note = nullptr;
            return;
        }
        Audio_NoteVibratoInit(voice.layer.note);
        Audio_NotePortamentoInit(voice.layer.note);
        SPDLOG_INFO("[OoTxMM Transform] Started MM direct Goron voice {} (inst {}, note {}, tuning {:.3f}, freq {:.3f}, vel {:.3f})",
                    def.debugName, def.instrumentIndex, def.note, sound->tuning, voice.layer.noteFreqScale, velocity);
    }

    voice.active = true;
    if (!def.looped) {
        voice.oneShotFramesRemaining = def.oneShotFrames;
    }
}

void TickMmGoronDirectVoices() {
    for (size_t i = 0; i < sMmGoronDirectAudioState.voices.size(); ++i) {
        auto& voice = sMmGoronDirectAudioState.voices[i];
        if (!voice.initialized || !voice.active || voice.looped) {
            continue;
        }

        if (voice.oneShotFramesRemaining > 0) {
            voice.oneShotFramesRemaining--;
        } else {
            StopMmGoronDirectVoice(static_cast<MmGoronVoiceSlot>(i));
        }
    }
}

void EnsureMmGoronSfxPatched() {
    sMmGoronSfxState.patchApplied = EnsureMmGoronDirectAudioReady();
}

void StopMmGoronLoopStage() {
    StopMmGoronDirectVoice(MM_GORON_VOICE_ROLL_A);
    StopMmGoronDirectVoice(MM_GORON_VOICE_ROLL_B);
    StopMmGoronDirectVoice(MM_GORON_VOICE_HEAT_LOOP);
    StopMmGoronDirectVoice(MM_GORON_VOICE_SPIKE_A);
    StopMmGoronDirectVoice(MM_GORON_VOICE_SPIKE_B);
    sMmGoronSfxState.activeLoopStage = 0;
}

void StopAllMmGoronSfx() {
    StopMmGoronLoopStage();
    StopMmGoronDirectVoice(MM_GORON_VOICE_TO_BALL);
    StopMmGoronDirectVoice(MM_GORON_VOICE_TO_GORON);
    StopMmGoronDirectVoice(MM_GORON_VOICE_HEAT_START);
    StopMmGoronDirectVoice(MM_GORON_VOICE_SPIKE_START);
    StopMmGoronDirectVoice(MM_GORON_VOICE_ATTACK_N);
    StopMmGoronDirectVoice(MM_GORON_VOICE_ATTACK_L);
    StopMmGoronDirectVoice(MM_GORON_VOICE_AUTO_JUMP);
    StopMmGoronDirectVoice(MM_GORON_VOICE_CLIMB_END);
    StopMmGoronDirectVoice(MM_GORON_VOICE_FOOTSTEP);
    for (auto& sound : sMmVoicePcmSounds) {
        StopMmVoicePcmSlot(sound);
    }
}

void PlayMmGoronCarrier(u16 sfxId, Vec3f* pos, f32* freq, f32* vol) {
    (void)sfxId;
    (void)pos;
    (void)freq;
    (void)vol;
}

void PlayMmGoronOneShot(u16 sfxId, Vec3f* pos, f32 pitchScale) {
    EnsureMmGoronSfxPatched();
    if (!sMmGoronSfxState.patchApplied) {
        return;
    }
    (void)pos;

    MmGoronVoiceSlot slot;
    switch (sfxId) {
        case kMmCarrierSfxGoronToBall:
            slot = MM_GORON_VOICE_TO_BALL;
            break;
        case kMmCarrierSfxBallToGoron:
            slot = MM_GORON_VOICE_TO_GORON;
            break;
        case kMmCarrierSfxGoronHeatStart:
            slot = MM_GORON_VOICE_HEAT_START;
            break;
        case kMmCarrierSfxGoronSpikeStart:
            slot = MM_GORON_VOICE_SPIKE_START;
            break;
        default:
            return;
    }
    StartOrUpdateMmGoronDirectVoice(slot, pitchScale, sMmGoronSfxState.oneShotVolume);
}

bool PlayMmGoronVoiceOneShot(MmGoronVoiceSlot slot, f32 pitchScale = 1.0f) {
    EnsureMmGoronSfxPatched();
    if (!sMmGoronSfxState.patchApplied || slot >= MM_GORON_VOICE_COUNT) {
        return false;
    }

    if (kMmGoronVoiceDefinitions[slot].useSoundEffect) {
        return PlayMmGoronVoicePcm(slot, pitchScale);
    }

    StopMmGoronDirectVoice(slot);
    StartOrUpdateMmGoronDirectVoice(slot, pitchScale, sMmGoronSfxState.oneShotVolume);
    return true;
}

void UpdateMmGoronAudioForState(Player* player) {
    EnsureMmGoronSfxPatched();
    TickMmGoronDirectVoices();
    if (!sMmGoronSfxState.patchApplied || player == nullptr) {
        return;
    }

    const bool isRolling = sRuntimeState.goronAction == GORON_ACTION_ROLL;
    const bool isHeat = sRuntimeState.goronPowerStage == 1;
    const bool isSpike = sRuntimeState.goronPowerStage >= 2;
    const s16 targetStage = !isRolling ? 0 : (isSpike ? 3 : (isHeat ? 2 : 1));

    if (targetStage != sMmGoronSfxState.activeLoopStage) {
        StopMmGoronLoopStage();
        sMmGoronSfxState.activeLoopStage = targetStage;
    }

    if (!isRolling) {
        return;
    }

    const f32 basePitch = CLAMP(sRuntimeState.goronRollSpeed / 10.0f, 0.85f, 1.20f);
    sMmGoronSfxState.rollFreqA = basePitch;
    sMmGoronSfxState.rollFreqB = basePitch;
    sMmGoronSfxState.heatFreq = 1.0f;
    sMmGoronSfxState.spikeFreqA = 1.0f;
    sMmGoronSfxState.spikeFreqB = 1.0f;

    StartOrUpdateMmGoronDirectVoice(MM_GORON_VOICE_ROLL_A, sMmGoronSfxState.rollFreqA, sMmGoronSfxState.rollVolume);
    StartOrUpdateMmGoronDirectVoice(MM_GORON_VOICE_ROLL_B, sMmGoronSfxState.rollFreqB, sMmGoronSfxState.rollVolume);

    if (isHeat) {
        StartOrUpdateMmGoronDirectVoice(MM_GORON_VOICE_HEAT_LOOP, sMmGoronSfxState.heatFreq, sMmGoronSfxState.heatVolume);
    } else {
        StopMmGoronDirectVoice(MM_GORON_VOICE_HEAT_LOOP);
    }

    // MM/2Ship spike audio reads more like a transition hit than a separate sustained loop.
    // Keep the heat layer under powered roll and let the spike-start one-shot carry the spike cue.
    StopMmGoronDirectVoice(MM_GORON_VOICE_SPIKE_A);
    StopMmGoronDirectVoice(MM_GORON_VOICE_SPIKE_B);
}

void PatchDisplayListSegmentRefsToNoOp(std::shared_ptr<Fast::DisplayList> displayList, u8 segment, const char* debugName) {
    if (displayList == nullptr) {
        return;
    }

    int patchCount = 0;
    for (Gfx& instruction : displayList->Instructions) {
        const u8 opcode = instruction.words.w0 >> 24;
        const u8 instructionSegment = instruction.words.w1 >> 24;
        if ((opcode == G_DL || opcode == G_DL_INDEX) && instructionSegment == segment) {
            instruction = gsSPNoOp();
            patchCount++;
        }
    }

    SPDLOG_INFO("[OoTxMM Transform] Patched {} seg0x{:02X} refs to no-op: {}",
                debugName != nullptr ? debugName : "DL", segment, patchCount);
}

void LogDisplayListKeyCommands(const std::shared_ptr<Fast::DisplayList>& displayList, const char* debugName) {
    if (displayList == nullptr) {
        SPDLOG_INFO("[OoTxMM Transform] {} unavailable", debugName != nullptr ? debugName : "DL");
        return;
    }

    SPDLOG_INFO("[OoTxMM Transform] {} instruction count={}", debugName != nullptr ? debugName : "DL",
                displayList->Instructions.size());

    for (size_t i = 0; i < displayList->Instructions.size(); ++i) {
        const Gfx& instruction = displayList->Instructions[i];
        const uint8_t opcode = instruction.words.w0 >> 24;
        const uint8_t seg = instruction.words.w1 >> 24;

        if (opcode == G_DL || opcode == G_DL_INDEX || opcode == G_SETTIMG || opcode == G_SETTILE ||
            opcode == G_LOADTLUT || opcode == G_SETENVCOLOR || opcode == G_SETPRIMCOLOR) {
            SPDLOG_INFO("[OoTxMM Transform] {} DL[{}] op=0x{:02X} w0=0x{:08X} w1=0x{:08X} seg=0x{:02X}",
                        debugName != nullptr ? debugName : "DL", i, opcode, instruction.words.w0,
                        instruction.words.w1, seg);
        }
    }
}

void* TryLoadNativeRawPointer(const char* primaryPath, const char* otrPath) {
    auto resourceManager = Ship::Context::GetInstance()->GetResourceManager();
    if (resourceManager == nullptr) {
        return nullptr;
    }

    auto tryLoad = [&](const char* path) -> void* {
        if (path == nullptr) {
            return nullptr;
        }

        auto resource = resourceManager->LoadResource(path, true);
        return resource != nullptr ? resource->GetRawPointer() : nullptr;
    };

    if (void* pointer = tryLoad(primaryPath)) {
        return pointer;
    }

    return tryLoad(otrPath);
}

bool InitMmPlayerAnimHeader(std::shared_ptr<SOH::PlayerAnimation> animation, s16 frameCount, LinkAnimationHeader* outHeader) {
    if (animation == nullptr || outHeader == nullptr || frameCount <= 0) {
        return false;
    }

    outHeader->common.frameCount = frameCount;
    outHeader->segment = animation->GetPointer();
    return true;
}

bool InitCopiedMmPlayerAnimHeader(std::shared_ptr<SOH::PlayerAnimation> animation, s16 hintFrameCount, s32 s16PerFrame,
                                  LinkAnimationHeader* outHeader, std::vector<s16>& storage) {
    (void)hintFrameCount;
    storage.clear();
    if (animation == nullptr || outHeader == nullptr || s16PerFrame <= 0) {
        return false;
    }

    const size_t sourceSize = animation->GetPointerSize();
    const size_t sourceS16Count = sourceSize / sizeof(s16);
    if (sourceS16Count < static_cast<size_t>(s16PerFrame)) {
        return false;
    }

    s32 frameCount = static_cast<s32>(sourceS16Count / static_cast<size_t>(s16PerFrame));
    if (frameCount <= 0) {
        return false;
    }

    const size_t copyS16Count = static_cast<size_t>(frameCount) * static_cast<size_t>(s16PerFrame);
    storage.resize(copyS16Count);
    std::memcpy(storage.data(), animation->GetPointer(), copyS16Count * sizeof(s16));

    for (s32 frame = 0; frame < frameCount; frame++) {
        s16* frameStart = storage.data() + (frame * s16PerFrame);
        frameStart[0] = -57;
        frameStart[2] = 0;
    }

    outHeader->common.frameCount = static_cast<s16>(frameCount);
    outHeader->segment = storage.data();
    return true;
}

void ResetRuntimeState() {
    if (sRuntimeState.zoraTunicForced && sRuntimeState.ownerPlayer != nullptr) {
        sRuntimeState.ownerPlayer->currentTunic = sRuntimeState.savedTunic;
    }
    ResetTransformMaskCameraEffects();
    if (sRuntimeState.initialized) {
        ResourceMgr_UnregisterSkeleton(&sRuntimeState.skelAnime);
        std::memset(&sRuntimeState.skelAnime, 0, sizeof(sRuntimeState.skelAnime));
    }

    sRuntimeState.initialized = false;
    sRuntimeState.assetsReady = false;
    sRuntimeState.ownerPlay = nullptr;
    sRuntimeState.ownerPlayer = nullptr;
    sRuntimeState.ownerSceneNum = -1;
    sRuntimeState.activeMask = PLAYER_MASK_NONE;
    sRuntimeState.savedTunic = PLAYER_TUNIC_KOKIRI;
    sRuntimeState.zoraTunicForced = false;
    sRuntimeState.motionState = TRANSFORM_MASK_MOTION_UNSET;
    sRuntimeState.idleAnimResource = nullptr;
    sRuntimeState.walkAnimResource = nullptr;
    sRuntimeState.runAnimResource = nullptr;
    sRuntimeState.attackAnimResource = nullptr;
    sRuntimeState.attackAnimResource2 = nullptr;
    sRuntimeState.attackAnimResource3 = nullptr;
    sRuntimeState.spinAnimResource = nullptr;
    sRuntimeState.jumpAnimResource = nullptr;
    sRuntimeState.fallAnimResource = nullptr;
    sRuntimeState.hopAnimResource = nullptr;
    sRuntimeState.climbStartAnimResource = nullptr;
    sRuntimeState.climbLoopAnimResource = nullptr;
    sRuntimeState.defendAnimResource = nullptr;
    sRuntimeState.defendWaitAnimResource = nullptr;
    sRuntimeState.defendEndAnimResource = nullptr;
    sRuntimeState.ocarinaStartAnimResource = nullptr;
    sRuntimeState.ocarinaPlayAnimResource = nullptr;
    sRuntimeState.swimAnimCopy.clear();
    sRuntimeState.swimIdleAnimCopy.clear();
    sRuntimeState.zoraFishSwimAnimCopy.clear();
    sRuntimeState.zoraWaterRollAnimCopy.clear();
    sRuntimeState.zoraSwimToWaitAnimCopy.clear();
    sRuntimeState.zoraSwimDownAnimCopy.clear();
    sRuntimeState.zoraSwimDeepEndAnimCopy.clear();
    sRuntimeState.zoraWaitToSwimWaitAnimCopy.clear();
    sRuntimeState.zoraLandToSwimWaitAnimCopy.clear();
    sRuntimeState.doorAnimResource = nullptr;
    sRuntimeState.chestAnimResource = nullptr;
    sRuntimeState.swimAnimResource = nullptr;
    sRuntimeState.swimIdleAnimResource = nullptr;
    sRuntimeState.zoraFishSwimAnimResource = nullptr;
    sRuntimeState.zoraWaterRollAnimResource = nullptr;
    sRuntimeState.zoraSwimToWaitAnimResource = nullptr;
    sRuntimeState.zoraSwimDownAnimResource = nullptr;
    sRuntimeState.zoraSwimDeepEndAnimResource = nullptr;
    sRuntimeState.zoraWaitToSwimWaitAnimResource = nullptr;
    sRuntimeState.zoraLandToSwimWaitAnimResource = nullptr;
    sRuntimeState.zoraCutterAttackAnimResource = nullptr;
    sRuntimeState.zoraCutterCatchAnimResource = nullptr;
    sRuntimeState.zoraCutterWaitAnimResource = nullptr;
    sRuntimeState.zoraJumpKickEndAnimResource = nullptr;
    sRuntimeState.zoraBarrierDLResource = nullptr;
    sRuntimeState.goronPunchEndAnimResource = nullptr;
    sRuntimeState.goronPunchEndAnimResource2 = nullptr;
    sRuntimeState.goronPunchEndAnimResource3 = nullptr;
    sRuntimeState.goronPunchEndRAnimResource = nullptr;
    sRuntimeState.goronPunchEndRAnimResource2 = nullptr;
    sRuntimeState.goronPunchEndRAnimResource3 = nullptr;
    sRuntimeState.goronSidehopLeftAnimResource = nullptr;
    sRuntimeState.goronSidehopRightAnimResource = nullptr;
    sRuntimeState.goronBackflipAnimResource = nullptr;
    sRuntimeState.goronSidehopLeftEndAnimResource = nullptr;
    sRuntimeState.goronSidehopRightEndAnimResource = nullptr;
    sRuntimeState.goronBackflipEndAnimResource = nullptr;
    sRuntimeState.goronClimbStartBAnimResource = nullptr;
    sRuntimeState.goronClimbUpRAnimResource = nullptr;
    sRuntimeState.goronClimbEndALAnimResource = nullptr;
    sRuntimeState.goronClimbEndARAnimResource = nullptr;
    sRuntimeState.goronClimbEndBLAnimResource = nullptr;
    sRuntimeState.goronClimbEndBRAnimResource = nullptr;
    sRuntimeState.dekuClimbUpLAnimResource = nullptr;
    sRuntimeState.dekuClimbUpRAnimResource = nullptr;
    sRuntimeState.goronDoorBAnimResource = nullptr;
    sRuntimeState.goronMaskOffStartAnimResource = nullptr;
    sRuntimeState.ztargetIdleRightAnimResource = nullptr;
    sRuntimeState.ztargetIdleLeftAnimResource = nullptr;
    sRuntimeState.ztargetSideWalkLeftAnimResource = nullptr;
    sRuntimeState.ztargetSideWalkRightAnimResource = nullptr;
    sRuntimeState.ztargetBackWalkAnimResource = nullptr;
    std::memset(&sRuntimeState.idleAnimHeader, 0, sizeof(sRuntimeState.idleAnimHeader));
    std::memset(&sRuntimeState.walkAnimHeader, 0, sizeof(sRuntimeState.walkAnimHeader));
    std::memset(&sRuntimeState.runAnimHeader, 0, sizeof(sRuntimeState.runAnimHeader));
    std::memset(&sRuntimeState.doorAnimHeader, 0, sizeof(sRuntimeState.doorAnimHeader));
    std::memset(&sRuntimeState.chestAnimHeader, 0, sizeof(sRuntimeState.chestAnimHeader));
    std::memset(&sRuntimeState.swimAnimHeader, 0, sizeof(sRuntimeState.swimAnimHeader));
    std::memset(&sRuntimeState.swimIdleAnimHeader, 0, sizeof(sRuntimeState.swimIdleAnimHeader));
    std::memset(&sRuntimeState.zoraFishSwimAnimHeader, 0, sizeof(sRuntimeState.zoraFishSwimAnimHeader));
    std::memset(&sRuntimeState.zoraWaterRollAnimHeader, 0, sizeof(sRuntimeState.zoraWaterRollAnimHeader));
    std::memset(&sRuntimeState.zoraSwimToWaitAnimHeader, 0, sizeof(sRuntimeState.zoraSwimToWaitAnimHeader));
    std::memset(&sRuntimeState.zoraSwimDownAnimHeader, 0, sizeof(sRuntimeState.zoraSwimDownAnimHeader));
    std::memset(&sRuntimeState.zoraSwimDeepEndAnimHeader, 0, sizeof(sRuntimeState.zoraSwimDeepEndAnimHeader));
    std::memset(&sRuntimeState.zoraWaitToSwimWaitAnimHeader, 0, sizeof(sRuntimeState.zoraWaitToSwimWaitAnimHeader));
    std::memset(&sRuntimeState.zoraLandToSwimWaitAnimHeader, 0, sizeof(sRuntimeState.zoraLandToSwimWaitAnimHeader));
    std::memset(&sRuntimeState.zoraCutterAttackAnimHeader, 0, sizeof(sRuntimeState.zoraCutterAttackAnimHeader));
    std::memset(&sRuntimeState.zoraCutterCatchAnimHeader, 0, sizeof(sRuntimeState.zoraCutterCatchAnimHeader));
    std::memset(&sRuntimeState.zoraCutterWaitAnimHeader, 0, sizeof(sRuntimeState.zoraCutterWaitAnimHeader));
    std::memset(&sRuntimeState.zoraJumpKickEndAnimHeader, 0, sizeof(sRuntimeState.zoraJumpKickEndAnimHeader));
    std::memset(&sRuntimeState.attackAnimHeader, 0, sizeof(sRuntimeState.attackAnimHeader));
    std::memset(&sRuntimeState.attackAnimHeader2, 0, sizeof(sRuntimeState.attackAnimHeader2));
    std::memset(&sRuntimeState.attackAnimHeader3, 0, sizeof(sRuntimeState.attackAnimHeader3));
    std::memset(&sRuntimeState.spinAnimHeader, 0, sizeof(sRuntimeState.spinAnimHeader));
    std::memset(&sRuntimeState.jumpAnimHeader, 0, sizeof(sRuntimeState.jumpAnimHeader));
    std::memset(&sRuntimeState.fallAnimHeader, 0, sizeof(sRuntimeState.fallAnimHeader));
    std::memset(&sRuntimeState.hopAnimHeader, 0, sizeof(sRuntimeState.hopAnimHeader));
    std::memset(&sRuntimeState.climbStartAnimHeader, 0, sizeof(sRuntimeState.climbStartAnimHeader));
    std::memset(&sRuntimeState.climbLoopAnimHeader, 0, sizeof(sRuntimeState.climbLoopAnimHeader));
    std::memset(&sRuntimeState.defendAnimHeader, 0, sizeof(sRuntimeState.defendAnimHeader));
    std::memset(&sRuntimeState.defendWaitAnimHeader, 0, sizeof(sRuntimeState.defendWaitAnimHeader));
    std::memset(&sRuntimeState.defendEndAnimHeader, 0, sizeof(sRuntimeState.defendEndAnimHeader));
    std::memset(&sRuntimeState.goronPunchEndAnimHeader, 0, sizeof(sRuntimeState.goronPunchEndAnimHeader));
    std::memset(&sRuntimeState.goronPunchEndAnimHeader2, 0, sizeof(sRuntimeState.goronPunchEndAnimHeader2));
    std::memset(&sRuntimeState.goronPunchEndAnimHeader3, 0, sizeof(sRuntimeState.goronPunchEndAnimHeader3));
    std::memset(&sRuntimeState.goronPunchEndRAnimHeader, 0, sizeof(sRuntimeState.goronPunchEndRAnimHeader));
    std::memset(&sRuntimeState.goronPunchEndRAnimHeader2, 0, sizeof(sRuntimeState.goronPunchEndRAnimHeader2));
    std::memset(&sRuntimeState.goronPunchEndRAnimHeader3, 0, sizeof(sRuntimeState.goronPunchEndRAnimHeader3));
    std::memset(&sRuntimeState.goronSidehopLeftAnimHeader, 0, sizeof(sRuntimeState.goronSidehopLeftAnimHeader));
    std::memset(&sRuntimeState.goronSidehopRightAnimHeader, 0, sizeof(sRuntimeState.goronSidehopRightAnimHeader));
    std::memset(&sRuntimeState.goronBackflipAnimHeader, 0, sizeof(sRuntimeState.goronBackflipAnimHeader));
    std::memset(&sRuntimeState.goronSidehopLeftEndAnimHeader, 0, sizeof(sRuntimeState.goronSidehopLeftEndAnimHeader));
    std::memset(&sRuntimeState.goronSidehopRightEndAnimHeader, 0, sizeof(sRuntimeState.goronSidehopRightEndAnimHeader));
    std::memset(&sRuntimeState.goronBackflipEndAnimHeader, 0, sizeof(sRuntimeState.goronBackflipEndAnimHeader));
    std::memset(&sRuntimeState.goronClimbStartBAnimHeader, 0, sizeof(sRuntimeState.goronClimbStartBAnimHeader));
    std::memset(&sRuntimeState.goronClimbUpRAnimHeader, 0, sizeof(sRuntimeState.goronClimbUpRAnimHeader));
    std::memset(&sRuntimeState.goronClimbEndALAnimHeader, 0, sizeof(sRuntimeState.goronClimbEndALAnimHeader));
    std::memset(&sRuntimeState.goronClimbEndARAnimHeader, 0, sizeof(sRuntimeState.goronClimbEndARAnimHeader));
    std::memset(&sRuntimeState.goronClimbEndBLAnimHeader, 0, sizeof(sRuntimeState.goronClimbEndBLAnimHeader));
    std::memset(&sRuntimeState.goronClimbEndBRAnimHeader, 0, sizeof(sRuntimeState.goronClimbEndBRAnimHeader));
    std::memset(&sRuntimeState.dekuClimbUpLAnimHeader, 0, sizeof(sRuntimeState.dekuClimbUpLAnimHeader));
    std::memset(&sRuntimeState.dekuClimbUpRAnimHeader, 0, sizeof(sRuntimeState.dekuClimbUpRAnimHeader));
    std::memset(&sRuntimeState.goronDoorBAnimHeader, 0, sizeof(sRuntimeState.goronDoorBAnimHeader));
    std::memset(&sRuntimeState.goronMaskOffStartAnimHeader, 0, sizeof(sRuntimeState.goronMaskOffStartAnimHeader));
    std::memset(&sRuntimeState.ztargetIdleRightAnimHeader, 0, sizeof(sRuntimeState.ztargetIdleRightAnimHeader));
    std::memset(&sRuntimeState.ztargetIdleLeftAnimHeader, 0, sizeof(sRuntimeState.ztargetIdleLeftAnimHeader));
    std::memset(&sRuntimeState.ztargetSideWalkLeftAnimHeader, 0, sizeof(sRuntimeState.ztargetSideWalkLeftAnimHeader));
    std::memset(&sRuntimeState.ztargetSideWalkRightAnimHeader, 0, sizeof(sRuntimeState.ztargetSideWalkRightAnimHeader));
    std::memset(&sRuntimeState.ztargetBackWalkAnimHeader, 0, sizeof(sRuntimeState.ztargetBackWalkAnimHeader));
    std::memset(&sRuntimeState.ocarinaStartAnimHeader, 0, sizeof(sRuntimeState.ocarinaStartAnimHeader));
    std::memset(&sRuntimeState.ocarinaPlayAnimHeader, 0, sizeof(sRuntimeState.ocarinaPlayAnimHeader));
    std::memset(&sRuntimeState.goronShieldSkelAnime, 0, sizeof(sRuntimeState.goronShieldSkelAnime));
    sRuntimeState.goronShieldSkelReady = false;
    sRuntimeState.eyeTexture = nullptr;
    sRuntimeState.mouthTexture = nullptr;
    sRuntimeState.ocarinaDLs.fill(nullptr);
    sRuntimeState.ocarinaDLCount = 0;
    sRuntimeState.ocarinaLimbIndex = -1;
    sRuntimeState.attackComboIndex = 0xFF;
    sRuntimeState.goronRolledUpDLResource = nullptr;
    sRuntimeState.goronPoweredRollDLResource = nullptr;
    sRuntimeState.goronHeatEffectDLResource1 = nullptr;
    sRuntimeState.goronHeatEffectDLResource2 = nullptr;
    sRuntimeState.goronPunchEffectDLResource = nullptr;
    sRuntimeState.zoraLeftFinDLResource = nullptr;
    sRuntimeState.zoraRightFinDLResource = nullptr;
    sRuntimeState.dekuBubbleStillDLResource = nullptr;
    sRuntimeState.dekuBubbleMoveDLResource = nullptr;
    sRuntimeState.dekuBubbleSetupDLResource = nullptr;
    sRuntimeState.dekuSpinTrailDLResource = nullptr;
    sRuntimeState.dekuShieldDLResource = nullptr;
    sRuntimeState.fdLeftHandSwordDLResource = nullptr;
    sRuntimeState.fdLeftHandEmptyDLResource = nullptr;
    sRuntimeState.fdLeftHandBottleDLResource = nullptr;
    sRuntimeState.fdRightHandEmptyDLResource = nullptr;
    sRuntimeState.fdSwordBeamDLResource = nullptr;
    sRuntimeState.goronRolledUpDL = nullptr;
    sRuntimeState.goronSpikeShellDL = nullptr;
    sRuntimeState.goronSpikeRockBodyDL = nullptr;
    sRuntimeState.goronSpikeMetalBodyDL = nullptr;
    sRuntimeState.goronSpikeEffectDL1 = nullptr;
    sRuntimeState.goronSpikeEffectDL2 = nullptr;
    sRuntimeState.goronPunchEffectDL = nullptr;
    sRuntimeState.goronSpikeMatAnim1 = nullptr;
    sRuntimeState.goronSpikeMatAnim2 = nullptr;
    sRuntimeState.zoraLeftFinDL = nullptr;
    sRuntimeState.zoraRightFinDL = nullptr;
    sRuntimeState.dekuBubbleStillDL = nullptr;
    sRuntimeState.dekuBubbleMoveDL = nullptr;
    sRuntimeState.dekuBubbleSetupDL = nullptr;
    sRuntimeState.dekuSpinTrailDL = nullptr;
    sRuntimeState.dekuShieldDL = nullptr;
    sRuntimeState.fdLeftHandSwordDL = nullptr;
    sRuntimeState.fdLeftHandEmptyDL = nullptr;
    sRuntimeState.fdLeftHandBottleDL = nullptr;
    sRuntimeState.fdRightHandEmptyDL = nullptr;
    sRuntimeState.fdSwordBeamDL = nullptr;
    sRuntimeState.goronAction = GORON_ACTION_NONE;
    sRuntimeState.goronPunchIndex = 0;
    sRuntimeState.goronPunchChainCount = 0;
    sRuntimeState.goronQueuedPunch = false;
    sRuntimeState.goronPunchHitSfxPlayed = false;
    sRuntimeState.goronPunchAnchorValid = false;
    sRuntimeState.goronPunchAnchorPos = {};
    sRuntimeState.goronActionTimer = 0;
    sRuntimeState.goronRollChargeTimer = 0;
    sRuntimeState.goronSpikePulseTimer = 0;
    sRuntimeState.goronPowerStage = 0;
    sRuntimeState.goronWallReboundTimer = 0;
    sRuntimeState.goronSlamImpactTimer = 0;
    sRuntimeState.goronSlamImpacted = false;
    sRuntimeState.goronRollSpeed = 0.0f;
    sRuntimeState.goronRollBallSpeed = 0.0f;
    sRuntimeState.goronRollBounce = 0.0f;
    sRuntimeState.goronRollSquash = 0.0f;
    sRuntimeState.goronRollTilt = 0.0f;
    sRuntimeState.goronRollColorLerp = 0.0f;
    sRuntimeState.goronRollVisualAngle = 0.0f;
    sRuntimeState.goronRollBouncePhase = 0.0f;
    sRuntimeState.goronRollHomeYaw = 0;
    sRuntimeState.goronActionLockedYaw = 0;
    sRuntimeState.goronRollSpinRate = 0;
    sRuntimeState.goronRollSfxCounter = 0;
    sRuntimeState.goronRollDriftYaw = 0;
    sRuntimeState.goronMagicDrainTimer = 0;
    sRuntimeState.goronSpikeMode = false;
    sRuntimeState.goronClimbUseRight = false;
    sRuntimeState.goronLastMotionSfx = TRANSFORM_MASK_MOTION_UNSET;
    sRuntimeState.zoraBoomerangState = ZORA_BOOMERANG_IDLE;
    sRuntimeState.zoraBoomerangTimer = 0;
    sRuntimeState.zoraBoomerangAimYaw = 0;
    sRuntimeState.zoraBoomerangAimPitch = 0;
    sRuntimeState.zoraBoomerangLockedYaw = 0;
    sRuntimeState.zoraBoomerangActorL = nullptr;
    sRuntimeState.zoraBoomerangActorR = nullptr;
    std::memset(&sRuntimeState.zoraBoomerangCameraTarget, 0, sizeof(sRuntimeState.zoraBoomerangCameraTarget));
    sRuntimeState.zoraSwimState = ZORA_SWIM_IDLE;
    sRuntimeState.zoraSwimPitch = 0;
    sRuntimeState.zoraSwimRoll = 0;
    sRuntimeState.zoraSwimRollSmoothed = 0;
    sRuntimeState.zoraSwimYawRate = 0;
    sRuntimeState.zoraSwimPhaseCounter = 0;
    sRuntimeState.zoraSwimActionTimer = 0;
    sRuntimeState.zoraSwimSpeed = 0.0f;
    sRuntimeState.zoraDolphinYaw = 0;
    sRuntimeState.zoraDolphinLinearVelocity = 0.0f;
    sRuntimeState.zoraFastSwimActive = false;
    sRuntimeState.zoraDolphinReentryLocked = false;
    sRuntimeState.zoraBarrierActive = false;
    sRuntimeState.zoraBarrierIntensity = 0;
    sRuntimeState.zoraBarrierMagicDrainTimer = 0;
    sRuntimeState.zoraBarrierDL = nullptr;
    sRuntimeState.zoraBootsActive = false;
    sRuntimeState.dekuAction = DEKU_ACTION_NONE;
    sRuntimeState.dekuBubble = {};
    sRuntimeState.dekuBubbleCharge = 0.0f;
    sRuntimeState.dekuBubbleChargeTimer = 0;
    sRuntimeState.dekuBubbleAimPitch = 0;
    sRuntimeState.dekuBubbleAimYaw = 0;
    sRuntimeState.visualTurnTilt = 0;
    sRuntimeState.visualTurnTiltLastYaw = 0;
    sRuntimeState.visualTurnTiltYawValid = false;
    sRuntimeState.dekuSpinTimer = 0.0f;
    sRuntimeState.dekuSpinSpeed = 0.0f;
    sRuntimeState.dekuSpinRotAccum = 0;
    sRuntimeState.dekuSpinTracerIndex = -1;
    sRuntimeState.dekuSpinTracerActive = false;
    sRuntimeState.dekuWaterHopsRemaining = 5;
    sRuntimeState.dekuWaterHopAnimTimer = 0;
    sRuntimeState.transformFlashTimer = 0;
    StopAllMmGoronSfx();
}

bool CanUseGoronActionButtons(const Player* player) {
    if (player == nullptr) {
        return false;
    }

    const bool blockedByState =
        (player->stateFlags1 & (PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_GETTING_ITEM |
                                PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_CARRYING_ACTOR |
                                PLAYER_STATE1_FIRST_PERSON)) != 0;
    const bool blockedByContext = player->doorType != PLAYER_DOORTYPE_NONE || player->heldActor != nullptr || IsUsingOcarina(player);
    return !blockedByState && !blockedByContext;
}

bool CanUseDekuActionButtons(const Player* player) {
    if (player == nullptr) {
        return false;
    }

    const bool blockedByState =
        (player->stateFlags1 & (PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_GETTING_ITEM |
                                PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_CARRYING_ACTOR)) != 0;
    const bool blockedByContext = player->doorType != PLAYER_DOORTYPE_NONE || player->heldActor != nullptr ||
                                  player->talkActor != nullptr || player->interactRangeActor != nullptr ||
                                  IsUsingOcarina(player);
    return !blockedByState && !blockedByContext;
}

void SyncTransformMaskPlayerAnchor(Player* player) {
    if (player == nullptr || !OoTxMm_IsTransformationMaskPlayerMask(player->currentMask)) {
        return;
    }

    const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask);
    const f32 height = form != nullptr ? form->colliderHeight : 80.0f;
    const f32 headHeightRatio =
        player->currentMask == PLAYER_MASK_GORON ? 0.58f : (player->currentMask == PLAYER_MASK_ZORA ? 0.64f : 0.85f);
    const f32 footY = player->actor.world.pos.y;
    const f32 waistY = footY + (height * 0.45f);
    const f32 headY = footY + (height * headHeightRatio);

    player->actor.home.pos = player->actor.world.pos;
    player->actor.focus.pos.x = player->actor.world.pos.x;
    player->actor.focus.pos.y = headY;
    player->actor.focus.pos.z = player->actor.world.pos.z;

    for (s32 i = 0; i < PLAYER_BODYPART_MAX; ++i) {
        player->bodyPartsPos[i].x = player->actor.world.pos.x;
        player->bodyPartsPos[i].y = waistY;
        player->bodyPartsPos[i].z = player->actor.world.pos.z;
    }

    player->bodyPartsPos[PLAYER_BODYPART_HEAD].y = headY;
    player->bodyPartsPos[PLAYER_BODYPART_WAIST].y = waistY;
    player->bodyPartsPos[PLAYER_BODYPART_L_FOOT].y = footY;
    player->bodyPartsPos[PLAYER_BODYPART_R_FOOT].y = footY;

    if (player->currentMask == PLAYER_MASK_GORON) {
        const f32 handForward = 32.0f;
        const f32 handSide = 22.0f;
        const f32 sinYaw = Math_SinS(player->actor.shape.rot.y);
        const f32 cosYaw = Math_CosS(player->actor.shape.rot.y);
        const f32 sideX = Math_SinS(player->actor.shape.rot.y + 0x4000) * handSide;
        const f32 sideZ = Math_CosS(player->actor.shape.rot.y + 0x4000) * handSide;
        const f32 handY = waistY + 16.0f;

        player->bodyPartsPos[PLAYER_BODYPART_L_HAND].x = player->actor.world.pos.x + (sinYaw * handForward) + sideX;
        player->bodyPartsPos[PLAYER_BODYPART_L_HAND].y = handY;
        player->bodyPartsPos[PLAYER_BODYPART_L_HAND].z = player->actor.world.pos.z + (cosYaw * handForward) + sideZ;
        player->bodyPartsPos[PLAYER_BODYPART_R_HAND].x = player->actor.world.pos.x + (sinYaw * handForward) - sideX;
        player->bodyPartsPos[PLAYER_BODYPART_R_HAND].y = handY;
        player->bodyPartsPos[PLAYER_BODYPART_R_HAND].z = player->actor.world.pos.z + (cosYaw * handForward) - sideZ;
    }
}

void SyncTransformMaskShadowFeet(Player* player) {
    if (player == nullptr || !OoTxMm_IsTransformationMaskPlayerMask(player->currentMask)) {
        return;
    }

    const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask);
    const f32 radius = form != nullptr ? form->colliderRadius : 16.0f;
    const f32 footSpread = std::max(8.0f, radius * 0.45f);
    const f32 floorY = std::isfinite(player->actor.floorHeight) ? player->actor.floorHeight : player->actor.world.pos.y;
    const s16 sideYaw = player->actor.shape.rot.y + 0x4000;
    const f32 sideX = Math_SinS(sideYaw) * footSpread;
    const f32 sideZ = Math_CosS(sideYaw) * footSpread;

    player->actor.shape.feetPos[FOOT_LEFT].x = player->actor.world.pos.x + sideX;
    player->actor.shape.feetPos[FOOT_LEFT].y = floorY;
    player->actor.shape.feetPos[FOOT_LEFT].z = player->actor.world.pos.z + sideZ;
    player->actor.shape.feetPos[FOOT_RIGHT].x = player->actor.world.pos.x - sideX;
    player->actor.shape.feetPos[FOOT_RIGHT].y = floorY;
    player->actor.shape.feetPos[FOOT_RIGHT].z = player->actor.world.pos.z - sideZ;
    player->actor.shape.feetFloorFlags = (player->actor.bgCheckFlags & 1) ? 3 : 0;
}

void SyncGoronHeldActor(Player* player) {
    if (player == nullptr || player->currentMask != PLAYER_MASK_GORON || player->heldActor == nullptr ||
        (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) == 0) {
        return;
    }

    const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask);
    const f32 carryHeight = (form != nullptr ? form->colliderHeight : 80.0f) + 4.0f;
    const f32 forward = 9.0f;
    const f32 sinYaw = Math_SinS(player->actor.shape.rot.y);
    const f32 cosYaw = Math_CosS(player->actor.shape.rot.y);
    Actor* heldActor = player->heldActor;

    heldActor->world.pos.x = player->actor.world.pos.x + (sinYaw * forward);
    heldActor->world.pos.y = player->actor.world.pos.y + carryHeight;
    heldActor->world.pos.z = player->actor.world.pos.z + (cosYaw * forward);
    heldActor->world.rot.y = heldActor->shape.rot.y = player->actor.shape.rot.y + player->unk_3BC.y;
}

void SyncTransformHeldActorToHand(Player* player) {
    if (player == nullptr || !OoTxMm_IsTransformationMaskPlayerMask(player->currentMask) || player->heldActor == nullptr ||
        (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) == 0) {
        return;
    }

    if (player->currentMask == PLAYER_MASK_GORON) {
        SyncGoronHeldActor(player);
        return;
    }

    Actor* heldActor = player->heldActor;
    heldActor->world.pos = player->leftHandPos;
    heldActor->world.pos.y += 4.0f;
    heldActor->world.rot.y = heldActor->shape.rot.y = player->actor.shape.rot.y + player->unk_3BC.y;
}

bool HasAvailableMagic() {
    return gSaveContext.magicLevel > 0 && gSaveContext.magic > 0;
}

static Color_RGB8 OoTxMm_LerpColor(const Color_RGB8& from, const Color_RGB8& to, f32 t) {
    t = CLAMP(t, 0.0f, 1.0f);
    Color_RGB8 out;
    out.r = static_cast<u8>(from.r + ((to.r - from.r) * t));
    out.g = static_cast<u8>(from.g + ((to.g - from.g) * t));
    out.b = static_cast<u8>(from.b + ((to.b - from.b) * t));
    return out;
}

bool ConsumeMagicQuietly(s16 amount) {
    if (gSaveContext.magicLevel <= 0 || gSaveContext.magic < amount) {
        return false;
    }

    gSaveContext.magic -= amount;
    gSaveContext.magicTarget = gSaveContext.magic;
    if (gSaveContext.magicState != MAGIC_STATE_FILL && gSaveContext.magicState != MAGIC_STATE_STEP_CAPACITY) {
        gSaveContext.magicState = MAGIC_STATE_IDLE;
    }
    return true;
}

void StopGoronAction(PlayState* play, Player* player) {
    const bool wasRolling =
        sRuntimeState.goronAction == GORON_ACTION_ROLL || sRuntimeState.goronAction == GORON_ACTION_ROLL_START ||
        sRuntimeState.goronAction == GORON_ACTION_SLAM_JUMP || sRuntimeState.goronAction == GORON_ACTION_SLAM_POUND;
    sRuntimeState.goronAction = GORON_ACTION_NONE;
    sRuntimeState.goronQueuedPunch = false;
    sRuntimeState.goronPunchChainCount = 0;
    sRuntimeState.goronPunchAnchorValid = false;
    sRuntimeState.goronPunchAnchorPos = {};
    sRuntimeState.goronActionTimer = 0;
    sRuntimeState.goronRollChargeTimer = 0;
    sRuntimeState.goronSpikePulseTimer = 0;
    sRuntimeState.goronSlamImpactTimer = 0;
    sRuntimeState.goronSlamImpacted = false;
    sRuntimeState.goronRollSpeed = 0.0f;
    sRuntimeState.goronRollBallSpeed = 0.0f;
    sRuntimeState.goronRollBounce = 0.0f;
    sRuntimeState.goronRollSquash = 0.0f;
    sRuntimeState.goronRollTilt = 0.0f;
    sRuntimeState.goronRollColorLerp = 0.0f;
    sRuntimeState.goronRollVisualAngle = 0.0f;
    sRuntimeState.goronRollBouncePhase = 0.0f;
    sRuntimeState.goronRollHomeYaw = 0;
    sRuntimeState.goronRollSpinRate = 0;
    sRuntimeState.goronRollSfxCounter = 0;
    sRuntimeState.goronRollDriftYaw = 0;
    sRuntimeState.goronMagicDrainTimer = 0;
    sRuntimeState.goronPowerStage = 0;
    sRuntimeState.goronWallReboundTimer = 0;
    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;
    player->stateFlags1 &= ~(PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL);
    player->stateFlags2 &= ~PLAYER_STATE2_HOPPING;
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    player->actor.velocity.y = 0.0f;
    player->actor.gravity = -2.0f;
    player->actor.shape.rot.x = 0;
    player->actor.shape.rot.z = 0;
    player->actor.shape.yOffset = 0.0f;
    player->actor.world.rot.x = 0;
    player->pushedSpeed = 0.0f;

    if (wasRolling) {
        PlayMmGoronOneShot(kMmCarrierSfxBallToGoron, &player->actor.projectedPos, 1.0f);
        if (sRuntimeState.idleAnimHeader.segment != nullptr) {
            LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.idleAnimHeader, 1.0f, 0.0f,
                                 Animation_GetLastFrame(&sRuntimeState.idleAnimHeader), ANIMMODE_LOOP, 0.0f);
            sRuntimeState.motionState = TRANSFORM_MASK_MOTION_IDLE;
        }
    }

    if (sRuntimeState.goronSpikeMode) {
        sRuntimeState.goronSpikeMode = false;
        sRuntimeState.goronSpikePulseTimer = 0;
        Magic_Reset(play);
    }
}

void ResetGoronTransientActionState(Player* player) {
    StopAllMmGoronSfx();
    sRuntimeState.goronAction = GORON_ACTION_NONE;
    sRuntimeState.goronQueuedPunch = false;
    sRuntimeState.goronPunchChainCount = 0;
    sRuntimeState.goronPunchHitSfxPlayed = false;
    sRuntimeState.goronPunchAnchorValid = false;
    sRuntimeState.goronPunchAnchorPos = {};
    sRuntimeState.goronActionTimer = 0;
    sRuntimeState.goronRollChargeTimer = 0;
    sRuntimeState.goronSpikePulseTimer = 0;
    sRuntimeState.goronPowerStage = 0;
    sRuntimeState.goronWallReboundTimer = 0;
    sRuntimeState.goronSlamImpactTimer = 0;
    sRuntimeState.goronSlamImpacted = false;
    sRuntimeState.goronRollSpeed = 0.0f;
    sRuntimeState.goronRollBallSpeed = 0.0f;
    sRuntimeState.goronRollBounce = 0.0f;
    sRuntimeState.goronRollSquash = 0.0f;
    sRuntimeState.goronRollTilt = 0.0f;
    sRuntimeState.goronRollColorLerp = 0.0f;
    sRuntimeState.goronRollVisualAngle = 0.0f;
    sRuntimeState.goronRollBouncePhase = 0.0f;
    sRuntimeState.goronRollSpinRate = 0;
    sRuntimeState.goronRollSfxCounter = 0;
    sRuntimeState.goronRollDriftYaw = 0;
    sRuntimeState.goronMagicDrainTimer = 0;
    sRuntimeState.goronSpikeMode = false;
    sRuntimeState.motionState = TRANSFORM_MASK_MOTION_UNSET;

    if (player != nullptr) {
        player->stateFlags1 &= ~PLAYER_STATE1_INPUT_DISABLED;
        player->stateFlags1 &= ~(PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL);
        player->stateFlags2 &= ~PLAYER_STATE2_HOPPING;
        player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
        player->actor.velocity.y = 0.0f;
        player->actor.gravity = -2.0f;
        player->actor.speedXZ = 0.0f;
        player->linearVelocity = 0.0f;
        player->pushedSpeed = 0.0f;
        player->actor.shape.rot.x = 0;
        player->actor.shape.rot.z = 0;
        player->actor.world.rot.x = 0;
        player->actor.shape.yOffset = 0.0f;
        player->meleeWeaponState = 0;
    }
}

void StartGoronPunch(PlayState* play, Player* player, const TransformMaskFormDefinition* form, bool chained) {
    (void)play;
    sRuntimeState.goronAction = GORON_ACTION_PUNCH;
    sRuntimeState.goronQueuedPunch = false;
    if (!chained || sRuntimeState.goronPunchChainCount == 0 || sRuntimeState.goronPunchChainCount >= 3) {
        sRuntimeState.goronPunchIndex = 0;
        sRuntimeState.goronPunchChainCount = 1;
    } else {
        sRuntimeState.goronPunchIndex = sRuntimeState.goronPunchChainCount;
        sRuntimeState.goronPunchChainCount++;
    }
    sRuntimeState.attackComboIndex = sRuntimeState.goronPunchIndex;
    sRuntimeState.motionState = TRANSFORM_MASK_MOTION_UNSET;
    sRuntimeState.goronPunchHitSfxPlayed = false;
    if (!chained) {
        sRuntimeState.goronActionLockedYaw = player->actor.shape.rot.y;
        sRuntimeState.goronPunchAnchorValid = false;
    } else if (sRuntimeState.goronPunchIndex == 2) {
        sRuntimeState.goronPunchAnchorPos = player->actor.world.pos;
        sRuntimeState.goronPunchAnchorValid = true;
    }
    LockGoronPlayerMotion(player);

    PlayGoronActionSfx(player, sRuntimeState.goronPunchIndex == 1 ? NA_SE_IT_SWORD_SWING_HARD : NA_SE_IT_SWORD_SWING);
    if (player->currentMask == PLAYER_MASK_ZORA) {
        PlayMmZoraVoiceOneShot(sRuntimeState.goronPunchIndex == 2 ? 0x68A1 : 0x68A0);
    } else {
        PlayMmGoronVoiceOneShot(sRuntimeState.goronPunchIndex == 2 ? MM_GORON_VOICE_ATTACK_L : MM_GORON_VOICE_ATTACK_N);
    }
    sRuntimeState.goronActionTimer = 0;
}

bool IsGoronPunchAnimationFinished() {
    if (sRuntimeState.activeMask == PLAYER_MASK_ZORA) {
        LinkAnimationHeader* punchHeader = nullptr;
        switch (sRuntimeState.goronPunchIndex) {
            case 1:
                punchHeader = &sRuntimeState.attackAnimHeader2;
                break;
            case 2:
                punchHeader = &sRuntimeState.attackAnimHeader3;
                break;
            case 0:
            default:
                punchHeader = &sRuntimeState.attackAnimHeader;
                break;
        }

        return punchHeader != nullptr && punchHeader->segment != nullptr && sRuntimeState.skelAnime.animation == punchHeader &&
               sRuntimeState.skelAnime.curFrame >= (Animation_GetLastFrame(punchHeader) - 0.5f);
    }

    LinkAnimationHeader* endHeader = nullptr;
    switch (sRuntimeState.goronPunchIndex) {
        case 1:
            endHeader = &sRuntimeState.goronPunchEndAnimHeader2;
            break;
        case 2:
            endHeader = &sRuntimeState.goronPunchEndAnimHeader3;
            break;
        case 0:
        default:
            endHeader = &sRuntimeState.goronPunchEndAnimHeader;
            break;
    }

    return endHeader != nullptr && endHeader->segment != nullptr && sRuntimeState.skelAnime.animation == endHeader &&
           sRuntimeState.skelAnime.curFrame >= (Animation_GetLastFrame(endHeader) - 0.5f);
}

bool IsGoronPunchStrikeActive() {
    switch (sRuntimeState.goronPunchIndex) {
        case 1:
            return sRuntimeState.skelAnime.animation == &sRuntimeState.attackAnimHeader2;
        case 2:
            return sRuntimeState.skelAnime.animation == &sRuntimeState.attackAnimHeader3;
        case 0:
        default:
            return sRuntimeState.skelAnime.animation == &sRuntimeState.attackAnimHeader;
    }
}

void StartGoronRoll(Player* player) {
    sRuntimeState.goronAction = GORON_ACTION_ROLL_START;
    sRuntimeState.goronQueuedPunch = false;
    sRuntimeState.goronPunchChainCount = 0;
    sRuntimeState.goronActionTimer = 0;
    sRuntimeState.goronRollChargeTimer = 0;
    sRuntimeState.goronSpikePulseTimer = 0;
    sRuntimeState.goronPowerStage = 0;
    sRuntimeState.goronWallReboundTimer = 0;
    sRuntimeState.goronSpikeMode = false;
    sRuntimeState.goronRollSpeed = std::max(4.0f, fabsf(player->actor.speedXZ));
    sRuntimeState.goronRollBallSpeed = sRuntimeState.goronRollSpeed;
    sRuntimeState.goronRollBounce = 0.0f;
    sRuntimeState.goronRollSquash = 0.0f;
    sRuntimeState.goronRollTilt = 0.0f;
    sRuntimeState.goronRollColorLerp = 0.0f;
    sRuntimeState.goronRollVisualAngle = 0.0f;
    sRuntimeState.goronRollBouncePhase = 0.0f;
    sRuntimeState.goronRollHomeYaw = player->yaw;
    sRuntimeState.goronRollSpinRate = 0;
    sRuntimeState.goronRollSfxCounter = 0;
    sRuntimeState.goronRollDriftYaw = 0;
    sRuntimeState.goronMagicDrainTimer = 10;
    sRuntimeState.goronSlamImpactTimer = 0;
    sRuntimeState.goronSlamImpacted = false;
    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;
    player->actor.shape.rot.x = 0;
    player->actor.shape.yOffset = 0.0f;
    PlayMmGoronOneShot(kMmCarrierSfxGoronToBall, &player->actor.projectedPos, 1.0f);
}

void TriggerGoronSlam(PlayState* play, Player* player, const TransformMaskFormDefinition* form) {
    (void)form;
    sRuntimeState.goronAction = GORON_ACTION_SLAM_JUMP;
    sRuntimeState.goronQueuedPunch = false;
    sRuntimeState.goronPunchChainCount = 0;
    sRuntimeState.goronActionTimer = 0;
    sRuntimeState.goronRollChargeTimer = 0;
    sRuntimeState.goronSpikePulseTimer = 0;
    sRuntimeState.goronSlamImpactTimer = 10;
    sRuntimeState.goronSlamImpacted = false;
    sRuntimeState.goronRollSpeed = 0.0f;
    sRuntimeState.goronRollBallSpeed = 0.0f;
    sRuntimeState.goronRollSpinRate = std::max<s16>(sRuntimeState.goronRollSpinRate, 0x1F40);
    sRuntimeState.goronRollSfxCounter = 0;
    sRuntimeState.goronPowerStage = 1;
    sRuntimeState.goronRollTilt = 1.0f;
    sRuntimeState.goronActionLockedYaw = player->actor.shape.rot.y;
    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;
    player->pushedSpeed = 0.0f;
    player->actor.shape.yOffset = kGoronRollingShapeYOffset;
    player->actor.velocity.y = 14.0f;
    player->actor.gravity = -2.0f;
    player->actor.bgCheckFlags &= ~1;
    player->stateFlags1 |= PLAYER_STATE1_JUMPING;
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    PlayMmGoronOneShot(kMmCarrierSfxGoronToBall, &player->actor.projectedPos, 1.0f);

    if (sRuntimeState.goronSpikeMode) {
        sRuntimeState.goronSpikeMode = false;
        Magic_Reset(play);
    }
}

void UpdateGoronMechanics(PlayState* play, Player* player, const TransformMaskFormDefinition* form) {
    EnsureMmGoronSfxPatched();
    TickMmGoronDirectVoices();

    if ((player->stateFlags1 & PLAYER_STATE1_IN_WATER) &&
        (play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->transitionMode == TRANS_MODE_OFF)) {
        ResetGoronTransientActionState(player);
        player->currentMask = PLAYER_MASK_NONE;
        gSaveContext.ship.maskMemory = PLAYER_MASK_NONE;
        ResetRuntimeState();
        Play_TriggerVoidOut(play);
        return;
    }

    Input* input = &play->state.input[0];
    const bool canUseButtons = CanUseGoronActionButtons(player);
    const bool holdingA = CHECK_BTN_ALL(input->cur.button, BTN_A);
    const bool pressedA = CHECK_BTN_ALL(input->press.button, BTN_A);
    const bool pressedB = CHECK_BTN_ALL(input->press.button, BTN_B);
    const bool isGrounded = (player->actor.bgCheckFlags & 1) != 0;
    const bool isRollingAction =
        sRuntimeState.goronAction == GORON_ACTION_ROLL || sRuntimeState.goronAction == GORON_ACTION_ROLL_START ||
        sRuntimeState.goronAction == GORON_ACTION_SLAM_JUMP || sRuntimeState.goronAction == GORON_ACTION_SLAM_POUND;

    if (!canUseButtons) {
        const u32 hardBlockedFlags = PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_IN_ITEM_CS |
                                     PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_CARRYING_ACTOR |
                                     PLAYER_STATE1_FIRST_PERSON;
        if ((player->stateFlags1 & (PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_TALKING | PLAYER_STATE1_IN_CUTSCENE)) != 0 ||
            player->heldActor != nullptr || player->talkActor != nullptr) {
            sRuntimeState.goronAction = GORON_ACTION_NONE;
            sRuntimeState.goronQueuedPunch = false;
            sRuntimeState.goronPunchChainCount = 0;
            sRuntimeState.goronPunchAnchorValid = false;
            sRuntimeState.goronPunchHitSfxPlayed = false;
            player->stateFlags1 &= ~PLAYER_STATE1_INPUT_DISABLED;
            SyncGoronHeldActor(player);
            return;
        }
        if (isRollingAction && (player->stateFlags1 & hardBlockedFlags) == 0) {
            player->stateFlags1 &= ~PLAYER_STATE1_INPUT_DISABLED;
        } else {
            StopGoronAction(play, player);
            return;
        }
    }

    if (sRuntimeState.goronAction == GORON_ACTION_SLAM_JUMP) {
        player->actor.speedXZ = 0.0f;
        player->linearVelocity = 0.0f;
        player->actor.shape.yOffset = kGoronRollingShapeYOffset;

        if (player->actor.velocity.y > 0.0f) {
            if ((player->actor.velocity.y + player->actor.gravity) < 0.0f) {
                player->actor.velocity.y = -player->actor.gravity;
            }
        } else if (player->actor.velocity.y > -1.0f) {
            player->actor.gravity = -0.2f;
        } else {
            player->actor.gravity = -10.0f;
        }

        if (sRuntimeState.goronRollSpinRate < 0x1F40) {
            sRuntimeState.goronRollSpinRate = 0x1F40;
        }
        player->actor.shape.rot.x += sRuntimeState.goronRollSpinRate;
        Math_ScaledStepToS(&player->actor.shape.rot.y, sRuntimeState.goronRollHomeYaw, 0x7D0);
        Math_AsymStepToF(&sRuntimeState.goronRollColorLerp, 1.0f, 0.8f, 0.05f);

        sRuntimeState.goronActionTimer++;
        if (isGrounded && sRuntimeState.goronActionTimer > 3 && player->actor.velocity.y <= 0.0f) {
            sRuntimeState.goronAction = GORON_ACTION_SLAM_POUND;
            sRuntimeState.goronSlamImpactTimer = 10;
            sRuntimeState.goronActionTimer = 0;
            player->actor.gravity = -2.0f;
            player->actor.velocity.y = 0.0f;
            player->stateFlags1 &= ~PLAYER_STATE1_JUMPING;

            PlayGoronActionSfx(player, NA_SE_IT_HAMMER_HIT);
            Vec3f zero = { 0.0f, 0.0f, 0.0f };
            EffectSsBlast_SpawnWhiteShockwave(play, &player->actor.world.pos, &zero, &zero);
            Actor_SpawnFloorDustRing(play, &player->actor, &player->actor.world.pos, player->actor.shape.shadowScale * 1.5f,
                                     4, 8.0f, 500, 10, 1);
            s16 quake = Quake_Add(GET_ACTIVE_CAM(play), 3);
            Quake_SetSpeed(quake, 27767);
            Quake_SetQuakeValues(quake, 7, 0, 0, 0);
            Quake_SetCountdown(quake, 20);
            func_800AA000(0.0f, 255, 20, 150);
            RegisterGoronGroundPoundCollider(play, player);
        }
        return;
    }

    if (sRuntimeState.goronAction == GORON_ACTION_SLAM_POUND) {
        player->actor.speedXZ = 0.0f;
        player->linearVelocity = 0.0f;
        player->actor.shape.yOffset = kGoronRollingShapeYOffset;
        Math_AsymStepToF(&sRuntimeState.goronRollColorLerp, 1.0f, 0.8f, 0.05f);

        if (sRuntimeState.goronSlamImpactTimer > 0) {
            sRuntimeState.goronSlamImpactTimer--;
            if (sRuntimeState.goronSlamImpactTimer >= 8) {
                RegisterGoronGroundPoundCollider(play, player);
            }
        } else {
            if (holdingA) {
                sRuntimeState.goronAction = GORON_ACTION_ROLL;
                sRuntimeState.goronPowerStage = 0;
                sRuntimeState.goronRollChargeTimer = 4;
                sRuntimeState.goronRollSpeed = 0.0f;
                sRuntimeState.goronRollBallSpeed = 0.0f;
                player->actor.gravity = -2.0f;
            } else {
                StopGoronAction(play, player);
            }
        }
        return;
    }

    if (sRuntimeState.goronAction == GORON_ACTION_PUNCH) {
        LockGoronPlayerMotion(player);
        RegisterGoronBombDamageCollider(play, player, false);
        if (IsGoronPunchStrikeActive() && !sRuntimeState.goronPunchHitSfxPlayed) {
            PlayGoronActionSfx(player, NA_SE_IT_HAMMER_HIT);
            sRuntimeState.goronPunchHitSfxPlayed = true;
        }
        if (pressedB && sRuntimeState.goronPunchChainCount < 3) {
            sRuntimeState.goronQueuedPunch = true;
        }

        if (IsGoronPunchAnimationFinished()) {
            if (sRuntimeState.goronQueuedPunch && sRuntimeState.goronPunchChainCount < 3) {
                StartGoronPunch(play, player, form, true);
            } else {
                sRuntimeState.goronAction = GORON_ACTION_NONE;
                sRuntimeState.goronQueuedPunch = false;
                sRuntimeState.goronPunchChainCount = 0;
                sRuntimeState.goronPunchAnchorValid = false;
            }
        }
        return;
    }

    if (sRuntimeState.goronAction == GORON_ACTION_ROLL_START) {
        if (!holdingA) {
            StopGoronAction(play, player);
            return;
        }

        if (holdingA && pressedB) {
            TriggerGoronSlam(play, player, form);
            return;
        }

        if (sRuntimeState.spinAnimHeader.segment != nullptr &&
            sRuntimeState.skelAnime.animation == &sRuntimeState.spinAnimHeader &&
            Animation_OnFrame(&sRuntimeState.skelAnime, Animation_GetLastFrame(&sRuntimeState.spinAnimHeader))) {
            sRuntimeState.goronAction = GORON_ACTION_ROLL;
            player->actor.shape.yOffset = kGoronRollingShapeYOffset;
            UpdateMmGoronAudioForState(player);
            return;
        }

        if (sRuntimeState.goronActionTimer > 0) {
            sRuntimeState.goronActionTimer--;
        }
        return;
    }

    if (pressedB && sRuntimeState.goronAction != GORON_ACTION_ROLL) {
        StartGoronPunch(play, player, form, false);
        return;
    }

    const bool continueExistingRoll =
        (sRuntimeState.goronAction == GORON_ACTION_ROLL_START || sRuntimeState.goronAction == GORON_ACTION_ROLL);
    const bool canStartRoll = pressedA && isGrounded && IsGoronRollInputIntent(input) &&
                              !IsGoronVanillaHopInputIntent(player, input);

    if (canStartRoll || continueExistingRoll) {
        if (sRuntimeState.goronAction != GORON_ACTION_ROLL && sRuntimeState.goronAction != GORON_ACTION_ROLL_START) {
            StartGoronRoll(player);
        }

        if (holdingA && pressedB) {
            TriggerGoronSlam(play, player, form);
            return;
        }

        if (!holdingA) {
            StopGoronAction(play, player);
            return;
        }

        if (sRuntimeState.goronAction == GORON_ACTION_ROLL_START) {
            return;
        }

        player->stateFlags1 &= ~(PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL);
        RegisterGoronBombDamageCollider(play, player, true);
        if (sGoronAttackCollider.base.atFlags & (AT_HIT | AT_BOUNCED)) {
            sGoronAttackCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
            player->stateFlags1 &= ~PLAYER_STATE1_INPUT_DISABLED;
            sRuntimeState.goronRollBallSpeed = std::max<f32>(sRuntimeState.goronRollBallSpeed, 8.0f);
            sRuntimeState.goronRollSpeed = sRuntimeState.goronRollBallSpeed;
        }
        player->actor.shape.yOffset = kGoronRollingShapeYOffset;

        f32 speedTarget = 0.0f;
        s16 yawTarget = player->yaw;
        const f32 stickMagnitude =
            sqrtf(static_cast<f32>((input->rel.stick_x * input->rel.stick_x) + (input->rel.stick_y * input->rel.stick_y)));
        const bool hasRollDirection = stickMagnitude > 10.0f;
        if (hasRollDirection) {
            const s16 stickAngle = Math_Atan2S(input->rel.stick_y, -input->rel.stick_x);
            yawTarget = stickAngle + Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));
            speedTarget = (std::min(stickMagnitude, 60.0f) / 60.0f) * 8.0f * 2.6f;
        }

        if (sRuntimeState.goronWallReboundTimer > 0) {
            sRuntimeState.goronWallReboundTimer--;
        }

        if ((player->actor.bgCheckFlags & 8) != 0 && sRuntimeState.goronRollBallSpeed >= 12.0f &&
            sRuntimeState.goronWallReboundTimer == 0) {
            const s16 wallAngle = player->actor.wallYaw + 0x8000;
            const s16 relWallAngle = player->yaw - wallAngle;
            const s16 bounceAngle = ((relWallAngle >= 0) ? 1 : -1) * ((ABS(relWallAngle) + 0x100) & ~0x1FF);
            player->yaw += static_cast<s16>(0x8000 - (bounceAngle * 2));
            sRuntimeState.goronRollHomeYaw = player->yaw;
            sRuntimeState.goronRollBounce += sRuntimeState.goronRollBallSpeed * 0.05f;
            sRuntimeState.goronWallReboundTimer = 4;
            yawTarget = sRuntimeState.goronRollHomeYaw;
            PlayGoronActionSfx(player, NA_SE_PL_BODY_HIT);
        }
        if (sRuntimeState.goronWallReboundTimer > 0) {
            yawTarget = sRuntimeState.goronRollHomeYaw;
            speedTarget = std::max<f32>(speedTarget, sRuntimeState.goronRollBallSpeed);
        }

        const s16 previousPowerStage = sRuntimeState.goronPowerStage;
        if (sRuntimeState.goronPowerStage >= 2) {
            speedTarget = 18.0f;
            Math_StepToS(&sRuntimeState.goronRollChargeTimer, 4, 1);
            if (--sRuntimeState.goronMagicDrainTimer <= 0) {
                if (!ConsumeMagicQuietly(1) || !holdingA ||
                    (sRuntimeState.goronRollChargeTimer == 4 && sRuntimeState.goronRollBallSpeed < 12.0f)) {
                    sRuntimeState.goronPowerStage = 1;
                    sRuntimeState.goronRollChargeTimer = 4;
                    sRuntimeState.goronSpikeMode = true;
                    Magic_Reset(play);
                }
                sRuntimeState.goronMagicDrainTimer = 10;
            }
        } else if (isGrounded) {
            if (HasAvailableMagic() && sRuntimeState.goronRollSpinRate >= kGoronSpikeSpinThreshold) {
                sRuntimeState.goronRollChargeTimer =
                    std::min<s16>(kGoronSpikeVelocityFrames, sRuntimeState.goronRollChargeTimer + 1);
            } else {
                sRuntimeState.goronRollChargeTimer = 4;
            }

            if (sRuntimeState.goronRollChargeTimer >= kGoronSpikeVelocityFrames && ConsumeMagicQuietly(2)) {
                sRuntimeState.goronRollBallSpeed = 18.0f;
                sRuntimeState.goronPowerStage = 2;
                sRuntimeState.goronSpikeMode = true;
                sRuntimeState.goronMagicDrainTimer = 10;
            } else if (sRuntimeState.goronRollChargeTimer >= kGoronHeatVelocityFrames) {
                sRuntimeState.goronPowerStage = 1;
                sRuntimeState.goronSpikeMode = true;
            } else {
                sRuntimeState.goronPowerStage = 0;
                sRuntimeState.goronSpikeMode = false;
            }
        }

        if (previousPowerStage < 1 && sRuntimeState.goronPowerStage >= 1) {
            PlayMmGoronOneShot(kMmCarrierSfxGoronHeatStart, &player->actor.projectedPos, 1.0f);
        }
        if (previousPowerStage < 2 && sRuntimeState.goronPowerStage >= 2) {
            PlayMmGoronOneShot(kMmCarrierSfxGoronSpikeStart, &player->actor.projectedPos, 1.0f);
        }

        if (isGrounded) {
            const s16 steeringLean = static_cast<s16>((yawTarget - player->yaw) * -0.5f);
            sRuntimeState.goronRollBounce += static_cast<f32>(SQ(steeringLean)) * 8e-9f;
            sRuntimeState.goronRollBounce = CLAMP(sRuntimeState.goronRollBounce, 0.0f, 0.9f);

            const s16 yawDiff = player->yaw - sRuntimeState.goronRollHomeYaw;
            f32 forwardSpeed = (1.0f - sRuntimeState.goronRollBounce) * sRuntimeState.goronRollBallSpeed * Math_CosS(yawDiff);
            if (forwardSpeed < 0.0f || (speedTarget == 0.0f && ABS(yawDiff) > 0xFA0)) {
                forwardSpeed = 0.0f;
            }

            Math_StepToF(&sRuntimeState.goronRollBounce, 0.0f, fabsf(Math_CosS(yawDiff)) * 20.0f);
            Math_AsymStepToF(&forwardSpeed, speedTarget, std::max<f32>(0.0f, ABS(sRuntimeState.goronRollSpinRate) * 0.0003f),
                             std::max<f32>(0.0f, (Math_SinS(player->floorPitch) * 8.0f) + 0.6f));

            const s16 turnRate =
                fabsf(forwardSpeed) < 2.0f ? static_cast<s16>(fabsf(forwardSpeed) * 50.0f) + 50
                                           : static_cast<s16>(fabsf(forwardSpeed) * 20.0f) + 300;
            Math_ScaledStepToS(&sRuntimeState.goronRollHomeYaw, yawTarget, turnRate);
            player->yaw = sRuntimeState.goronRollHomeYaw;

            sRuntimeState.goronRollBallSpeed = std::min<f32>(18.0f, forwardSpeed);
            player->linearVelocity = sRuntimeState.goronRollBallSpeed * Math_CosS(player->floorPitch);
            player->actor.velocity.y = sRuntimeState.goronRollBallSpeed * Math_SinS(player->floorPitch);
            player->actor.world.rot.y = player->yaw;
        } else if (sRuntimeState.goronPowerStage >= 2) {
            player->actor.gravity = -1.0f;
            if (player->actor.velocity.y > 0.0f) {
                player->actor.velocity.y = 0.0f;
            }
            player->stateFlags1 &= ~(PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL);
            player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
            Math_ScaledStepToS(&sRuntimeState.goronRollHomeYaw, yawTarget, 0x190);
            sRuntimeState.goronRollBallSpeed =
                std::max<f32>(sRuntimeState.goronRollBallSpeed,
                              sqrtf(SQ(player->linearVelocity) + SQ(player->actor.velocity.y)));
            sRuntimeState.goronRollBallSpeed = std::min<f32>(18.0f, sRuntimeState.goronRollBallSpeed);
            player->yaw = sRuntimeState.goronRollHomeYaw;
            player->actor.world.rot.y = player->yaw;
        } else {
            player->actor.gravity = -2.0f;
            if (player->actor.velocity.y > 0.0f) {
                player->actor.velocity.y = 0.0f;
            }
            player->stateFlags1 &= ~(PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL);
            player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
            sRuntimeState.goronRollTilt += player->actor.velocity.y * 0.005f;
            sRuntimeState.goronRollBallSpeed =
                std::max<f32>(sRuntimeState.goronRollBallSpeed, fabsf(player->linearVelocity));
            sRuntimeState.goronRollBallSpeed = std::min<f32>(18.0f, sRuntimeState.goronRollBallSpeed);
            player->yaw = sRuntimeState.goronRollHomeYaw;
            player->actor.world.rot.y = player->yaw;
        }

        sRuntimeState.goronRollSpeed = sRuntimeState.goronRollBallSpeed;
        player->actor.speedXZ = sRuntimeState.goronRollSpeed;
        player->linearVelocity = sRuntimeState.goronRollSpeed;
        player->actor.world.rot.y = player->yaw;
        Math_ScaledStepToS(&player->actor.shape.rot.y, sRuntimeState.goronRollHomeYaw, 0x7D0);

        const s16 previousRollRot = player->actor.shape.rot.x;
        const s16 spinTarget = static_cast<s16>((speedTarget < 2.0f) ? speedTarget * 300.0f : speedTarget * 900.0f);
        const s16 spinDiff = spinTarget - sRuntimeState.goronRollSpinRate;
        const s16 spinStep = spinDiff >= 0 ? 0x7D0 : 0x4B0;
        if (ABS(spinDiff) <= spinStep) {
            sRuntimeState.goronRollSpinRate = spinTarget;
        } else {
            sRuntimeState.goronRollSpinRate += spinDiff > 0 ? spinStep : -spinStep;
        }
        player->actor.shape.rot.x += sRuntimeState.goronRollSpinRate;
        player->actor.world.rot.x = player->actor.shape.rot.x;
        sRuntimeState.goronRollVisualAngle = BINANG_TO_RAD(player->actor.shape.rot.x);
        sRuntimeState.goronRollBouncePhase += sRuntimeState.goronRollSpeed * 0.22f;
        if (sRuntimeState.goronRollBouncePhase > (M_PI * 2.0f)) {
            sRuntimeState.goronRollBouncePhase -= M_PI * 2.0f;
        }

        if (isGrounded && sRuntimeState.goronRollSpinRate != 0) {
            const s16 currentRollRot = player->actor.shape.rot.x;
            const bool crossedCycle =
                ((previousRollRot <= 0) && (currentRollRot > 0)) || ((previousRollRot >= 0) && (currentRollRot < 0));
            if (crossedCycle) {
                UpdateMmGoronAudioForState(player);
            }
        }

        const f32 squashTarget = ABS(sRuntimeState.goronRollSpinRate) * 0.00004f;
        sRuntimeState.goronRollTilt += (sRuntimeState.goronRollSquash < squashTarget) ? 0.08f : -0.07f;
        sRuntimeState.goronRollTilt = CLAMP(sRuntimeState.goronRollTilt, -0.2f, 0.14f);
        if (fabsf(sRuntimeState.goronRollTilt) < 0.12f) {
            if (Math_StepUntilF(&sRuntimeState.goronRollSquash, squashTarget, sRuntimeState.goronRollTilt)) {
                sRuntimeState.goronRollTilt = 0.0f;
            }
        } else {
            sRuntimeState.goronRollSquash += sRuntimeState.goronRollTilt;
            sRuntimeState.goronRollSquash = CLAMP(sRuntimeState.goronRollSquash, -0.7f, 0.3f);
        }
        Math_AsymStepToF(&sRuntimeState.goronRollColorLerp,
                         (sRuntimeState.goronAction == GORON_ACTION_SLAM_JUMP ||
                          sRuntimeState.goronAction == GORON_ACTION_SLAM_POUND)
                             ? 1.0f
                             : 0.0f,
                         0.8f, 0.05f);

        if (sRuntimeState.goronSpikeMode) {
            const bool usingMmHeatEffects = sRuntimeState.goronSpikeEffectDL1 != nullptr || sRuntimeState.goronSpikeEffectDL2 != nullptr;
            if (!usingMmHeatEffects && sRuntimeState.goronPowerStage < 2 && (play->state.frames & 3) == 0) {
                for (s32 i = 0; i < 4; ++i) {
                    const s16 angle = static_cast<s16>((play->state.frames * 0x900) + (i * 0x4000));
                    Vec3f flamePos = player->actor.world.pos;
                    flamePos.y += 10.0f;
                    flamePos.x += Math_SinS(angle) * 16.0f;
                    flamePos.z += Math_CosS(angle) * 16.0f;
                    EffectSsEnFire_SpawnVec3f(play, &player->actor, &flamePos, 90, 0, 0, -1);
                }
            }
            if (sRuntimeState.goronSpikePulseTimer > 0) {
                sRuntimeState.goronSpikePulseTimer--;
            } else if (!ConsumeMagicQuietly(1)) {
                sRuntimeState.goronSpikeMode = false;
                sRuntimeState.goronPowerStage = 0;
                sRuntimeState.goronRollChargeTimer = 0;
                Magic_Reset(play);
            } else {
                sRuntimeState.goronSpikePulseTimer = 20;
            }
        }
        UpdateMmGoronAudioForState(player);
        return;
    }

    if (sRuntimeState.goronAction == GORON_ACTION_ROLL || sRuntimeState.goronAction == GORON_ACTION_ROLL_START) {
        StopGoronAction(play, player);
    } else {
        player->actor.shape.rot.x = 0;
        player->actor.shape.yOffset = 0.0f;
        player->actor.world.rot.x = 0;
    }
    UpdateMmGoronAudioForState(player);
}

bool IsZoraBoomerangAlive(Actor* boomerang) {
    return boomerang != nullptr && boomerang->update != nullptr;
}

Actor* UpdateZoraBoomerangCameraTarget(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr) {
        return nullptr;
    }

    const bool leftAlive = IsZoraBoomerangAlive(sRuntimeState.zoraBoomerangActorL);
    const bool rightAlive = IsZoraBoomerangAlive(sRuntimeState.zoraBoomerangActorR);
    if (!leftAlive && !rightAlive) {
        return nullptr;
    }

    Vec3f avg = {};
    s32 count = 0;
    if (leftAlive) {
        avg.x += sRuntimeState.zoraBoomerangActorL->world.pos.x;
        avg.y += sRuntimeState.zoraBoomerangActorL->world.pos.y;
        avg.z += sRuntimeState.zoraBoomerangActorL->world.pos.z;
        count++;
    }
    if (rightAlive) {
        avg.x += sRuntimeState.zoraBoomerangActorR->world.pos.x;
        avg.y += sRuntimeState.zoraBoomerangActorR->world.pos.y;
        avg.z += sRuntimeState.zoraBoomerangActorR->world.pos.z;
        count++;
    }

    const f32 invCount = 1.0f / count;
    avg.x *= invCount;
    avg.y *= invCount;
    avg.z *= invCount;

    Actor* target = &sRuntimeState.zoraBoomerangCameraTarget;
    target->id = ACTOR_PLAYER;
    target->category = ACTORCAT_PLAYER;
    target->room = player->actor.room;
    target->update = TransformMaskVirtualActorUpdate;
    target->world.pos = avg;
    target->focus.pos = avg;
    target->focus.pos.y += 8.0f;
    target->world.rot = player->actor.world.rot;
    target->focus.rot = player->actor.focus.rot;

    player->boomerangActor = target;
    Camera* camera = Play_GetCamera(play, MAIN_CAM);
    if (camera != nullptr) {
        if (camera->mode != CAM_MODE_FOLLOWBOOMERANG) {
            Camera_ChangeMode(camera, CAM_MODE_FOLLOWBOOMERANG);
        }
        Camera_SetParam(camera, 8, target);
    }

    return target;
}

extern "C" void OoTxMm_UpdateZoraBoomerangAimCamera(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || !OoTxMm_IsZoraBoomerangAiming(player)) {
        return;
    }

    // MM/2Ship Player_GetHeight places Zora's aim camera exactly 68 units above the collision-box bottom.
    const f32 headHeight = 68.0f;
    const s16 yaw = player->actor.focus.rot.y;
    const s16 pitch = player->actor.focus.rot.x;
    const f32 cosPitch = Math_CosS(pitch);
    Vec3f forward = {
        Math_SinS(yaw) * cosPitch,
        -Math_SinS(pitch),
        Math_CosS(yaw) * cosPitch,
    };
    Vec3f head = {
        player->actor.world.pos.x,
        player->actor.world.pos.y + headHeight,
        player->actor.world.pos.z,
    };
    Vec3f at = {
        head.x + forward.x * 140.0f,
        head.y + forward.y * 140.0f,
        head.z + forward.z * 140.0f,
    };
    Vec3f eye = {
        head.x - forward.x * 15.0f,
        head.y - forward.y * 15.0f + 1.5f,
        head.z - forward.z * 15.0f,
    };

    Camera* camera = Play_GetCamera(play, MAIN_CAM);
    if (camera != nullptr && camera->mode != CAM_MODE_BOOMERANG) {
        Camera_ChangeMode(camera, CAM_MODE_BOOMERANG);
    }
    Play_CameraSetAtEye(play, MAIN_CAM, &at, &eye);
}

void StartZoraBoomerangAim(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || sRuntimeState.zoraBoomerangState != ZORA_BOOMERANG_IDLE) {
        return;
    }

    if (sRuntimeState.zoraCutterWaitAnimHeader.segment != nullptr) {
        LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.zoraCutterWaitAnimHeader, 1.0f, 0.0f,
                             Animation_GetLastFrame(&sRuntimeState.zoraCutterWaitAnimHeader), ANIMMODE_LOOP, -6.0f);
    }

    sRuntimeState.zoraBoomerangState = ZORA_BOOMERANG_AIM;
    sRuntimeState.zoraBoomerangTimer = 0;
    sRuntimeState.zoraBoomerangAimYaw = 0;
    sRuntimeState.zoraBoomerangAimPitch = 0;
    sRuntimeState.zoraBoomerangLockedYaw = player->actor.shape.rot.y;
    sRuntimeState.zoraBoomerangActorL = nullptr;
    sRuntimeState.zoraBoomerangActorR = nullptr;
    sRuntimeState.motionState = TRANSFORM_MASK_MOTION_ZORA_BOOMERANG;

    player->linearVelocity = 0.0f;
    player->actor.speedXZ = 0.0f;
    player->heldItemAction = PLAYER_IA_BOOMERANG;
    player->itemAction = PLAYER_IA_BOOMERANG;
    player->unk_834 = 10;
    player->stateFlags1 |= PLAYER_STATE1_USING_BOOMERANG;
    player->stateFlags1 &= ~(PLAYER_STATE1_ITEM_IN_HAND | PLAYER_STATE1_READY_TO_FIRE | PLAYER_STATE1_FIRST_PERSON);
    player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
    player->unk_6AD = 2;
    OoTxMm_PlayerStartZoraBoomerangAimCamera(player, play);
}

void FinishZoraBoomerang(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr) {
        return;
    }

    sRuntimeState.zoraBoomerangActorL = nullptr;
    sRuntimeState.zoraBoomerangActorR = nullptr;
    sRuntimeState.zoraBoomerangState = ZORA_BOOMERANG_IDLE;
    sRuntimeState.zoraBoomerangTimer = 0;
    sRuntimeState.zoraBoomerangAimYaw = 0;
    sRuntimeState.zoraBoomerangAimPitch = 0;
    player->stateFlags1 &= ~PLAYER_STATE1_BOOMERANG_THROWN;
    player->stateFlags1 &= ~(PLAYER_STATE1_USING_BOOMERANG | PLAYER_STATE1_ITEM_IN_HAND | PLAYER_STATE1_READY_TO_FIRE);
    if (!IsZTargeting(player)) {
        player->stateFlags1 &= ~PLAYER_STATE1_PARALLEL;
    }
    player->boomerangActor = nullptr;
    sRuntimeState.zoraBoomerangCameraTarget.update = nullptr;
    player->heldItemAction = PLAYER_IA_SWORD_MASTER;
    player->itemAction = PLAYER_IA_SWORD_MASTER;
    player->upperLimbRot.x = 0;
    player->upperLimbRot.y = 0;
}

void SpawnZoraBoomerangPair(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr) {
        return;
    }

    const s16 rotY = player->actor.shape.rot.y;
    const s16 pitchX = sRuntimeState.zoraBoomerangAimPitch;
    const bool hasTarget = player->focusActor != nullptr;
    const s16 yawLeft = hasTarget ? static_cast<s16>(rotY + 0x36B0) : static_cast<s16>(rotY - 0x190);
    const s16 yawRight = hasTarget ? static_cast<s16>(rotY - 0x36B0) : static_cast<s16>(rotY + 0x190);
    Vec3f leftPos = player->bodyPartsPos[PLAYER_BODYPART_L_HAND];
    Vec3f rightPos = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];
    leftPos.y = player->actor.world.pos.y + 50.0f;
    rightPos.y = player->actor.world.pos.y + 50.0f;

    EnBoom* leftBoom = reinterpret_cast<EnBoom*>(
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOOM, leftPos.x, leftPos.y, leftPos.z, pitchX, yawLeft, 0, 1));
    EnBoom* rightBoom = reinterpret_cast<EnBoom*>(
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOOM, rightPos.x, rightPos.y, rightPos.z, pitchX, yawRight, 0, 2));

    if (leftBoom != nullptr) {
        leftBoom->returnTimer = 20;
        leftBoom->moveTo = player->focusActor;
        sRuntimeState.zoraBoomerangActorL = &leftBoom->actor;
        player->boomerangActor = &leftBoom->actor;
    }

    if (rightBoom != nullptr) {
        rightBoom->returnTimer = 20;
        rightBoom->moveTo = player->focusActor;
        sRuntimeState.zoraBoomerangActorR = &rightBoom->actor;
        if (leftBoom != nullptr) {
            leftBoom->actor.child = &rightBoom->actor;
            rightBoom->actor.parent = &leftBoom->actor;
        } else {
            player->boomerangActor = &rightBoom->actor;
        }
    }

    if (leftBoom != nullptr || rightBoom != nullptr) {
        player->stateFlags1 |= PLAYER_STATE1_BOOMERANG_THROWN;
        if (!IsZTargeting(player)) {
            player->stateFlags1 &= ~PLAYER_STATE1_PARALLEL;
        }
        player->heldItemAction = PLAYER_IA_SWORD_MASTER;
        player->itemAction = PLAYER_IA_SWORD_MASTER;
        UpdateZoraBoomerangCameraTarget(play, player);
        PlayGoronActionSfx(player, NA_SE_IT_BOOMERANG_THROW);
        PlayMmZoraVoiceOneShot(0x68A0);
    }
}

void UpdateZoraBoomerang(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || sRuntimeState.zoraBoomerangState == ZORA_BOOMERANG_IDLE) {
        return;
    }

    Input* input = &play->state.input[0];

    if (sRuntimeState.zoraBoomerangState == ZORA_BOOMERANG_AIM) {
        player->linearVelocity = 0.0f;
        player->actor.speedXZ = 0.0f;
        player->unk_6AD = 2;
        player->heldItemAction = PLAYER_IA_BOOMERANG;
        player->itemAction = PLAYER_IA_BOOMERANG;
        if (player->unk_834 == 0) {
            player->unk_834 = 10;
        }
        player->stateFlags1 |= PLAYER_STATE1_USING_BOOMERANG;
        player->stateFlags1 &= ~(PLAYER_STATE1_ITEM_IN_HAND | PLAYER_STATE1_READY_TO_FIRE | PLAYER_STATE1_FIRST_PERSON);

        sRuntimeState.zoraBoomerangAimPitch = player->actor.focus.rot.x;
        sRuntimeState.zoraBoomerangAimYaw = player->actor.focus.rot.y - sRuntimeState.zoraBoomerangLockedYaw;
        player->actor.shape.rot.y = player->actor.focus.rot.y;
        player->actor.world.rot.y = player->actor.shape.rot.y;
        player->yaw = player->actor.shape.rot.y;
        player->upperLimbRot.x = sRuntimeState.zoraBoomerangAimPitch;
        player->upperLimbRot.y = 0;

        if (!CHECK_BTN_ALL(input->cur.button, BTN_B)) {
            if (sRuntimeState.zoraCutterAttackAnimHeader.segment != nullptr) {
                LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.zoraCutterAttackAnimHeader, 1.0f,
                                     0.0f, Animation_GetLastFrame(&sRuntimeState.zoraCutterAttackAnimHeader),
                                     ANIMMODE_ONCE, -4.0f);
                sRuntimeState.zoraBoomerangState = ZORA_BOOMERANG_THROW;
                sRuntimeState.zoraBoomerangTimer = 0;
                player->unk_6AD = 0;
                player->stateFlags1 &= ~(PLAYER_STATE1_USING_BOOMERANG | PLAYER_STATE1_ITEM_IN_HAND |
                                         PLAYER_STATE1_READY_TO_FIRE);
            } else {
                FinishZoraBoomerang(play, player);
            }
        }
        return;
    }

    if (sRuntimeState.zoraBoomerangState == ZORA_BOOMERANG_THROW) {
        player->linearVelocity = 0.0f;
        player->actor.speedXZ = 0.0f;
        player->stateFlags1 |= PLAYER_STATE1_PARALLEL;
        player->actor.shape.rot.y = sRuntimeState.zoraBoomerangLockedYaw + sRuntimeState.zoraBoomerangAimYaw;
        player->actor.world.rot.y = player->actor.shape.rot.y;
        sRuntimeState.zoraBoomerangTimer++;

        const f32 endFrame = Animation_GetLastFrame(&sRuntimeState.zoraCutterAttackAnimHeader);
        const bool animDone = sRuntimeState.skelAnime.curFrame >= endFrame - 0.5f;
        if (animDone && sRuntimeState.zoraBoomerangTimer < 6) {
            sRuntimeState.zoraBoomerangTimer = 6;
        }

        if (sRuntimeState.zoraBoomerangTimer >= 6) {
            SpawnZoraBoomerangPair(play, player);
            sRuntimeState.zoraBoomerangState = ZORA_BOOMERANG_FLIGHT;
            sRuntimeState.zoraBoomerangTimer = 0;
        }
        return;
    }

    if (sRuntimeState.zoraBoomerangState == ZORA_BOOMERANG_FLIGHT) {
        const bool leftDone = !IsZoraBoomerangAlive(sRuntimeState.zoraBoomerangActorL);
        const bool rightDone = !IsZoraBoomerangAlive(sRuntimeState.zoraBoomerangActorR);
        if ((leftDone && rightDone) || sRuntimeState.zoraBoomerangTimer > 540) {
            if (IsZoraBoomerangAlive(sRuntimeState.zoraBoomerangActorL)) {
                Actor_Kill(sRuntimeState.zoraBoomerangActorL);
            }
            if (IsZoraBoomerangAlive(sRuntimeState.zoraBoomerangActorR)) {
                Actor_Kill(sRuntimeState.zoraBoomerangActorR);
            }
            player->stateFlags1 &= ~PLAYER_STATE1_BOOMERANG_THROWN;
            player->stateFlags1 &= ~(PLAYER_STATE1_USING_BOOMERANG | PLAYER_STATE1_ITEM_IN_HAND |
                                     PLAYER_STATE1_READY_TO_FIRE);
            if (!IsZTargeting(player)) {
                player->stateFlags1 &= ~PLAYER_STATE1_PARALLEL;
            }
            player->boomerangActor = nullptr;
            sRuntimeState.zoraBoomerangCameraTarget.update = nullptr;
            player->heldItemAction = PLAYER_IA_SWORD_MASTER;
            player->itemAction = PLAYER_IA_SWORD_MASTER;
            sRuntimeState.zoraBoomerangActorL = nullptr;
            sRuntimeState.zoraBoomerangActorR = nullptr;
            sRuntimeState.zoraBoomerangState = ZORA_BOOMERANG_CATCH;
            sRuntimeState.zoraBoomerangTimer = 0;
            player->upperLimbRot.x = 0;
            player->upperLimbRot.y = 0;
            if (sRuntimeState.zoraCutterCatchAnimHeader.segment != nullptr) {
                LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.zoraCutterCatchAnimHeader, 1.0f,
                                     0.0f, Animation_GetLastFrame(&sRuntimeState.zoraCutterCatchAnimHeader),
                                     ANIMMODE_ONCE, -6.0f);
            } else {
                FinishZoraBoomerang(play, player);
            }
            PlayGoronActionSfx(player, NA_SE_PL_CATCH_BOOMERANG);
            return;
        }

        UpdateZoraBoomerangCameraTarget(play, player);
        sRuntimeState.zoraBoomerangTimer++;
        return;
    }

    if (sRuntimeState.zoraBoomerangState == ZORA_BOOMERANG_CATCH &&
        sRuntimeState.skelAnime.curFrame >= Animation_GetLastFrame(&sRuntimeState.zoraCutterCatchAnimHeader) - 0.5f) {
        FinishZoraBoomerang(play, player);
    }
}

void ResetZoraSwimRuntime(Player* player, PlayState* play) {
    sRuntimeState.zoraSwimState = ZORA_SWIM_IDLE;
    sRuntimeState.zoraFastSwimActive = false;
    sRuntimeState.zoraSwimPitch = 0;
    sRuntimeState.zoraSwimRoll = 0;
    sRuntimeState.zoraSwimRollSmoothed = 0;
    sRuntimeState.zoraSwimYawRate = 0;
    sRuntimeState.zoraSwimPhaseCounter = 0;
    sRuntimeState.zoraSwimSpeed = 0.0f;
    sRuntimeState.zoraDolphinReentryLocked = false;
    if (player != nullptr) {
        player->actor.shape.rot.x = 0;
        player->actor.shape.rot.z = 0;
        player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
        player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
        if (sRuntimeState.zoraBootsActive && play != nullptr) {
            player->currentBoots = PLAYER_BOOTS_KOKIRI;
            Player_SetBootData(play, player);
        }
    }
    sRuntimeState.zoraBootsActive = false;
}

void NormalizeZoraFastSwimCamera(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr) {
        return;
    }

    player->stateFlags2 &= ~(PLAYER_STATE2_UNDERWATER | PLAYER_STATE2_DIVING);

    Camera* camera = Play_GetCamera(play, MAIN_CAM);
    if (camera == nullptr) {
        return;
    }

    if (camera->setting == CAM_SET_PIVOT_WATER_SURFACE) {
        const s16 previousSetting = static_cast<s16>(camera->waterPrevCamSetting);
        if (previousSetting > CAM_SET_NONE && previousSetting < CAM_SET_MAX &&
            previousSetting != CAM_SET_PIVOT_WATER_SURFACE) {
            Camera_ChangeSetting(camera, previousSetting);
        } else {
            Camera_ChangeSetting(camera, CAM_SET_NORMAL0);
        }
    }

    if (camera->mode != CAM_MODE_NORMAL) {
        Camera_ChangeMode(camera, CAM_MODE_NORMAL);
    }
}

void StartZoraFastSwim(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr) {
        return;
    }

    sRuntimeState.zoraSwimState = sRuntimeState.zoraWaterRollAnimHeader.segment != nullptr ? ZORA_SWIM_FAST_START : ZORA_SWIM_FAST;
    sRuntimeState.zoraFastSwimActive = true;
    sRuntimeState.zoraBootsActive = false;
    sRuntimeState.zoraSwimPhaseCounter = 1;
    sRuntimeState.zoraSwimSpeed = std::max<f32>(player->linearVelocity, 8.0f);
    sRuntimeState.zoraSwimPitch = 0;
    sRuntimeState.zoraSwimRoll = 0;
    sRuntimeState.zoraSwimRollSmoothed = 0;
    sRuntimeState.zoraSwimYawRate = 0;
    sRuntimeState.zoraSwimActionTimer = 0;
    sRuntimeState.zoraDolphinYaw = 0;
    sRuntimeState.zoraDolphinLinearVelocity = 0.0f;
    sRuntimeState.zoraDolphinReentryLocked = false;
    sRuntimeState.motionState = TRANSFORM_MASK_MOTION_ZORA_FAST_SWIM;

    player->currentBoots = PLAYER_BOOTS_KOKIRI;
    Player_SetBootData(play, player);
    player->actor.velocity.y = 0.0f;
    player->actor.gravity = 0.0f;
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    player->stateFlags1 &= ~(PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL);
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    NormalizeZoraFastSwimCamera(play, player);

    if (sRuntimeState.zoraWaterRollAnimHeader.segment != nullptr) {
        LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.zoraWaterRollAnimHeader, 1.0f, 4.0f,
                             Animation_GetLastFrame(&sRuntimeState.zoraWaterRollAnimHeader), ANIMMODE_ONCE, -6.0f);
    } else if (sRuntimeState.zoraFishSwimAnimHeader.segment != nullptr) {
        LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.zoraFishSwimAnimHeader, 1.0f, 0.0f,
                             Animation_GetLastFrame(&sRuntimeState.zoraFishSwimAnimHeader), ANIMMODE_LOOP, -6.0f);
    }

    PlayMmZoraSwimDashSfx();
}

void ExitZoraFastSwim(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr) {
        return;
    }

    sRuntimeState.zoraFastSwimActive = false;
    sRuntimeState.zoraSwimYawRate = 0;
    player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
    player->actor.shape.rot.x = 0;
    player->actor.shape.rot.z = 0;

    if (sRuntimeState.zoraSwimToWaitAnimHeader.segment != nullptr) {
        sRuntimeState.zoraSwimState = ZORA_SWIM_FAST_EXIT;
        LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.zoraSwimToWaitAnimHeader, 1.0f, 0.0f,
                             Animation_GetLastFrame(&sRuntimeState.zoraSwimToWaitAnimHeader), ANIMMODE_ONCE, -6.0f);
    } else {
        sRuntimeState.zoraSwimState = ZORA_SWIM_IDLE;
    }
}

void SpawnZoraSwimEffects(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || player->actor.yDistToWater < 20.0f) {
        return;
    }

    Vec3f ripplePos = { player->actor.world.pos.x, player->actor.world.pos.y + player->actor.yDistToWater,
                        player->actor.world.pos.z };

    if ((sRuntimeState.zoraSwimActionTimer & 7) == 0 && fabsf(player->linearVelocity) > 0.5f) {
        EffectSsGRipple_Spawn(play, &ripplePos, 100, 500, 0);
    }

    if (player->actor.yDistToWater > kZoraDeepThreshold) {
        s32 bubbleCount = 0;
        if (sRuntimeState.zoraFastSwimActive) {
            const f32 factor =
                (ABS(sRuntimeState.zoraSwimYawRate) * 0.004f) + (sRuntimeState.zoraSwimSpeed * 0.38f);
            bubbleCount = static_cast<s32>(factor);
            if (bubbleCount == 0 && Rand_ZeroOne() < 0.2f) {
                bubbleCount = 1;
            }
        } else if (player->actor.velocity.y < 0.0f) {
            bubbleCount = static_cast<s32>(player->actor.velocity.y * -0.3f);
        } else if (Rand_ZeroOne() < 0.1f) {
            bubbleCount = 1;
        }

        bubbleCount = CLAMP(bubbleCount, 0, 8);
        Vec3f bubblePos = player->actor.world.pos;
        bubblePos.y += 20.0f;
        for (s32 i = 0; i < bubbleCount; i++) {
            EffectSsBubble_Spawn(play, &bubblePos, 20.0f, 10.0f, 20.0f, 0.13f);
        }
    }

    if (sRuntimeState.zoraSwimActionTimer == 1 && sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_START) {
        s16 splashScale = static_cast<s16>((fabsf(player->linearVelocity) * 50.0f) +
                                           (player->actor.yDistToWater * 5.0f));
        splashScale = CLAMP(splashScale, 0, 500);
        const s16 splashType = fabsf(player->linearVelocity) > 10.0f ? 1 : 0;
        EffectSsGSplash_Spawn(play, &ripplePos, nullptr, nullptr, splashType, splashScale);
    }

    if (sRuntimeState.zoraFastSwimActive && (sRuntimeState.zoraSwimActionTimer & 3) == 0) {
        const f32 waterY = player->actor.world.pos.y + player->actor.yDistToWater;
        static const s32 sSwimSplashParts[] = {
            PLAYER_BODYPART_L_HAND,
            PLAYER_BODYPART_R_HAND,
            PLAYER_BODYPART_L_FOOT,
            PLAYER_BODYPART_R_FOOT,
        };

        for (s32 bodyPart : sSwimSplashParts) {
            if (fabsf(player->bodyPartsPos[bodyPart].y - waterY) < 30.0f) {
                Vec3f splashPos = player->bodyPartsPos[bodyPart];
                splashPos.y = waterY;
                EffectSsGSplash_Spawn(play, &splashPos, nullptr, nullptr, 0, 80);
            }
        }
    }
}

void SpawnZoraDolphinSurfaceEffects(PlayState* play, Player* player, f32 waterY) {
    if (play == nullptr || player == nullptr) {
        return;
    }

    Vec3f surfacePos = player->actor.world.pos;
    surfacePos.y = waterY;
    const f32 totalSpeed = sqrtf(SQ(player->linearVelocity) + SQ(player->actor.velocity.y));
    const s16 splashScale = CLAMP(static_cast<s16>(totalSpeed * 50.0f), 160, 500);
    const s16 splashType = totalSpeed > 10.0f ? 1 : 0;

    EffectSsGRipple_Spawn(play, &surfacePos, 100, 500, 0);
    EffectSsGSplash_Spawn(play, &surfacePos, nullptr, nullptr, splashType, splashScale);
}

bool StartZoraDolphinJump(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr) {
        return false;
    }

    const bool fastSwimVisual =
        sRuntimeState.zoraFastSwimActive || sRuntimeState.zoraSwimState == ZORA_SWIM_FAST ||
        sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_START || sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_EXIT;
    if (!fastSwimVisual || sRuntimeState.zoraDolphinReentryLocked) {
        return false;
    }

    // A pitch beyond straight-up gives Math_CosS a negative result and reverses horizontal travel.
    // MM's player action prevents that invalid surface-exit trajectory before launch.
    const s16 launchPitch = std::max<s16>(sRuntimeState.zoraSwimPitch, -0x3FFF);
    if (launchPitch >= -0x1555) {
        return false;
    }

    const f32 projectedYVelocity = -Math_SinS(launchPitch) * sRuntimeState.zoraSwimSpeed;
    const f32 predictedDepth = player->actor.yDistToWater - projectedYVelocity;
    if (predictedDepth >= kZoraDeepThreshold) {
        return false;
    }

    f32 launchSpeed = sRuntimeState.zoraSwimSpeed * 1.5f;
    launchSpeed = std::min(launchSpeed, 13.5f);
    if (launchSpeed < 2.0f) {
        return false;
    }

    sRuntimeState.zoraSwimPitch = launchPitch;
    sRuntimeState.zoraDolphinYaw = player->yaw;
    player->linearVelocity = Math_CosS(sRuntimeState.zoraSwimPitch) * launchSpeed;
    sRuntimeState.zoraDolphinLinearVelocity = player->linearVelocity;
    player->actor.speedXZ = player->linearVelocity;
    player->actor.velocity.y = -Math_SinS(sRuntimeState.zoraSwimPitch) * launchSpeed;
    player->actor.gravity = -1.0f;
    player->actor.bgCheckFlags &= ~1;

    // MM's form action owns water-state transitions while the dolphin arc starts. Our transform update runs
    // after OoT's water handler, so move only to the detected water boundary before handing movement to the arc.
    // Pitch, horizontal speed, and vertical speed remain entirely trajectory-driven.
    if (player->actor.yDistToWater > 0.0f) {
        const f32 waterY = player->actor.world.pos.y + player->actor.yDistToWater;
        SpawnZoraDolphinSurfaceEffects(play, player, waterY);
        player->actor.world.pos.y += player->actor.yDistToWater + 1.0f;
        player->actor.prevPos.y = player->actor.world.pos.y;
        player->actor.yDistToWater = -1.0f;
    }

    player->stateFlags1 |= PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL;
    player->stateFlags1 &= ~PLAYER_STATE1_IN_WATER;
    player->stateFlags2 &= ~(PLAYER_STATE2_UNDERWATER | PLAYER_STATE2_DIVING);
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR | PLAYER_STATE3_PAUSE_ACTION_FUNC;
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    sRuntimeState.zoraSwimState = ZORA_SWIM_DOLPHIN_JUMP;
    sRuntimeState.zoraSwimYawRate = 0;
    sRuntimeState.zoraFastSwimActive = true;
    sRuntimeState.zoraDolphinDebugCooldown = 30;
    sRuntimeState.zoraSwimActionTimer = 0;

    NormalizeZoraFastSwimCamera(play, player);

    SPDLOG_INFO("[TransformMasks][Zora] Dolphin launch pitch={} yDist={} velY={} speed={} flags1={:#x} flags2={:#x} flags3={:#x}",
                sRuntimeState.zoraSwimPitch, player->actor.yDistToWater, player->actor.velocity.y,
                sRuntimeState.zoraSwimSpeed, player->stateFlags1, player->stateFlags2, player->stateFlags3);

    if (sRuntimeState.zoraFishSwimAnimHeader.segment != nullptr &&
        sRuntimeState.skelAnime.animation != &sRuntimeState.zoraFishSwimAnimHeader) {
        LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.zoraFishSwimAnimHeader, 1.0f, 0.0f,
                             Animation_GetLastFrame(&sRuntimeState.zoraFishSwimAnimHeader), ANIMMODE_LOOP, -6.0f);
    }

    PlayGoronActionSfx(player, NA_SE_EV_JUMP_OUT_WATER);
    return true;
}

void UpdateZoraDolphinJump(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr) {
        return;
    }

    player->stateFlags1 |= PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL;
    player->stateFlags1 &= ~PLAYER_STATE1_IN_WATER;
    player->stateFlags2 &= ~(PLAYER_STATE2_UNDERWATER | PLAYER_STATE2_DIVING);
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR | PLAYER_STATE3_PAUSE_ACTION_FUNC;
    player->yaw = sRuntimeState.zoraDolphinYaw;
    player->actor.world.rot.y = sRuntimeState.zoraDolphinYaw;
    player->actor.shape.rot.y = sRuntimeState.zoraDolphinYaw;
    player->linearVelocity = sRuntimeState.zoraDolphinLinearVelocity;
    player->actor.speedXZ = sRuntimeState.zoraDolphinLinearVelocity;
    NormalizeZoraFastSwimCamera(play, player);
    Math_SmoothStepToS(&sRuntimeState.zoraSwimRoll, 0, 6, 0x7D0, 0x190);
    Math_SmoothStepToS(&sRuntimeState.zoraSwimRollSmoothed, sRuntimeState.zoraSwimRoll, 2, 0x5DC, 0x64);

    if (player->actor.yDistToWater > kZoraSwimReentryThreshold && player->actor.velocity.y <= 0.0f) {
        const f32 waterY = player->actor.world.pos.y + player->actor.yDistToWater;
        SpawnZoraDolphinSurfaceEffects(play, player, waterY);
        player->actor.gravity = 0.0f;
        sRuntimeState.zoraSwimState =
            sRuntimeState.zoraWaterRollAnimHeader.segment != nullptr ? ZORA_SWIM_FAST_START : ZORA_SWIM_FAST;
        sRuntimeState.zoraSwimPhaseCounter = 1;
        sRuntimeState.zoraSwimSpeed = std::max<f32>(player->linearVelocity, 8.0f);
        sRuntimeState.zoraSwimPitch = 0;
        sRuntimeState.zoraSwimRoll = 0;
        sRuntimeState.zoraSwimRollSmoothed = 0;
        sRuntimeState.zoraSwimYawRate = 0;
        sRuntimeState.zoraDolphinLinearVelocity = 0.0f;
        sRuntimeState.zoraFastSwimActive = true;
        sRuntimeState.zoraDolphinReentryLocked = true;
        sRuntimeState.zoraSwimActionTimer = 1;
        player->actor.velocity.y = 0.0f;
        player->stateFlags1 |= PLAYER_STATE1_IN_WATER;
        player->stateFlags1 &= ~(PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL);
        player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
        player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
        player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
        NormalizeZoraFastSwimCamera(play, player);
        if (sRuntimeState.zoraWaterRollAnimHeader.segment != nullptr) {
            LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.zoraWaterRollAnimHeader, 1.0f, 4.0f,
                                 Animation_GetLastFrame(&sRuntimeState.zoraWaterRollAnimHeader), ANIMMODE_ONCE, -6.0f);
        }
        PlayMmZoraSwimDashSfx();
        return;
    }

    if ((player->actor.bgCheckFlags & 1) != 0 && player->actor.velocity.y <= 0.0f) {
        ResetZoraSwimRuntime(player, play);
        player->actor.gravity = -1.0f;
        sRuntimeState.motionState = TRANSFORM_MASK_MOTION_UNSET;
        Player_SetupRoll(player, play);
        return;
    }

    player->actor.gravity = -1.0f;
    sRuntimeState.zoraSwimPitch = Math_Atan2S(player->linearVelocity, -player->actor.velocity.y);
}

void UpdateZoraFastSwim(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr) {
        return;
    }

    Input* input = &play->state.input[0];

    if (sRuntimeState.zoraSwimState == ZORA_SWIM_DOLPHIN_JUMP) {
        UpdateZoraDolphinJump(play, player);
        return;
    }

    if ((player->stateFlags1 & PLAYER_STATE1_IN_WATER) == 0) {
        ResetZoraSwimRuntime(player, play);
        return;
    }

    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    player->stateFlags1 &= ~(PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL);
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    if (sRuntimeState.zoraDolphinReentryLocked && player->actor.yDistToWater >= kZoraDeepThreshold) {
        sRuntimeState.zoraDolphinReentryLocked = false;
    }
    player->actor.gravity = 0.0f;
    NormalizeZoraFastSwimCamera(play, player);
    SpawnZoraSwimEffects(play, player);

    if (sRuntimeState.zoraFastSwimActive && StartZoraDolphinJump(play, player)) {
        return;
    }

    if (sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_START) {
        if (!CHECK_BTN_ALL(input->cur.button, BTN_A)) {
            ExitZoraFastSwim(play, player);
            return;
        }

        if (sRuntimeState.skelAnime.curFrame >= 13.0f) {
            sRuntimeState.zoraSwimSpeed = std::max<f32>(sRuntimeState.zoraSwimSpeed, 16.0f);
            sRuntimeState.zoraFastSwimActive = true;
        }

        if (sRuntimeState.skelAnime.curFrame >= Animation_GetLastFrame(&sRuntimeState.zoraWaterRollAnimHeader) - 0.5f) {
            sRuntimeState.zoraSwimState = ZORA_SWIM_FAST;
            sRuntimeState.zoraSwimRoll = 0;
            sRuntimeState.zoraSwimRollSmoothed = 0;
            sRuntimeState.zoraSwimYawRate = 0;
            if (sRuntimeState.zoraFishSwimAnimHeader.segment != nullptr) {
                LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.zoraFishSwimAnimHeader, 1.0f, 0.0f,
                                     Animation_GetLastFrame(&sRuntimeState.zoraFishSwimAnimHeader), ANIMMODE_LOOP, -6.0f);
            }
        }
    } else if (sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_EXIT) {
        Math_SmoothStepToS(&sRuntimeState.zoraSwimRoll, 0, 4, 0xFA0, 0x190);
        if (sRuntimeState.skelAnime.curFrame >= Animation_GetLastFrame(&sRuntimeState.zoraSwimToWaitAnimHeader) - 0.5f) {
            sRuntimeState.zoraSwimState = ZORA_SWIM_IDLE;
            sRuntimeState.zoraSwimPitch = 0;
            sRuntimeState.zoraSwimRoll = 0;
            sRuntimeState.zoraSwimRollSmoothed = 0;
        }
        return;
    } else {
        if (!CHECK_BTN_ALL(input->cur.button, BTN_A)) {
            ExitZoraFastSwim(play, player);
            return;
        }
        if (sRuntimeState.zoraFishSwimAnimHeader.segment != nullptr &&
            sRuntimeState.skelAnime.animation != &sRuntimeState.zoraFishSwimAnimHeader) {
            LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.zoraFishSwimAnimHeader, 1.0f, 0.0f,
                                 Animation_GetLastFrame(&sRuntimeState.zoraFishSwimAnimHeader), ANIMMODE_LOOP, -6.0f);
        }
    }

    const f32 speedTarget = sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_START ? 12.0f : 9.0f;
    Math_AsymStepToF(&sRuntimeState.zoraSwimSpeed, speedTarget, 1.0f, (fabsf(sRuntimeState.zoraSwimSpeed) * 0.01f) + 0.4f);
    if (player->actor.bgCheckFlags & 8) {
        sRuntimeState.zoraSwimSpeed *= 0.5f;
        player->linearVelocity *= 0.5f;
    }
    PlayGoronActionSfx(player, NA_SE_PL_SWIM);

    const s8 stickX = input->rel.stick_x;
    const s8 stickY = input->rel.stick_y;
    s16 pitchTarget = static_cast<s16>(stickY * 0xC8);
    if (player->actor.yDistToWater < 120.0f && sRuntimeState.zoraDolphinDebugCooldown == 0) {
        SPDLOG_INFO("[TransformMasks][Zora] Near surface state={} fast={} yDist={} velY={} pitch={} pitchTarget={} speed={} stick=({}, {}) flags1={:#x} flags2={:#x}",
                    static_cast<int>(sRuntimeState.zoraSwimState), sRuntimeState.zoraFastSwimActive,
                    player->actor.yDistToWater, player->actor.velocity.y, sRuntimeState.zoraSwimPitch, pitchTarget,
                    sRuntimeState.zoraSwimSpeed, input->rel.stick_x, input->rel.stick_y, player->stateFlags1,
                    player->stateFlags2);
        sRuntimeState.zoraDolphinDebugCooldown = 30;
    } else if (sRuntimeState.zoraDolphinDebugCooldown > 0) {
        sRuntimeState.zoraDolphinDebugCooldown--;
    }
    if (sRuntimeState.zoraSwimPitch >= -0x1555 && player->actor.yDistToWater < (kZoraSurfaceDepth + 10.0f) &&
        pitchTarget < 0x7D0) {
        pitchTarget = 0x7D0;
    }
    Math_SmoothStepToS(&sRuntimeState.zoraSwimPitch, pitchTarget, 4, 0xFA0, 0x190);

    const f32 cosVal = Math_CosS(static_cast<s16>(stickX * 0x10E));
    s16 yawDelta = static_cast<s16>(((stickX >= 0) ? 1.0f : -1.0f) * (1.0f - cosVal) * -1100.0f);
    yawDelta = CLAMP(yawDelta, -0x1F40, 0x1F40);
    player->yaw += yawDelta;
    player->actor.world.rot.y = player->yaw;
    player->actor.shape.rot.y = player->yaw;

    const s16 rollTarget = static_cast<s16>(stickX * (sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_START ? 0xC8 : 0x64));
    if (Math_ScaledStepToS(&sRuntimeState.zoraSwimYawRate, rollTarget, 0x384) && rollTarget == 0) {
        Math_SmoothStepToS(&sRuntimeState.zoraSwimRoll, 0, 4, 0x5DC, 0x64);
        Math_SmoothStepToS(&sRuntimeState.zoraSwimRollSmoothed, sRuntimeState.zoraSwimRoll, 2, 0x5DC, 0x64);
    } else {
        sRuntimeState.zoraSwimRoll += sRuntimeState.zoraSwimYawRate;
        Math_SmoothStepToS(&sRuntimeState.zoraSwimRollSmoothed, sRuntimeState.zoraSwimRoll, 2, 0x5DC, 0x64);
    }

    player->linearVelocity = Math_CosS(sRuntimeState.zoraSwimPitch) * sRuntimeState.zoraSwimSpeed;
    player->actor.speedXZ = player->linearVelocity;
    player->actor.velocity.y = -Math_SinS(sRuntimeState.zoraSwimPitch) * sRuntimeState.zoraSwimSpeed;

    sRuntimeState.zoraSwimActionTimer++;
}

void ToggleZoraIronBoots(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr) {
        return;
    }

    sRuntimeState.zoraBootsActive = !sRuntimeState.zoraBootsActive;
    player->currentBoots = sRuntimeState.zoraBootsActive ? PLAYER_BOOTS_IRON : PLAYER_BOOTS_KOKIRI;
    Player_SetBootData(play, player);
    PlayGoronActionSfx(player, sRuntimeState.zoraBootsActive ? NA_SE_PL_BOUND : NA_SE_PL_DIVE_BUBBLE);
}

void UpdateZoraSwimming(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || player->currentMask != PLAYER_MASK_ZORA) {
        return;
    }

    Input* input = &play->state.input[0];
    const bool inWater = (player->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0;

    player->currentTunic = PLAYER_TUNIC_ZORA;
    player->underwaterTimer = 0;

    if (!inWater) {
        if (sRuntimeState.zoraSwimState == ZORA_SWIM_DOLPHIN_JUMP) {
            UpdateZoraFastSwim(play, player);
            return;
        }
        if (sRuntimeState.zoraFastSwimActive || sRuntimeState.zoraBootsActive) {
            ResetZoraSwimRuntime(player, play);
        }
        return;
    }

    if (sRuntimeState.zoraFastSwimActive || sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_START ||
        sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_EXIT || sRuntimeState.zoraSwimState == ZORA_SWIM_DOLPHIN_JUMP) {
        UpdateZoraFastSwim(play, player);
        return;
    }

    sRuntimeState.zoraSwimState = ZORA_SWIM_IDLE;
    sRuntimeState.zoraSwimPitch = 0;
    sRuntimeState.zoraSwimRoll = 0;
    sRuntimeState.zoraSwimRollSmoothed = 0;
    sRuntimeState.zoraSwimYawRate = 0;
    player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;

    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        StartZoraFastSwim(play, player);
        return;
    }

    const bool onWaterFloor = sRuntimeState.zoraBootsActive && ((player->actor.bgCheckFlags & 1) != 0);
    if (CHECK_BTN_ALL(input->press.button, BTN_B) && !onWaterFloor) {
        ToggleZoraIronBoots(play, player);
    }
}

void ResetZoraBarrierEnvironment(PlayState* play) {
    if (play == nullptr) {
        return;
    }

    play->envCtx.adjAmbientColor[0] = 0;
    play->envCtx.adjAmbientColor[1] = 0;
    play->envCtx.adjAmbientColor[2] = 0;
    play->envCtx.adjLight1Color[0] = 0;
    play->envCtx.adjLight1Color[1] = 0;
    play->envCtx.adjLight1Color[2] = 0;
    play->envCtx.adjFogColor[0] = 0;
    play->envCtx.adjFogColor[1] = 0;
    play->envCtx.adjFogColor[2] = 0;
    play->envCtx.adjFogNear = 0;
}

void UpdateZoraBarrier(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || player->currentMask != PLAYER_MASK_ZORA) {
        return;
    }

    const s16 previousIntensity = sRuntimeState.zoraBarrierIntensity;
    const Input* input = &play->state.input[0];
    const bool onWaterFloor =
        sRuntimeState.zoraBootsActive && player->currentBoots == PLAYER_BOOTS_IRON && (player->actor.bgCheckFlags & 1) != 0;
    const bool swimming = (player->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0 && !onWaterFloor;
    const bool groundShielding = !swimming && (player->stateFlags1 & PLAYER_STATE1_SHIELDING) != 0;
    const bool wantsBarrier = swimming ? CHECK_BTN_ALL(input->cur.button, BTN_R)
                                       : (groundShielding && CHECK_BTN_ALL(input->cur.button, BTN_B));

    if (wantsBarrier && HasAvailableMagic()) {
        if (!sRuntimeState.zoraBarrierActive) {
            sRuntimeState.zoraBarrierMagicDrainTimer = 10;
        }
        sRuntimeState.zoraBarrierActive = true;

        if (!sZoraBarrierColliderInitialized || sZoraBarrierColliderActor != &player->actor) {
            Collider_InitCylinder(play, &sZoraBarrierCollider);
            Collider_SetCylinder(play, &sZoraBarrierCollider, &player->actor, &sZoraBarrierColliderInit);
            sZoraBarrierColliderInitialized = true;
            sZoraBarrierColliderActor = &player->actor;
        }

        if (--sRuntimeState.zoraBarrierMagicDrainTimer <= 0) {
            if (!ConsumeMagicQuietly(1)) {
                sRuntimeState.zoraBarrierActive = false;
            }
            sRuntimeState.zoraBarrierMagicDrainTimer = 10;
        }
    } else {
        sRuntimeState.zoraBarrierActive = false;
    }

    const s16 targetIntensity =
        sRuntimeState.zoraBarrierActive ? static_cast<s16>(gSaveContext.magic >= 16 ? 255 : (gSaveContext.magic * 255) / 16)
                                        : 0;
    Math_StepToS(&sRuntimeState.zoraBarrierIntensity, targetIntensity, 50);

    if (sRuntimeState.zoraBarrierIntensity > 0 && sZoraBarrierColliderInitialized) {
        sZoraBarrierCollider.dim.pos.x = static_cast<s16>(player->actor.world.pos.x);
        sZoraBarrierCollider.dim.pos.y = static_cast<s16>(player->actor.world.pos.y);
        sZoraBarrierCollider.dim.pos.z = static_cast<s16>(player->actor.world.pos.z);
        CollisionCheck_SetAT(play, &play->colChkCtx, &sZoraBarrierCollider.base);
    }

    if (sRuntimeState.zoraBarrierIntensity > 0) {
        const f32 blend = sRuntimeState.zoraBarrierIntensity / 255.0f;
        play->envCtx.adjAmbientColor[0] = static_cast<s16>(-blend * 40.0f);
        play->envCtx.adjAmbientColor[1] = static_cast<s16>(-blend * 40.0f);
        play->envCtx.adjAmbientColor[2] = static_cast<s16>(-blend * 20.0f);
        play->envCtx.adjLight1Color[0] = static_cast<s16>(blend * 30.0f);
        play->envCtx.adjLight1Color[1] = static_cast<s16>(blend * 30.0f);
        play->envCtx.adjLight1Color[2] = static_cast<s16>(blend * 60.0f);
        play->envCtx.adjFogColor[0] = static_cast<s16>(-blend * 20.0f);
        play->envCtx.adjFogColor[1] = static_cast<s16>(-blend * 20.0f);
        play->envCtx.adjFogColor[2] = static_cast<s16>(blend * 30.0f);
        play->envCtx.adjFogNear = static_cast<s16>(-blend * 100.0f);
    } else if (previousIntensity > 0) {
        ResetZoraBarrierEnvironment(play);
    }

    UpdateMmZoraBarrierSfx(sRuntimeState.zoraBarrierIntensity > 0);
}

void UpdateZoraMechanics(PlayState* play, Player* player, const TransformMaskFormDefinition* form) {
    EnsureMmGoronSfxPatched();

    if (play == nullptr || player == nullptr || form == nullptr) {
        return;
    }

    Input* input = &play->state.input[0];
    const bool pressedB = CHECK_BTN_ALL(input->press.button, BTN_B);
    const bool canUseButtons = CanUseGoronActionButtons(player);

    UpdateZoraSwimming(play, player);
    UpdateZoraBarrier(play, player);
    if (sRuntimeState.zoraBarrierActive && (player->stateFlags1 & PLAYER_STATE1_IN_WATER) == 0) {
        return;
    }
    if (sRuntimeState.zoraFastSwimActive || sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_START ||
        sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_EXIT || sRuntimeState.zoraSwimState == ZORA_SWIM_DOLPHIN_JUMP) {
        return;
    }

    if (sRuntimeState.zoraBoomerangState != ZORA_BOOMERANG_IDLE) {
        UpdateZoraBoomerang(play, player);
        if (sRuntimeState.zoraBoomerangState != ZORA_BOOMERANG_FLIGHT) {
            return;
        }
    }

    const bool zoraOnWaterFloor =
        sRuntimeState.zoraBootsActive && (player->currentBoots == PLAYER_BOOTS_IRON) && ((player->actor.bgCheckFlags & 1) != 0);
    if ((player->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0 && !zoraOnWaterFloor) {
        if (sRuntimeState.goronAction == GORON_ACTION_PUNCH) {
            StopGoronAction(play, player);
        }
        return;
    }

    if (!canUseButtons) {
        if (sRuntimeState.goronAction == GORON_ACTION_PUNCH) {
            StopGoronAction(play, player);
        }
        return;
    }

    if (sRuntimeState.goronAction == GORON_ACTION_PUNCH) {
        LockGoronPlayerMotion(player);
        RegisterZoraPunchDamageCollider(play, player);
        if (IsZoraPunchStrikeActive() && !sRuntimeState.goronPunchHitSfxPlayed) {
            PlayGoronActionSfx(player, NA_SE_IT_HAMMER_HIT);
            sRuntimeState.goronPunchHitSfxPlayed = true;
        }
        if (pressedB && sRuntimeState.goronPunchChainCount < 3) {
            sRuntimeState.goronQueuedPunch = true;
        }

        if (IsGoronPunchAnimationFinished()) {
            if (sRuntimeState.goronQueuedPunch && sRuntimeState.goronPunchChainCount < 3) {
                StartGoronPunch(play, player, form, true);
            } else if (CHECK_BTN_ALL(input->cur.button, BTN_B) &&
                       sRuntimeState.zoraCutterWaitAnimHeader.segment != nullptr) {
                sRuntimeState.goronAction = GORON_ACTION_NONE;
                sRuntimeState.goronQueuedPunch = false;
                sRuntimeState.goronPunchChainCount = 0;
                sRuntimeState.goronPunchAnchorValid = false;
                StartZoraBoomerangAim(play, player);
            } else {
                sRuntimeState.goronAction = GORON_ACTION_NONE;
                sRuntimeState.goronQueuedPunch = false;
                sRuntimeState.goronPunchChainCount = 0;
                sRuntimeState.goronPunchAnchorValid = false;
            }
        }
        return;
    }

    if (pressedB) {
        StartGoronPunch(play, player, form, false);
    }
}

bool EnsureRuntimeInitialized(PlayState* play, Player* player, u8 playerMask) {
    if (!OoTxMm_IsTransformationMaskPlayerMask(playerMask)) {
        ResetRuntimeState();
        return false;
    }

    const bool sameMaskSceneReload =
        sRuntimeState.initialized && sRuntimeState.activeMask == playerMask &&
        (sRuntimeState.ownerPlay != play || sRuntimeState.ownerPlayer != player || sRuntimeState.ownerSceneNum != play->sceneNum);

    if (sRuntimeState.initialized &&
        (sRuntimeState.ownerPlay != play || sRuntimeState.ownerPlayer != player || sRuntimeState.ownerSceneNum != play->sceneNum)) {
        ResetRuntimeState();
    }

    if (sRuntimeState.activeMask == playerMask && sRuntimeState.initialized && sRuntimeState.assetsReady) {
        return true;
    }

    ResetRuntimeState();

    if (!SohCrossover::EnsureMmO2rArchiveLoaded()) {
        osSyncPrintf("[OoTxMM Transform] mm.o2r unavailable for mask %d\n", playerMask);
        SPDLOG_INFO("[OoTxMM Transform] mm.o2r unavailable for mask {}", playerMask);
        return false;
    }

    EnsureSpecialItemUiResourcesLoaded();

    const TransformMaskFormDefinition* form = GetFormDefinition(playerMask);
    if (form == nullptr) {
        osSyncPrintf("[OoTxMM Transform] No form definition for mask %d\n", playerMask);
        SPDLOG_INFO("[OoTxMM Transform] No form definition for mask {}", playerMask);
        return false;
    }

    FlexSkeletonHeader* skeleton = TryLoadMmSkeleton(form->skeletonPath, form->skeletonPathOtr);
    sRuntimeState.idleAnimResource = TryLoadMmPlayerAnimData(form->idleAnimPath, form->idleAnimPathOtr);
    sRuntimeState.walkAnimResource = TryLoadMmPlayerAnimData(form->walkAnimPath, form->walkAnimPathOtr);
    sRuntimeState.runAnimResource = TryLoadMmPlayerAnimData(form->runAnimPath, form->runAnimPathOtr);
    sRuntimeState.attackAnimResource = TryLoadMmPlayerAnimData(form->attackAnimPath, form->attackAnimPathOtr);
    sRuntimeState.attackAnimResource2 = TryLoadMmPlayerAnimData(form->attackAnimPath2, form->attackAnimPath2Otr);
    sRuntimeState.attackAnimResource3 = TryLoadMmPlayerAnimData(form->attackAnimPath3, form->attackAnimPath3Otr);
    sRuntimeState.spinAnimResource = TryLoadMmPlayerAnimData(form->spinAnimPath, form->spinAnimPathOtr);
    sRuntimeState.jumpAnimResource = TryLoadMmPlayerAnimData(form->jumpAnimPath, form->jumpAnimPathOtr);
    sRuntimeState.fallAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_link_normal_fall_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_link_normal_fall_Data");
    sRuntimeState.hopAnimResource = TryLoadMmPlayerAnimData(form->hopAnimPath, form->hopAnimPathOtr);
    sRuntimeState.climbStartAnimResource = TryLoadMmPlayerAnimData(form->climbStartAnimPath, form->climbStartAnimPathOtr);
    sRuntimeState.climbLoopAnimResource = TryLoadMmPlayerAnimData(form->climbLoopAnimPath, form->climbLoopAnimPathOtr);
    sRuntimeState.defendAnimResource = TryLoadMmPlayerAnimData(form->defendAnimPath, form->defendAnimPathOtr);
    sRuntimeState.defendWaitAnimResource = TryLoadMmPlayerAnimData(form->defendWaitAnimPath, form->defendWaitAnimPathOtr);
    sRuntimeState.defendEndAnimResource = TryLoadMmPlayerAnimData(form->defendEndAnimPath, form->defendEndAnimPathOtr);
    sRuntimeState.goronPunchEndAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pg_punchAend_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_pg_punchAend_Data");
    sRuntimeState.goronPunchEndAnimResource2 =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pg_punchBend_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_pg_punchBend_Data");
    sRuntimeState.goronPunchEndAnimResource3 =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pg_punchCend_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_pg_punchCend_Data");
    sRuntimeState.goronPunchEndRAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pg_punchAendR_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_pg_punchAendR_Data");
    sRuntimeState.goronPunchEndRAnimResource2 =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pg_punchBendR_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_pg_punchBendR_Data");
    sRuntimeState.goronPunchEndRAnimResource3 =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pg_punchCendR_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_pg_punchCendR_Data");
    sRuntimeState.goronSidehopLeftAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_link_fighter_Lside_jump_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_link_fighter_Lside_jump_Data");
    sRuntimeState.goronSidehopRightAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_link_fighter_Rside_jump_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_link_fighter_Rside_jump_Data");
    sRuntimeState.goronBackflipAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_link_fighter_backturn_jump_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_link_fighter_backturn_jump_Data");
    sRuntimeState.goronSidehopLeftEndAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_link_fighter_Lside_jump_end_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_link_fighter_Lside_jump_end_Data");
    sRuntimeState.goronSidehopRightEndAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_link_fighter_Rside_jump_end_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_link_fighter_Rside_jump_end_Data");
    sRuntimeState.goronBackflipEndAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_link_fighter_backturn_jump_end_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_link_fighter_backturn_jump_end_Data");
    sRuntimeState.goronClimbStartBAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pg_climb_startB_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_pg_climb_startB_Data");
    sRuntimeState.goronClimbUpRAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pg_climb_upR_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_pg_climb_upR_Data");
    sRuntimeState.goronClimbEndALAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pg_climb_endAL_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_pg_climb_endAL_Data");
    sRuntimeState.goronClimbEndARAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pg_climb_endAR_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_pg_climb_endAR_Data");
    sRuntimeState.goronClimbEndBLAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pg_climb_endBL_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_pg_climb_endBL_Data");
    sRuntimeState.goronClimbEndBRAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pg_climb_endBR_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_pg_climb_endBR_Data");
    if (playerMask == PLAYER_MASK_DEKU) {
        sRuntimeState.dekuClimbUpLAnimResource =
            TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_clink_normal_climb_upL_Data",
                                    "__OTR__misc/link_animetion/gPlayerAnim_clink_normal_climb_upL_Data");
        sRuntimeState.dekuClimbUpRAnimResource =
            TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_clink_normal_climb_upR_Data",
                                    "__OTR__misc/link_animetion/gPlayerAnim_clink_normal_climb_upR_Data");
    }
    sRuntimeState.goronDoorBAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pg_doorB_open_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_pg_doorB_open_Data");
    sRuntimeState.goronMaskOffStartAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pg_maskoffstart_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_pg_maskoffstart_Data");
    sRuntimeState.ztargetIdleRightAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_link_normal_waitR_free_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_link_normal_waitR_free_Data");
    sRuntimeState.ztargetIdleLeftAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_link_normal_waitL_free_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_link_normal_waitL_free_Data");
    sRuntimeState.ztargetSideWalkLeftAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_link_normal_side_walkL_free_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_link_normal_side_walkL_free_Data");
    sRuntimeState.ztargetSideWalkRightAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_link_normal_side_walkR_free_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_link_normal_side_walkR_free_Data");
    sRuntimeState.ztargetBackWalkAnimResource =
        TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_link_normal_back_walk_Data",
                                "__OTR__misc/link_animetion/gPlayerAnim_link_normal_back_walk_Data");
    sRuntimeState.ocarinaStartAnimResource = TryLoadMmPlayerAnimData(form->ocarinaStartAnimPath, form->ocarinaStartAnimPathOtr);
    sRuntimeState.ocarinaPlayAnimResource = TryLoadMmPlayerAnimData(form->ocarinaPlayAnimPath, form->ocarinaPlayAnimPathOtr);
    sRuntimeState.doorAnimResource = TryLoadMmPlayerAnimData(form->doorAnimPath, form->doorAnimPathOtr);
    sRuntimeState.chestAnimResource = TryLoadMmPlayerAnimData(form->chestAnimPath, form->chestAnimPathOtr);
    sRuntimeState.swimAnimResource = TryLoadMmPlayerAnimData(form->swimAnimPath, form->swimAnimPathOtr);
    sRuntimeState.swimIdleAnimResource = TryLoadMmPlayerAnimData(form->swimIdleAnimPath, form->swimIdleAnimPathOtr);
    if (playerMask == PLAYER_MASK_ZORA) {
        sRuntimeState.zoraWaterRollAnimResource =
            TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pz_waterroll_Data",
                                    "__OTR__misc/link_animetion/gPlayerAnim_pz_waterroll_Data");
        sRuntimeState.zoraFishSwimAnimResource =
            TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pz_fishswim_Data",
                                    "__OTR__misc/link_animetion/gPlayerAnim_pz_fishswim_Data");
        sRuntimeState.zoraSwimToWaitAnimResource =
            TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pz_swimtowait_Data",
                                    "__OTR__misc/link_animetion/gPlayerAnim_pz_swimtowait_Data");
        sRuntimeState.zoraSwimDownAnimResource =
            TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_link_swimer_swim_down_Data",
                                    "__OTR__misc/link_animetion/gPlayerAnim_link_swimer_swim_down_Data");
        sRuntimeState.zoraSwimDeepEndAnimResource =
            TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_link_swimer_swim_deep_end_Data",
                                    "__OTR__misc/link_animetion/gPlayerAnim_link_swimer_swim_deep_end_Data");
        sRuntimeState.zoraWaitToSwimWaitAnimResource =
            TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_link_swimer_wait2swim_wait_Data",
                                    "__OTR__misc/link_animetion/gPlayerAnim_link_swimer_wait2swim_wait_Data");
        sRuntimeState.zoraLandToSwimWaitAnimResource =
            TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_link_swimer_land2swim_wait_Data",
                                    "__OTR__misc/link_animetion/gPlayerAnim_link_swimer_land2swim_wait_Data");
        sRuntimeState.zoraCutterAttackAnimResource =
            TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pz_cutterattack_Data",
                                    "__OTR__misc/link_animetion/gPlayerAnim_pz_cutterattack_Data");
        sRuntimeState.zoraCutterCatchAnimResource =
            TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pz_cuttercatch_Data",
                                    "__OTR__misc/link_animetion/gPlayerAnim_pz_cuttercatch_Data");
        sRuntimeState.zoraCutterWaitAnimResource =
            TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pz_cutterwaitanim_Data",
                                    "__OTR__misc/link_animetion/gPlayerAnim_pz_cutterwaitanim_Data");
        sRuntimeState.zoraJumpKickEndAnimResource =
            TryLoadMmPlayerAnimData("misc/link_animetion/gPlayerAnim_pz_jumpATend_Data",
                                    "__OTR__misc/link_animetion/gPlayerAnim_pz_jumpATend_Data");
    }
    sRuntimeState.eyeTexture = TryLoadMmRawPointer(form->eyeTexturePath, form->eyeTexturePathOtr);
    sRuntimeState.mouthTexture = TryLoadMmRawPointer(form->mouthTexturePath, form->mouthTexturePathOtr);
    auto addOcarinaDisplayList = [&](const char* path, const char* otrPath) {
        if (sRuntimeState.ocarinaDLCount >= sRuntimeState.ocarinaDLs.size()) {
            return;
        }

        if (void* dlist = TryLoadMmRawPointer(path, otrPath)) {
            sRuntimeState.ocarinaDLs[sRuntimeState.ocarinaDLCount++] = dlist;
        }
    };

    switch (playerMask) {
        case PLAYER_MASK_DEKU:
            sRuntimeState.ocarinaLimbIndex = LINK_DEKU_LIMB_HEAD;
            sRuntimeState.dekuBubbleSetupDLResource =
                TryLoadMmDisplayListResource("objects/gameplay_keep/gameplay_keep_DL_06F380",
                                             "__OTR__objects/gameplay_keep/gameplay_keep_DL_06F380");
            sRuntimeState.dekuBubbleStillDLResource =
                TryLoadMmDisplayListResource("objects/gameplay_keep/gameplay_keep_DL_06F9F0",
                                             "__OTR__objects/gameplay_keep/gameplay_keep_DL_06F9F0");
            sRuntimeState.dekuBubbleMoveDLResource =
                TryLoadMmDisplayListResource("objects/gameplay_keep/gameplay_keep_DL_06FAE0",
                                             "__OTR__objects/gameplay_keep/gameplay_keep_DL_06FAE0");
            sRuntimeState.dekuBubbleSetupDL =
                sRuntimeState.dekuBubbleSetupDLResource != nullptr &&
                        !sRuntimeState.dekuBubbleSetupDLResource->Instructions.empty()
                    ? reinterpret_cast<Gfx*>(sRuntimeState.dekuBubbleSetupDLResource->Instructions.data())
                    : TryLoadMmRawPointer("objects/gameplay_keep/gameplay_keep_DL_06F380",
                                          "__OTR__objects/gameplay_keep/gameplay_keep_DL_06F380");
            sRuntimeState.dekuBubbleStillDL =
                sRuntimeState.dekuBubbleStillDLResource != nullptr &&
                        !sRuntimeState.dekuBubbleStillDLResource->Instructions.empty()
                    ? reinterpret_cast<Gfx*>(sRuntimeState.dekuBubbleStillDLResource->Instructions.data())
                    : TryLoadMmRawPointer("objects/gameplay_keep/gameplay_keep_DL_06F9F0",
                                          "__OTR__objects/gameplay_keep/gameplay_keep_DL_06F9F0");
            sRuntimeState.dekuBubbleMoveDL =
                sRuntimeState.dekuBubbleMoveDLResource != nullptr &&
                        !sRuntimeState.dekuBubbleMoveDLResource->Instructions.empty()
                    ? reinterpret_cast<Gfx*>(sRuntimeState.dekuBubbleMoveDLResource->Instructions.data())
                    : TryLoadMmRawPointer("objects/gameplay_keep/gameplay_keep_DL_06FAE0",
                                          "__OTR__objects/gameplay_keep/gameplay_keep_DL_06FAE0");
            sRuntimeState.dekuSpinTrailDLResource =
                TryLoadMmDisplayListResource("objects/gameplay_keep/gSpinAttack1DL",
                                             "__OTR__objects/gameplay_keep/gSpinAttack1DL");
            sRuntimeState.dekuSpinTrailDL =
                sRuntimeState.dekuSpinTrailDLResource != nullptr &&
                        !sRuntimeState.dekuSpinTrailDLResource->Instructions.empty()
                    ? reinterpret_cast<Gfx*>(sRuntimeState.dekuSpinTrailDLResource->Instructions.data())
                    : TryLoadNativeRawPointer("objects/gameplay_keep/gSpinAttack1DL",
                                              "__OTR__objects/gameplay_keep/gSpinAttack1DL");
            sRuntimeState.dekuShieldDLResource =
                TryLoadMmDisplayListResource("objects/object_link_nuts/object_link_nuts_DL_00A348",
                                             "__OTR__objects/object_link_nuts/object_link_nuts_DL_00A348");
            sRuntimeState.dekuShieldDL =
                sRuntimeState.dekuShieldDLResource != nullptr &&
                        !sRuntimeState.dekuShieldDLResource->Instructions.empty()
                    ? reinterpret_cast<Gfx*>(sRuntimeState.dekuShieldDLResource->Instructions.data())
                    : TryLoadMmRawPointer("objects/object_link_nuts/object_link_nuts_DL_00A348",
                                          "__OTR__objects/object_link_nuts/object_link_nuts_DL_00A348");
            addOcarinaDisplayList("objects/object_link_nuts/object_link_nuts_DL_007390",
                                  "__OTR__objects/object_link_nuts/object_link_nuts_DL_007390");
            addOcarinaDisplayList("objects/object_link_nuts/object_link_nuts_DL_007548",
                                  "__OTR__objects/object_link_nuts/object_link_nuts_DL_007548");
            addOcarinaDisplayList("objects/object_link_nuts/object_link_nuts_DL_0076A0",
                                  "__OTR__objects/object_link_nuts/object_link_nuts_DL_0076A0");
            addOcarinaDisplayList("objects/object_link_nuts/object_link_nuts_DL_0077D0",
                                  "__OTR__objects/object_link_nuts/object_link_nuts_DL_0077D0");
            addOcarinaDisplayList("objects/object_link_nuts/object_link_nuts_DL_007900",
                                  "__OTR__objects/object_link_nuts/object_link_nuts_DL_007900");
            addOcarinaDisplayList("objects/object_link_nuts/object_link_nuts_DL_007A28",
                                  "__OTR__objects/object_link_nuts/object_link_nuts_DL_007A28");
            break;
        case PLAYER_MASK_GORON:
            sRuntimeState.ocarinaLimbIndex = LINK_GORON_LIMB_TORSO;
            if (FlexSkeletonHeader* shieldSkel =
                    TryLoadMmSkeleton("objects/object_link_goron/gLinkGoronShieldingSkel",
                                      "__OTR__objects/object_link_goron/gLinkGoronShieldingSkel")) {
                if (AnimationHeader* shieldAnim = reinterpret_cast<AnimationHeader*>(
                        TryLoadMmRawPointer("objects/object_link_goron/gLinkGoronShieldingAnim",
                                            "__OTR__objects/object_link_goron/gLinkGoronShieldingAnim"))) {
                    SkelAnime_InitFlex(play, &sRuntimeState.goronShieldSkelAnime, shieldSkel, shieldAnim, nullptr,
                                       nullptr, 0);
                    sRuntimeState.goronShieldSkelReady = true;
                }
            }
            sRuntimeState.goronRolledUpDLResource =
                TryLoadMmDisplayListResource("objects/object_link_goron/gLinkGoronCurledDL",
                                             "__OTR__objects/object_link_goron/gLinkGoronCurledDL");
            LogDisplayListKeyCommands(sRuntimeState.goronRolledUpDLResource, "CurledDL");
            PatchDisplayListSegmentRefsToNoOp(sRuntimeState.goronRolledUpDLResource, 0x0C, "CurledDL");
            if (sRuntimeState.goronRolledUpDLResource != nullptr && !sRuntimeState.goronRolledUpDLResource->Instructions.empty()) {
                sRuntimeState.goronRolledUpDL =
                    reinterpret_cast<Gfx*>(sRuntimeState.goronRolledUpDLResource->Instructions.data());
            } else {
                sRuntimeState.goronRolledUpDL = TryLoadNativeRawPointer("objects/object_oF1d_map/gGoronDL_00C140",
                                                                        "__OTR__objects/object_oF1d_map/gGoronDL_00C140");
            }
            sRuntimeState.goronPoweredRollDLResource =
                TryLoadMmDisplayListResource("objects/object_link_goron/object_link_goron_DL_00C540",
                                             "__OTR__objects/object_link_goron/object_link_goron_DL_00C540");
            LogDisplayListKeyCommands(sRuntimeState.goronPoweredRollDLResource, "SpikeDL");
            PatchDisplayListSegmentRefsToNoOp(sRuntimeState.goronPoweredRollDLResource, 0x0C, "SpikeDL");
            sRuntimeState.goronHeatEffectDLResource1 =
                TryLoadMmDisplayListResource("objects/object_link_goron/object_link_goron_DL_0127B0",
                                             "__OTR__objects/object_link_goron/object_link_goron_DL_0127B0");
            LogDisplayListKeyCommands(sRuntimeState.goronHeatEffectDLResource1, "EnergyEffect1DL");
            PatchDisplayListSegmentRefsToNoOp(sRuntimeState.goronHeatEffectDLResource1, 0x08, "EnergyEffect1DL");
            sRuntimeState.goronHeatEffectDLResource2 =
                TryLoadMmDisplayListResource("objects/object_link_goron/object_link_goron_DL_0134D0",
                                             "__OTR__objects/object_link_goron/object_link_goron_DL_0134D0");
            LogDisplayListKeyCommands(sRuntimeState.goronHeatEffectDLResource2, "EnergyEffect2DL");
            PatchDisplayListSegmentRefsToNoOp(sRuntimeState.goronHeatEffectDLResource2, 0x08, "EnergyEffect2DL");
            sRuntimeState.goronPunchEffectDLResource =
                TryLoadMmDisplayListResource("objects/object_link_goron/gLinkGoronGoronPunchEffectDL",
                                             "__OTR__objects/object_link_goron/gLinkGoronGoronPunchEffectDL");
            LogDisplayListKeyCommands(sRuntimeState.goronPunchEffectDLResource, "GoronPunchEffectDL");
            PatchDisplayListSegmentRefsToNoOp(sRuntimeState.goronPunchEffectDLResource, 0x08, "GoronPunchEffectDL");
            sRuntimeState.goronSpikeShellDL =
                TryLoadNativeRawPointer("objects/object_trap/gUnusedSpikeDL",
                                        "__OTR__objects/object_trap/gUnusedSpikeDL");
            sRuntimeState.goronSpikeRockBodyDL = nullptr;
            sRuntimeState.goronSpikeMetalBodyDL = nullptr;
            sRuntimeState.goronSpikeEffectDL1 = sRuntimeState.goronHeatEffectDLResource1 != nullptr &&
                                                        !sRuntimeState.goronHeatEffectDLResource1->Instructions.empty()
                                                    ? reinterpret_cast<Gfx*>(sRuntimeState.goronHeatEffectDLResource1->Instructions.data())
                                                    : nullptr;
            sRuntimeState.goronSpikeEffectDL2 = sRuntimeState.goronHeatEffectDLResource2 != nullptr &&
                                                        !sRuntimeState.goronHeatEffectDLResource2->Instructions.empty()
                                                    ? reinterpret_cast<Gfx*>(sRuntimeState.goronHeatEffectDLResource2->Instructions.data())
                                                    : nullptr;
            sRuntimeState.goronPunchEffectDL = sRuntimeState.goronPunchEffectDLResource != nullptr &&
                                                       !sRuntimeState.goronPunchEffectDLResource->Instructions.empty()
                                                   ? reinterpret_cast<Gfx*>(sRuntimeState.goronPunchEffectDLResource->Instructions.data())
                                                   : nullptr;
            sRuntimeState.goronSpikeMatAnim1 = nullptr;
            sRuntimeState.goronSpikeMatAnim2 = nullptr;
            addOcarinaDisplayList("objects/object_link_goron/object_link_goron_DL_00FC18",
                                  "__OTR__objects/object_link_goron/object_link_goron_DL_00FC18");
            addOcarinaDisplayList("objects/object_link_goron/object_link_goron_DL_00FCF0",
                                  "__OTR__objects/object_link_goron/object_link_goron_DL_00FCF0");
            addOcarinaDisplayList("objects/object_link_goron/object_link_goron_DL_00FF18",
                                  "__OTR__objects/object_link_goron/object_link_goron_DL_00FF18");
            addOcarinaDisplayList("objects/object_link_goron/object_link_goron_DL_010140",
                                  "__OTR__objects/object_link_goron/object_link_goron_DL_010140");
            addOcarinaDisplayList("objects/object_link_goron/object_link_goron_DL_010368",
                                  "__OTR__objects/object_link_goron/object_link_goron_DL_010368");
            addOcarinaDisplayList("objects/object_link_goron/object_link_goron_DL_010590",
                                  "__OTR__objects/object_link_goron/object_link_goron_DL_010590");
            break;
        case PLAYER_MASK_ZORA:
            sRuntimeState.ocarinaLimbIndex = LINK_ZORA_LIMB_LEFT_HAND;
            sRuntimeState.zoraLeftFinDLResource =
                TryLoadMmDisplayListResource("objects/object_link_zora/object_link_zora_DL_00CC38",
                                             "__OTR__objects/object_link_zora/object_link_zora_DL_00CC38");
            PatchDisplayListSegmentRefsToNoOp(sRuntimeState.zoraLeftFinDLResource, 0x08, "ZoraLeftFinDL");
            sRuntimeState.zoraRightFinDLResource =
                TryLoadMmDisplayListResource("objects/object_link_zora/object_link_zora_DL_00CDA0",
                                             "__OTR__objects/object_link_zora/object_link_zora_DL_00CDA0");
            PatchDisplayListSegmentRefsToNoOp(sRuntimeState.zoraRightFinDLResource, 0x08, "ZoraRightFinDL");
            sRuntimeState.zoraLeftFinDL = sRuntimeState.zoraLeftFinDLResource != nullptr &&
                                                  !sRuntimeState.zoraLeftFinDLResource->Instructions.empty()
                                              ? reinterpret_cast<Gfx*>(sRuntimeState.zoraLeftFinDLResource->Instructions.data())
                                              : TryLoadMmRawPointer("objects/object_link_zora/object_link_zora_DL_00CC38",
                                                                    "__OTR__objects/object_link_zora/object_link_zora_DL_00CC38");
            sRuntimeState.zoraRightFinDL = sRuntimeState.zoraRightFinDLResource != nullptr &&
                                                   !sRuntimeState.zoraRightFinDLResource->Instructions.empty()
                                               ? reinterpret_cast<Gfx*>(sRuntimeState.zoraRightFinDLResource->Instructions.data())
                                               : TryLoadMmRawPointer("objects/object_link_zora/object_link_zora_DL_00CDA0",
                                                                     "__OTR__objects/object_link_zora/object_link_zora_DL_00CDA0");
            sRuntimeState.zoraBarrierDLResource =
                TryLoadMmDisplayListResource("objects/object_link_zora/object_link_zora_DL_011760",
                                             "__OTR__objects/object_link_zora/object_link_zora_DL_011760");
            sRuntimeState.zoraBarrierDL =
                sRuntimeState.zoraBarrierDLResource != nullptr && !sRuntimeState.zoraBarrierDLResource->Instructions.empty()
                    ? reinterpret_cast<Gfx*>(sRuntimeState.zoraBarrierDLResource->Instructions.data())
                    : TryLoadMmRawPointer("objects/object_link_zora/object_link_zora_DL_011760",
                                          "__OTR__objects/object_link_zora/object_link_zora_DL_011760");
            addOcarinaDisplayList("objects/object_link_zora/object_link_zora_DL_00CC38",
                                  "__OTR__objects/object_link_zora/object_link_zora_DL_00CC38");
            addOcarinaDisplayList("objects/object_link_zora/object_link_zora_DL_00CDA0",
                                  "__OTR__objects/object_link_zora/object_link_zora_DL_00CDA0");
            addOcarinaDisplayList("objects/object_link_zora/object_link_zora_DL_00E088",
                                  "__OTR__objects/object_link_zora/object_link_zora_DL_00E088");
            addOcarinaDisplayList("objects/object_link_zora/object_link_zora_DL_00E2A0",
                                  "__OTR__objects/object_link_zora/object_link_zora_DL_00E2A0");
            break;
        case PLAYER_MASK_FIERCE_DEITY:
            sRuntimeState.fdLeftHandSwordDLResource =
                TryLoadMmDisplayListResource("objects/object_link_boy/gLinkFierceDeityLeftHandHoldingSwordDL",
                                             "__OTR__objects/object_link_boy/gLinkFierceDeityLeftHandHoldingSwordDL");
            sRuntimeState.fdLeftHandEmptyDLResource =
                TryLoadMmDisplayListResource("objects/object_link_boy/gLinkFierceDeityLeftHandEmptyDL",
                                             "__OTR__objects/object_link_boy/gLinkFierceDeityLeftHandEmptyDL");
            sRuntimeState.fdLeftHandBottleDLResource =
                TryLoadMmDisplayListResource("objects/object_link_boy/gLinkFierceDeityLeftHandHoldingBottleDL",
                                             "__OTR__objects/object_link_boy/gLinkFierceDeityLeftHandHoldingBottleDL");
            sRuntimeState.fdRightHandEmptyDLResource =
                TryLoadMmDisplayListResource("objects/object_link_boy/gLinkFierceDeityRightHandEmptyDL",
                                             "__OTR__objects/object_link_boy/gLinkFierceDeityRightHandEmptyDL");
            sRuntimeState.fdSwordBeamDLResource =
                TryLoadMmDisplayListResource("objects/gameplay_keep/gSwordBeamDL",
                                             "__OTR__objects/gameplay_keep/gSwordBeamDL");
            sRuntimeState.fdLeftHandSwordDL =
                sRuntimeState.fdLeftHandSwordDLResource != nullptr &&
                        !sRuntimeState.fdLeftHandSwordDLResource->Instructions.empty()
                    ? reinterpret_cast<Gfx*>(sRuntimeState.fdLeftHandSwordDLResource->Instructions.data())
                    : TryLoadMmRawPointer("objects/object_link_boy/gLinkFierceDeityLeftHandHoldingSwordDL",
                                          "__OTR__objects/object_link_boy/gLinkFierceDeityLeftHandHoldingSwordDL");
            sRuntimeState.fdLeftHandEmptyDL =
                sRuntimeState.fdLeftHandEmptyDLResource != nullptr &&
                        !sRuntimeState.fdLeftHandEmptyDLResource->Instructions.empty()
                    ? reinterpret_cast<Gfx*>(sRuntimeState.fdLeftHandEmptyDLResource->Instructions.data())
                    : TryLoadMmRawPointer("objects/object_link_boy/gLinkFierceDeityLeftHandEmptyDL",
                                          "__OTR__objects/object_link_boy/gLinkFierceDeityLeftHandEmptyDL");
            sRuntimeState.fdLeftHandBottleDL =
                sRuntimeState.fdLeftHandBottleDLResource != nullptr &&
                        !sRuntimeState.fdLeftHandBottleDLResource->Instructions.empty()
                    ? reinterpret_cast<Gfx*>(sRuntimeState.fdLeftHandBottleDLResource->Instructions.data())
                    : TryLoadMmRawPointer("objects/object_link_boy/gLinkFierceDeityLeftHandHoldingBottleDL",
                                          "__OTR__objects/object_link_boy/gLinkFierceDeityLeftHandHoldingBottleDL");
            sRuntimeState.fdRightHandEmptyDL =
                sRuntimeState.fdRightHandEmptyDLResource != nullptr &&
                        !sRuntimeState.fdRightHandEmptyDLResource->Instructions.empty()
                    ? reinterpret_cast<Gfx*>(sRuntimeState.fdRightHandEmptyDLResource->Instructions.data())
                    : TryLoadMmRawPointer("objects/object_link_boy/gLinkFierceDeityRightHandEmptyDL",
                                          "__OTR__objects/object_link_boy/gLinkFierceDeityRightHandEmptyDL");
            sRuntimeState.fdSwordBeamDL =
                sRuntimeState.fdSwordBeamDLResource != nullptr && !sRuntimeState.fdSwordBeamDLResource->Instructions.empty()
                    ? reinterpret_cast<Gfx*>(sRuntimeState.fdSwordBeamDLResource->Instructions.data())
                    : TryLoadMmRawPointer("objects/gameplay_keep/gSwordBeamDL",
                                          "__OTR__objects/gameplay_keep/gSwordBeamDL");
            break;
        default:
            break;
    }

    const bool hasIdle = InitMmPlayerAnimHeader(sRuntimeState.idleAnimResource, form->idleFrameCount, &sRuntimeState.idleAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.walkAnimResource, form->walkFrameCount, &sRuntimeState.walkAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.runAnimResource, form->runFrameCount, &sRuntimeState.runAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.attackAnimResource, form->attackFrameCount, &sRuntimeState.attackAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.attackAnimResource2, form->attackFrameCount2, &sRuntimeState.attackAnimHeader2);
    InitMmPlayerAnimHeader(sRuntimeState.attackAnimResource3, form->attackFrameCount3, &sRuntimeState.attackAnimHeader3);
    InitMmPlayerAnimHeader(sRuntimeState.spinAnimResource, form->spinFrameCount, &sRuntimeState.spinAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.jumpAnimResource, form->jumpFrameCount, &sRuntimeState.jumpAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.fallAnimResource, 4, &sRuntimeState.fallAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.hopAnimResource, form->hopFrameCount, &sRuntimeState.hopAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.climbStartAnimResource, form->climbStartFrameCount, &sRuntimeState.climbStartAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.climbLoopAnimResource, form->climbLoopFrameCount, &sRuntimeState.climbLoopAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.defendAnimResource, form->defendFrameCount, &sRuntimeState.defendAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.defendWaitAnimResource, form->defendWaitFrameCount, &sRuntimeState.defendWaitAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.defendEndAnimResource, form->defendEndFrameCount, &sRuntimeState.defendEndAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.goronPunchEndAnimResource, 13, &sRuntimeState.goronPunchEndAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.goronPunchEndAnimResource2, 10, &sRuntimeState.goronPunchEndAnimHeader2);
    InitMmPlayerAnimHeader(sRuntimeState.goronPunchEndAnimResource3, 10, &sRuntimeState.goronPunchEndAnimHeader3);
    InitMmPlayerAnimHeader(sRuntimeState.goronPunchEndRAnimResource, 13, &sRuntimeState.goronPunchEndRAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.goronPunchEndRAnimResource2, 10, &sRuntimeState.goronPunchEndRAnimHeader2);
    InitMmPlayerAnimHeader(sRuntimeState.goronPunchEndRAnimResource3, 10, &sRuntimeState.goronPunchEndRAnimHeader3);
    InitMmPlayerAnimHeader(sRuntimeState.goronSidehopLeftAnimResource, 15, &sRuntimeState.goronSidehopLeftAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.goronSidehopRightAnimResource, 15, &sRuntimeState.goronSidehopRightAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.goronBackflipAnimResource, 15, &sRuntimeState.goronBackflipAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.goronSidehopLeftEndAnimResource, 10, &sRuntimeState.goronSidehopLeftEndAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.goronSidehopRightEndAnimResource, 10, &sRuntimeState.goronSidehopRightEndAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.goronBackflipEndAnimResource, 10, &sRuntimeState.goronBackflipEndAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.goronClimbStartBAnimResource, 20, &sRuntimeState.goronClimbStartBAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.goronClimbUpRAnimResource, 10, &sRuntimeState.goronClimbUpRAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.goronClimbEndALAnimResource, 20, &sRuntimeState.goronClimbEndALAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.goronClimbEndARAnimResource, 20, &sRuntimeState.goronClimbEndARAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.goronClimbEndBLAnimResource, 20, &sRuntimeState.goronClimbEndBLAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.goronClimbEndBRAnimResource, 20, &sRuntimeState.goronClimbEndBRAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.dekuClimbUpLAnimResource, 10, &sRuntimeState.dekuClimbUpLAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.dekuClimbUpRAnimResource, 10, &sRuntimeState.dekuClimbUpRAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.ztargetIdleRightAnimResource, 4, &sRuntimeState.ztargetIdleRightAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.ztargetIdleLeftAnimResource, 4, &sRuntimeState.ztargetIdleLeftAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.ztargetSideWalkLeftAnimResource, 12, &sRuntimeState.ztargetSideWalkLeftAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.ztargetSideWalkRightAnimResource, 12, &sRuntimeState.ztargetSideWalkRightAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.ztargetBackWalkAnimResource, 12, &sRuntimeState.ztargetBackWalkAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.ocarinaStartAnimResource, form->ocarinaStartFrameCount, &sRuntimeState.ocarinaStartAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.ocarinaPlayAnimResource, form->ocarinaPlayFrameCount, &sRuntimeState.ocarinaPlayAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.doorAnimResource, form->doorFrameCount, &sRuntimeState.doorAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.goronDoorBAnimResource, 30, &sRuntimeState.goronDoorBAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.goronMaskOffStartAnimResource, 15, &sRuntimeState.goronMaskOffStartAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.chestAnimResource, form->chestFrameCount, &sRuntimeState.chestAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.swimAnimResource, form->swimFrameCount, &sRuntimeState.swimAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.swimIdleAnimResource, form->swimIdleFrameCount, &sRuntimeState.swimIdleAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.zoraFishSwimAnimResource, 10, &sRuntimeState.zoraFishSwimAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.zoraWaterRollAnimResource, 18, &sRuntimeState.zoraWaterRollAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.zoraSwimToWaitAnimResource, 10, &sRuntimeState.zoraSwimToWaitAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.zoraSwimDownAnimResource, 20, &sRuntimeState.zoraSwimDownAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.zoraSwimDeepEndAnimResource, 10, &sRuntimeState.zoraSwimDeepEndAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.zoraWaitToSwimWaitAnimResource, 10, &sRuntimeState.zoraWaitToSwimWaitAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.zoraLandToSwimWaitAnimResource, 10, &sRuntimeState.zoraLandToSwimWaitAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.zoraCutterAttackAnimResource, 9, &sRuntimeState.zoraCutterAttackAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.zoraCutterCatchAnimResource, 6, &sRuntimeState.zoraCutterCatchAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.zoraCutterWaitAnimResource, 29, &sRuntimeState.zoraCutterWaitAnimHeader);
    InitMmPlayerAnimHeader(sRuntimeState.zoraJumpKickEndAnimResource, 13, &sRuntimeState.zoraJumpKickEndAnimHeader);

    if (skeleton == nullptr || !hasIdle) {
        osSyncPrintf("[OoTxMM Transform] Missing form assets for mask %d (skel=%s anim=%s)\n", playerMask,
                     skeleton == nullptr ? "no" : "yes", hasIdle ? "yes" : "no");
        SPDLOG_INFO("[OoTxMM Transform] Missing form assets for mask {} (skel={} anim={})", playerMask,
                    skeleton == nullptr ? "no" : "yes", hasIdle ? "yes" : "no");
        return false;
    }

    SkelAnime_InitLink(play, &sRuntimeState.skelAnime, skeleton, &sRuntimeState.idleAnimHeader, 9, nullptr, nullptr, 0);
    sRuntimeState.skelAnime.baseTransl.x = 0;
    sRuntimeState.skelAnime.baseTransl.y = 0;
    sRuntimeState.skelAnime.baseTransl.z = 0;
    if (playerMask == PLAYER_MASK_ZORA) {
        const s32 s16PerFrame = static_cast<s32>(sRuntimeState.skelAnime.limbCount) * 3 + 1;
        InitCopiedMmPlayerAnimHeader(sRuntimeState.swimAnimResource, form->swimFrameCount, s16PerFrame,
                                     &sRuntimeState.swimAnimHeader, sRuntimeState.swimAnimCopy);
        InitCopiedMmPlayerAnimHeader(sRuntimeState.swimIdleAnimResource, form->swimIdleFrameCount, s16PerFrame,
                                     &sRuntimeState.swimIdleAnimHeader, sRuntimeState.swimIdleAnimCopy);
        InitCopiedMmPlayerAnimHeader(sRuntimeState.zoraFishSwimAnimResource, 10, s16PerFrame,
                                     &sRuntimeState.zoraFishSwimAnimHeader, sRuntimeState.zoraFishSwimAnimCopy);
        InitCopiedMmPlayerAnimHeader(sRuntimeState.zoraWaterRollAnimResource, 18, s16PerFrame,
                                     &sRuntimeState.zoraWaterRollAnimHeader, sRuntimeState.zoraWaterRollAnimCopy);
        InitCopiedMmPlayerAnimHeader(sRuntimeState.zoraSwimToWaitAnimResource, 10, s16PerFrame,
                                     &sRuntimeState.zoraSwimToWaitAnimHeader, sRuntimeState.zoraSwimToWaitAnimCopy);
        InitCopiedMmPlayerAnimHeader(sRuntimeState.zoraSwimDownAnimResource, 20, s16PerFrame,
                                     &sRuntimeState.zoraSwimDownAnimHeader, sRuntimeState.zoraSwimDownAnimCopy);
        InitCopiedMmPlayerAnimHeader(sRuntimeState.zoraSwimDeepEndAnimResource, 10, s16PerFrame,
                                     &sRuntimeState.zoraSwimDeepEndAnimHeader, sRuntimeState.zoraSwimDeepEndAnimCopy);
        InitCopiedMmPlayerAnimHeader(sRuntimeState.zoraWaitToSwimWaitAnimResource, 10, s16PerFrame,
                                     &sRuntimeState.zoraWaitToSwimWaitAnimHeader, sRuntimeState.zoraWaitToSwimWaitAnimCopy);
        InitCopiedMmPlayerAnimHeader(sRuntimeState.zoraLandToSwimWaitAnimResource, 10, s16PerFrame,
                                     &sRuntimeState.zoraLandToSwimWaitAnimHeader, sRuntimeState.zoraLandToSwimWaitAnimCopy);
    }
    sRuntimeState.initialized = true;
    sRuntimeState.assetsReady = true;
    sRuntimeState.ownerPlay = play;
    sRuntimeState.ownerPlayer = player;
    sRuntimeState.ownerSceneNum = play->sceneNum;
    sRuntimeState.activeMask = playerMask;
    if (playerMask == PLAYER_MASK_ZORA) {
        sRuntimeState.savedTunic = player->currentTunic;
        player->currentTunic = PLAYER_TUNIC_ZORA;
        player->underwaterTimer = 0;
        sRuntimeState.zoraTunicForced = true;
    }
    if (!sameMaskSceneReload) {
        StartTransformMaskPutOnEffect(player);
    }
    sRuntimeState.motionState = TRANSFORM_MASK_MOTION_UNSET;
    osSyncPrintf("[OoTxMM Transform] Initialized form runtime for mask %d\n", playerMask);
    SPDLOG_INFO("[OoTxMM Transform] Initialized form runtime for mask {}", playerMask);
    return true;
}

bool IsGettingItemPose(const Player* player) {
    return player != nullptr && (player->stateFlags1 & (PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_IN_ITEM_CS)) != 0;
}

bool IsPlayerVanillaRollAnimation(const Player* player) {
    if (player == nullptr) {
        return false;
    }

    return player->actionFunc == Player_Action_Roll;
}

void CopyOotPlayerPoseToTransformSkeleton(Player* player) {
    if (player == nullptr || player->skelAnime.jointTable == nullptr || sRuntimeState.skelAnime.jointTable == nullptr) {
        return;
    }

    const s32 playerLimbCount = static_cast<s32>(player->skelAnime.dListCount) + 2;
    const s32 transformLimbCount = static_cast<s32>(sRuntimeState.skelAnime.dListCount) + 2;
    const s32 jointCount = std::min<s32>(PLAYER_LIMB_BUF_COUNT, std::min(playerLimbCount, transformLimbCount));
    if (jointCount <= 0) {
        return;
    }

    memcpy(sRuntimeState.skelAnime.jointTable, player->skelAnime.jointTable, sizeof(Vec3s) * jointCount);
}

bool IsZoraOnWaterFloor(const Player* player) {
    return player != nullptr && sRuntimeState.zoraBootsActive && player->currentBoots == PLAYER_BOOTS_IRON &&
           ((player->actor.bgCheckFlags & 1) != 0);
}

LinkAnimationHeader* GetZoraWaterAnimation(Player* player) {
    if (player == nullptr) {
        return nullptr;
    }

    if (sRuntimeState.zoraBootsActive && !IsZoraOnWaterFloor(player)) {
        if (sRuntimeState.swimIdleAnimHeader.segment != nullptr) {
            return &sRuntimeState.swimIdleAnimHeader;
        }
        return sRuntimeState.zoraWaitToSwimWaitAnimHeader.segment != nullptr ? &sRuntimeState.zoraWaitToSwimWaitAnimHeader
                                                                             : nullptr;
    }

    if (player->actor.yDistToWater > 30.0f && player->actor.velocity.y > 0.35f) {
        if (sRuntimeState.zoraSwimDeepEndAnimHeader.segment != nullptr) {
            return &sRuntimeState.zoraSwimDeepEndAnimHeader;
        }
    }

    if (player->actor.yDistToWater < 24.0f && fabsf(player->actor.speedXZ) <= 0.2f) {
        if (sRuntimeState.zoraLandToSwimWaitAnimHeader.segment != nullptr) {
            return &sRuntimeState.zoraLandToSwimWaitAnimHeader;
        }
    }

    if (fabsf(player->actor.speedXZ) > 0.2f && sRuntimeState.swimAnimHeader.segment != nullptr) {
        return &sRuntimeState.swimAnimHeader;
    }
    if (sRuntimeState.swimIdleAnimHeader.segment != nullptr) {
        return &sRuntimeState.swimIdleAnimHeader;
    }
    return sRuntimeState.swimAnimHeader.segment != nullptr ? &sRuntimeState.swimAnimHeader : nullptr;
}

void UpdateMotionAnimation(PlayState* play, Player* player) {
    const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask);
    if (form == nullptr) {
        return;
    }

    if (IsUsingOcarina(player) &&
        (sRuntimeState.ocarinaStartAnimHeader.segment != nullptr || sRuntimeState.ocarinaPlayAnimHeader.segment != nullptr)) {
        if (sRuntimeState.skelAnime.animation == &sRuntimeState.ocarinaStartAnimHeader &&
            sRuntimeState.ocarinaPlayAnimHeader.segment != nullptr &&
            Animation_OnFrame(&sRuntimeState.skelAnime, Animation_GetLastFrame(&sRuntimeState.ocarinaStartAnimHeader))) {
            LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.ocarinaPlayAnimHeader, 1.0f, 0.0f,
                                 Animation_GetLastFrame(&sRuntimeState.ocarinaPlayAnimHeader), ANIMMODE_LOOP, 0.0f);
        } else if (sRuntimeState.skelAnime.animation != &sRuntimeState.ocarinaStartAnimHeader &&
                   sRuntimeState.skelAnime.animation != &sRuntimeState.ocarinaPlayAnimHeader) {
            if (sRuntimeState.ocarinaStartAnimHeader.segment != nullptr) {
                LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.ocarinaStartAnimHeader, 1.0f, 0.0f,
                                     Animation_GetLastFrame(&sRuntimeState.ocarinaStartAnimHeader), ANIMMODE_ONCE, 0.0f);
            } else {
                LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.ocarinaPlayAnimHeader, 1.0f, 0.0f,
                                     Animation_GetLastFrame(&sRuntimeState.ocarinaPlayAnimHeader), ANIMMODE_LOOP, 0.0f);
            }
        }

        sRuntimeState.motionState = TRANSFORM_MASK_MOTION_OCARINA;
        return;
    }

    if (player->currentMask == PLAYER_MASK_ZORA && sRuntimeState.zoraBoomerangState != ZORA_BOOMERANG_IDLE &&
        sRuntimeState.zoraBoomerangState != ZORA_BOOMERANG_FLIGHT) {
        LinkAnimationHeader* zoraBoomerangAnim = nullptr;
        s32 animMode = ANIMMODE_LOOP;
        f32 morphFrames = -6.0f;
        if (sRuntimeState.zoraBoomerangState == ZORA_BOOMERANG_THROW &&
            sRuntimeState.zoraCutterAttackAnimHeader.segment != nullptr) {
            zoraBoomerangAnim = &sRuntimeState.zoraCutterAttackAnimHeader;
            animMode = ANIMMODE_ONCE;
            morphFrames = -4.0f;
        } else if (sRuntimeState.zoraBoomerangState == ZORA_BOOMERANG_CATCH &&
                   sRuntimeState.zoraCutterCatchAnimHeader.segment != nullptr) {
            zoraBoomerangAnim = &sRuntimeState.zoraCutterCatchAnimHeader;
            animMode = ANIMMODE_ONCE;
        } else if (sRuntimeState.zoraCutterWaitAnimHeader.segment != nullptr) {
            zoraBoomerangAnim = &sRuntimeState.zoraCutterWaitAnimHeader;
        }

        if (zoraBoomerangAnim != nullptr && sRuntimeState.skelAnime.animation != zoraBoomerangAnim) {
            LinkAnimation_Change(play, &sRuntimeState.skelAnime, zoraBoomerangAnim, 1.0f, 0.0f,
                                 Animation_GetLastFrame(zoraBoomerangAnim), animMode, morphFrames);
        }
        sRuntimeState.motionState = TRANSFORM_MASK_MOTION_ZORA_BOOMERANG;
        return;
    }

    if (player->currentMask == PLAYER_MASK_FIERCE_DEITY && ShouldFierceDeityUseOotActionPose(player)) {
        CopyOotPlayerPoseToTransformSkeleton(player);
        sRuntimeState.motionState = TRANSFORM_MASK_MOTION_OOT_ACTION;
        return;
    }

    if ((player->currentMask == PLAYER_MASK_GORON || player->currentMask == PLAYER_MASK_ZORA) &&
        ((player->stateFlags1 & (PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_TALKING | PLAYER_STATE1_IN_CUTSCENE)) != 0 ||
         player->heldActor != nullptr)) {
        CopyOotPlayerPoseToTransformSkeleton(player);
        sRuntimeState.motionState = TRANSFORM_MASK_MOTION_OOT_ACTION;
        return;
    }

    TransformMaskMotionState nextMotion = TRANSFORM_MASK_MOTION_IDLE;
    const bool dekuAirborneSidehop =
        player->currentMask == PLAYER_MASK_DEKU && (player->actor.bgCheckFlags & 1) == 0 &&
        (sRuntimeState.skelAnime.animation == &sRuntimeState.goronSidehopLeftAnimHeader ||
         sRuntimeState.skelAnime.animation == &sRuntimeState.goronSidehopRightAnimHeader);
    if ((player->stateFlags2 & PLAYER_STATE2_HOPPING) != 0) {
        nextMotion = TRANSFORM_MASK_MOTION_HOP;
    } else if (player->currentMask == PLAYER_MASK_DEKU && sRuntimeState.dekuAction == DEKU_ACTION_SPIN) {
        nextMotion = TRANSFORM_MASK_MOTION_SPIN;
    } else if (player->currentMask == PLAYER_MASK_DEKU && sRuntimeState.dekuWaterHopAnimTimer > 0 &&
               sRuntimeState.jumpAnimHeader.segment != nullptr) {
        nextMotion = TRANSFORM_MASK_MOTION_JUMP;
    } else if (player->currentMask == PLAYER_MASK_DEKU &&
               (sRuntimeState.dekuAction == DEKU_ACTION_BUBBLE_AIM ||
                sRuntimeState.dekuAction == DEKU_ACTION_BUBBLE_FIRE)) {
        nextMotion = TRANSFORM_MASK_MOTION_ATTACK;
    } else if (player->currentMask == PLAYER_MASK_GORON &&
        (sRuntimeState.goronAction == GORON_ACTION_ROLL || sRuntimeState.goronAction == GORON_ACTION_ROLL_START ||
         sRuntimeState.goronAction == GORON_ACTION_SLAM_JUMP || sRuntimeState.goronAction == GORON_ACTION_SLAM_POUND)) {
        nextMotion = TRANSFORM_MASK_MOTION_SPIN;
    } else if (player->currentMask == PLAYER_MASK_ZORA &&
               (sRuntimeState.zoraFastSwimActive || sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_START ||
                sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_EXIT ||
                sRuntimeState.zoraSwimState == ZORA_SWIM_DOLPHIN_JUMP)) {
        nextMotion = TRANSFORM_MASK_MOTION_ZORA_FAST_SWIM;
    } else if (player->currentMask == PLAYER_MASK_ZORA && IsPlayerVanillaRollAnimation(player) &&
               sRuntimeState.spinAnimHeader.segment != nullptr) {
        nextMotion = TRANSFORM_MASK_MOTION_SPIN;
    } else if (player->currentMask == PLAYER_MASK_FIERCE_DEITY && IsPlayerVanillaRollAnimation(player) &&
               sRuntimeState.spinAnimHeader.segment != nullptr) {
        nextMotion = TRANSFORM_MASK_MOTION_SPIN;
    } else if ((player->currentMask == PLAYER_MASK_GORON || player->currentMask == PLAYER_MASK_ZORA) &&
               sRuntimeState.goronAction == GORON_ACTION_PUNCH) {
        nextMotion = TRANSFORM_MASK_MOTION_ATTACK;
    } else if (((player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) != 0) ||
               (((player->stateFlags1 & (PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE)) != 0) &&
                !OoTxMm_ShouldBlockLedgeClimb(player))) {
        nextMotion = TRANSFORM_MASK_MOTION_CLIMB;
    } else if (IsGettingItemPose(player)) {
        nextMotion = TRANSFORM_MASK_MOTION_GET_ITEM;
    } else if (player->doorType != PLAYER_DOORTYPE_NONE && player->doorTimer > 0 && sRuntimeState.doorAnimHeader.segment != nullptr) {
        nextMotion = TRANSFORM_MASK_MOTION_DOOR;
    } else if (player->currentMask == PLAYER_MASK_ZORA && (player->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0 &&
               !IsZoraOnWaterFloor(player) && GetZoraWaterAnimation(player) != nullptr) {
        nextMotion = TRANSFORM_MASK_MOTION_SWIM;
    } else if (player->currentMask == PLAYER_MASK_FIERCE_DEITY && (player->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0 &&
               sRuntimeState.swimAnimHeader.segment != nullptr) {
        nextMotion = TRANSFORM_MASK_MOTION_SWIM;
    } else if (IsUsingOcarina(player) &&
               (sRuntimeState.ocarinaStartAnimHeader.segment != nullptr ||
                sRuntimeState.ocarinaPlayAnimHeader.segment != nullptr)) {
        nextMotion = TRANSFORM_MASK_MOTION_OCARINA;
    } else if ((player->stateFlags1 & PLAYER_STATE1_SHIELDING) != 0 &&
               sRuntimeState.defendWaitAnimHeader.segment != nullptr) {
        nextMotion = TRANSFORM_MASK_MOTION_DEFEND;
    } else if (player->currentMask == PLAYER_MASK_ZORA && player->meleeWeaponState != 0 &&
               sRuntimeState.jumpAnimHeader.segment != nullptr) {
        nextMotion = TRANSFORM_MASK_MOTION_JUMP;
    } else if ((player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING) != 0) {
        nextMotion = TRANSFORM_MASK_MOTION_SPIN;
    } else if (dekuAirborneSidehop) {
        nextMotion = TRANSFORM_MASK_MOTION_HOP;
    } else if (player->currentMask == PLAYER_MASK_ZORA && sRuntimeState.motionState == TRANSFORM_MASK_MOTION_HOP &&
               (player->actor.bgCheckFlags & 1) == 0) {
        nextMotion = TRANSFORM_MASK_MOTION_HOP;
    } else if ((player->stateFlags3 & PLAYER_STATE3_MIDAIR) != 0 ||
               (player->stateFlags1 & (PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL)) != 0) {
        nextMotion = ((player->currentMask == PLAYER_MASK_DEKU || player->currentMask == PLAYER_MASK_ZORA) &&
                      player->actor.velocity.y < -0.1f && sRuntimeState.fallAnimHeader.segment != nullptr)
                         ? TRANSFORM_MASK_MOTION_FALL
                         : TRANSFORM_MASK_MOTION_JUMP;
    } else if (player->meleeWeaponState != 0) {
        nextMotion = TRANSFORM_MASK_MOTION_ATTACK;
    } else if (IsZTargeting(player) &&
               (sRuntimeState.ztargetIdleRightAnimHeader.segment != nullptr ||
                sRuntimeState.ztargetIdleLeftAnimHeader.segment != nullptr ||
                sRuntimeState.ztargetSideWalkLeftAnimHeader.segment != nullptr ||
                sRuntimeState.ztargetSideWalkRightAnimHeader.segment != nullptr ||
                sRuntimeState.ztargetBackWalkAnimHeader.segment != nullptr)) {
        nextMotion = GetZTargetMotion(player);
    } else if (fabsf(player->actor.speedXZ) > 4.5f) {
        nextMotion = TRANSFORM_MASK_MOTION_RUN;
    } else if (fabsf(player->actor.speedXZ) > 0.1f) {
        nextMotion = TRANSFORM_MASK_MOTION_WALK;
    }

    if (nextMotion == TRANSFORM_MASK_MOTION_GET_ITEM) {
        CopyOotPlayerPoseToTransformSkeleton(player);
        sRuntimeState.motionState = TRANSFORM_MASK_MOTION_GET_ITEM;
        return;
    }

    if ((player->currentMask == PLAYER_MASK_GORON || player->currentMask == PLAYER_MASK_ZORA) &&
        (nextMotion == TRANSFORM_MASK_MOTION_WALK || nextMotion == TRANSFORM_MASK_MOTION_RUN)) {
        // MM shares OoT's live link_normal walk/run pose. This keeps visible feet, animation rate, and OoT's
        // animation-driven footstep events on the same clock, including when speed modifiers are active.
        CopyOotPlayerPoseToTransformSkeleton(player);
        sRuntimeState.motionState = TRANSFORM_MASK_MOTION_OOT_ACTION;
        return;
    }

    if (nextMotion == sRuntimeState.motionState) {
        RefreshTransformLocomotionAnimSpeed(player, nextMotion);
        if (nextMotion == TRANSFORM_MASK_MOTION_DEFEND && player->currentMask == PLAYER_MASK_DEKU) {
            const f32 guardFrame = sRuntimeState.defendWaitAnimHeader.segment != nullptr
                                       ? Animation_GetLastFrame(&sRuntimeState.defendWaitAnimHeader)
                                       : Animation_GetLastFrame(&sRuntimeState.defendAnimHeader);
            if (sRuntimeState.skelAnime.curFrame >= (guardFrame - 0.1f)) {
                sRuntimeState.skelAnime.curFrame = guardFrame;
                sRuntimeState.skelAnime.playSpeed = 0.0f;
            }
        } else if (nextMotion == TRANSFORM_MASK_MOTION_DEFEND &&
            sRuntimeState.skelAnime.animation == &sRuntimeState.defendAnimHeader &&
            sRuntimeState.defendWaitAnimHeader.segment != nullptr &&
            Animation_OnFrame(&sRuntimeState.skelAnime, Animation_GetLastFrame(&sRuntimeState.defendAnimHeader))) {
            LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.defendWaitAnimHeader, 1.0f, 0.0f,
                                 Animation_GetLastFrame(&sRuntimeState.defendWaitAnimHeader), ANIMMODE_LOOP, 0.0f);
            if (player->currentMask == PLAYER_MASK_GORON && sRuntimeState.goronShieldSkelReady) {
                SkelAnime_Update(&sRuntimeState.goronShieldSkelAnime);
            }
        } else if (nextMotion == TRANSFORM_MASK_MOTION_DEFEND && player->currentMask == PLAYER_MASK_GORON &&
                   sRuntimeState.goronShieldSkelReady) {
            SkelAnime_Update(&sRuntimeState.goronShieldSkelAnime);
        } else if (nextMotion == TRANSFORM_MASK_MOTION_CLIMB &&
                   (player->currentMask == PLAYER_MASK_GORON || player->currentMask == PLAYER_MASK_DEKU)) {
            if ((player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) != 0) {
                const s8 climbStickY = play->state.input[0].rel.stick_y;
                LinkAnimationHeader* climbUpL =
                    player->currentMask == PLAYER_MASK_DEKU && sRuntimeState.dekuClimbUpLAnimHeader.segment != nullptr
                        ? &sRuntimeState.dekuClimbUpLAnimHeader
                        : &sRuntimeState.climbLoopAnimHeader;
                LinkAnimationHeader* climbUpR =
                    player->currentMask == PLAYER_MASK_DEKU && sRuntimeState.dekuClimbUpRAnimHeader.segment != nullptr
                        ? &sRuntimeState.dekuClimbUpRAnimHeader
                        : (sRuntimeState.goronClimbUpRAnimHeader.segment != nullptr ? &sRuntimeState.goronClimbUpRAnimHeader
                                                                                     : climbUpL);
                if (ABS(climbStickY) <= 10) {
                    sRuntimeState.skelAnime.playSpeed = 0.0f;
                } else {
                    LinkAnimationHeader* climbLoop = sRuntimeState.goronClimbUseRight ? climbUpR : climbUpL;
                    sRuntimeState.skelAnime.playSpeed = climbStickY > 0 ? 1.0f : -1.0f;
                    if (climbLoop != nullptr && climbLoop->segment != nullptr &&
                        sRuntimeState.skelAnime.animation != climbLoop) {
                        LinkAnimation_Change(play, &sRuntimeState.skelAnime, climbLoop, sRuntimeState.skelAnime.playSpeed,
                                             climbStickY > 0 ? 0.0f : Animation_GetLastFrame(climbLoop),
                                             climbStickY > 0 ? Animation_GetLastFrame(climbLoop) : 0.0f, ANIMMODE_ONCE, 0.0f);
                    } else if (climbLoop != nullptr && climbLoop->segment != nullptr &&
                               sRuntimeState.skelAnime.playSpeed > 0.0f &&
                               sRuntimeState.skelAnime.curFrame >= (Animation_GetLastFrame(climbLoop) - 0.5f)) {
                        sRuntimeState.goronClimbUseRight = !sRuntimeState.goronClimbUseRight;
                        LinkAnimationHeader* nextClimbLoop = sRuntimeState.goronClimbUseRight ? climbUpR : climbUpL;
                        LinkAnimation_Change(play, &sRuntimeState.skelAnime, nextClimbLoop, 1.0f, 0.0f,
                                             Animation_GetLastFrame(nextClimbLoop), ANIMMODE_ONCE, 0.0f);
                    } else if (climbLoop != nullptr && climbLoop->segment != nullptr &&
                               sRuntimeState.skelAnime.playSpeed < 0.0f && sRuntimeState.skelAnime.curFrame <= 0.5f) {
                        sRuntimeState.goronClimbUseRight = !sRuntimeState.goronClimbUseRight;
                        LinkAnimationHeader* nextClimbLoop = sRuntimeState.goronClimbUseRight ? climbUpR : climbUpL;
                        LinkAnimation_Change(play, &sRuntimeState.skelAnime, nextClimbLoop, -1.0f,
                                             Animation_GetLastFrame(nextClimbLoop), 0.0f, ANIMMODE_ONCE, 0.0f);
                    }
                }
            }
        } else if (nextMotion == TRANSFORM_MASK_MOTION_OCARINA &&
                   sRuntimeState.skelAnime.animation == &sRuntimeState.ocarinaStartAnimHeader &&
                   sRuntimeState.ocarinaPlayAnimHeader.segment != nullptr &&
                   Animation_OnFrame(&sRuntimeState.skelAnime, Animation_GetLastFrame(&sRuntimeState.ocarinaStartAnimHeader))) {
            LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.ocarinaPlayAnimHeader, 1.0f, 0.0f,
                                 Animation_GetLastFrame(&sRuntimeState.ocarinaPlayAnimHeader), ANIMMODE_LOOP, 0.0f);
        } else if (nextMotion == TRANSFORM_MASK_MOTION_JUMP && player->currentMask == PLAYER_MASK_ZORA &&
                   sRuntimeState.zoraJumpKickEndAnimHeader.segment != nullptr &&
                   (player->meleeWeaponAnimation == PLAYER_MWA_JUMPSLASH_FINISH ||
                    player->meleeWeaponAnimation == PLAYER_MWA_FLIPSLASH_FINISH) &&
                   sRuntimeState.skelAnime.animation != &sRuntimeState.zoraJumpKickEndAnimHeader) {
            LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.zoraJumpKickEndAnimHeader, 1.0f, 0.0f,
                                 Animation_GetLastFrame(&sRuntimeState.zoraJumpKickEndAnimHeader), ANIMMODE_ONCE, -4.0f);
        } else if (nextMotion == TRANSFORM_MASK_MOTION_ATTACK &&
                   (player->currentMask == PLAYER_MASK_GORON || player->currentMask == PLAYER_MASK_ZORA) &&
                   sRuntimeState.goronAction == GORON_ACTION_PUNCH) {
            LinkAnimationHeader* punchHeader = nullptr;
            LinkAnimationHeader* punchEndHeader = nullptr;
            switch (sRuntimeState.goronPunchIndex) {
                case 1:
                    punchHeader = &sRuntimeState.attackAnimHeader2;
                    punchEndHeader = &sRuntimeState.goronPunchEndAnimHeader2;
                    break;
                case 2:
                    punchHeader = &sRuntimeState.attackAnimHeader3;
                    punchEndHeader = &sRuntimeState.goronPunchEndAnimHeader3;
                    break;
                default:
                    punchHeader = &sRuntimeState.attackAnimHeader;
                    punchEndHeader = &sRuntimeState.goronPunchEndAnimHeader;
                    break;
            }

            if (player->currentMask == PLAYER_MASK_GORON && punchHeader != nullptr && punchEndHeader != nullptr &&
               punchEndHeader->segment != nullptr && sRuntimeState.skelAnime.animation == punchHeader &&
               Animation_OnFrame(&sRuntimeState.skelAnime, Animation_GetLastFrame(punchHeader))) {
                LinkAnimation_Change(play, &sRuntimeState.skelAnime, punchEndHeader, 1.0f, 0.0f,
                                     Animation_GetLastFrame(punchEndHeader), ANIMMODE_ONCE, 0.0f);
            }
        } else if (nextMotion == TRANSFORM_MASK_MOTION_HOP) {
            LinkAnimationHeader* hopStartHeader = nullptr;
            LinkAnimationHeader* hopEndHeader = nullptr;
            bool sidehopLanding = false;
            if (sRuntimeState.skelAnime.animation == &sRuntimeState.goronSidehopLeftAnimHeader) {
                hopStartHeader = &sRuntimeState.goronSidehopLeftAnimHeader;
                hopEndHeader = &sRuntimeState.goronSidehopLeftEndAnimHeader;
                sidehopLanding = true;
            } else if (sRuntimeState.skelAnime.animation == &sRuntimeState.goronSidehopRightAnimHeader) {
                hopStartHeader = &sRuntimeState.goronSidehopRightAnimHeader;
                hopEndHeader = &sRuntimeState.goronSidehopRightEndAnimHeader;
                sidehopLanding = true;
            } else if (sRuntimeState.skelAnime.animation == &sRuntimeState.goronBackflipAnimHeader) {
                hopStartHeader = &sRuntimeState.goronBackflipAnimHeader;
                hopEndHeader = &sRuntimeState.goronBackflipEndAnimHeader;
            }

            if (hopStartHeader != nullptr && sRuntimeState.skelAnime.curFrame >= 3.0f &&
                ((player->actor.bgCheckFlags & 1) != 0)) {
                if (player->currentMask == PLAYER_MASK_GORON) {
                    PlayGoronActionSfx(player, NA_SE_PL_LAND);
                }
                if (player->currentMask == PLAYER_MASK_DEKU && sidehopLanding &&
                    sRuntimeState.ztargetIdleRightAnimHeader.segment != nullptr) {
                    player->stateFlags2 &= ~PLAYER_STATE2_HOPPING;
                    LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.ztargetIdleRightAnimHeader,
                                         1.0f, 0.0f,
                                         Animation_GetLastFrame(&sRuntimeState.ztargetIdleRightAnimHeader),
                                         ANIMMODE_LOOP, 0.0f);
                    sRuntimeState.motionState = TRANSFORM_MASK_MOTION_ZTARGET_IDLE;
                } else if (sidehopLanding && sRuntimeState.idleAnimHeader.segment != nullptr) {
                    LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.idleAnimHeader, 1.0f, 0.0f,
                                         Animation_GetLastFrame(&sRuntimeState.idleAnimHeader), ANIMMODE_LOOP, 0.0f);
                    sRuntimeState.motionState = TRANSFORM_MASK_MOTION_IDLE;
                } else if (sRuntimeState.ztargetIdleRightAnimHeader.segment != nullptr) {
                    LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.ztargetIdleRightAnimHeader, 1.0f, 0.0f,
                                         Animation_GetLastFrame(&sRuntimeState.ztargetIdleRightAnimHeader), ANIMMODE_LOOP,
                                         0.0f);
                    sRuntimeState.motionState = TRANSFORM_MASK_MOTION_ZTARGET_IDLE;
                } else if (sRuntimeState.idleAnimHeader.segment != nullptr) {
                    LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.idleAnimHeader, 1.0f, 0.0f,
                                         Animation_GetLastFrame(&sRuntimeState.idleAnimHeader), ANIMMODE_LOOP, 0.0f);
                    sRuntimeState.motionState = TRANSFORM_MASK_MOTION_IDLE;
                } else if (hopEndHeader != nullptr && hopEndHeader->segment != nullptr) {
                    LinkAnimation_Change(play, &sRuntimeState.skelAnime, hopEndHeader, 1.0f, 0.0f,
                                         Animation_GetLastFrame(hopEndHeader), ANIMMODE_ONCE, 0.0f);
                } else {
                    sRuntimeState.motionState = TRANSFORM_MASK_MOTION_IDLE;
                }
            }
        } else if (nextMotion == TRANSFORM_MASK_MOTION_SWIM && player->currentMask == PLAYER_MASK_ZORA) {
            LinkAnimationHeader* waterAnim = GetZoraWaterAnimation(player);
            if (waterAnim != nullptr && sRuntimeState.skelAnime.animation != waterAnim) {
                const s32 waterMode = (waterAnim == &sRuntimeState.zoraSwimDownAnimHeader ||
                                       waterAnim == &sRuntimeState.zoraSwimDeepEndAnimHeader ||
                                       waterAnim == &sRuntimeState.zoraWaitToSwimWaitAnimHeader ||
                                       waterAnim == &sRuntimeState.zoraLandToSwimWaitAnimHeader)
                                          ? ANIMMODE_ONCE
                                          : ANIMMODE_LOOP;
                LinkAnimation_Change(play, &sRuntimeState.skelAnime, waterAnim, 1.0f, 0.0f,
                                     Animation_GetLastFrame(waterAnim), waterMode, -6.0f);
            }
        } else if (nextMotion == TRANSFORM_MASK_MOTION_OCARINA &&
                   sRuntimeState.skelAnime.animation != &sRuntimeState.ocarinaStartAnimHeader &&
                   sRuntimeState.skelAnime.animation != &sRuntimeState.ocarinaPlayAnimHeader) {
            if (sRuntimeState.ocarinaStartAnimHeader.segment != nullptr) {
                LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.ocarinaStartAnimHeader, 1.0f, 0.0f,
                                     Animation_GetLastFrame(&sRuntimeState.ocarinaStartAnimHeader), ANIMMODE_ONCE, 0.0f);
            } else if (sRuntimeState.ocarinaPlayAnimHeader.segment != nullptr) {
                LinkAnimation_Change(play, &sRuntimeState.skelAnime, &sRuntimeState.ocarinaPlayAnimHeader, 1.0f, 0.0f,
                                     Animation_GetLastFrame(&sRuntimeState.ocarinaPlayAnimHeader), ANIMMODE_LOOP, 0.0f);
            }
        }
        return;
    }

    LinkAnimationHeader* targetAnim = nullptr;
    switch (nextMotion) {
        case TRANSFORM_MASK_MOTION_DOOR:
            if (sRuntimeState.doorAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.doorAnimHeader;
            }
            break;
        case TRANSFORM_MASK_MOTION_GET_ITEM:
            break;
        case TRANSFORM_MASK_MOTION_SWIM:
            if (player->currentMask == PLAYER_MASK_ZORA) {
                targetAnim = GetZoraWaterAnimation(player);
            } else {
                if (fabsf(player->actor.speedXZ) > 0.2f && sRuntimeState.swimAnimHeader.segment != nullptr) {
                    targetAnim = &sRuntimeState.swimAnimHeader;
                } else if (sRuntimeState.swimIdleAnimHeader.segment != nullptr) {
                    targetAnim = &sRuntimeState.swimIdleAnimHeader;
                } else if (sRuntimeState.swimAnimHeader.segment != nullptr) {
                    targetAnim = &sRuntimeState.swimAnimHeader;
                }
            }
            break;
        case TRANSFORM_MASK_MOTION_ZORA_FAST_SWIM:
            if (sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_START &&
                sRuntimeState.zoraWaterRollAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.zoraWaterRollAnimHeader;
            } else if (sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_EXIT &&
                       sRuntimeState.zoraSwimToWaitAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.zoraSwimToWaitAnimHeader;
            } else if (sRuntimeState.zoraSwimState == ZORA_SWIM_DOLPHIN_JUMP &&
                       sRuntimeState.zoraFishSwimAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.zoraFishSwimAnimHeader;
            } else if (sRuntimeState.zoraFishSwimAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.zoraFishSwimAnimHeader;
            } else if (sRuntimeState.swimIdleAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.swimIdleAnimHeader;
            }
            break;
        case TRANSFORM_MASK_MOTION_ATTACK:
            if ((player->currentMask == PLAYER_MASK_GORON || player->currentMask == PLAYER_MASK_ZORA) &&
                sRuntimeState.goronAction == GORON_ACTION_PUNCH) {
                if (sRuntimeState.goronPunchIndex == 1 && sRuntimeState.attackAnimHeader2.segment != nullptr) {
                    targetAnim = &sRuntimeState.attackAnimHeader2;
                } else if (sRuntimeState.goronPunchIndex == 2 && sRuntimeState.attackAnimHeader3.segment != nullptr) {
                    targetAnim = &sRuntimeState.attackAnimHeader3;
                } else if (sRuntimeState.attackAnimHeader.segment != nullptr) {
                    targetAnim = &sRuntimeState.attackAnimHeader;
                }
                break;
            }

            if (nextMotion != sRuntimeState.motionState) {
                sRuntimeState.attackComboIndex =
                    (sRuntimeState.attackComboIndex + 1) %
                    (sRuntimeState.attackAnimHeader3.segment != nullptr
                         ? 3
                         : (sRuntimeState.attackAnimHeader2.segment != nullptr ? 2 : 1));
            }
            if (sRuntimeState.attackComboIndex == 1 && sRuntimeState.attackAnimHeader2.segment != nullptr) {
                targetAnim = &sRuntimeState.attackAnimHeader2;
            } else if (sRuntimeState.attackComboIndex == 2 && sRuntimeState.attackAnimHeader3.segment != nullptr) {
                targetAnim = &sRuntimeState.attackAnimHeader3;
            } else if (sRuntimeState.attackAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.attackAnimHeader;
            }
            break;
        case TRANSFORM_MASK_MOTION_DEFEND:
            if (sRuntimeState.motionState != TRANSFORM_MASK_MOTION_DEFEND &&
                sRuntimeState.defendAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.defendAnimHeader;
            } else if (sRuntimeState.defendWaitAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.defendWaitAnimHeader;
            } else if (sRuntimeState.defendAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.defendAnimHeader;
            }
            break;
        case TRANSFORM_MASK_MOTION_OCARINA:
            if (sRuntimeState.motionState != TRANSFORM_MASK_MOTION_OCARINA &&
                sRuntimeState.ocarinaStartAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.ocarinaStartAnimHeader;
            } else if (sRuntimeState.ocarinaPlayAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.ocarinaPlayAnimHeader;
            } else if (sRuntimeState.ocarinaStartAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.ocarinaStartAnimHeader;
            }
            break;
        case TRANSFORM_MASK_MOTION_SPIN:
            if (sRuntimeState.spinAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.spinAnimHeader;
            }
            break;
        case TRANSFORM_MASK_MOTION_JUMP:
            if (player->currentMask == PLAYER_MASK_ZORA && sRuntimeState.zoraJumpKickEndAnimHeader.segment != nullptr &&
                (player->meleeWeaponAnimation == PLAYER_MWA_JUMPSLASH_FINISH ||
                 player->meleeWeaponAnimation == PLAYER_MWA_FLIPSLASH_FINISH)) {
                targetAnim = &sRuntimeState.zoraJumpKickEndAnimHeader;
            } else if (sRuntimeState.jumpAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.jumpAnimHeader;
            }
            break;
        case TRANSFORM_MASK_MOTION_FALL:
            if (sRuntimeState.fallAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.fallAnimHeader;
            } else if (sRuntimeState.jumpAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.jumpAnimHeader;
            }
            break;
        case TRANSFORM_MASK_MOTION_HOP:
            switch (player->av1.actionVar1) {
                case PLAYER_STICK_DIR_LEFT:
                    if (sRuntimeState.goronSidehopLeftAnimHeader.segment != nullptr) {
                        targetAnim = &sRuntimeState.goronSidehopLeftAnimHeader;
                    }
                    break;
                case PLAYER_STICK_DIR_RIGHT:
                    if (sRuntimeState.goronSidehopRightAnimHeader.segment != nullptr) {
                        targetAnim = &sRuntimeState.goronSidehopRightAnimHeader;
                    }
                    break;
                case PLAYER_STICK_DIR_BACKWARD:
                default:
                    if (sRuntimeState.goronBackflipAnimHeader.segment != nullptr) {
                        targetAnim = &sRuntimeState.goronBackflipAnimHeader;
                    }
                    break;
            }
            if (targetAnim == nullptr && sRuntimeState.hopAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.hopAnimHeader;
            }
            break;
        case TRANSFORM_MASK_MOTION_CLIMB:
            if ((player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) != 0) {
                if (player->currentMask == PLAYER_MASK_DEKU && sRuntimeState.goronClimbUseRight &&
                    sRuntimeState.dekuClimbUpRAnimHeader.segment != nullptr) {
                    targetAnim = &sRuntimeState.dekuClimbUpRAnimHeader;
                } else if (player->currentMask == PLAYER_MASK_DEKU &&
                           sRuntimeState.dekuClimbUpLAnimHeader.segment != nullptr) {
                    targetAnim = &sRuntimeState.dekuClimbUpLAnimHeader;
                } else if (sRuntimeState.goronClimbUseRight && sRuntimeState.goronClimbUpRAnimHeader.segment != nullptr) {
                    targetAnim = &sRuntimeState.goronClimbUpRAnimHeader;
                } else if (sRuntimeState.climbLoopAnimHeader.segment != nullptr) {
                    targetAnim = &sRuntimeState.climbLoopAnimHeader;
                }
            } else if ((player->stateFlags1 & PLAYER_STATE1_HANGING_OFF_LEDGE) != 0 &&
                       sRuntimeState.climbStartAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.climbStartAnimHeader;
            } else if ((player->stateFlags1 & PLAYER_STATE1_CLIMBING_LEDGE) != 0 &&
                       sRuntimeState.climbLoopAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.climbLoopAnimHeader;
            } else if (sRuntimeState.motionState != TRANSFORM_MASK_MOTION_CLIMB &&
                sRuntimeState.climbStartAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.climbStartAnimHeader;
            } else if (sRuntimeState.climbLoopAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.climbLoopAnimHeader;
            } else if (sRuntimeState.climbStartAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.climbStartAnimHeader;
            }
            break;
        case TRANSFORM_MASK_MOTION_ZTARGET_IDLE:
            if (sRuntimeState.ztargetIdleRightAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.ztargetIdleRightAnimHeader;
            } else if (sRuntimeState.ztargetIdleLeftAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.ztargetIdleLeftAnimHeader;
            }
            break;
        case TRANSFORM_MASK_MOTION_ZTARGET_SIDEWALK_LEFT:
            if (sRuntimeState.ztargetSideWalkLeftAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.ztargetSideWalkLeftAnimHeader;
            }
            break;
        case TRANSFORM_MASK_MOTION_ZTARGET_SIDEWALK_RIGHT:
            if (sRuntimeState.ztargetSideWalkRightAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.ztargetSideWalkRightAnimHeader;
            }
            break;
        case TRANSFORM_MASK_MOTION_ZTARGET_BACKWALK:
            if (sRuntimeState.ztargetBackWalkAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.ztargetBackWalkAnimHeader;
            }
            break;
        case TRANSFORM_MASK_MOTION_RUN:
            if (sRuntimeState.runAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.runAnimHeader;
            }
            break;
        case TRANSFORM_MASK_MOTION_WALK:
            if (sRuntimeState.walkAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.walkAnimHeader;
            }
            break;
        case TRANSFORM_MASK_MOTION_IDLE:
        default:
            if (sRuntimeState.idleAnimHeader.segment != nullptr) {
                targetAnim = &sRuntimeState.idleAnimHeader;
            }
            break;
    }

    if (targetAnim == nullptr) {
        if (sRuntimeState.idleAnimHeader.segment == nullptr) {
            return;
        }
        targetAnim = &sRuntimeState.idleAnimHeader;
    }

    f32 animSpeed = 1.0f;
    if (player->currentMask == PLAYER_MASK_ZORA && nextMotion == TRANSFORM_MASK_MOTION_ATTACK &&
        sRuntimeState.goronAction == GORON_ACTION_PUNCH) {
        animSpeed = 0.75f;
    } else if ((player->currentMask == PLAYER_MASK_GORON || player->currentMask == PLAYER_MASK_ZORA) &&
               nextMotion == TRANSFORM_MASK_MOTION_HOP) {
        animSpeed = 2.0f / 3.0f;
    } else if ((player->currentMask == PLAYER_MASK_GORON || player->currentMask == PLAYER_MASK_DEKU) &&
               nextMotion == TRANSFORM_MASK_MOTION_CLIMB &&
               (player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) != 0) {
        const s8 climbStickY = play->state.input[0].rel.stick_y;
        animSpeed = ABS(climbStickY) <= 10 ? 0.0f : (climbStickY > 0 ? 1.0f : -1.0f);
    } else if (IsTransformLocomotionMotion(nextMotion) ||
               (nextMotion == TRANSFORM_MASK_MOTION_WALK && IsZTargeting(player))) {
        animSpeed = GetTransformLocomotionAnimSpeed(player, nextMotion);
    }

    const bool goronRollingLoop =
        player->currentMask == PLAYER_MASK_GORON && sRuntimeState.goronAction == GORON_ACTION_ROLL &&
        targetAnim == &sRuntimeState.spinAnimHeader;
    const bool playOnce = !goronRollingLoop &&
                          ((targetAnim == &sRuntimeState.attackAnimHeader) || (targetAnim == &sRuntimeState.attackAnimHeader2) ||
                          (targetAnim == &sRuntimeState.attackAnimHeader3) || (targetAnim == &sRuntimeState.spinAnimHeader) ||
                          (targetAnim == &sRuntimeState.jumpAnimHeader) || (targetAnim == &sRuntimeState.hopAnimHeader) ||
                          (targetAnim == &sRuntimeState.zoraJumpKickEndAnimHeader) ||
                          (targetAnim == &sRuntimeState.zoraWaterRollAnimHeader) ||
                          (targetAnim == &sRuntimeState.zoraSwimToWaitAnimHeader) ||
                          (targetAnim == &sRuntimeState.zoraSwimDownAnimHeader) ||
                          (targetAnim == &sRuntimeState.zoraSwimDeepEndAnimHeader) ||
                          (targetAnim == &sRuntimeState.zoraWaitToSwimWaitAnimHeader) ||
                          (targetAnim == &sRuntimeState.zoraLandToSwimWaitAnimHeader) ||
                          (targetAnim == &sRuntimeState.goronSidehopLeftAnimHeader) ||
                          (targetAnim == &sRuntimeState.goronSidehopRightAnimHeader) ||
                          (targetAnim == &sRuntimeState.goronBackflipAnimHeader) ||
                          (targetAnim == &sRuntimeState.climbLoopAnimHeader &&
                           (player->stateFlags1 & PLAYER_STATE1_CLIMBING_LEDGE) != 0) ||
                          (targetAnim == &sRuntimeState.defendAnimHeader) || (targetAnim == &sRuntimeState.ocarinaStartAnimHeader) ||
                          (targetAnim == &sRuntimeState.doorAnimHeader) || (targetAnim == &sRuntimeState.chestAnimHeader));
    const s32 animMode = playOnce ? ANIMMODE_ONCE : ANIMMODE_LOOP;
    LinkAnimation_Change(play, &sRuntimeState.skelAnime, targetAnim, animSpeed, 0.0f, Animation_GetLastFrame(targetAnim),
                         animMode, 0.0f);
    if (player->currentMask == PLAYER_MASK_GORON && nextMotion == TRANSFORM_MASK_MOTION_DEFEND &&
        sRuntimeState.goronShieldSkelReady) {
        SkelAnime_Update(&sRuntimeState.goronShieldSkelAnime);
    }
    if (player->currentMask == PLAYER_MASK_GORON && sRuntimeState.goronLastMotionSfx != nextMotion) {
        if (sRuntimeState.goronLastMotionSfx == TRANSFORM_MASK_MOTION_CLIMB &&
            nextMotion != TRANSFORM_MASK_MOTION_CLIMB) {
            PlayMmGoronVoiceOneShot(MM_GORON_VOICE_CLIMB_END);
        }
        if (nextMotion == TRANSFORM_MASK_MOTION_HOP) {
            PlayGoronActionSfx(player, NA_SE_PL_JUMP);
        } else if (nextMotion == TRANSFORM_MASK_MOTION_CLIMB) {
            PlayGoronActionSfx(player, NA_SE_PL_CLIMB_CLIFF);
        } else if (nextMotion == TRANSFORM_MASK_MOTION_DEFEND) {
            PlayGoronActionSfx(player, NA_SE_PL_BODY_HIT);
        }
        sRuntimeState.goronLastMotionSfx = nextMotion;
    }
    if ((player->currentMask == PLAYER_MASK_GORON || player->currentMask == PLAYER_MASK_ZORA ||
         player->currentMask == PLAYER_MASK_DEKU) &&
        (nextMotion == TRANSFORM_MASK_MOTION_ATTACK || nextMotion == TRANSFORM_MASK_MOTION_HOP ||
         nextMotion == TRANSFORM_MASK_MOTION_CLIMB || nextMotion == TRANSFORM_MASK_MOTION_DOOR ||
         nextMotion == TRANSFORM_MASK_MOTION_GET_ITEM)) {
        sRuntimeState.skelAnime.prevTransl = sRuntimeState.skelAnime.jointTable[0];
        sRuntimeState.skelAnime.prevRot = player->actor.shape.rot.y;
    }
    sRuntimeState.motionState = nextMotion;
}

void SyncGoronAnimationRoot(Player* player) {
    if (player == nullptr || (player->currentMask != PLAYER_MASK_GORON && player->currentMask != PLAYER_MASK_ZORA &&
                              player->currentMask != PLAYER_MASK_DEKU)) {
        return;
    }

    const bool appliesRootMotion =
        (player->currentMask == PLAYER_MASK_GORON || player->currentMask == PLAYER_MASK_ZORA) &&
        sRuntimeState.goronAction == GORON_ACTION_PUNCH;
    const bool normalizesVisualRoot =
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_IDLE ||
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_WALK ||
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_RUN ||
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_JUMP ||
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_FALL ||
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_HOP ||
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_CLIMB ||
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_DOOR ||
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_GET_ITEM ||
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_DEFEND ||
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_OCARINA ||
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_SWIM ||
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_ZTARGET_IDLE ||
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_ZTARGET_SIDEWALK_LEFT ||
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_ZTARGET_SIDEWALK_RIGHT ||
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_ZTARGET_BACKWALK ||
        sRuntimeState.motionState == TRANSFORM_MASK_MOTION_OOT_ACTION;

    if (!appliesRootMotion && !normalizesVisualRoot) {
        return;
    }

    Vec3f diff = {};
    sRuntimeState.skelAnime.movementFlags = appliesRootMotion ? 0 : ANIM_FLAG_NOMOVE;
    SkelAnime_UpdateTranslation(&sRuntimeState.skelAnime, &diff, player->actor.shape.rot.y);

    if (appliesRootMotion) {
        if (!std::isfinite(diff.x) || !std::isfinite(diff.z)) {
            sRuntimeState.skelAnime.prevTransl = sRuntimeState.skelAnime.jointTable[0];
            sRuntimeState.skelAnime.prevRot = player->actor.shape.rot.y;
            return;
        }

        const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask);
        const f32 scale = player->actor.scale.x * (form != nullptr ? form->drawScale : 1.0f);
        f32 nextX = player->actor.world.pos.x + (diff.x * scale);
        f32 nextZ = player->actor.world.pos.z + (diff.z * scale);
        if (!std::isfinite(nextX) || !std::isfinite(nextZ)) {
            sRuntimeState.skelAnime.prevTransl = sRuntimeState.skelAnime.jointTable[0];
            sRuntimeState.skelAnime.prevRot = player->actor.shape.rot.y;
            return;
        }

        if (sRuntimeState.goronPunchIndex == 2 && sRuntimeState.goronPunchAnchorValid) {
            const f32 dx = nextX - sRuntimeState.goronPunchAnchorPos.x;
            const f32 dz = nextZ - sRuntimeState.goronPunchAnchorPos.z;
            const f32 distSq = (dx * dx) + (dz * dz);
            constexpr f32 kMaxThirdPunchRootTravel = 28.0f;
            if (distSq > (kMaxThirdPunchRootTravel * kMaxThirdPunchRootTravel)) {
                const f32 dist = sqrtf(distSq);
                if (dist > 0.001f) {
                    const f32 scaleToAnchor = kMaxThirdPunchRootTravel / dist;
                    nextX = sRuntimeState.goronPunchAnchorPos.x + (dx * scaleToAnchor);
                    nextZ = sRuntimeState.goronPunchAnchorPos.z + (dz * scaleToAnchor);
                } else {
                    nextX = sRuntimeState.goronPunchAnchorPos.x;
                    nextZ = sRuntimeState.goronPunchAnchorPos.z;
                }
            }
        }

        player->actor.world.pos.x = nextX;
        player->actor.world.pos.z = nextZ;
        player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw;
    }
}

} // namespace

extern "C" bool OoTxMm_IsMaskSelectItem(s16 itemId) {
    for (const auto maskItem : kMaskCycleItems) {
        if (maskItem == itemId) {
            return true;
        }
    }

    return false;
}

extern "C" s32 OoTxMm_GetMaskCycleIndex(s16 itemId) {
    for (size_t i = 0; i < kMaskCycleItems.size(); ++i) {
        if (kMaskCycleItems[i] == itemId) {
            return static_cast<s32>(i);
        }
    }

    return -1;
}

extern "C" s16 OoTxMm_GetMaskCycleItem(s32 index) {
    if (index < 0) {
        index = static_cast<s32>(kMaskCycleItems.size()) - 1;
    }

    return kMaskCycleItems[index % static_cast<s32>(kMaskCycleItems.size())];
}

extern "C" s16 OoTxMm_GetMaskCyclePrevItem(s16 itemId) {
    const s32 currentIndex = OoTxMm_GetMaskCycleIndex(itemId);
    if (currentIndex < 0) {
        return kMaskCycleItems.back();
    }

    return OoTxMm_GetMaskCycleItem(currentIndex - 1);
}

extern "C" s16 OoTxMm_GetMaskCycleNextItem(s16 itemId) {
    const s32 currentIndex = OoTxMm_GetMaskCycleIndex(itemId);
    if (currentIndex < 0) {
        return kMaskCycleItems.front();
    }

    return OoTxMm_GetMaskCycleItem(currentIndex + 1);
}

bool IsMaskCycleItemOwnedInRando(s16 itemId, s16 currentItem) {
    switch (itemId) {
        case ITEM_MASK_KEATON:
            return Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_KEATON);
        case ITEM_MASK_SKULL:
            return Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_SKULL);
        case ITEM_MASK_SPOOKY:
            return Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_SPOOKY);
        case ITEM_MASK_BUNNY:
            return Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_BUNNY);
        case ITEM_MASK_GORON:
            return Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_GORON);
        case ITEM_MASK_ZORA:
            return Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_ZORA);
        case ITEM_MASK_GERUDO:
            return Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_GERUDO);
        case ITEM_MASK_TRUTH:
            return Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_TRUTH);
        case ITEM_MASK_DEKU:
            return Flags_GetRandomizerInf(RAND_INF_OOTXMM_HAS_MASK_DEKU) || itemId == currentItem;
        case ITEM_MASK_FIERCE_DEITY:
            return Flags_GetRandomizerInf(RAND_INF_OOTXMM_HAS_MASK_FIERCE_DEITY) || itemId == currentItem;
        default:
            return false;
    }
}

s16 GetOwnedMaskCycleItem(s16 itemId, s32 direction) {
    if (!IS_RANDO) {
        return direction < 0 ? OoTxMm_GetMaskCyclePrevItem(itemId) : OoTxMm_GetMaskCycleNextItem(itemId);
    }

    const s32 currentIndex = OoTxMm_GetMaskCycleIndex(itemId);
    if (currentIndex < 0) {
        return itemId;
    }

    for (s32 i = 1; i <= static_cast<s32>(kMaskCycleItems.size()); ++i) {
        const s16 candidate = OoTxMm_GetMaskCycleItem(currentIndex + (direction * i));
        if (IsMaskCycleItemOwnedInRando(candidate, itemId)) {
            return candidate;
        }
    }

    return itemId;
}

extern "C" s16 OoTxMm_GetOwnedMaskCyclePrevItem(s16 itemId) {
    return GetOwnedMaskCycleItem(itemId, -1);
}

extern "C" s16 OoTxMm_GetOwnedMaskCycleNextItem(s16 itemId) {
    return GetOwnedMaskCycleItem(itemId, 1);
}

extern "C" u8 OoTxMm_ItemIdToPlayerMask(s16 itemId) {
    switch (itemId) {
        case ITEM_MASK_KEATON:
            return PLAYER_MASK_KEATON;
        case ITEM_MASK_SKULL:
            return PLAYER_MASK_SKULL;
        case ITEM_MASK_SPOOKY:
            return PLAYER_MASK_SPOOKY;
        case ITEM_MASK_BUNNY:
            return PLAYER_MASK_BUNNY;
        case ITEM_MASK_GORON:
            return PLAYER_MASK_GORON;
        case ITEM_MASK_ZORA:
            return PLAYER_MASK_ZORA;
        case ITEM_MASK_GERUDO:
            return PLAYER_MASK_GERUDO;
        case ITEM_MASK_TRUTH:
            return PLAYER_MASK_TRUTH;
        case ITEM_MASK_DEKU:
            return PLAYER_MASK_DEKU;
        case ITEM_MASK_FIERCE_DEITY:
            return PLAYER_MASK_FIERCE_DEITY;
        default:
            return PLAYER_MASK_NONE;
    }
}

extern "C" s16 OoTxMm_PlayerMaskToItemId(u8 playerMask) {
    switch (playerMask) {
        case PLAYER_MASK_KEATON:
            return ITEM_MASK_KEATON;
        case PLAYER_MASK_SKULL:
            return ITEM_MASK_SKULL;
        case PLAYER_MASK_SPOOKY:
            return ITEM_MASK_SPOOKY;
        case PLAYER_MASK_BUNNY:
            return ITEM_MASK_BUNNY;
        case PLAYER_MASK_GORON:
            return ITEM_MASK_GORON;
        case PLAYER_MASK_ZORA:
            return ITEM_MASK_ZORA;
        case PLAYER_MASK_GERUDO:
            return ITEM_MASK_GERUDO;
        case PLAYER_MASK_TRUTH:
            return ITEM_MASK_TRUTH;
        case PLAYER_MASK_DEKU:
            return ITEM_MASK_DEKU;
        case PLAYER_MASK_FIERCE_DEITY:
            return ITEM_MASK_FIERCE_DEITY;
        default:
            return ITEM_NONE;
    }
}

extern "C" bool OoTxMm_IsTransformationMaskPlayerMask(u8 playerMask) {
    return playerMask == PLAYER_MASK_DEKU || playerMask == PLAYER_MASK_GORON || playerMask == PLAYER_MASK_ZORA ||
           playerMask == PLAYER_MASK_FIERCE_DEITY;
}

extern "C" bool OoTxMm_IsFierceDeitySkinMode(Player* player) {
    return player != nullptr && player->currentMask == PLAYER_MASK_FIERCE_DEITY && sRuntimeState.initialized &&
           sRuntimeState.activeMask == PLAYER_MASK_FIERCE_DEITY;
}

extern "C" u16 OoTxMm_GetTransformMaskInputBlockMask(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr) {
        return 0;
    }

    if (player->currentMask == PLAYER_MASK_DEKU) {
        if (sRuntimeState.dekuAction == DEKU_ACTION_BUBBLE_AIM || sRuntimeState.dekuAction == DEKU_ACTION_BUBBLE_FIRE ||
            sRuntimeState.dekuAction == DEKU_ACTION_SPIN) {
            return BTN_A | BTN_B;
        }
        if (player->interactRangeActor != nullptr && player->talkActor == nullptr &&
            player->doorType == PLAYER_DOORTYPE_NONE && player->getItemId == GI_NONE) {
            return BTN_A;
        }
        if (CanUseDekuActionButtons(player)) {
            const Input* input = &play->state.input[0];
            u16 mask = BTN_B;
            if (!IsZTargeting(player) && CHECK_BTN_ALL(input->press.button, BTN_A) && player->interactRangeActor == nullptr &&
                player->talkActor == nullptr && player->doorType == PLAYER_DOORTYPE_NONE) {
                mask |= BTN_A;
            }
            return mask;
        }
    }

    if (player->currentMask == PLAYER_MASK_GORON && CanUseGoronActionButtons(player)) {
        u16 mask = BTN_B;
        const Input* input = &play->state.input[0];
        const bool rollOwnsA =
            sRuntimeState.goronAction == GORON_ACTION_ROLL || sRuntimeState.goronAction == GORON_ACTION_ROLL_START ||
            sRuntimeState.goronAction == GORON_ACTION_SLAM_JUMP ||
            sRuntimeState.goronAction == GORON_ACTION_SLAM_POUND;
        if ((rollOwnsA || IsGoronRollInputIntent(input)) && player->interactRangeActor == nullptr &&
            player->talkActor == nullptr && player->doorType == PLAYER_DOORTYPE_NONE &&
            !IsGoronVanillaHopInputIntent(player, input)) {
            mask |= BTN_A;
        }
        return mask;
    }

    if (player->currentMask == PLAYER_MASK_ZORA && CanUseGoronActionButtons(player) &&
        (player->stateFlags1 & PLAYER_STATE1_IN_WATER) == 0) {
        return BTN_B;
    }

    return 0;
}

extern "C" f32 OoTxMm_GetTransformMaskMovementSpeedMultiplier(Player* player) {
    if (player == nullptr) {
        return 1.0f;
    }

    if (OoTxMm_IsFierceDeitySkinMode(player)) {
        return 1.5f;
    }

    if (player->currentMask == PLAYER_MASK_DEKU && sRuntimeState.dekuAction == DEKU_ACTION_SPIN) {
        return 2.0f;
    }

    return 1.0f;
}

void ApplyFierceDeitySkinRules(Player* player) {
    if (!OoTxMm_IsFierceDeitySkinMode(player)) {
        return;
    }

    if (player->modelAnimType < PLAYER_ANIMTYPE_3) {
        player->modelAnimType = PLAYER_ANIMTYPE_3;
    }

    if (player->actor.yDistToWater > 50.0f && player->actor.yDistToWater <= 80.0f) {
        player->actor.yDistToWater = 50.0f;
    }
}

extern "C" void OoTxMm_UpdateTransformMaskState(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr) {
        return;
    }

    if (sRuntimeState.activeMask == PLAYER_MASK_ZORA && player->currentMask != PLAYER_MASK_ZORA &&
        sRuntimeState.zoraBarrierIntensity > 0) {
        ResetZoraBarrierEnvironment(play);
    }

    if (!OoTxMm_IsTransformationMaskPlayerMask(player->currentMask)) {
        ResetRuntimeState();
        return;
    }

    if (!EnsureRuntimeInitialized(play, player, player->currentMask)) {
        return;
    }

    if (sRuntimeState.transformFlashTimer > 0) {
        sRuntimeState.transformFlashTimer--;
    }

    if (player->currentMask == PLAYER_MASK_GORON || player->currentMask == PLAYER_MASK_ZORA) {
        player->modelAnimType = PLAYER_ANIMTYPE_0;
    }

    if (player->currentMask == PLAYER_MASK_FIERCE_DEITY) {
        ApplyFierceDeitySkinRules(player);
    }

    if (player->currentMask == PLAYER_MASK_GORON) {
        player->stateFlags1 &= ~(PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE);
        if (const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask); form != nullptr) {
            UpdateGoronMechanics(play, player, form);
        }
        if (!OoTxMm_IsTransformationMaskPlayerMask(player->currentMask)) {
            return;
        }
        SyncGoronHeldActor(player);
    }
    if (player->currentMask == PLAYER_MASK_DEKU) {
        if (const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask); form != nullptr) {
            UpdateDekuMechanics(play, player, form);
        }
    }
    if (player->currentMask == PLAYER_MASK_ZORA) {
        if (const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask); form != nullptr) {
            UpdateZoraMechanics(play, player, form);
            ApplyZoraJumpSlashDamage(player);
        }
    }

    UpdateMotionAnimation(play, player);
    if (sRuntimeState.motionState != TRANSFORM_MASK_MOTION_OOT_ACTION) {
        LinkAnimation_Update(play, &sRuntimeState.skelAnime);
        CaptureDekuStableRootY(player);
        NeutralizeDekuSidehopRoot(player);
    } else {
        CopyOotPlayerPoseToTransformSkeleton(player);
    }
    SyncGoronAnimationRoot(player);
    SyncTransformMaskPlayerAnchor(player);
}

extern "C" bool OoTxMm_ShouldOverridePlayerDraw(Player* player) {
    if (player == nullptr) {
        return false;
    }

    return OoTxMm_IsTransformationMaskPlayerMask(player->currentMask) && sRuntimeState.initialized &&
           sRuntimeState.assetsReady && sRuntimeState.ownerPlayer == player && sRuntimeState.activeMask == player->currentMask;
}

extern "C" bool OoTxMm_ShouldHideTransformMaskPlayerDraw(Player* player) {
    return player != nullptr && OoTxMm_ShouldOverridePlayerDraw(player) &&
           (player->stateFlags1 & PLAYER_STATE1_FIRST_PERSON) != 0;
}

extern "C" bool OoTxMm_ShouldSuspendOverride(Player* player) {
    if (!OoTxMm_ShouldOverridePlayerDraw(player)) {
        return false;
    }

    return (player->stateFlags2 & PLAYER_STATE2_CRAWLING) != 0;
}

extern "C" void* OoTxMm_GetTransformMaskEyeTexture(Player* player) {
    if (!OoTxMm_ShouldOverridePlayerDraw(player)) {
        return nullptr;
    }

    return sRuntimeState.eyeTexture;
}

extern "C" void* OoTxMm_GetTransformMaskMouthTexture(Player* player) {
    if (!OoTxMm_ShouldOverridePlayerDraw(player)) {
        return nullptr;
    }

    return sRuntimeState.mouthTexture;
}

extern "C" void* OoTxMm_GetSpecialItemIconTexture(s16 itemId, void* fallback) {
    EnsureSpecialItemUiResourcesLoaded();

    switch (itemId) {
        case ITEM_MASK_DEKU:
            return sSpecialItemUiState.dekuMaskIcon != nullptr ? sSpecialItemUiState.dekuMaskIcon : fallback;
        case ITEM_MASK_FIERCE_DEITY:
            return sSpecialItemUiState.fierceDeityMaskIcon != nullptr ? sSpecialItemUiState.fierceDeityMaskIcon : fallback;
        default:
            return fallback;
    }
}

extern "C" const char* OoTxMm_GetSpecialItemNameTexture(s16 itemId, s32 language, const char* fallback) {
    (void)language;

    const auto mmArchive = SohCrossover::GetMmO2rArchive();
    if (mmArchive == nullptr) {
        return fallback;
    }

    switch (itemId) {
        case ITEM_MASK_DEKU:
            return TryLoadMmRawPointer(kDekuMaskItemNamePath, kDekuMaskItemNamePathOtr) != nullptr
                       ? kDekuMaskItemNamePathOtr
                       : fallback;
        case ITEM_MASK_FIERCE_DEITY:
            return TryLoadMmRawPointer(kFierceDeityMaskItemNamePath, kFierceDeityMaskItemNamePathOtr) != nullptr
                       ? kFierceDeityMaskItemNamePathOtr
                       : fallback;
        default:
            return fallback;
    }
}

extern "C" f32 OoTxMm_GetTransformMaskLedgeReachBonus(Player* player) {
    if (player == nullptr) {
        return 0.0f;
    }

    if (OoTxMm_ShouldBlockLedgeClimb(player)) {
        return 0.0f;
    }

    const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask);
    return form != nullptr ? form->ledgeReachBonus : 0.0f;
}

extern "C" f32 OoTxMm_GetTransformMaskLedgeCheckHeightBonus(Player* player) {
    if (player == nullptr) {
        return 0.0f;
    }

    if (OoTxMm_ShouldBlockLedgeClimb(player)) {
        return 0.0f;
    }

    const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask);
    return form != nullptr ? form->ledgeCheckHeightBonus : 0.0f;
}

extern "C" bool OoTxMm_ShouldBlockLedgeClimb(Player* player) {
    return player != nullptr && player->currentMask == PLAYER_MASK_GORON;
}

extern "C" bool OoTxMm_IsTransformMaskFireImmune(Player* player) {
    return player != nullptr && player->currentMask == PLAYER_MASK_GORON;
}

extern "C" bool OoTxMm_ShouldForceZoraJumpSlash(Player* player) {
    return player != nullptr && player->currentMask == PLAYER_MASK_ZORA;
}

extern "C" LinkAnimationHeader* OoTxMm_GetZoraJumpSlashAnim(Player* player, s32 phase) {
    if (player == nullptr || player->currentMask != PLAYER_MASK_ZORA || !sRuntimeState.initialized ||
        sRuntimeState.activeMask != PLAYER_MASK_ZORA) {
        return nullptr;
    }

    if (phase == PLAYER_MWA_JUMPSLASH_START || phase == PLAYER_MWA_FLIPSLASH_START) {
        return sRuntimeState.jumpAnimHeader.segment != nullptr ? &sRuntimeState.jumpAnimHeader : nullptr;
    }

    if (phase == PLAYER_MWA_JUMPSLASH_FINISH || phase == PLAYER_MWA_FLIPSLASH_FINISH) {
        if (sRuntimeState.zoraJumpKickEndAnimHeader.segment != nullptr) {
            return &sRuntimeState.zoraJumpKickEndAnimHeader;
        }
        return sRuntimeState.jumpAnimHeader.segment != nullptr ? &sRuntimeState.jumpAnimHeader : nullptr;
    }

    return nullptr;
}

extern "C" bool OoTxMm_IsZoraBoomerangAiming(Player* player) {
    return player != nullptr && player->currentMask == PLAYER_MASK_ZORA && sRuntimeState.initialized &&
           sRuntimeState.activeMask == PLAYER_MASK_ZORA && sRuntimeState.zoraBoomerangState == ZORA_BOOMERANG_AIM;
}

extern "C" f32 OoTxMm_GetTransformMaskColliderHeightScale(Player* player) {
    if (player == nullptr) {
        return 1.0f;
    }

    const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask);
    return form != nullptr ? form->colliderHeightScale : 1.0f;
}

extern "C" f32 OoTxMm_GetTransformMaskColliderRadiusScale(Player* player) {
    if (player == nullptr) {
        return 1.0f;
    }

    const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask);
    return form != nullptr ? form->colliderRadiusScale : 1.0f;
}

extern "C" f32 OoTxMm_GetTransformMaskColliderMinHeight(Player* player) {
    if (player == nullptr) {
        return 0.0f;
    }

    const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask);
    return form != nullptr ? form->colliderMinHeight : 0.0f;
}

extern "C" f32 OoTxMm_GetTransformMaskColliderMinRadius(Player* player) {
    if (player == nullptr) {
        return 0.0f;
    }

    const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask);
    return form != nullptr ? form->colliderMinRadius : 0.0f;
}

extern "C" f32 OoTxMm_GetTransformMaskColliderYShiftOffset(Player* player) {
    if (player == nullptr) {
        return 0.0f;
    }

    const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask);
    return form != nullptr ? form->colliderYShiftOffset : 0.0f;
}

extern "C" f32 OoTxMm_GetTransformMaskColliderHeight(Player* player) {
    if (player == nullptr) {
        return 0.0f;
    }

    const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask);
    return form != nullptr ? form->colliderHeight : 0.0f;
}

extern "C" f32 OoTxMm_GetTransformMaskColliderRadius(Player* player) {
    if (player == nullptr) {
        return 0.0f;
    }

    const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask);
    return form != nullptr ? form->colliderRadius : 0.0f;
}

extern "C" f32 OoTxMm_GetTransformMaskColliderYShift(Player* player) {
    if (player == nullptr) {
        return 0.0f;
    }

    const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask);
    return form != nullptr ? form->colliderYShift : 0.0f;
}

extern "C" f32 OoTxMm_GetTransformMaskCameraHeight(Player* player) {
    if (player == nullptr) {
        return 0.0f;
    }

    if (OoTxMm_IsFierceDeitySkinMode(player)) {
        return 124.0f;
    }

    if (player->currentMask != PLAYER_MASK_ZORA) {
        return 0.0f;
    }

    const bool isFastSwimming =
        sRuntimeState.zoraFastSwimActive || sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_START ||
        sRuntimeState.zoraSwimState == ZORA_SWIM_FAST || sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_EXIT ||
        sRuntimeState.zoraSwimState == ZORA_SWIM_DOLPHIN_JUMP;
    return isFastSwimming ? 68.0f : 0.0f;
}

extern "C" bool OoTxMm_ShouldSuppressJumpSfx(Player* player) {
    if (player == nullptr) {
        return false;
    }

    if (player->currentMask == PLAYER_MASK_DEKU) {
        return sRuntimeState.dekuAction == DEKU_ACTION_SPIN || sRuntimeState.dekuWaterHopAnimTimer > 0;
    }

    return player->currentMask == PLAYER_MASK_GORON &&
           (sRuntimeState.goronAction == GORON_ACTION_ROLL || sRuntimeState.goronAction == GORON_ACTION_ROLL_START ||
            sRuntimeState.goronAction == GORON_ACTION_SLAM_JUMP ||
            sRuntimeState.goronAction == GORON_ACTION_SLAM_POUND || (player->stateFlags2 & PLAYER_STATE2_HOPPING) != 0);
}

extern "C" bool OoTxMm_ShouldSuppressLinkVoiceSfx(Player* player, u16 sfxId) {
    if (player == nullptr ||
        (!OoTxMm_IsTransformationMaskPlayerMask(player->currentMask) &&
         !IsTransformAudioActiveForMask(player, PLAYER_MASK_ZORA))) {
        return false;
    }

    if (player->currentMask == PLAYER_MASK_FIERCE_DEITY) {
        return false;
    }

    switch (sfxId) {
        case NA_SE_VO_LI_AUTO_JUMP:
        case NA_SE_VO_LI_SWORD_N:
        case NA_SE_VO_LI_SWORD_L:
        case NA_SE_VO_LI_CLIMB_END:
        case NA_SE_VO_LI_HANG:
        case NA_SE_VO_LI_FALL_S:
        case NA_SE_VO_LI_FALL_L:
        case NA_SE_VO_LI_LAND_DAMAGE_S:
        case NA_SE_VO_LI_DAMAGE_S:
        case NA_SE_VO_LI_DOWN:
        case NA_SE_VO_LI_LASH:
        case NA_SE_VO_LI_PUSH:
        case NA_SE_VO_LI_SURPRISE:
        case NA_SE_VO_LI_GROAN:
        case NA_SE_VO_LI_BREATH_REST:
        case NA_SE_VO_LI_BREATH_DRINK:
            return true;
        default:
            return false;
    }
}

extern "C" bool OoTxMm_PlayTransformMaskVoiceSfx(Player* player, u16 sfxId) {
    if (player == nullptr ||
        (!OoTxMm_IsTransformationMaskPlayerMask(player->currentMask) &&
         !IsTransformAudioActiveForMask(player, PLAYER_MASK_ZORA))) {
        return false;
    }

    if (player->currentMask == PLAYER_MASK_FIERCE_DEITY) {
        return false;
    }

    if (IsTransformAudioActiveForMask(player, PLAYER_MASK_ZORA)) {
        switch (sfxId) {
            case NA_SE_VO_LI_SWORD_N:
            case NA_SE_VO_LI_LASH:
                return PlayMmZoraVoiceOneShot(0x68A0);
            case NA_SE_VO_LI_SWORD_L:
                return PlayMmZoraVoiceOneShot(0x68A1);
            case NA_SE_VO_LI_AUTO_JUMP:
                return PlayMmZoraVoiceOneShot(0x68B4);
            case NA_SE_VO_LI_CLIMB_END:
                return PlayMmZoraVoiceOneShot(0x68A4);
            case NA_SE_VO_LI_FALL_S:
                return PlayMmZoraVoiceOneShot(0x68A7);
            case NA_SE_VO_LI_FALL_L:
                return PlayMmZoraVoiceOneShot(0x68A8);
            case NA_SE_VO_LI_DAMAGE_S:
            case NA_SE_VO_LI_LAND_DAMAGE_S:
                return PlayMmZoraVoiceOneShot(0x68A5);
            case NA_SE_VO_LI_BREATH_REST:
            case NA_SE_VO_LI_BREATH_DRINK:
                return PlayMmZoraVoiceOneShot(0x68A9);
            default:
                return false;
        }
    }

    if (player->currentMask == PLAYER_MASK_DEKU) {
        switch (sfxId) {
            case NA_SE_VO_LI_SWORD_N:
            case NA_SE_VO_LI_LASH:
            case NA_SE_VO_LI_SWORD_L:
            case NA_SE_VO_LI_AUTO_JUMP:
            case NA_SE_VO_LI_CLIMB_END:
            case NA_SE_VO_LI_FALL_S:
            case NA_SE_VO_LI_FALL_L:
            case NA_SE_VO_LI_DAMAGE_S:
            case NA_SE_VO_LI_LAND_DAMAGE_S:
            case NA_SE_VO_LI_BREATH_REST:
            case NA_SE_VO_LI_BREATH_DRINK:
                return true;
            default:
                return false;
        }
    }

    if (player->currentMask != PLAYER_MASK_GORON) {
        return false;
    }

    switch (sfxId) {
        case NA_SE_VO_LI_SWORD_N:
        case NA_SE_VO_LI_LASH:
            return PlayMmGoronVoiceOneShot(MM_GORON_VOICE_ATTACK_N);
        case NA_SE_VO_LI_SWORD_L:
            return PlayMmGoronVoiceOneShot(MM_GORON_VOICE_ATTACK_L);
        case NA_SE_VO_LI_AUTO_JUMP:
            return PlayMmGoronVoiceOneShot(MM_GORON_VOICE_AUTO_JUMP);
        case NA_SE_VO_LI_CLIMB_END:
            return PlayMmGoronVoiceOneShot(MM_GORON_VOICE_CLIMB_END);
        default:
            return false;
    }
}

extern "C" bool OoTxMm_PlayTransformMaskStepSfx(Player* player, f32 pitchAdjustment) {
    if (player == nullptr) {
        return false;
    }

    if (player->currentMask == PLAYER_MASK_ZORA) {
        if (sRuntimeState.zoraFastSwimActive || sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_START ||
            sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_EXIT ||
            sRuntimeState.zoraSwimState == ZORA_SWIM_DOLPHIN_JUMP) {
            return true;
        }
        PlayMmZoraStepSfx(player, pitchAdjustment);
        return true;
    }

    if (player->currentMask == PLAYER_MASK_DEKU) {
        if (sRuntimeState.dekuAction == DEKU_ACTION_SPIN || sRuntimeState.dekuWaterHopAnimTimer > 0) {
            return true;
        }
        PlayMmDekuStepSfx(player, pitchAdjustment);
        return true;
    }

    if (player->currentMask != PLAYER_MASK_GORON) {
        return false;
    }

    if (sRuntimeState.goronAction == GORON_ACTION_ROLL || sRuntimeState.goronAction == GORON_ACTION_ROLL_START ||
        sRuntimeState.goronAction == GORON_ACTION_SLAM_JUMP || sRuntimeState.goronAction == GORON_ACTION_SLAM_POUND) {
        return true;
    }

    (void)pitchAdjustment;
    return false;
}

extern "C" bool OoTxMm_PlayTransformMaskGeneralSfx(Player* player, u16 sfxId) {
    if (player == nullptr ||
        (!IsTransformAudioActiveForMask(player, PLAYER_MASK_ZORA) &&
         !IsTransformAudioActiveForMask(player, PLAYER_MASK_DEKU))) {
        return false;
    }

    if (sfxId == NA_SE_PL_FACE_UP) {
        if (IsTransformAudioActiveForMask(player, PLAYER_MASK_DEKU)) {
            return true;
        }
        return PlayMmZoraVoiceOneShot(0x68A9);
    }

    return false;
}

void DrawZoraBarrier(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || sRuntimeState.zoraBarrierDL == nullptr ||
        sRuntimeState.zoraBarrierIntensity <= 0) {
        return;
    }

    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    const bool swimming = (player->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0 && !IsZoraOnWaterFloor(player);
    const f32 scale = sRuntimeState.zoraBarrierIntensity * ((swimming ? 0.1f : 0.05f) / 51.0f);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Push();

    if (swimming) {
        const f32 yAdj = (Math_CosS(sRuntimeState.zoraSwimPitch) - 1.0f) * 2.0f;
        Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y + 40.0f + yAdj,
                         player->actor.world.pos.z, MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD(sRuntimeState.zoraSwimPitch), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(sRuntimeState.zoraSwimRollSmoothed), MTXMODE_APPLY);
        Matrix_RotateX(M_PI, MTXMODE_APPLY);
        Matrix_Translate(0.0f, 0.0f, -40.0f, MTXMODE_APPLY);
    } else {
        Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y + 40.0f, player->actor.world.pos.z,
                         MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD(static_cast<s16>(-0x4000)), MTXMODE_APPLY);
        Matrix_Translate(0.0f, 0.0f, -18.0f, MTXMODE_APPLY);
    }

    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)gCullBackDList);
    const u32 frames = play->gameplayFrames;
    gSPSegment(POLY_XLU_DISP++, 0x0A,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, -(s32)frames, (s32)(frames * 20), 0x20, 0x40, 1,
                                           -(s32)(frames * 2), (s32)(frames * 10), 0x20, 0x40));
    gSPSegment(POLY_XLU_DISP++, 0x0B,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, (s32)(frames * 3), (s32)(frames * 20), 0x20, 0x40, 1,
                                           -(s32)(frames * 12), (s32)(frames * 10), 0x40, 0x20));
    gDPSetEnvColor(POLY_XLU_DISP++, 100, 200, 255, static_cast<u8>(sRuntimeState.zoraBarrierIntensity));
    gSPDisplayList(POLY_XLU_DISP++, static_cast<Gfx*>(sRuntimeState.zoraBarrierDL));

    Matrix_Pop();
}

extern "C" void OoTxMm_MixTransformMaskAudioInto(s16* outBuf, u32 numSamples) {
    if (outBuf == nullptr || numSamples == 0) {
        return;
    }

    const f32 masterVol = static_cast<f32>(CVarGetInteger("gSettings.Volume.Master", 40)) / 100.0f;
    for (auto& sound : sMmVoicePcmSounds) {
        if (!sound.active || sound.pcmData == nullptr || sound.pcmLength < 2) {
            continue;
        }

        for (u32 i = 0; i < numSamples; ++i) {
            if (sound.envVolume < 1.0f && !sound.release) {
                sound.envVolume = std::min(1.0f, sound.envVolume + (1.0f / 64.0f));
            }
            if (!sound.release && sound.envReleaseAt > 0.0f && static_cast<f32>(i) >= sound.envReleaseAt) {
                sound.release = true;
            }
            if (sound.release) {
                sound.envVolume -= (1.0f / 320.0f);
                if (sound.envVolume <= 0.0f) {
                    StopMmVoicePcmSlot(sound);
                    break;
                }
            }

            u32 pos = static_cast<u32>(sound.pcmPosition);
            if (pos >= sound.pcmLength - 2) {
                if (sound.loop) {
                    sound.pcmPosition = 0.0f;
                    pos = 0;
                } else {
                    StopMmVoicePcmSlot(sound);
                    break;
                }
            }

            const f32 frac = sound.pcmPosition - static_cast<f32>(pos);
            const f32 sample = sound.pcmData[pos] * (1.0f - frac) + sound.pcmData[pos + 1] * frac;
            const f32 scaled = sample * sound.volume * sound.envVolume * masterVol * kMmGoronVoicePcmVolumeScale;

            s32 left = outBuf[i * 2] + static_cast<s32>(scaled);
            s32 right = outBuf[i * 2 + 1] + static_cast<s32>(scaled);
            outBuf[i * 2] = static_cast<s16>(CLAMP(left, -32768, 32767));
            outBuf[i * 2 + 1] = static_cast<s16>(CLAMP(right, -32768, 32767));
            sound.pcmPosition += sound.advance;
        }

        if (sound.active && sound.envReleaseAt > 0.0f) {
            sound.envReleaseAt -= static_cast<f32>(numSamples);
        }
    }
}

extern "C" void OoTxMm_DrawTransformMaskPlayer(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || !OoTxMm_ShouldOverridePlayerDraw(player)) {
        return;
    }

    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    const TransformMaskFormDefinition* form = GetFormDefinition(player->currentMask);

    const bool isGettingItemPose = IsGettingItemPose(player);
    const bool isOotActionPose = sRuntimeState.motionState == TRANSFORM_MASK_MOTION_OOT_ACTION;
    if (isGettingItemPose || isOotActionPose) {
        CopyOotPlayerPoseToTransformSkeleton(player);
    } else {
        NeutralizeDekuSidehopRoot(player);
        player->skelAnime.jointTable[0].x = sRuntimeState.skelAnime.jointTable[0].x;
        player->skelAnime.jointTable[0].y = sRuntimeState.skelAnime.jointTable[0].y;
        player->skelAnime.jointTable[0].z = sRuntimeState.skelAnime.jointTable[0].z;
        player->skelAnime.jointTable[1].x = sRuntimeState.skelAnime.jointTable[1].x;
        player->skelAnime.jointTable[1].y = sRuntimeState.skelAnime.jointTable[1].y;
        player->skelAnime.jointTable[1].z = sRuntimeState.skelAnime.jointTable[1].z;
    }
    bool isGoronRolling = player->currentMask == PLAYER_MASK_GORON &&
                           (sRuntimeState.goronAction == GORON_ACTION_ROLL ||
                            sRuntimeState.goronAction == GORON_ACTION_SLAM_JUMP ||
                            sRuntimeState.goronAction == GORON_ACTION_SLAM_POUND);
    const bool isZoraFastSwimDraw =
        player->currentMask == PLAYER_MASK_ZORA &&
        (sRuntimeState.zoraFastSwimActive || sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_START ||
         sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_EXIT || sRuntimeState.zoraSwimState == ZORA_SWIM_DOLPHIN_JUMP);
    player->actor.shape.shadowDraw = isGoronRolling ? ActorShadow_DrawCircle : ActorShadow_DrawFeet;
    Matrix_Push();
    if (form != nullptr && form->drawScale != 1.0f && !isGoronRolling) {
        Matrix_Scale(form->drawScale, form->drawScale, form->drawScale, MTXMODE_APPLY);
    }
    if (form != nullptr && form->drawYOffset != 0.0f && !isGoronRolling && !isZoraFastSwimDraw) {
        Matrix_Translate(0.0f, form->drawYOffset, 0.0f, MTXMODE_APPLY);
    }
    if (player->currentMask == PLAYER_MASK_DEKU && sRuntimeState.motionState == TRANSFORM_MASK_MOTION_CLIMB &&
        (player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) == 0 && form != nullptr) {
        f32 ledgeVisualOffset = -form->colliderHeight;
        if ((player->stateFlags1 & PLAYER_STATE1_CLIMBING_LEDGE) != 0 && sRuntimeState.climbLoopAnimHeader.segment != nullptr) {
            const f32 lastFrame = std::max<f32>(1.0f, static_cast<f32>(Animation_GetLastFrame(&sRuntimeState.climbLoopAnimHeader)));
            const f32 climbT = CLAMP(sRuntimeState.skelAnime.curFrame / lastFrame, 0.0f, 1.0f);
            ledgeVisualOffset *= (1.0f - climbT);
        }
        Matrix_Translate(0.0f, ledgeVisualOffset, 0.0f, MTXMODE_APPLY);
    }
    if (player->currentMask == PLAYER_MASK_DEKU && sRuntimeState.dekuAction == DEKU_ACTION_SPIN) {
        Matrix_Translate(0.0f, kDekuSpinDrawYOffset, 0.0f, MTXMODE_APPLY);
        Matrix_RotateY(BINANG_TO_RAD(static_cast<s16>(sRuntimeState.dekuSpinRotAccum)), MTXMODE_APPLY);
    }
    const bool allowTurnTilt = !isGoronRolling && !isZoraFastSwimDraw &&
                               sRuntimeState.dekuAction != DEKU_ACTION_BUBBLE_AIM &&
                               sRuntimeState.dekuAction != DEKU_ACTION_SPIN &&
                               sRuntimeState.motionState != TRANSFORM_MASK_MOTION_DEFEND &&
                               sRuntimeState.motionState != TRANSFORM_MASK_MOTION_GET_ITEM &&
                               sRuntimeState.motionState != TRANSFORM_MASK_MOTION_OOT_ACTION;
    if (allowTurnTilt) {
        s16 yawDelta = 0;
        if (sRuntimeState.visualTurnTiltYawValid) {
            yawDelta = player->actor.shape.rot.y - sRuntimeState.visualTurnTiltLastYaw;
        }
        sRuntimeState.visualTurnTiltLastYaw = player->actor.shape.rot.y;
        sRuntimeState.visualTurnTiltYawValid = true;
        const f32 speedFactor = CLAMP(fabsf(player->actor.speedXZ) / 10.0f, 0.0f, 1.0f);
        const s16 targetTilt = static_cast<s16>(CLAMP(static_cast<s32>(-yawDelta * 3), -0x1000, 0x1000) * speedFactor);
        Math_SmoothStepToS(&sRuntimeState.visualTurnTilt, targetTilt, 3, 0x380, 0x20);
        if (sRuntimeState.visualTurnTilt != 0) {
            Matrix_RotateZ(BINANG_TO_RAD(sRuntimeState.visualTurnTilt), MTXMODE_APPLY);
        }
    } else {
        sRuntimeState.visualTurnTiltYawValid = false;
        Math_StepToS(&sRuntimeState.visualTurnTilt, 0, 0x300);
    }
    if (form != nullptr && IsUsingOcarina(player) && form->ocarinaDrawYOffset != 0.0f) {
        Matrix_Translate(0.0f, form->ocarinaDrawYOffset, 0.0f, MTXMODE_APPLY);
    }
    if (player->currentMask == PLAYER_MASK_GORON && sRuntimeState.motionState == TRANSFORM_MASK_MOTION_DEFEND &&
        sRuntimeState.goronShieldSkelReady) {
        player->stateFlags1 |= PLAYER_STATE1_SHIELDING;
        func_8002EBCC(&player->actor, play, 0);
        func_8002ED80(&player->actor, play, 0);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        SkelAnime_DrawFlexOpa(play, sRuntimeState.goronShieldSkelAnime.skeleton,
                              sRuntimeState.goronShieldSkelAnime.jointTable,
                              sRuntimeState.goronShieldSkelAnime.dListCount, NULL, NULL, &player->actor);
        SyncTransformMaskShadowFeet(player);
        Matrix_Pop();
        return;
    }

    if (isGoronRolling && sRuntimeState.goronRolledUpDL != nullptr) {
        // Match OoT's native Goron rolling draw more closely: rely on the actor basis that is already active
        // and use the dedicated rolling display list rather than the curled standing one.
        static Color_RGB8 sGoronRollColorNormal = { 255, 255, 255 };
        const f32 speedT = CLAMP(sRuntimeState.goronRollSpeed / 18.0f, 0.0f, 1.0f);
        const f32 bounce = fabsf(sinf(sRuntimeState.goronRollBouncePhase)) * speedT;
        Matrix_Translate(0.0f, bounce * 3.0f, 0.0f, MTXMODE_APPLY);
        Matrix_RotateZYX(player->actor.shape.rot.x, 0, player->actor.shape.rot.z, MTXMODE_APPLY);
        // MM mirrors MM/2Ship's ball squash/stretch: Y stretches, X compresses,
        // and Z follows the larger of the two to keep the rolling silhouette alive.
        {
            const f32 squash = sRuntimeState.goronRollSquash;
            const f32 scaleY = squash + 1.0f;
            const f32 scaleX = 1.0f - (squash * 0.5f);
            const f32 scaleZ = std::max(scaleY, scaleX);
            Matrix_Scale(kGoronRollBodyScaleX * scaleX * (1.0f + (bounce * 0.035f)),
                         kGoronRollBodyScaleY * scaleY * (1.0f - (bounce * 0.025f)),
                         kGoronRollBodyScaleZ * scaleZ * (1.0f + (bounce * 0.035f)), MTXMODE_APPLY);
        }
        func_8002EBCC(&player->actor, play, 0);
        func_8002ED80(&player->actor, play, 0);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        const Color_RGB8 rollColor = OoTxMm_LerpColor(sGoronRollColorNormal, { 80, 80, 200 }, sRuntimeState.goronRollColorLerp);
        gDPSetEnvColor(POLY_OPA_DISP++, rollColor.r, rollColor.g, rollColor.b, 255);
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, static_cast<Gfx*>(sRuntimeState.goronRolledUpDL));

        if (sRuntimeState.goronSpikeMode && !IsGoronSpikeShellVisible() &&
            (sRuntimeState.goronSpikeEffectDL1 != nullptr || sRuntimeState.goronSpikeEffectDL2 != nullptr)) {
            const f32 pulse = 1.0f + (0.04f * sinf((play->state.frames & 0x1F) * (M_PI / 16.0f)));

            Matrix_Push();
            Matrix_Scale(pulse, pulse, pulse, MTXMODE_APPLY);
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gDPSetEnvColor(POLY_XLU_DISP++, 155, 0, 0, 255);
            if (sRuntimeState.goronSpikeEffectDL1 != nullptr) {
                gSPDisplayList(POLY_XLU_DISP++, static_cast<Gfx*>(sRuntimeState.goronSpikeEffectDL1));
            }
            if (sRuntimeState.goronSpikeEffectDL2 != nullptr) {
                gSPDisplayList(POLY_XLU_DISP++, static_cast<Gfx*>(sRuntimeState.goronSpikeEffectDL2));
            }
            Matrix_Pop();
        }

        if (IsGoronSpikeShellVisible() && sRuntimeState.goronPoweredRollDLResource != nullptr &&
            !sRuntimeState.goronPoweredRollDLResource->Instructions.empty()) {
            Matrix_Push();
            Matrix_Scale(1.0f, 1.0f, 1.0f, MTXMODE_APPLY);
            Gfx_SetupDL_25Opa(play->state.gfxCtx);
            gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++,
                           reinterpret_cast<Gfx*>(sRuntimeState.goronPoweredRollDLResource->Instructions.data()));
            Matrix_Pop();
        }

        if (IsGoronSpikeShellVisible() &&
            (sRuntimeState.goronPoweredRollDLResource == nullptr || sRuntimeState.goronPoweredRollDLResource->Instructions.empty()) &&
            sRuntimeState.goronSpikeShellDL != nullptr) {
            Matrix_Push();
            Matrix_Translate(0.0f, 4.0f, 0.0f, MTXMODE_APPLY);
            Matrix_Scale(0.9f, 0.9f, 0.9f, MTXMODE_APPLY);

            Matrix_Scale(0.72f, 0.72f, 0.72f, MTXMODE_APPLY);
            Gfx_SetupDL_25Opa(play->state.gfxCtx);
            gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, static_cast<Gfx*>(sRuntimeState.goronSpikeShellDL));
            Matrix_Pop();
        }
        SyncTransformMaskShadowFeet(player);
        Matrix_Pop();
        return;
    }

    SkelAnime_DrawFlexOpa(play, sRuntimeState.skelAnime.skeleton, sRuntimeState.skelAnime.jointTable,
                          sRuntimeState.skelAnime.dListCount,
                          [](PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) -> s32 {
                              Player* player = static_cast<Player*>(thisx);
                              (void)play;

                              if (player == nullptr) {
                                  return false;
                              }

                              if (player->currentMask == PLAYER_MASK_FIERCE_DEITY && dList != nullptr) {
                                  if (limbIndex == PLAYER_LIMB_L_HAND) {
                                      switch (player->leftHandType) {
                                          case PLAYER_MODELTYPE_LH_SWORD:
                                          case PLAYER_MODELTYPE_LH_SWORD_2:
                                          case PLAYER_MODELTYPE_LH_BGS:
                                              if (sRuntimeState.fdLeftHandSwordDL != nullptr) {
                                                  *dList = static_cast<Gfx*>(sRuntimeState.fdLeftHandSwordDL);
                                              }
                                              break;
                                          case PLAYER_MODELTYPE_LH_BOTTLE:
                                              if (sRuntimeState.fdLeftHandBottleDL != nullptr) {
                                                  *dList = static_cast<Gfx*>(sRuntimeState.fdLeftHandBottleDL);
                                              }
                                              break;
                                          case PLAYER_MODELTYPE_LH_HAMMER:
                                              break;
                                          default:
                                              if (sRuntimeState.fdLeftHandEmptyDL != nullptr) {
                                                  *dList = static_cast<Gfx*>(sRuntimeState.fdLeftHandEmptyDL);
                                              }
                                              break;
                                      }
                                  } else if (limbIndex == PLAYER_LIMB_R_HAND) {
                                      if (sRuntimeState.fdRightHandEmptyDL != nullptr) {
                                          *dList = static_cast<Gfx*>(sRuntimeState.fdRightHandEmptyDL);
                                      }
                                  } else if (limbIndex == PLAYER_LIMB_SHEATH) {
                                      *dList = nullptr;
                                  }
                              }

                              if (player->currentMask == PLAYER_MASK_ZORA &&
                                  (sRuntimeState.zoraFastSwimActive ||
                                   sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_START ||
                                   sRuntimeState.zoraSwimState == ZORA_SWIM_FAST_EXIT ||
                                   sRuntimeState.zoraSwimState == ZORA_SWIM_DOLPHIN_JUMP) &&
                                  limbIndex == 1 && pos != nullptr && rot != nullptr) {
                                  Vec3f origPos = *pos;
                                  Vec3s origRot = *rot;
                                  const f32 yAdj = (Math_CosS(sRuntimeState.zoraSwimPitch) - 1.0f) * 200.0f;

                                  Matrix_Translate(origPos.x, origPos.y + yAdj, origPos.z, MTXMODE_APPLY);
                                  Matrix_RotateX(BINANG_TO_RAD(sRuntimeState.zoraSwimPitch), MTXMODE_APPLY);
                                  Matrix_RotateZ(BINANG_TO_RAD(sRuntimeState.zoraSwimRollSmoothed), MTXMODE_APPLY);
                                  Matrix_RotateZYX(origRot.x, origRot.y, origRot.z, MTXMODE_APPLY);

                                  pos->x = 0.0f;
                                  pos->y = 0.0f;
                                  pos->z = 0.0f;
                                  rot->x = 0;
                                  rot->y = 0;
                                  rot->z = 0;
                              }

                              if (player->currentMask == PLAYER_MASK_ZORA &&
                                  sRuntimeState.zoraBoomerangState == ZORA_BOOMERANG_AIM &&
                                  limbIndex == PLAYER_LIMB_UPPER) {
                                  if (player->upperLimbRot.y != 0) {
                                      Matrix_RotateY(player->upperLimbRot.y * (M_PI / 0x8000), MTXMODE_APPLY);
                                  }
                                  if (player->upperLimbRot.x != 0) {
                                      Matrix_RotateX(player->upperLimbRot.x * (M_PI / 0x8000), MTXMODE_APPLY);
                                  }
                              }

                              if (player->currentMask == PLAYER_MASK_DEKU &&
                                  (player->stateFlags1 & PLAYER_STATE1_SHIELDING) != 0 &&
                                  limbIndex == PLAYER_LIMB_UPPER) {
                                  if (player->upperLimbRot.y != 0) {
                                      Matrix_RotateY(player->upperLimbRot.y * (M_PI / 0x8000), MTXMODE_APPLY);
                                  }
                                  if (player->upperLimbRot.x != 0) {
                                      Matrix_RotateX(player->upperLimbRot.x * (M_PI / 0x8000), MTXMODE_APPLY);
                                  }
                              }

                              return false;
                          },
                          [](PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
                              Player* player = static_cast<Player*>(thisx);
                              (void)rot;
                              (void)dList;
                              if (player == nullptr) {
                                  return;
                              }

                              GraphicsContext* __gfxCtx = play->state.gfxCtx;
                              if (limbIndex > 0 && limbIndex < PLAYER_LIMB_MAX) {
                                  Vec3f limbPos = {};
                                  Matrix_MultVec3f(&limbPos, &limbPos);
                                  if (limbIndex == PLAYER_LIMB_L_HAND) {
                                      player->leftHandPos = limbPos;
                                      player->bodyPartsPos[PLAYER_BODYPART_L_HAND] = limbPos;
                                      SyncTransformHeldActorToHand(player);
                                  } else if (limbIndex == PLAYER_LIMB_R_HAND) {
                                      player->bodyPartsPos[PLAYER_BODYPART_R_HAND] = limbPos;
                                  } else if (limbIndex == PLAYER_LIMB_HEAD) {
                                      player->bodyPartsPos[PLAYER_BODYPART_HEAD] = limbPos;
                                      player->actor.focus.pos = limbPos;
                                  } else if (limbIndex == PLAYER_LIMB_WAIST) {
                                      player->bodyPartsPos[PLAYER_BODYPART_WAIST] = limbPos;
                                  } else if (limbIndex == PLAYER_LIMB_L_FOOT) {
                                      player->bodyPartsPos[PLAYER_BODYPART_L_FOOT] = limbPos;
                                  } else if (limbIndex == PLAYER_LIMB_R_FOOT) {
                                      player->bodyPartsPos[PLAYER_BODYPART_R_FOOT] = limbPos;
                                  }

                                  if (limbIndex == PLAYER_LIMB_L_FOOT || limbIndex == PLAYER_LIMB_R_FOOT) {
                                      Vec3f footOffset = {};
                                      Actor_SetFeetPos(&player->actor, limbIndex, PLAYER_LIMB_L_FOOT, &footOffset,
                                                       PLAYER_LIMB_R_FOOT, &footOffset);
                                  }
                              }

                              if (player->currentMask == PLAYER_MASK_DEKU && limbIndex == PLAYER_LIMB_UPPER &&
                                  sRuntimeState.dekuShieldDL != nullptr &&
                                  (sRuntimeState.motionState == TRANSFORM_MASK_MOTION_DEFEND ||
                                   (player->stateFlags1 & PLAYER_STATE1_SHIELDING) != 0)) {
                                  const f32 curFrame = sRuntimeState.skelAnime.curFrame;
                                  f32 scaleX = 1.0f;
                                  f32 scaleY = 1.0f;
                                  f32 scaleZ = 1.0f;

                                  if (curFrame <= 0.0f) {
                                      scaleX = 0.0f;
                                      scaleY = 0.0f;
                                      scaleZ = 0.0f;
                                  } else if (curFrame < 2.0f) {
                                      const f32 t = curFrame * 0.5f;
                                      scaleX = 0.80f * t;
                                      scaleY = 1.10f * t;
                                      scaleZ = 0.80f * t;
                                  } else if (curFrame < 3.0f) {
                                      const f32 t = curFrame - 2.0f;
                                      scaleX = 0.80f + (0.20f * t);
                                      scaleY = 1.10f - (0.10f * t);
                                      scaleZ = 0.80f + (0.20f * t);
                                  }

                                  if (scaleX > 0.001f) {
                                      Matrix_Push();
                                      Matrix_RotateZ(M_PI / 2.0f, MTXMODE_APPLY);
                                      Matrix_Scale(scaleX, scaleY, scaleZ, MTXMODE_APPLY);
                                      Gfx_SetupDL_25Opa(play->state.gfxCtx);
                                      gSPMatrix(POLY_OPA_DISP++,
                                                Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                                                G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                                      gSPDisplayList(POLY_OPA_DISP++, static_cast<Gfx*>(sRuntimeState.dekuShieldDL));
                                      Matrix_Pop();
                                  }
                              }

                              if (player->currentMask == PLAYER_MASK_ZORA &&
                                  (limbIndex == PLAYER_LIMB_L_FOREARM || limbIndex == PLAYER_LIMB_R_FOREARM)) {
                                  void* finDL = limbIndex == PLAYER_LIMB_L_FOREARM ? sRuntimeState.zoraLeftFinDL
                                                                                   : sRuntimeState.zoraRightFinDL;
                                  const bool finsThrown =
                                      sRuntimeState.zoraBoomerangState == ZORA_BOOMERANG_THROW ||
                                      sRuntimeState.zoraBoomerangState == ZORA_BOOMERANG_FLIGHT;
                                  if (finDL != nullptr && !finsThrown) {
                                      const bool fullFinScale =
                                          sRuntimeState.goronAction == GORON_ACTION_PUNCH ||
                                          (player->stateFlags1 & PLAYER_STATE1_SHIELDING) != 0 ||
                                          sRuntimeState.zoraBoomerangState == ZORA_BOOMERANG_AIM ||
                                          player->heldItemAction == PLAYER_IA_BOOMERANG;
                                      const f32 scaleX = fullFinScale ? 1.0f : 0.6f;
                                      const f32 scaleY = fullFinScale ? 1.0f : 0.9f;
                                      const f32 scaleZ = fullFinScale ? 1.0f : 1.05f;

                                      Matrix_Push();
                                      Matrix_Scale(scaleX, scaleY, scaleZ, MTXMODE_APPLY);
                                      Gfx_SetupDL_25Opa(play->state.gfxCtx);
                                      gSPMatrix(POLY_OPA_DISP++,
                                                Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                                                G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                                      gSPDisplayList(POLY_OPA_DISP++, static_cast<Gfx*>(finDL));
                                      Matrix_Pop();
                                  }
                              }

                              if (player->currentMask == PLAYER_MASK_GORON && sRuntimeState.goronPunchEffectDL != nullptr &&
                                  sRuntimeState.goronAction == GORON_ACTION_PUNCH && IsGoronPunchStrikeActive()) {
                                  const u8 punchStep = sRuntimeState.goronPunchIndex;
                                  const f32 curFrame = sRuntimeState.skelAnime.curFrame;
                                  const f32 hitStart = 5.0f;
                                  const f32 hitEnd = punchStep == 0 ? 11.0f : (punchStep == 1 ? 15.0f : 16.0f);
                                  const bool isHitFrame = curFrame >= hitStart && curFrame <= hitEnd;
                                  const bool isTargetLimb =
                                      (punchStep == 0 && limbIndex == PLAYER_LIMB_L_HAND) ||
                                      (punchStep == 1 && limbIndex == PLAYER_LIMB_R_HAND) ||
                                      (punchStep == 2 && limbIndex == PLAYER_LIMB_WAIST);

                                  if (isHitFrame && isTargetLimb) {
                                      const f32 progress = CLAMP((curFrame - hitStart) / (hitEnd - hitStart + 1.0f), 0.0f, 1.0f);
                                      const u8 alpha = static_cast<u8>(200.0f * (1.0f - (progress * 0.5f)));
                                      Gfx_SetupDL_25Xlu(play->state.gfxCtx);
                                      gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, alpha);
                                      gSPMatrix(POLY_XLU_DISP++,
                                                Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                                                G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                                      gSPDisplayList(POLY_XLU_DISP++, static_cast<Gfx*>(sRuntimeState.goronPunchEffectDL));
                                  }
                              }

                              if (IsUsingOcarina(player) && sRuntimeState.ocarinaDLCount != 0 &&
                                  limbIndex == sRuntimeState.ocarinaLimbIndex) {
                                  Matrix_Push();
                                  if (const OcarinaInstrumentTransform* instrumentTransform =
                                          GetOcarinaInstrumentTransform(player->currentMask);
                                      instrumentTransform != nullptr) {
                                      Matrix_Translate(instrumentTransform->translateX, instrumentTransform->translateY,
                                                       instrumentTransform->translateZ, MTXMODE_APPLY);
                                      Matrix_RotateX(instrumentTransform->rotateX, MTXMODE_APPLY);
                                      Matrix_RotateY(instrumentTransform->rotateY, MTXMODE_APPLY);
                                      Matrix_RotateZ(instrumentTransform->rotateZ, MTXMODE_APPLY);
                                  }
                                  gSPMatrix(POLY_OPA_DISP++,
                                            Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                                            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                                  for (size_t i = 0; i < sRuntimeState.ocarinaDLCount; ++i) {
                                      if (sRuntimeState.ocarinaDLs[i] != nullptr) {
                                          gSPDisplayList(POLY_OPA_DISP++,
                                                         static_cast<Gfx*>(sRuntimeState.ocarinaDLs[i]));
                                      }
                                  }
                                  Matrix_Pop();
                              }
                          },
                          player);
    Matrix_Pop();
    if (player->currentMask == PLAYER_MASK_ZORA && sRuntimeState.zoraBarrierIntensity > 0) {
        DrawZoraBarrier(play, player);
    }
}

extern "C" void OoTxMm_DrawTransformMaskOverlays(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || !sRuntimeState.initialized) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    if (sRuntimeState.transformFlashTimer > 0) {
        const u8 alpha = static_cast<u8>(CLAMP(sRuntimeState.transformFlashTimer * 64, 0, 255));
        Environment_FillScreen(play->state.gfxCtx, 255, 255, 255, alpha, FILL_SCREEN_XLU);
    }

    if (player->currentMask == PLAYER_MASK_DEKU && sRuntimeState.dekuAction == DEKU_ACTION_BUBBLE_AIM &&
        sRuntimeState.dekuBubbleCharge > 0.25f) {
        const Vec3f bubblePos = GetDekuBubbleAimPreviewPos(play, player);

        Gfx* effectBubbleDL = ResourceMgr_LoadGfxByName(gEffBubbleDL);
        char* effectBubbleTex = ResourceMgr_LoadTexOrDListByName(gEffBubble1Tex);
        if (effectBubbleDL != nullptr && effectBubbleTex != nullptr) {
            Matrix_Push();
            Matrix_Translate(bubblePos.x, bubblePos.y, bubblePos.z, MTXMODE_NEW);
            const f32 chargeRatio = CLAMP(sRuntimeState.dekuBubbleCharge / kDekuBubbleMaxCharge, 0.0f, 1.0f);
            const f32 bubbleScale = 0.30f + (chargeRatio * 2.25f);
            Matrix_Scale(bubbleScale, bubbleScale, bubbleScale, MTXMODE_APPLY);
            Gfx_SetupDL_25Opa(play->state.gfxCtx);
            gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
            gDPSetEnvColor(POLY_OPA_DISP++, 150, 150, 150, 0);
            gSPSegment(POLY_OPA_DISP++, 0x08, reinterpret_cast<uintptr_t>(effectBubbleTex));
            gSPDisplayList(POLY_OPA_DISP++, effectBubbleDL);
            Matrix_Pop();
        } else if ((play->state.frames & 3) == 0) {
            Vec3f fallbackBubblePos = bubblePos;
            EffectSsBubble_Spawn(play, &fallbackBubblePos, 0.0f, 0.78125f, 1.5625f, 0.0203125f);
        }
    }

    if (player->currentMask == PLAYER_MASK_DEKU && sRuntimeState.dekuBubble.active) {
        void* bubbleDL =
            sRuntimeState.dekuBubble.state == 0 ? sRuntimeState.dekuBubbleStillDL : sRuntimeState.dekuBubbleMoveDL;
        if (bubbleDL != nullptr) {
            Matrix_Push();
            Matrix_Translate(sRuntimeState.dekuBubble.pos.x, sRuntimeState.dekuBubble.pos.y,
                             sRuntimeState.dekuBubble.pos.z, MTXMODE_NEW);
            Matrix_RotateZYX(sRuntimeState.dekuBubble.rotX, sRuntimeState.dekuBubble.rotY, 0, MTXMODE_APPLY);
            Matrix_Translate(0.0f, 0.0f, 42.0f, MTXMODE_APPLY);
            const f32 bubbleScale = sRuntimeState.dekuBubble.scale * 0.001875f;
            Matrix_Scale(bubbleScale, bubbleScale, bubbleScale, MTXMODE_APPLY);
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gDPSetEnvColor(POLY_XLU_DISP++, 170, 220, 255, 160);
            if (sRuntimeState.dekuBubbleSetupDL != nullptr) {
                gSPDisplayList(POLY_XLU_DISP++, static_cast<Gfx*>(sRuntimeState.dekuBubbleSetupDL));
            }
            gSPDisplayList(POLY_XLU_DISP++, static_cast<Gfx*>(bubbleDL));
            Matrix_Pop();
        } else if ((play->state.frames & 3) == 0) {
            EffectSsBubble_Spawn(play, &sRuntimeState.dekuBubble.pos, 0.0f, 0.78125f, 1.5625f, 0.0203125f);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void* OoTxMm_GetZoraBoomerangDisplayList(s16 params) {
    if (params == 1) {
        return sRuntimeState.zoraLeftFinDL;
    }
    if (params == 2) {
        return sRuntimeState.zoraRightFinDL;
    }
    return nullptr;
}

extern "C" void* OoTxMm_GetFierceDeitySwordBeamDisplayList(PlayState* play) {
    (void)play;

    if (sRuntimeState.fdSwordBeamDL != nullptr) {
        return sRuntimeState.fdSwordBeamDL;
    }

    sRuntimeState.fdSwordBeamDLResource =
        TryLoadMmDisplayListResource("objects/gameplay_keep/gSwordBeamDL", "__OTR__objects/gameplay_keep/gSwordBeamDL");
    sRuntimeState.fdSwordBeamDL =
        sRuntimeState.fdSwordBeamDLResource != nullptr && !sRuntimeState.fdSwordBeamDLResource->Instructions.empty()
            ? reinterpret_cast<Gfx*>(sRuntimeState.fdSwordBeamDLResource->Instructions.data())
            : TryLoadMmRawPointer("objects/gameplay_keep/gSwordBeamDL", "__OTR__objects/gameplay_keep/gSwordBeamDL");

    return sRuntimeState.fdSwordBeamDL;
}
