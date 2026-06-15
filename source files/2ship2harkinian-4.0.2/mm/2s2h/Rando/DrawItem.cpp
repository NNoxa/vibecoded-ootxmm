#include "Rando/Rando.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/Crossover/Crossover.h"
#include "2s2h/Rando/DrawFuncs.h"
#include "2s2h_assets.h"
#include <fast/resource/type/DisplayList.h>
#include <spdlog/spdlog.h>
#include <ship/resource/ResourceManager.h>

extern "C" {
#include "variables.h"
#include "functions.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_gi_melody/object_gi_melody.h"
#include "assets/objects/object_gi_key/object_gi_key.h"
#include "assets/objects/object_gi_bosskey/object_gi_bosskey.h"
#include "objects/object_gi_hearts/object_gi_hearts.h"
#include "objects/object_gi_liquid/object_gi_liquid.h"
#include "objects/object_box/object_box.h"
#include "objects/object_sek/object_sek.h"
#include "objects/object_st/object_st.h"

#include "assets/overlays/ovl_Arrow_Ice/ovl_Arrow_Ice.h"

Gfx* ResourceMgr_LoadGfxByName(const char* path);
}

static bool OoTxMmModelIdStartsWith(const std::string& modelId, const char* prefix) {
    return modelId.rfind(prefix, 0) == 0;
}

static Gfx* LoadOoTxMmForeignOotGfxByName(const char* path) {
    if (path == nullptr) {
        return nullptr;
    }

    const auto ootArchive = TwoShipCrossover::GetOotO2rArchive();
    if (ootArchive == nullptr) {
        return nullptr;
    }

    auto resourceManager = Ship::Context::GetInstance()->GetResourceManager();
    if (resourceManager == nullptr) {
        return nullptr;
    }

    auto resource = std::static_pointer_cast<Fast::DisplayList>(
        resourceManager->LoadResource(Ship::ResourceIdentifier(path, reinterpret_cast<uintptr_t>(ootArchive.get()), ootArchive), true));
    if (resource == nullptr || resource->Instructions.empty()) {
        return nullptr;
    }

    return reinterpret_cast<Gfx*>(&resource->Instructions[0]);
}

