#pragma once

#include "libultraship/libultra/types.h"
#include "z64animation.h"

typedef struct PlayState PlayState;
typedef struct Player Player;

#ifdef __cplusplus
extern "C" {
#endif

bool OoTxMm_IsMaskSelectItem(s16 itemId);
s32 OoTxMm_GetMaskCycleIndex(s16 itemId);
s16 OoTxMm_GetMaskCycleItem(s32 index);
s16 OoTxMm_GetMaskCyclePrevItem(s16 itemId);
s16 OoTxMm_GetMaskCycleNextItem(s16 itemId);
s16 OoTxMm_GetOwnedMaskCyclePrevItem(s16 itemId);
s16 OoTxMm_GetOwnedMaskCycleNextItem(s16 itemId);
u8 OoTxMm_ItemIdToPlayerMask(s16 itemId);
s16 OoTxMm_PlayerMaskToItemId(u8 playerMask);
bool OoTxMm_IsTransformationMaskPlayerMask(u8 playerMask);
bool OoTxMm_IsFierceDeitySkinMode(Player* player);
u16 OoTxMm_GetTransformMaskInputBlockMask(PlayState* play, Player* player);
void OoTxMm_UpdateTransformMaskState(PlayState* play, Player* player);
f32 OoTxMm_GetTransformMaskMovementSpeedMultiplier(Player* player);
bool OoTxMm_ShouldOverridePlayerDraw(Player* player);
bool OoTxMm_ShouldSuspendOverride(Player* player);
bool OoTxMm_ShouldHideTransformMaskPlayerDraw(Player* player);
void* OoTxMm_GetTransformMaskEyeTexture(Player* player);
void* OoTxMm_GetTransformMaskMouthTexture(Player* player);
void* OoTxMm_GetSpecialItemIconTexture(s16 itemId, void* fallback);
const char* OoTxMm_GetSpecialItemNameTexture(s16 itemId, s32 language, const char* fallback);
f32 OoTxMm_GetTransformMaskLedgeReachBonus(Player* player);
f32 OoTxMm_GetTransformMaskLedgeCheckHeightBonus(Player* player);
f32 OoTxMm_GetTransformMaskColliderHeightScale(Player* player);
f32 OoTxMm_GetTransformMaskColliderRadiusScale(Player* player);
f32 OoTxMm_GetTransformMaskColliderMinHeight(Player* player);
f32 OoTxMm_GetTransformMaskColliderMinRadius(Player* player);
f32 OoTxMm_GetTransformMaskColliderYShiftOffset(Player* player);
f32 OoTxMm_GetTransformMaskColliderHeight(Player* player);
f32 OoTxMm_GetTransformMaskColliderRadius(Player* player);
f32 OoTxMm_GetTransformMaskColliderYShift(Player* player);
f32 OoTxMm_GetTransformMaskCameraHeight(Player* player);
bool OoTxMm_ShouldBlockLedgeClimb(Player* player);
bool OoTxMm_IsTransformMaskFireImmune(Player* player);
bool OoTxMm_ShouldForceZoraJumpSlash(Player* player);
LinkAnimationHeader* OoTxMm_GetZoraJumpSlashAnim(Player* player, s32 phase);
bool OoTxMm_IsZoraBoomerangAiming(Player* player);
void OoTxMm_PlayerStartZoraBoomerangPipeline(Player* player, PlayState* play);
void OoTxMm_PlayerSetZoraBoomerangHeld(Player* player, s32 held);
void OoTxMm_PlayerCleanupZoraBoomerangPipeline(Player* player);
void OoTxMm_PlayerStartZoraBoomerangAimCamera(Player* player, PlayState* play);
void OoTxMm_UpdateZoraBoomerangAimCamera(PlayState* play, Player* player);
bool OoTxMm_ShouldSuppressJumpSfx(Player* player);
bool OoTxMm_ShouldSuppressLinkVoiceSfx(Player* player, u16 sfxId);
bool OoTxMm_PlayTransformMaskVoiceSfx(Player* player, u16 sfxId);
bool OoTxMm_PlayTransformMaskStepSfx(Player* player, f32 pitchAdjustment);
bool OoTxMm_PlayTransformMaskGeneralSfx(Player* player, u16 sfxId);
void* OoTxMm_GetZoraBoomerangDisplayList(s16 params);
void* OoTxMm_GetFierceDeitySwordBeamDisplayList(PlayState* play);
void OoTxMm_DrawTransformMaskPlayer(PlayState* play, Player* player);
void OoTxMm_DrawTransformMaskOverlays(PlayState* play, Player* player);

#ifdef __cplusplus
}
#endif