static bool DrawOoTxMmOpaqueDlists(std::initializer_list<const char*> dlistPaths, float scale, s16 xRotation = 0x4000) {
    OPEN_DISPS(gPlayState->state.gfxCtx);
    Gfx_SetupDL25_Opa(gPlayState->state.gfxCtx);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    if (xRotation != 0) {
        Matrix_RotateXS(xRotation, MTXMODE_APPLY);
    }
    gSPMatrix(POLY_OPA_DISP++, Matrix_ToMtx((Mtx*)GRAPH_ALLOC(gPlayState->state.gfxCtx, sizeof(Mtx))),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    for (const char* dlistPath : dlistPaths) {
        if (dlistPath == nullptr) {
            continue;
        }
        Gfx* dlist = ResourceMgr_LoadGfxByName(dlistPath);
        if (dlist != nullptr) {
            gSPDisplayList(POLY_OPA_DISP++, dlist);
        }
    }
    CLOSE_DISPS(gPlayState->state.gfxCtx);
    return true;
}

static bool DrawOoTxMmWalletModel() {
    return DrawOoTxMmOpaqueDlists(
        {
            "__OTR__objects/object_gi_purse/gGiAdultWalletColorDL",
            "__OTR__objects/object_gi_purse/gGiAdultWalletRupeeOuterColorDL",
            "__OTR__objects/object_gi_purse/gGiAdultWalletStringColorDL",
            "__OTR__objects/object_gi_purse/gGiAdultWalletRupeeInnerColorDL",
            "__OTR__objects/object_gi_purse/gGiWalletDL",
            "__OTR__objects/object_gi_purse/gGiWalletRupeeOuterDL",
            "__OTR__objects/object_gi_purse/gGiWalletStringDL",
            "__OTR__objects/object_gi_purse/gGiWalletRupeeInnerDL",
        },
        0.012f);
}

static bool DrawOoTxMmBombBagModel() {
    return DrawOoTxMmOpaqueDlists(
        {
            "__OTR__objects/object_gi_bombpouch/gGiBombBag20BagColorDL",
            "__OTR__objects/object_gi_bombpouch/gGiBombBag20RingColorDL",
            "__OTR__objects/object_gi_bombpouch/gGiBombBagDL",
            "__OTR__objects/object_gi_bombpouch/gGiBombBagRingDL",
        },
        0.012f);
}

static bool DrawOoTxMmLensModel() {
    OPEN_DISPS(gPlayState->state.gfxCtx);
    Matrix_Scale(0.012f, 0.012f, 0.012f, MTXMODE_APPLY);
    Matrix_RotateXS(0x4000, MTXMODE_APPLY);

    Gfx_SetupDL25_Opa(gPlayState->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_ToMtx((Mtx*)GRAPH_ALLOC(gPlayState->state.gfxCtx, sizeof(Mtx))),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    if (Gfx* baseDlist = ResourceMgr_LoadGfxByName("__OTR__objects/object_gi_glasses/gGiLensDL");
        baseDlist != nullptr) {
        gSPDisplayList(POLY_OPA_DISP++, baseDlist);
    }

    Gfx_SetupDL25_Xlu(gPlayState->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, Matrix_ToMtx((Mtx*)GRAPH_ALLOC(gPlayState->state.gfxCtx, sizeof(Mtx))),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    if (Gfx* glassDlist = ResourceMgr_LoadGfxByName("__OTR__objects/object_gi_glasses/gGiLensGlassDL");
        glassDlist != nullptr) {
        gSPDisplayList(POLY_XLU_DISP++, glassDlist);
    }
    CLOSE_DISPS(gPlayState->state.gfxCtx);
    return true;
}

static bool DrawOoTxMmGoldSkulltulaTokenModel() {
    Matrix_RotateXS(16, MTXMODE_APPLY);

    OPEN_DISPS(gPlayState->state.gfxCtx);

    Gfx_SetupDL25_Opa(gPlayState->state.gfxCtx);
    Matrix_Scale(0.012f, 0.012f, 0.012f, MTXMODE_APPLY);
    Matrix_RotateXS(0x4000, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_ToMtx((Mtx*)GRAPH_ALLOC(gPlayState->state.gfxCtx, sizeof(Mtx))),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    if (Gfx* baseDlist = ResourceMgr_LoadGfxByName("__OTR__objects/object_gi_sutaru/gGiSkulltulaTokenDL");
        baseDlist != nullptr) {
        gSPDisplayList(POLY_OPA_DISP++, baseDlist);
    }

    Gfx_SetupDL25_Xlu(gPlayState->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 0, 255, 170, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 255, 0, 255);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(gPlayState->state.gfxCtx, G_TX_RENDERTILE, gPlayState->state.frames * 0,
                                             -(gPlayState->state.frames * 5), 32, 32, 1, gPlayState->state.frames * 0,
                                             gPlayState->state.frames * 0, 32, 64, 0, -5, 0, 0));
    gSPMatrix(POLY_XLU_DISP++, Matrix_ToMtx((Mtx*)GRAPH_ALLOC(gPlayState->state.gfxCtx, sizeof(Mtx))),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    if (Gfx* flameDlist = ResourceMgr_LoadGfxByName("__OTR__objects/object_gi_sutaru/gGiSkulltulaTokenFlameDL");
        flameDlist != nullptr) {
        gSPDisplayList(POLY_XLU_DISP++, flameDlist);
    }

    CLOSE_DISPS(gPlayState->state.gfxCtx);
    return true;
}

static bool DrawOoTxMmPlaceholderChest() {
    if (gPlayState == nullptr || gPlayState->state.gfxCtx == nullptr) {
        return false;
    }

    OPEN_DISPS(gPlayState->state.gfxCtx);
    Gfx_SetupDL25_Opa(gPlayState->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_RotateXS(0x4000, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_ToMtx((Mtx*)GRAPH_ALLOC(gPlayState->state.gfxCtx, sizeof(Mtx))),
              G_MTX_MODELVIEW | G_MTX_LOAD);

    if (Gfx* baseDlist = ResourceMgr_LoadGfxByName(gBoxChestBaseDL); baseDlist != nullptr) {
        gSPDisplayList(POLY_OPA_DISP++, baseDlist);
    }
    if (Gfx* lidDlist = ResourceMgr_LoadGfxByName(gBoxChestLidDL); lidDlist != nullptr) {
        gSPDisplayList(POLY_OPA_DISP++, lidDlist);
    }

    CLOSE_DISPS(gPlayState->state.gfxCtx);
    return true;
}

static bool DrawOoTxMmForeignOotOpaqueDlists(std::initializer_list<const char*> dlistPaths, float scale,
                                             s16 xRotation = 0) {
    if (gPlayState == nullptr || gPlayState->state.gfxCtx == nullptr) {
        return false;
    }

    bool drewAnything = false;
    OPEN_DISPS(gPlayState->state.gfxCtx);
    Gfx_SetupDL25_Opa(gPlayState->state.gfxCtx);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    if (xRotation != 0) {
        Matrix_RotateXS(xRotation, MTXMODE_APPLY);
    }
    gSPMatrix(POLY_OPA_DISP++, Matrix_ToMtx((Mtx*)GRAPH_ALLOC(gPlayState->state.gfxCtx, sizeof(Mtx))),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    for (const char* dlistPath : dlistPaths) {
        if (dlistPath == nullptr) {
            continue;
        }
        Gfx* dlist = LoadOoTxMmForeignOotGfxByName(dlistPath);
        if (dlist != nullptr) {
            gSPDisplayList(POLY_OPA_DISP++, dlist);
            drewAnything = true;
        }
    }
    CLOSE_DISPS(gPlayState->state.gfxCtx);
    return drewAnything;
}

static bool DrawOoTxMmForeignOotOpaqueXluDlists(std::initializer_list<const char*> opaPaths,
                                                std::initializer_list<const char*> xluPaths, float scale,
                                                s16 xRotation = 0) {
    if (gPlayState == nullptr || gPlayState->state.gfxCtx == nullptr) {
        return false;
    }

    bool drewAnything = false;
    OPEN_DISPS(gPlayState->state.gfxCtx);

    if (opaPaths.size() > 0) {
        Gfx_SetupDL25_Opa(gPlayState->state.gfxCtx);
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        if (xRotation != 0) {
            Matrix_RotateXS(xRotation, MTXMODE_APPLY);
        }
        gSPMatrix(POLY_OPA_DISP++, Matrix_ToMtx((Mtx*)GRAPH_ALLOC(gPlayState->state.gfxCtx, sizeof(Mtx))),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        for (const char* dlistPath : opaPaths) {
            if (dlistPath == nullptr) {
                continue;
            }

            if (Gfx* dlist = LoadOoTxMmForeignOotGfxByName(dlistPath); dlist != nullptr) {
                gSPDisplayList(POLY_OPA_DISP++, dlist);
                drewAnything = true;
            }
        }
    }

    if (xluPaths.size() > 0) {
        Gfx_SetupDL25_Xlu(gPlayState->state.gfxCtx);
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        if (xRotation != 0) {
            Matrix_RotateXS(xRotation, MTXMODE_APPLY);
        }
        gSPMatrix(POLY_XLU_DISP++, Matrix_ToMtx((Mtx*)GRAPH_ALLOC(gPlayState->state.gfxCtx, sizeof(Mtx))),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        for (const char* dlistPath : xluPaths) {
            if (dlistPath == nullptr) {
                continue;
            }

            if (Gfx* dlist = LoadOoTxMmForeignOotGfxByName(dlistPath); dlist != nullptr) {
                gSPDisplayList(POLY_XLU_DISP++, dlist);
                drewAnything = true;
            }
        }
    }

    CLOSE_DISPS(gPlayState->state.gfxCtx);
    return drewAnything;
}

static bool DrawOoTxMmForeignOotBowModel() {
    return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_bow/gGiBowDL" }, 0.012f);
}

static bool DrawOoTxMmForeignOotQuiverModel() {
    return DrawOoTxMmForeignOotOpaqueDlists(
        {
            "__OTR__objects/object_gi_arrowcase/gGiQuiver30InnerColorDL",
            "__OTR__objects/object_gi_arrowcase/gGiQuiver30OuterColorDL",
            "__OTR__objects/object_gi_arrowcase/gGiQuiverInnerDL",
            "__OTR__objects/object_gi_arrowcase/gGiQuiverOuterDL",
        },
        0.012f);
}

static bool DrawOoTxMmForeignOotEmeraldModel() {
    return DrawOoTxMmForeignOotOpaqueDlists(
        {
            "__OTR__objects/object_gi_jewel/gGiKokiriEmeraldSettingDL",
            "__OTR__objects/object_gi_jewel/gGiKokiriEmeraldGemDL",
        },
        1.0f);
}

static bool DrawOoTxMmForeignOotSongModel(const char* colorPath) {
    return DrawOoTxMmForeignOotOpaqueXluDlists({}, { colorPath, "__OTR__objects/object_gi_melody/gGiSongNoteDL" },
                                               1.0f);
}

static bool DrawOoTxMmForeignOotGenericSongModel() {
    return DrawOoTxMmForeignOotOpaqueXluDlists({}, { "__OTR__objects/object_gi_melody/gGiSongNoteDL" }, 1.0f);
}

static bool DrawOoTxMmForeignOotMagicSpellModel(const char* colorPath) {
    return DrawOoTxMmForeignOotOpaqueXluDlists(
        { "__OTR__objects/object_gi_goddess/gGiMagicSpellDiamondDL" },
        { colorPath, "__OTR__objects/object_gi_goddess/gGiMagicSpellOrbDL" }, 1.0f);
}

static bool DrawOoTxMmForeignOotMedallionModel(const char* facePath) {
    return DrawOoTxMmForeignOotOpaqueDlists({ facePath, "__OTR__objects/object_gi_medal/gGiMedallionDL" }, 1.0f);
}

static bool DrawOoTxMmForeignOotBossKeyModel() {
    return DrawOoTxMmForeignOotOpaqueXluDlists({ "__OTR__objects/object_gi_bosskey/gGiBossKeyDL" },
                                               { "__OTR__objects/object_gi_bosskey/gGiBossKeyGemDL" }, 1.0f);
}

static bool DrawOoTxMmForeignOotTunicModel(const char* collarColorPath, const char* tunicColorPath) {
    return DrawOoTxMmForeignOotOpaqueDlists(
        {
            "__OTR__objects/object_gi_clothes/gGiTunicCollarDL",
            collarColorPath,
            tunicColorPath,
            "__OTR__objects/object_gi_clothes/gGiTunicDL",
        },
        1.0f);
}

static bool DrawOoTxMmForeignOotJewelModel(const char* settingPath, const char* gemPath) {
    return DrawOoTxMmForeignOotOpaqueDlists({ settingPath, gemPath }, 1.0f);
}

static bool DrawOoTxMmForeignOotLetterModel() {
    return DrawOoTxMmForeignOotOpaqueXluDlists({ "__OTR__objects/object_gi_letter/gGiLetterDL" },
                                               { "__OTR__objects/object_gi_letter/gGiLetterWritingDL" }, 1.0f);
}

static bool DrawOoTxMmForeignOotBottleLetterModel() {
    return DrawOoTxMmForeignOotOpaqueXluDlists(
        { "__OTR__objects/object_gi_bottle_letter/gGiLetterBottleContentsDL" },
        { "__OTR__objects/object_gi_bottle_letter/gGiLetterBottleDL" }, 1.0f);
}

static bool DrawOoTxMmForeignOotChickenModel(const char* colorPath) {
    return DrawOoTxMmForeignOotOpaqueXluDlists(
        { "__OTR__objects/object_gi_niwatori/gGiChickenDL" },
        { colorPath, "__OTR__objects/object_gi_niwatori/gGiChickenEyesDL" }, 1.0f);
}

static bool DrawOoTxMmForeignOotEggModel() {
    return DrawOoTxMmForeignOotOpaqueXluDlists({ "__OTR__objects/object_gi_egg/gGiEggDL" },
                                               { "__OTR__objects/object_gi_egg/gGiEggMaterialDL" }, 1.0f);
}

static bool DrawOoTxMmForeignOotScaleModel(const char* waterColorPath, const char* scaleColorPath) {
    return DrawOoTxMmForeignOotOpaqueXluDlists(
        { scaleColorPath, "__OTR__objects/object_gi_scale/gGiScaleDL" },
        { waterColorPath, "__OTR__objects/object_gi_scale/gGiScaleWaterDL" }, 1.0f);
}

static bool DrawOoTxMmForeignOotGauntletsModel(const char* gauntletColorPath, const char* plateColorPath) {
    return DrawOoTxMmForeignOotOpaqueXluDlists(
        { "__OTR__objects/object_gi_gloves/gGiGauntletsDL", "__OTR__objects/object_gi_gloves/gGiGauntletsPlateDL" },
        { gauntletColorPath, plateColorPath }, 1.0f);
}

static bool DrawOoTxMmForeignOotFrogModel() {
    return DrawOoTxMmForeignOotOpaqueXluDlists({ "__OTR__objects/object_gi_frog/gGiFrogDL" },
                                               { "__OTR__objects/object_gi_frog/gGiFrogEyesDL" }, 1.0f);
}

static bool DrawOoTxMmForeignOotSoulModel() {
    return DrawOoTxMmForeignOotOpaqueXluDlists(
        { "__OTR__objects/object_gi_soul/gGiFairyContainerBaseCapDL" },
        {
            "__OTR__objects/object_gi_soul/gGiFairyContainerGlassDL",
            "__OTR__objects/object_gi_soul/gGiFairyContainerContentsDL",
        },
        1.0f);
}

static bool DrawOoTxMmForeignOotWalletModel(bool giantWallet) {
    if (giantWallet) {
        return DrawOoTxMmForeignOotOpaqueDlists(
            {
                "__OTR__objects/object_gi_purse/gGiWalletDL",
                "__OTR__objects/object_gi_purse/gGiGiantsWalletColorDL",
                "__OTR__objects/object_gi_purse/gGiWalletRupeeOuterDL",
                "__OTR__objects/object_gi_purse/gGiGiantsWalletRupeeOuterColorDL",
                "__OTR__objects/object_gi_purse/gGiWalletStringDL",
                "__OTR__objects/object_gi_purse/gGiGiantsWalletStringColorDL",
                "__OTR__objects/object_gi_purse/gGiWalletRupeeInnerDL",
                "__OTR__objects/object_gi_purse/gGiGiantsWalletRupeeInnerColorDL",
            },
            1.0f);
    }

    return DrawOoTxMmForeignOotOpaqueDlists(
        {
            "__OTR__objects/object_gi_purse/gGiWalletDL",
            "__OTR__objects/object_gi_purse/gGiAdultWalletColorDL",
            "__OTR__objects/object_gi_purse/gGiWalletRupeeOuterDL",
            "__OTR__objects/object_gi_purse/gGiAdultWalletRupeeOuterColorDL",
            "__OTR__objects/object_gi_purse/gGiWalletStringDL",
            "__OTR__objects/object_gi_purse/gGiAdultWalletStringColorDL",
            "__OTR__objects/object_gi_purse/gGiWalletRupeeInnerDL",
            "__OTR__objects/object_gi_purse/gGiAdultWalletRupeeInnerColorDL",
        },
        1.0f);
}

static bool DrawOoTxMmForeignOotQuiverModel(bool biggestQuiver) {
    return DrawOoTxMmForeignOotOpaqueDlists(
        {
            "__OTR__objects/object_gi_arrowcase/gGiQuiverInnerDL",
            biggestQuiver ? "__OTR__objects/object_gi_arrowcase/gGiQuiver50InnerColorDL"
                          : "__OTR__objects/object_gi_arrowcase/gGiQuiver30InnerColorDL",
            biggestQuiver ? "__OTR__objects/object_gi_arrowcase/gGiQuiver50OuterColorDL"
                          : "__OTR__objects/object_gi_arrowcase/gGiQuiver30OuterColorDL",
            "__OTR__objects/object_gi_arrowcase/gGiQuiverOuterDL",
        },
        1.0f);
}

static bool DrawOoTxMmForeignOotBulletBagModel(bool biggestBag) {
    return DrawOoTxMmForeignOotOpaqueDlists(
        {
            "__OTR__objects/object_gi_dekupouch/gGiBulletBagDL",
            biggestBag ? "__OTR__objects/object_gi_dekupouch/gGiBulletBag50ColorDL"
                       : "__OTR__objects/object_gi_dekupouch/gGiBulletBagColorDL",
            "__OTR__objects/object_gi_dekupouch/gGiBulletBagStringDL",
            biggestBag ? "__OTR__objects/object_gi_dekupouch/gGiBulletBag50StringColorDL"
                       : "__OTR__objects/object_gi_dekupouch/gGiBulletBagStringColorDL",
            "__OTR__objects/object_gi_dekupouch/gGiBulletBagWritingDL",
        },
        1.0f);
}

bool DrawOoTxMmSharedModel(RandoCheckId randoCheckId) {
    auto modelId = TwoShipCrossover::GetModelIdByCheckId(randoCheckId);
    std::string nativeCheckName;
    if (!modelId.has_value()) {
        const auto staticCheck = Rando::StaticData::Checks.find(randoCheckId);
        if (staticCheck == Rando::StaticData::Checks.end()) {
            return false;
        }

        nativeCheckName = staticCheck->second.name;
        modelId = TwoShipCrossover::GetModelIdByNativeCheck(staticCheck->second.name);
    } else {
        const auto staticCheck = Rando::StaticData::Checks.find(randoCheckId);
        if (staticCheck != Rando::StaticData::Checks.end()) {
            nativeCheckName = staticCheck->second.name;
        }
    }

    if (!modelId.has_value()) {
        return false;
    }

    if (!TwoShipCrossover::IsForeignOotExclusiveModelId(*modelId)) {
        return false;
    }

    static std::string sLastOoTxMmForeignDrawKey;
    const std::string drawKey = std::to_string(static_cast<int>(randoCheckId)) + ":" + *modelId;
    if (drawKey != sLastOoTxMmForeignDrawKey) {
        sLastOoTxMmForeignDrawKey = drawKey;
        SPDLOG_INFO("[OoTxMM] MM foreign OoT draw: check={} model={}",
                    nativeCheckName.empty() ? std::to_string(static_cast<int>(randoCheckId)) : nativeCheckName,
                    *modelId);
    }

    if (*modelId == "xoot.fairy_ocarina" || *modelId == "xoot.rg_fairy_ocarina") {
        return DrawOoTxMmForeignOotOpaqueXluDlists(
            {},
            {
                "__OTR__objects/object_gi_ocarina_0/gGiOcarinaFairyDL",
                "__OTR__objects/object_gi_ocarina_0/gGiOcarinaFairyHolesDL",
            },
            1.0f);
    }
    if (*modelId == "xoot.ocarina_of_time" || *modelId == "xoot.rg_ocarina_of_time") {
        return DrawOoTxMmForeignOotOpaqueXluDlists({}, { "__OTR__objects/object_gi_ocarina/gGiOcarinaTimeDL",
                                                         "__OTR__objects/object_gi_ocarina/gGiOcarinaTimeHolesDL" },
                                                   1.0f);
    }
    if (*modelId == "xoot.deku_shield" || *modelId == "xoot.buy_deku_shield") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_shield_1/gGiDekuShieldDL" }, 1.0f);
    }
    if (*modelId == "xoot.hylian_shield" || *modelId == "xoot.unique.hylian_shield" ||
        *modelId == "xoot.buy_hylian_shield" || *modelId == "xoot.unique.buy_hylian_shield") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_shield_2/gGiHylianShieldDL" }, 1.0f);
    }
    if (*modelId == "xoot.mirror_shield" || *modelId == "xoot.unique.mirror_shield") {
        return DrawOoTxMmForeignOotOpaqueDlists(
            { "__OTR__objects/object_gi_shield_3/gGiMirrorShieldDL",
              "__OTR__objects/object_gi_shield_3/gGiMirrorShieldSymbolDL" },
            1.0f);
    }
    if (*modelId == "xoot.hookshot" || *modelId == "xoot.unique.hookshot" || *modelId == "xoot.progressive_hookshot") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_hookshot/gGiHookshotDL" }, 1.0f);
    }
    if (*modelId == "xoot.longshot" || *modelId == "xoot.unique.longshot") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_hookshot/gGiLongshotDL" }, 1.0f);
    }
    if (*modelId == "xoot.progressive_bow" || *modelId == "xoot.unique.progressive_bow" ||
        *modelId == "xoot.fairy_bow" || *modelId == "xoot.rg_fairy_bow" || *modelId == "xoot.bow") {
        return DrawOoTxMmForeignOotQuiverModel(true);
    }
    if (*modelId == "xoot.progressive_slingshot") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_pachinko/gGiSlingshotDL" }, 1.0f);
    }
    if (*modelId == "xoot.megaton_hammer") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_hammer/gGiHammerDL" }, 1.0f);
    }
    if (*modelId == "xoot.boomerang") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_boomerang/gGiBoomerangDL" }, 1.0f);
    }
    if (*modelId == "xoot.kokiris_emerald" ||
        (OoTxMmModelIdStartsWith(*modelId, "xoot.spiritual_stone.") && modelId->find("emerald") != std::string::npos)) {
        return DrawOoTxMmForeignOotEmeraldModel();
    }
    if (*modelId == "xoot.gorons_ruby" ||
        (OoTxMmModelIdStartsWith(*modelId, "xoot.spiritual_stone.") && modelId->find("ruby") != std::string::npos)) {
        return DrawOoTxMmForeignOotJewelModel("__OTR__objects/object_gi_jewel/gGiGoronRubySettingDL",
                                              "__OTR__objects/object_gi_jewel/gGiGoronRubyGemDL");
    }
    if (*modelId == "xoot.zoras_sapphire" ||
        (OoTxMmModelIdStartsWith(*modelId, "xoot.spiritual_stone.") && modelId->find("sapphire") != std::string::npos)) {
        return DrawOoTxMmForeignOotJewelModel("__OTR__objects/object_gi_jewel/gGiZoraSapphireSettingDL",
                                              "__OTR__objects/object_gi_jewel/gGiZoraSapphireGemDL");
    }
    if (*modelId == "xoot.stone_of_agony") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_map/gGiStoneOfAgonyDL" }, 1.0f);
    }
    if (*modelId == "xoot.claim_check") {
        return DrawOoTxMmForeignOotOpaqueDlists(
            {
                "__OTR__objects/object_gi_ticketstone/gGiClaimCheckDL",
                "__OTR__objects/object_gi_ticketstone/gGiClaimCheckWritingDL",
            },
            1.0f);
    }
    if (*modelId == "xoot.skull_mask") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_skj_mask/gGiSkullMaskDL" }, 1.0f);
    }
    if (*modelId == "xoot.spooky_mask") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_redead_mask/gGiSpookyMaskDL" }, 1.0f);
    }
    if (*modelId == "xoot.gerudo_mask") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_gerudomask/gGiGerudoMaskDL" }, 1.0f);
    }
    if (*modelId == "xoot.gerudo_membership_card") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_gerudo/gGiGerudoCardDL" }, 1.0f);
    }
    if (OoTxMmModelIdStartsWith(*modelId, "xoot.") &&
        (modelId->find("_soul") != std::string::npos || modelId->find(".soul") != std::string::npos)) {
        return DrawOoTxMmForeignOotSoulModel();
    }
    if (*modelId == "xoot.zeldas_letter") {
        return DrawOoTxMmForeignOotLetterModel();
    }
    if (*modelId == "xoot.rutos_letter") {
        return DrawOoTxMmForeignOotBottleLetterModel();
    }
    if (*modelId == "xoot.pocket_egg") {
        return DrawOoTxMmForeignOotEggModel();
    }
    if (*modelId == "xoot.pocket_cucco") {
        return DrawOoTxMmForeignOotChickenModel("__OTR__objects/object_gi_niwatori/gGiChickenColorDL");
    }
    if (*modelId == "xoot.cojiro") {
        return DrawOoTxMmForeignOotChickenModel("__OTR__objects/object_gi_niwatori/gGiCojiroColorDL");
    }
    if (*modelId == "xoot.odd_mushroom") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_mushroom/gGiOddMushroomDL" }, 1.0f);
    }
    if (*modelId == "xoot.odd_potion") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_powder/gGiOddPotionDL" }, 1.0f);
    }
    if (*modelId == "xoot.poachers_saw") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_saw/gGiSawDL" }, 1.0f);
    }
    if (*modelId == "xoot.prescription") {
        return DrawOoTxMmForeignOotOpaqueXluDlists({ "__OTR__objects/object_gi_prescription/gGiPrescriptionDL" },
                                                   { "__OTR__objects/object_gi_prescription/gGiPrescriptionWritingDL" },
                                                   1.0f);
    }
    if (*modelId == "xoot.eyeball_frog" || *modelId == "xoot.eye_drops" ||
        *modelId == "xoot.worlds_finest_eyedrops") {
        return DrawOoTxMmForeignOotFrogModel();
    }
    if (*modelId == "xoot.boots.hover_boots") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_hoverboots/gGiHoverBootsDL" }, 1.0f);
    }
    if (*modelId == "xoot.boots.iron_boots") {
        return DrawOoTxMmForeignOotOpaqueXluDlists({ "__OTR__objects/object_gi_boots_2/gGiIronBootsDL" },
                                                   { "__OTR__objects/object_gi_boots_2/gGiIronBootsRivetsDL" }, 1.0f);
    }
    if (modelId->find("boss_key") != std::string::npos) {
        return DrawOoTxMmForeignOotBossKeyModel();
    }
    if (*modelId == "xoot.gerudo_fortress_small_key" || *modelId == "xoot.forest_temple_key_ring" ||
        *modelId == "xoot.fire_temple_key_ring" || *modelId == "xoot.water_temple_key_ring" ||
        *modelId == "xoot.spirit_temple_key_ring" || *modelId == "xoot.shadow_temple_key_ring" ||
        *modelId == "xoot.bottom_of_the_well_key_ring" || *modelId == "xoot.gerudo_training_ground_key_ring" ||
        *modelId == "xoot.training_ground_key_ring" ||
        *modelId == "xoot.gerudo_fortress_key_ring" || *modelId == "xoot.ganons_castle_key_ring" ||
        *modelId == "xoot.treasure_game_key_ring" || *modelId == "xoot.skeleton_key" ||
        modelId->find("key_ring") != std::string::npos) {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_key/gGiSmallKeyDL" }, 1.0f);
    }
    if (*modelId == "xoot.medallion.forest_medallion") {
        return DrawOoTxMmForeignOotMedallionModel("__OTR__objects/object_gi_medal/gGiForestMedallionFaceDL");
    }
    if (*modelId == "xoot.medallion.fire_medallion") {
        return DrawOoTxMmForeignOotMedallionModel("__OTR__objects/object_gi_medal/gGiFireMedallionFaceDL");
    }
    if (*modelId == "xoot.medallion.water_medallion") {
        return DrawOoTxMmForeignOotMedallionModel("__OTR__objects/object_gi_medal/gGiWaterMedallionFaceDL");
    }
    if (*modelId == "xoot.medallion.spirit_medallion") {
        return DrawOoTxMmForeignOotMedallionModel("__OTR__objects/object_gi_medal/gGiSpiritMedallionFaceDL");
    }
    if (*modelId == "xoot.medallion.shadow_medallion") {
        return DrawOoTxMmForeignOotMedallionModel("__OTR__objects/object_gi_medal/gGiShadowMedallionFaceDL");
    }
    if (*modelId == "xoot.medallion.light_medallion") {
        return DrawOoTxMmForeignOotMedallionModel("__OTR__objects/object_gi_medal/gGiLightMedallionFaceDL");
    }
    if (*modelId == "xoot.dins_fire") {
        return DrawOoTxMmForeignOotMagicSpellModel("__OTR__objects/object_gi_goddess/gGiDinsFireColorDL");
    }
    if (*modelId == "xoot.farores_wind") {
        return DrawOoTxMmForeignOotMagicSpellModel("__OTR__objects/object_gi_goddess/gGiFaroresWindColorDL");
    }
    if (*modelId == "xoot.nayrus_love") {
        return DrawOoTxMmForeignOotMagicSpellModel("__OTR__objects/object_gi_goddess/gGiNayrusLoveColorDL");
    }
    if (*modelId == "xoot.tunic.goron_tunic" || *modelId == "xoot.tunic.buy_goron_tunic") {
        return DrawOoTxMmForeignOotTunicModel("__OTR__objects/object_gi_clothes/gGiGoronCollarColorDL",
                                              "__OTR__objects/object_gi_clothes/gGiGoronTunicColorDL");
    }
    if (*modelId == "xoot.tunic.zora_tunic" || *modelId == "xoot.tunic.buy_zora_tunic") {
        return DrawOoTxMmForeignOotTunicModel("__OTR__objects/object_gi_clothes/gGiZoraCollarColorDL",
                                              "__OTR__objects/object_gi_clothes/gGiZoraTunicColorDL");
    }
    if (OoTxMmModelIdStartsWith(*modelId, "xoot.wallet") || modelId->find("wallet") != std::string::npos) {
        const bool giantWallet = modelId->find("giant") != std::string::npos || modelId->find("tycoon") != std::string::npos;
        return DrawOoTxMmForeignOotWalletModel(giantWallet);
    }
    if (OoTxMmModelIdStartsWith(*modelId, "xoot.quiver") || modelId->find("bullet_bag") != std::string::npos) {
        const bool biggestUpgrade = modelId->find("50") != std::string::npos || modelId->find("biggest") != std::string::npos;
        if (modelId->find("bullet_bag") != std::string::npos) {
            return DrawOoTxMmForeignOotBulletBagModel(biggestUpgrade);
        }
        return DrawOoTxMmForeignOotQuiverModel(biggestUpgrade);
    }
    if (modelId->find("deku_stick") != std::string::npos || modelId->find("stick_upgrade") != std::string::npos ||
        modelId->find("progressive_stick_capacity") != std::string::npos) {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_stick/gGiStickDL" }, 1.0f);
    }
    if (modelId->find("deku_nut") != std::string::npos || modelId->find("nut_upgrade") != std::string::npos ||
        modelId->find("progressive_nut_capacity") != std::string::npos) {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_nuts/gGiNutDL" }, 1.0f);
    }
    if (*modelId == "xoot.strength.gorons_bracelet" || *modelId == "xoot.strength.progressive_strength" ||
        *modelId == "xoot.strength_upgrade") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_bracelet/gGiGoronBraceletDL" }, 1.0f);
    }
    if (*modelId == "xoot.strength.silver_gauntlets") {
        return DrawOoTxMmForeignOotGauntletsModel("__OTR__objects/object_gi_gloves/gGiSilverGauntletsColorDL",
                                                  "__OTR__objects/object_gi_gloves/gGiSilverGauntletsPlateColorDL");
    }
    if (*modelId == "xoot.strength.golden_gauntlets" || *modelId == "xoot.strength.gold_gauntlets") {
        return DrawOoTxMmForeignOotGauntletsModel("__OTR__objects/object_gi_gloves/gGiGoldenGauntletsColorDL",
                                                  "__OTR__objects/object_gi_gloves/gGiGoldenGauntletsPlateColorDL");
    }
    if (*modelId == "xoot.scale.silver_scale" || *modelId == "xoot.scale.progressive_scale") {
        return DrawOoTxMmForeignOotScaleModel("__OTR__objects/object_gi_scale/gGiSilverScaleWaterColorDL",
                                              "__OTR__objects/object_gi_scale/gGiSilverScaleColorDL");
    }
    if (*modelId == "xoot.scale.golden_scale" || *modelId == "xoot.scale.gold_scale") {
        return DrawOoTxMmForeignOotScaleModel("__OTR__objects/object_gi_scale/gGiGoldenScaleWaterColorDL",
                                              "__OTR__objects/object_gi_scale/gGiGoldenScaleColorDL");
    }
    if (*modelId == "xoot.kokiri_sword" || *modelId == "xoot.rg_kokiri_sword") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_sword_1/gGiKokiriSwordDL" }, 1.0f);
    }
    if (*modelId == "xoot.master_sword" || *modelId == "xoot.rg_master_sword") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_custom_equip/gCustomMasterSwordDL" }, 1.0f);
    }
    if (*modelId == "xoot.fishing_pole") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_fishing_pole/gFishingPoleGiDL" }, 1.0f);
    }
    if (*modelId == "xoot.rocs_feather") {
        return DrawOoTxMmForeignOotOpaqueXluDlists({}, { "__OTR__objects/object_rocs_feather/gGiRocsFeatherDL" },
                                                   1.0f);
    }
    if (*modelId == "xoot.giants_knife" || *modelId == "xoot.biggorons_sword" || *modelId == "xoot.biggoron_sword") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_longsword/gGiBiggoronSwordDL" }, 1.0f);
    }
    if (*modelId == "xoot.gorons_sword_broken" || *modelId == "xoot.broken_gorons_sword") {
        return DrawOoTxMmForeignOotOpaqueDlists({ "__OTR__objects/object_gi_brokensword/gGiBrokenGoronSwordDL" },
                                                1.0f);
    }
    if (*modelId == "xoot.minuet_of_forest" || OoTxMmModelIdStartsWith(*modelId, "xoot.minuet_of_forest")) {
        return DrawOoTxMmForeignOotSongModel("__OTR__objects/object_gi_melody/gGiMinuetColorDL");
    }
    if (*modelId == "xoot.bolero_of_fire" || OoTxMmModelIdStartsWith(*modelId, "xoot.bolero_of_fire")) {
        return DrawOoTxMmForeignOotSongModel("__OTR__objects/object_gi_melody/gGiBoleroColorDL");
    }
    if (*modelId == "xoot.serenade_of_water" || OoTxMmModelIdStartsWith(*modelId, "xoot.serenade_of_water")) {
        return DrawOoTxMmForeignOotSongModel("__OTR__objects/object_gi_melody/gGiSerenadeColorDL");
    }
    if (*modelId == "xoot.requiem_of_spirit" || OoTxMmModelIdStartsWith(*modelId, "xoot.requiem_of_spirit")) {
        return DrawOoTxMmForeignOotSongModel("__OTR__objects/object_gi_melody/gGiRequiemColorDL");
    }
    if (*modelId == "xoot.nocturne_of_shadow" || OoTxMmModelIdStartsWith(*modelId, "xoot.nocturne_of_shadow")) {
        return DrawOoTxMmForeignOotSongModel("__OTR__objects/object_gi_melody/gGiNocturneColorDL");
    }
    if (*modelId == "xoot.prelude_of_light" || OoTxMmModelIdStartsWith(*modelId, "xoot.prelude_of_light")) {
        return DrawOoTxMmForeignOotSongModel("__OTR__objects/object_gi_melody/gGiPreludeColorDL");
    }
    if (*modelId == "xoot.zeldas_lullaby" || *modelId == "xoot.eponas_song" || *modelId == "xoot.sarias_song" ||
        *modelId == "xoot.suns_song") {
        return DrawOoTxMmForeignOotGenericSongModel();
    }

    return false;
}

s32 StrayFairyOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx,
                               Gfx** gfx) {
    if (limbIndex == STRAY_FAIRY_LIMB_RIGHT_FACING_HEAD) {
        *dList = NULL;
    }

    return false;
}

void DrawStrayFairy(RandoItemId randoItemId) {
    OPEN_DISPS(gPlayState->state.gfxCtx);

    Gfx_SetupDL25_Xlu(gPlayState->state.gfxCtx);

    switch (randoItemId) {
        case RI_WOODFALL_STRAY_FAIRY:
            AnimatedMat_Draw(gPlayState, (AnimatedMaterial*)&gStrayFairyWoodfallTexAnim);
            break;
        case RI_SNOWHEAD_STRAY_FAIRY:
            AnimatedMat_Draw(gPlayState, (AnimatedMaterial*)&gStrayFairySnowheadTexAnim);
            break;
        case RI_GREAT_BAY_STRAY_FAIRY:
            AnimatedMat_Draw(gPlayState, (AnimatedMaterial*)&gStrayFairyGreatBayTexAnim);
            break;
        case RI_STONE_TOWER_STRAY_FAIRY:
            AnimatedMat_Draw(gPlayState, (AnimatedMaterial*)&gStrayFairyStoneTowerTexAnim);
            break;
        default: // STRAY_FAIRY_AREA_CLOCK_TOWN
            AnimatedMat_Draw(gPlayState, (AnimatedMaterial*)&gStrayFairyClockTownTexAnim);
            break;
    }

    Matrix_ReplaceRotation(&gPlayState->billboardMtxF);
    Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);

    // Kind of a hack to draw the stray fairy, the drawback of this is that all stray fairies in the scene will animate
    // together, but worse is that the more there are the faster their animation will play (because of the
    // SkelAnime_Update below). This is still better than the previous solution which hand drew the fairy with DL
    // calls...
    static bool initialized = false;
    static SkelAnime skelAnime;
    static Vec3s jointTable[STRAY_FAIRY_LIMB_MAX];
    static u32 lastUpdate = 0;
    if (!initialized) {
        initialized = true;
        SkelAnime_InitFlex(gPlayState, &skelAnime, (FlexSkeletonHeader*)&gStrayFairySkel,
                           (AnimationHeader*)&gStrayFairyFlyingAnim, jointTable, jointTable, STRAY_FAIRY_LIMB_MAX);
    }
    if (gPlayState != NULL && lastUpdate != gPlayState->state.frames) {
        lastUpdate = gPlayState->state.frames;
        SkelAnime_Update(&skelAnime);
    }
    POLY_XLU_DISP = SkelAnime_DrawFlex(gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount,
                                       StrayFairyOverrideLimbDraw, NULL, NULL, POLY_XLU_DISP);

    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

void DrawSong(RandoItemId randoItemId) {
    OPEN_DISPS(gPlayState->state.gfxCtx);

    Gfx_SetupDL25_Xlu(gPlayState->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, gPlayState->state.gfxCtx);

    switch (randoItemId) {
        case RI_SONG_SUN:
            gDPSetEnvColor(POLY_XLU_DISP++, 237, 231, 62, 255);
            break;
        case RI_SONG_DOUBLE_TIME:
        case RI_SONG_INVERTED_TIME:
        case RI_SONG_TIME:
            gDPSetEnvColor(POLY_XLU_DISP++, 98, 177, 211, 255);
            break;
        case RI_SONG_HEALING:
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 150, 230, 255);
            break;
        case RI_SONG_STORMS:
            gDPSetEnvColor(POLY_XLU_DISP++, 146, 146, 146, 255);
            break;
        case RI_SONG_SARIA:
        case RI_SONG_SONATA:
            gDPSetEnvColor(POLY_XLU_DISP++, 98, 255, 98, 255);
            break;
        case RI_SONG_SOARING:
            gDPSetEnvColor(POLY_XLU_DISP++, 200, 160, 255, 255);
            break;
        case RI_SONG_ELEGY:
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 98, 0, 255);
            break;
        case RI_SONG_LULLABY_INTRO:
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 100, 100, 255);
            break;
        case RI_SONG_LULLABY:
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 20, 20, 255);
            break;
        case RI_SONG_OATH:
            gDPSetEnvColor(POLY_XLU_DISP++, 98, 0, 98, 255);
            break;
        case RI_SONG_EPONA:
            gDPSetEnvColor(POLY_XLU_DISP++, 146, 87, 49, 255);
            break;
        case RI_SONG_NOVA:
            gDPSetEnvColor(POLY_XLU_DISP++, 20, 20, 255, 255);
            break;
        default:
            break;
    }

    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)&gGiSongNoteDL);

    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

void DrawDoubleDefense() {
    OPEN_DISPS(gPlayState->state.gfxCtx);

    Gfx_SetupDL25_Xlu(gPlayState->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, gPlayState->state.gfxCtx);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, 255, 255, 255, 255);
    gSPGrayscale(POLY_XLU_DISP++, true);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)&gGiHeartBorderDL);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, 255, 0, 0, 100);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)&gGiHeartContainerDL);
    gSPGrayscale(POLY_XLU_DISP++, false);

    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

void DrawMilkRefill() {
    OPEN_DISPS(gPlayState->state.gfxCtx);

    Gfx_SetupDL25_Opa(gPlayState->state.gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(gPlayState->state.gfxCtx, G_TX_RENDERTILE, -gPlayState->state.frames,
                                             gPlayState->state.frames, 32, 32, 1, -gPlayState->state.frames,
                                             gPlayState->state.frames, 32, 32, -1, 1, -1, 1));
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, gPlayState->state.gfxCtx);
    // Container Color
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 200, 200, 200, 255);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiPotionContainerPotDL);
    // Liquid Color
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 200, 200, 200, 255);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiPotionContainerLiquidDL);

    Gfx_SetupDL25_Xlu(gPlayState->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, gPlayState->state.gfxCtx);
    // Pattern Color
    // Milk
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 13, 33, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 100, 100, 255, 255);
    gDPLoadTextureBlock(POLY_XLU_DISP++, gGiPotionContainerBluePatternTex, G_IM_FMT_IA, G_IM_SIZ_8b, 16, 32, 0,
                        G_TX_MIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_CLAMP, 4, 5, G_TX_NOLOD, G_TX_NOLOD);
    // Chateau
    // gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 250, 225, 78, 255);
    // gDPSetEnvColor(POLY_XLU_DISP++, 184, 42, 119, 255);
    // gDPLoadTextureBlock(POLY_XLU_DISP++, gGiPotionContainerRedPatternTex, G_IM_FMT_IA, G_IM_SIZ_8b, 16, 32, 0,
    // G_TX_MIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_CLAMP, 4, 5, G_TX_NOLOD, G_TX_NOLOD);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiPotionContainerPatternDL);

    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

void DrawOwlStatue() {
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_Translate(0, -3000, 0, MTXMODE_APPLY);
    Gfx_DrawDListOpa(gPlayState, (Gfx*)gOwlStatueOpenedDL);
}

static Gfx gGiSmallKeyCopyDL[75];

void DrawSmallKey(RandoItemId randoItemId) {
    OPEN_DISPS(gPlayState->state.gfxCtx);

    Gfx_SetupDL25_Opa(gPlayState->state.gfxCtx);
    switch (randoItemId) {
        case RI_WOODFALL_SMALL_KEY:
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, 255, 255, 255, 255);
            gDPSetEnvColor(POLY_OPA_DISP++, 236, 120, 186, 255);
            break;
        case RI_SNOWHEAD_SMALL_KEY:
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, 255, 255, 255, 255);
            gDPSetEnvColor(POLY_OPA_DISP++, 129, 173, 70, 255);
            break;
        case RI_GREAT_BAY_SMALL_KEY:
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, 255, 255, 255, 255);
            gDPSetEnvColor(POLY_OPA_DISP++, 99, 90, 183, 255);
            break;
        case RI_STONE_TOWER_SMALL_KEY:
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, 255, 255, 255, 255);
            gDPSetEnvColor(POLY_OPA_DISP++, 177, 165, 83, 255);
            break;
        default:
            break;
    }

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, gGiSmallKeyCopyDL);

    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

static Gfx gGiBossKeyCopyDL[87];

void DrawBossKey(RandoItemId randoItemId) {
    OPEN_DISPS(gPlayState->state.gfxCtx);

    Gfx_SetupDL25_Opa(gPlayState->state.gfxCtx);
    switch (randoItemId) {
        case RI_WOODFALL_BOSS_KEY:
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, 255, 255, 255, 255);
            gDPSetEnvColor(POLY_OPA_DISP++, 236, 120, 186, 255);
            break;
        case RI_SNOWHEAD_BOSS_KEY:
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, 255, 255, 255, 255);
            gDPSetEnvColor(POLY_OPA_DISP++, 129, 173, 70, 255);
            break;
        case RI_GREAT_BAY_BOSS_KEY:
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, 255, 255, 255, 255);
            gDPSetEnvColor(POLY_OPA_DISP++, 99, 90, 183, 255);
            break;
        case RI_STONE_TOWER_BOSS_KEY:
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, 255, 255, 255, 255);
            gDPSetEnvColor(POLY_OPA_DISP++, 177, 165, 83, 255);
            break;
        default:
            break;
    }

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, gGiBossKeyCopyDL);

    Gfx_SetupDL25_Xlu(gPlayState->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiBossKeyGemDL);

    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

static Gfx gSkulltulaTokenFlameCopyDL[76];

void DrawSkulltulaToken(RandoItemId randoItemId, Actor* actor) {
    // It is not known why this happens, but the eyes on the skulltula tokens disappear if they are are perfectly
    // parallel with the camera. This most likely a problem in our Fast3D (maybe z-index stuff?).
    // Tilting the token down by 16 units seems to be enough to get it to always render the eyes without being
    // noticeable that it is tilted. This issue was most prevalent for tokens in shops.
    Matrix_RotateXS(16, MTXMODE_APPLY);

    OPEN_DISPS(gPlayState->state.gfxCtx);

    Gfx_SetupDL25_Opa(gPlayState->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gSkulltulaTokenDL);

    Gfx_SetupDL25_Xlu(gPlayState->state.gfxCtx);

    if (randoItemId == RI_GS_TOKEN_OCEAN) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 0, 255, 255, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 255, 255);
    } else {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 0, 255, 170, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 255, 0, 255);
    }

    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(gPlayState->state.gfxCtx, G_TX_RENDERTILE, gPlayState->state.frames * 0,
                                             -(gPlayState->state.frames * 5), 32, 32, 1, gPlayState->state.frames * 0,
                                             gPlayState->state.frames * 0, 32, 64, 0, -5, 0, 0));
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gSkulltulaTokenFlameCopyDL);

    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

void DrawTrapModel() {
    OPEN_DISPS(gPlayState->state.gfxCtx);

    Gfx_SetupDL25_Xlu(gPlayState->state.gfxCtx);
    Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gTrapDL);

    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

void DrawTriforcePiece(RandoItemId randoItemId) {
    Gfx* triforcePieceModels[3] = {
        (Gfx*)gTriforcePiece0DL,
        (Gfx*)gTriforcePiece1DL,
        (Gfx*)gTriforcePiece2DL,
    };

    u16 currentTriforcePieces = gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces;

    OPEN_DISPS(gPlayState->state.gfxCtx);

    Gfx_SetupDL25_Xlu(gPlayState->state.gfxCtx);

    Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, gPlayState->state.gfxCtx);
    if (currentTriforcePieces >= RANDO_SAVE_OPTIONS[RO_TRIFORCE_PIECES_REQUIRED]) {
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gTriforcePieceCompletedDL);
    } else {
        if (randoItemId == RI_TRIFORCE_PIECE_PREVIOUS) {
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)triforcePieceModels[(currentTriforcePieces - 1) % 3]);
        } else {
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)triforcePieceModels[currentTriforcePieces % 3]);
        }
    }

    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

void DrawAbilityItem(RandoItemId randoItemId, Actor* actor) {
    Gfx* abilityItemModel[1] = {
        (Gfx*)gGiFlippersDL,
    };

    OPEN_DISPS(gPlayState->state.gfxCtx);

    Gfx_SetupDL25_Xlu(gPlayState->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)abilityItemModel[randoItemId - RI_ABILITY_SWIM]);

    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

void DrawOcarinaButtonItem(RandoItemId randoItemId, Actor* actor) {
    Gfx* ocarinaButtonModel[5] = {
        (Gfx*)gOcarinaAButtonDL,     (Gfx*)gOcarinaCDownButtonDL, (Gfx*)gOcarinaCRightButtonDL,
        (Gfx*)gOcarinaCLeftButtonDL, (Gfx*)gOcarinaCUpButtonDL,
    };

    OPEN_DISPS(gPlayState->state.gfxCtx);
    Gfx_SetupDL25_Opa(gPlayState->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)ocarinaButtonModel[randoItemId - RI_OCARINA_BUTTON_A]);

    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

// clang-format off
std::unordered_map<RandoItemId, std::function<void()>> soulDrawMap = {
    { RI_SOUL_ENEMY_ALIEN,          DrawAlien },
    { RI_SOUL_ENEMY_ARMOS,          DrawArmos },
    { RI_SOUL_ENEMY_BAD_BAT,        DrawBat },
    { RI_SOUL_ENEMY_BEAMOS,         DrawBeamos },
    { RI_SOUL_ENEMY_BUBBLE,         DrawBubble },
    { RI_SOUL_ENEMY_BOE,            DrawBoe },
    { RI_SOUL_ENEMY_CHUCHU,         DrawChuchu },
    { RI_SOUL_ENEMY_CAPTAIN_KEETA,  DrawCaptainKeeta },
    { RI_SOUL_ENEMY_DEATH_ARMOS,    DrawDeathArmos },
    { RI_SOUL_ENEMY_DEEP_PYTHON,    DrawDeepPython },
    { RI_SOUL_ENEMY_DEKU_BABA,      DrawDekuBaba },
    { RI_SOUL_ENEMY_DEXIHAND,       DrawDexihand },
    { RI_SOUL_ENEMY_DINOLFOS,       DrawDinolfos },
    { RI_SOUL_ENEMY_DODONGO,        DrawDodongo },
    { RI_SOUL_ENEMY_DRAGONFLY,      DrawDragonfly },
    { RI_SOUL_ENEMY_EENO,           DrawEeno },
    { RI_SOUL_ENEMY_EYEGORE,        DrawEyegore },
    { RI_SOUL_ENEMY_FREEZARD,       DrawFreezard },
    { RI_SOUL_ENEMY_GARO,           DrawGaro },
    { RI_SOUL_ENEMY_GEKKO,          DrawGekko },
    { RI_SOUL_ENEMY_GIANT_BEE,      DrawGiantBee },
    { RI_SOUL_ENEMY_GOMESS,         DrawGomess },
    { RI_SOUL_ENEMY_GUAY,           DrawGuay },
    { RI_SOUL_ENEMY_HIPLOOP,        DrawHiploop },
    { RI_SOUL_ENEMY_IGOS_DU_IKANA,  DrawIgosDuIkana },
    { RI_SOUL_ENEMY_IRON_KNUCKLE,   DrawIronKnuckle },
    { RI_SOUL_ENEMY_KEESE,          DrawKeese },
    { RI_SOUL_ENEMY_LEEVER,         DrawLeever },
    { RI_SOUL_ENEMY_LIKE_LIKE,      DrawLikeLike },
    { RI_SOUL_ENEMY_MAD_SCRUB,      DrawMadScrub },
    { RI_SOUL_ENEMY_NEJIRON,        DrawNejiron },
    { RI_SOUL_ENEMY_OCTOROK,        DrawOctorok },
    { RI_SOUL_ENEMY_PEAHAT,         DrawPeahat },
    { RI_SOUL_ENEMY_PIRATE,         DrawPirate },
    { RI_SOUL_ENEMY_POE,            DrawPoe },
    { RI_SOUL_ENEMY_REDEAD,         DrawRedead },
    { RI_SOUL_ENEMY_SHELLBLADE,     DrawShellBlade },
    { RI_SOUL_ENEMY_SKULLFISH,      DrawSkullfish },
    { RI_SOUL_ENEMY_SKULLTULA,      DrawSkulltula },
    { RI_SOUL_ENEMY_SNAPPER,        DrawSnapper },
    { RI_SOUL_ENEMY_STALCHILD,      DrawStalchild },
    { RI_SOUL_ENEMY_TAKKURI,        DrawTakkuri },
    { RI_SOUL_ENEMY_TEKTITE,        DrawTektite },
    { RI_SOUL_ENEMY_WALLMASTER,     DrawWallmaster },
    { RI_SOUL_ENEMY_WART,           DrawWart },
    { RI_SOUL_ENEMY_WIZROBE,        DrawWizrobe },
    { RI_SOUL_ENEMY_WOLFOS,         DrawWolfos },
};
// clang-format on

void DrawSoul(RandoItemId randoItemId) {
    auto it = soulDrawMap.find(randoItemId);
    if (it != soulDrawMap.end()) {
        it->second();
    }
}

void DrawSparkles(RandoItemId randoItemId, Actor* actor) {
    if (actor == NULL) {
        return;
    }

    if (gGameState->frames % 2 == 0) {
        return;
    }

    static Vec3f sVelocity = { 0.0f, 0.0f, 0.0f };
    static Vec3f sAccel = { 0.0f, 0.0f, 0.0f };
    static Color_RGBA8 sPrimColor = { 255, 255, 255, 255 };
    static Color_RGBA8 sEnvColor = { 255, 128, 0, 255 };
    Vec3f newPos;

    newPos.x = Rand_CenteredFloat(10.0f) + actor->world.pos.x;
    newPos.y = (Rand_ZeroOne() * 10.0f) + actor->world.pos.y;
    newPos.z = Rand_CenteredFloat(10.0f) + actor->world.pos.z;

    if (actor->id == ACTOR_EN_SI) {
        newPos.y = (Rand_ZeroOne() * 10.0f) + actor->world.pos.y - 5.0f;
    } else if (actor->id == ACTOR_EN_ITEM00) {
        newPos.x = Rand_CenteredFloat(20.0f) + actor->world.pos.x;
        newPos.y = (Rand_ZeroOne() * 10.0f) + actor->world.pos.y + 10.0f;
        newPos.z = Rand_CenteredFloat(20.0f) + actor->world.pos.z;
    }

    EffectSsKirakira_SpawnDispersed(gPlayState, &newPos, &sVelocity, &sAccel, &sPrimColor, &sEnvColor, 2000, 16);
}

void Rando::DrawItem(RandoItemId randoItemId, RandoCheckId randoCheckId, Actor* actor) {
    if (DrawOoTxMmSharedModel(randoCheckId)) {
        return;
    }

    // Apply hilites with actor world pos before drawing
    if (actor != NULL) {
        func_800B8118(actor, gPlayState, 0);
        func_800B8050(actor, gPlayState, 0);
    }

    switch (randoItemId) {
        case RI_JUNK:
            Rando::DrawItem(Rando::CurrentJunkItem(randoCheckId), randoCheckId, actor);
            break;
        case RI_GREAT_BAY_SMALL_KEY:
        case RI_SNOWHEAD_SMALL_KEY:
        case RI_STONE_TOWER_SMALL_KEY:
        case RI_WOODFALL_SMALL_KEY:
            DrawSmallKey(randoItemId);
            break;
        case RI_GREAT_BAY_BOSS_KEY:
        case RI_SNOWHEAD_BOSS_KEY:
        case RI_STONE_TOWER_BOSS_KEY:
        case RI_WOODFALL_BOSS_KEY:
            DrawBossKey(randoItemId);
            break;
        case RI_SONG_TIME:
        case RI_SONG_STORMS:
        case RI_SONG_SUN:
        case RI_SONG_HEALING:
        case RI_SONG_SARIA:
        case RI_SONG_SOARING:
        case RI_SONG_SONATA:
        case RI_SONG_ELEGY:
        case RI_SONG_LULLABY_INTRO:
        case RI_SONG_LULLABY:
        case RI_SONG_OATH:
        case RI_SONG_EPONA:
        case RI_SONG_NOVA:
        case RI_SONG_DOUBLE_TIME:
        case RI_SONG_INVERTED_TIME:
            DrawSong(randoItemId);
            break;
        case RI_CLOCK_TOWN_STRAY_FAIRY:
        case RI_WOODFALL_STRAY_FAIRY:
        case RI_SNOWHEAD_STRAY_FAIRY:
        case RI_GREAT_BAY_STRAY_FAIRY:
        case RI_STONE_TOWER_STRAY_FAIRY:
            DrawStrayFairy(randoItemId);
            break;
        case RI_DOUBLE_DEFENSE:
            DrawDoubleDefense();
            break;
        case RI_MILK_REFILL:
            DrawMilkRefill();
            break;
        case RI_GS_TOKEN_SWAMP:
        case RI_GS_TOKEN_OCEAN:
            DrawSkulltulaToken(randoItemId, actor);
            break;
        case RI_OWL_CLOCK_TOWN_SOUTH:
        case RI_OWL_GREAT_BAY_COAST:
        case RI_OWL_IKANA_CANYON:
        case RI_OWL_MILK_ROAD:
        case RI_OWL_MOUNTAIN_VILLAGE:
        case RI_OWL_SNOWHEAD:
        case RI_OWL_SOUTHERN_SWAMP:
        case RI_OWL_STONE_TOWER:
        case RI_OWL_WOODFALL:
        case RI_OWL_ZORA_CAPE:
            DrawOwlStatue();
            break;
        case RI_TIME_DAY_1:
        case RI_TIME_NIGHT_1:
        case RI_TIME_DAY_2:
        case RI_TIME_NIGHT_2:
        case RI_TIME_DAY_3:
        case RI_TIME_NIGHT_3:
        case RI_TIME_PROGRESSIVE:
            DrawClock(randoItemId, actor);
            break;
        case RI_PROGRESSIVE_LULLABY:
        case RI_PROGRESSIVE_MAGIC:
        case RI_PROGRESSIVE_BOW:
        case RI_PROGRESSIVE_BOMB_BAG:
        case RI_PROGRESSIVE_SWORD:
        case RI_PROGRESSIVE_WALLET:
            Rando::DrawItem(Rando::ConvertItem(randoItemId, randoCheckId), randoCheckId, actor);
            break;
        case RI_SOUL_ENEMY_ALIEN:
        case RI_SOUL_ENEMY_ARMOS:
        case RI_SOUL_ENEMY_BAD_BAT:
        case RI_SOUL_ENEMY_BEAMOS:
        case RI_SOUL_ENEMY_BOE:
        case RI_SOUL_ENEMY_BUBBLE:
        case RI_SOUL_ENEMY_CAPTAIN_KEETA:
        case RI_SOUL_ENEMY_CHUCHU:
        case RI_SOUL_ENEMY_DEATH_ARMOS:
        case RI_SOUL_ENEMY_DEEP_PYTHON:
        case RI_SOUL_ENEMY_DEKU_BABA:
        case RI_SOUL_ENEMY_DEXIHAND:
        case RI_SOUL_ENEMY_DINOLFOS:
        case RI_SOUL_ENEMY_DODONGO:
        case RI_SOUL_ENEMY_DRAGONFLY:
        case RI_SOUL_ENEMY_EENO:
        case RI_SOUL_ENEMY_EYEGORE:
        case RI_SOUL_ENEMY_FREEZARD:
        case RI_SOUL_ENEMY_GARO:
        case RI_SOUL_ENEMY_GEKKO:
        case RI_SOUL_ENEMY_GIANT_BEE:
        case RI_SOUL_ENEMY_GOMESS:
        case RI_SOUL_ENEMY_GUAY:
        case RI_SOUL_ENEMY_HIPLOOP:
        case RI_SOUL_ENEMY_IGOS_DU_IKANA:
        case RI_SOUL_ENEMY_IRON_KNUCKLE:
        case RI_SOUL_ENEMY_KEESE:
        case RI_SOUL_ENEMY_LEEVER:
        case RI_SOUL_ENEMY_LIKE_LIKE:
        case RI_SOUL_ENEMY_MAD_SCRUB:
        case RI_SOUL_ENEMY_NEJIRON:
        case RI_SOUL_ENEMY_OCTOROK:
        case RI_SOUL_ENEMY_PEAHAT:
        case RI_SOUL_ENEMY_PIRATE:
        case RI_SOUL_ENEMY_POE:
        case RI_SOUL_ENEMY_REDEAD:
        case RI_SOUL_ENEMY_SHELLBLADE:
        case RI_SOUL_ENEMY_SKULLFISH:
        case RI_SOUL_ENEMY_SKULLTULA:
        case RI_SOUL_ENEMY_SNAPPER:
        case RI_SOUL_ENEMY_STALCHILD:
        case RI_SOUL_ENEMY_TAKKURI:
        case RI_SOUL_ENEMY_TEKTITE:
        case RI_SOUL_ENEMY_WALLMASTER:
        case RI_SOUL_ENEMY_WART:
        case RI_SOUL_ENEMY_WIZROBE:
        case RI_SOUL_ENEMY_WOLFOS:
            DrawSoul(randoItemId);
            break;
        case RI_SOUL_BOSS_GOHT:
            DrawGoht();
            break;
        case RI_SOUL_BOSS_GYORG:
            DrawGyorg();
            break;
        case RI_SOUL_BOSS_MAJORA:
            DrawMajora();
            break;
        case RI_SOUL_BOSS_ODOLWA:
            DrawOdolwa();
            break;
        case RI_SOUL_BOSS_TWINMOLD:
            DrawTwinmold();
            break;
        case RI_FROG_BLUE:
        case RI_FROG_CYAN:
        case RI_FROG_PINK:
        case RI_FROG_WHITE:
            DrawMinifrog(randoItemId, actor);
            break;
        case RI_ABILITY_SWIM:
            DrawAbilityItem(randoItemId, actor);
            break;
        case RI_TRIFORCE_PIECE_PREVIOUS:
        case RI_TRIFORCE_PIECE:
            DrawTriforcePiece(randoItemId);
            break;
        case RI_TRAP:
            Rando::DrawItem(Rando::CurrentTrapItem(randoCheckId), randoCheckId, actor);
            break;
        case RI_MAX_TRAP:
            DrawTrapModel();
            break;
        case RI_OCARINA_BUTTON_A:
        case RI_OCARINA_BUTTON_C_DOWN:
        case RI_OCARINA_BUTTON_C_LEFT:
        case RI_OCARINA_BUTTON_C_RIGHT:
        case RI_OCARINA_BUTTON_C_UP:
            DrawOcarinaButtonItem(randoItemId, actor);
            break;
        case RI_UNKNOWN:
            DrawOoTxMmPlaceholderChest();
            break;
        case RI_NONE:
            break;
        default:
            GetItem_Draw(gPlayState, Rando::StaticData::Items[randoItemId].drawId);
            break;
    }

    switch (randoItemId) {
        case RI_NONE:
        case RI_ABILITY_SWIM:
        case RI_PROGRESSIVE_MAGIC:
        case RI_SINGLE_MAGIC:
        case RI_DOUBLE_MAGIC:
        case RI_TIME_PROGRESSIVE:
            DrawSparkles(randoItemId, actor);
            break;
        default:
            break;
    }
}

static RegisterShipInitFunc initializeGICopyDLs(
    []() {
        // Small keys
        Gfx* baseDL = ResourceMgr_LoadGfxByName(gGiSmallKeyDL);
        memcpy(gGiSmallKeyCopyDL, baseDL, sizeof(gGiSmallKeyCopyDL));
        gGiSmallKeyCopyDL[5] = gsDPNoOp();
        gGiSmallKeyCopyDL[6] = gsDPNoOp();

        // Boss keys
        baseDL = ResourceMgr_LoadGfxByName(gGiBossKeyDL);
        memcpy(gGiBossKeyCopyDL, baseDL, sizeof(gGiBossKeyCopyDL));
        gGiBossKeyCopyDL[5] = gsDPNoOp();
        gGiBossKeyCopyDL[6] = gsDPNoOp();

        // Token Flame
        baseDL = ResourceMgr_LoadGfxByName(gSkulltulaTokenFlameDL);
        memcpy(gSkulltulaTokenFlameCopyDL, baseDL, sizeof(gSkulltulaTokenFlameCopyDL));
        gSkulltulaTokenFlameCopyDL[5] = gsDPNoOp();
        gSkulltulaTokenFlameCopyDL[6] = gsDPNoOp();
    },
    {});
