#include "dusk/settings.h"
#include "dusk/config.hpp"

namespace dusk {

UserSettings g_userSettings = {
    .video = {
        .enableFullscreen {"video.enableFullscreen", false},
        .enableVsync {"video.enableVsync", true},
        .lockAspectRatio {"video.lockAspectRatio", false},
        .enableFpsOverlay {"game.enableFpsOverlay", false},
        .fpsOverlayCorner {"game.fpsOverlayCorner", 0},
    },

    .audio = {
        .masterVolume {"audio.masterVolume", 80},
        .mainMusicVolume {"audio.mainMusicVolume", 100},
        .subMusicVolume {"audio.subMusicVolume", 100},
        .soundEffectsVolume {"audio.soundEffectsVolume", 100},
        .fanfareVolume {"audio.fanfareVolume", 100},
        .enableReverb {"audio.enableReverb", true},
        .enableHrtf {"audio.enableHrtf", false},
        .menuSounds {"audio.menuSounds", true},
    },

    .game = {
        .language { "game.language", GameLanguage::English },

        // Quality of Life
        .enableQuickTransform {"game.enableQuickTransform", false},
        .hideTvSettingsScreen {"game.hideTvSettingsScreen", true},
        .biggerWallets {"game.biggerWallets", false},
        .noReturnRupees {"game.noReturnRupees", false},
        .disableRupeeCutscenes {"game.disableRupeeCutscenes", false},
        .noSwordRecoil {"game.noSwordRecoil", false},
        .damageMultiplier {"game.damageMultiplier", 1},
        .noHeartDrops {"game.noHeartDrops", false},
        .instantDeath {"game.instantDeath", false},
        .fastClimbing {"game.fastClimbing", false},
        .noMissClimbing {"game.noMissClimbing", false},
        .fastTears {"game.fastTears", false},
        .no2ndFishForCat {"game.no2ndFishForCat", false},
        .instantSaves {"game.instantSaves", false},
        .instantText {"game.instantText", false},
        .sunsSong {"game.sunsSong", false},
        .autoSave {"game.autoSave", false},

        // Preferences
        .enableMirrorMode {"game.enableMirrorMode", false},
        .minimalHUD {"game.minimalHUD", false},
        .pauseOnFocusLost {"game.pauseOnFocusLost", false},
        .enableLinkDollRotation {"game.enableLinkDollRotation", false},
        .enableAchievementToasts {"game.enableAchievementToasts", true},
        .enableControllerToasts {"game.enableControllerToasts", true},

        // Graphics
        .bloomMode {"game.bloomMode", BloomMode::Dusk},
        .bloomMultiplier {"game.bloomMultiplier", 1.0f},
        .disableWaterRefraction {"game.disableWaterRefraction", false},
        .enableFrameInterpolation {"game.enableFrameInterpolation", false},
        .internalResolutionScale {"game.internalResolutionScale", 0},
        .shadowResolutionMultiplier {"game.shadowResolutionMultiplier", 1},
        .enableDepthOfField {"game.enableDepthOfField", true},
        .enableMapBackground {"game.enableMapBackground", true},

        // Audio
        .noLowHpSound {"game.noLowHpSound", false},
        .midnasLamentNonStop {"game.midnasLamentNonStop", false},

        // Input
        .gyroMode {"game.gyroMode", GyroMode::Sensor},
        .enableGyroAim {"game.enableGyroAim", false},
        .enableGyroRollgoal {"game.enableGyroRollgoal", false},
        .gyroSensitivityX {"game.gyroSensitivityX", 1.0f},
        .gyroSensitivityY {"game.gyroSensitivityY", 1.0f},
        .gyroSensitivityRollgoal {"game.gyroSensitivityRollgoal", 1.0f},
        .gyroSmoothing {"game.gyroSmoothing", 0.65f},
        .gyroDeadband {"game.gyroDeadband", 0.04f},
        .gyroInvertPitch {"game.gyroInvertPitch", false},
        .gyroInvertYaw {"game.gyroInvertYaw", false},
        .freeCamera {"game.freeCamera", false},
        .invertCameraXAxis {"game.invertCameraXAxis", false},
        .invertCameraYAxis {"game.invertCameraYAxis", false},
        .invertFirstPersonXAxis {"game.invertFirstPersonXAxis", false},
        .invertFirstPersonYAxis {"game.invertFirstPersonYAxis", false},
        .freeCameraSensitivity {"game.freeCameraSensitivity", 1.0f},
        .debugFlyCam {"game.debugFlyCam", false},
        .debugFlyCamLockEvents {"game.debugFlyCamLockEvents", true},
        .allowBackgroundInput {"game.allowBackgroundInput", true},
        .enableInputBuffering {"game.enableInputBuffering", false},

        // Cheats
        .infiniteHearts {"game.infiniteHearts", false},
        .infiniteArrows {"game.infiniteArrows", false},
        .infiniteBombs {"game.infiniteBombs", false},
        .infiniteOil {"game.infiniteOil", false},
        .infiniteOxygen {"game.infiniteOxygen", false},
        .infiniteRupees {"game.infiniteRupees", false},
        .enableIndefiniteItemDrops {"game.enableIndefiniteItemDrops", false},
        .moonJump {"game.moonJump", false},
        .superClawshot {"game.superClawshot", false},
        .alwaysGreatspin {"game.alwaysGreatspin", false},
        .enableFastIronBoots {"game.enableFastIronBoots", false},
        .canTransformAnywhere {"game.canTransformAnywhere", false},
        .fastSpinner {"game.fastSpinner", false},
        .freeMagicArmor {"game.freeMagicArmor", false},

        // Technical
        .restoreWiiGlitches {"game.restoreWiiGlitches", false},

        // Controls
        .enableTurboKeybind {"game.enableTurboKeybind", false},

        // Tools
        .speedrunMode {"game.speedrunMode", false},
        .liveSplitEnabled {"game.liveSplitEnabled", false},
        .recordingMode {"game.recordingMode", false}
    },

    .backend = {
        .isoPath {"backend.isoPath", ""},
        .isoVerification {"backend.isoVerification", DiscVerificationState::Unknown},
        .graphicsBackend {"backend.graphicsBackend", "auto"},
        .skipPreLaunchUI {"backend.skipPreLaunchUI", false},
        .showPipelineCompilation {"backend.showPipelineCompilation", false},
        .enableExperimentalPbr {"backend.enableExperimentalPbr", false},
        .pbr = {
            .ambient {"backend.pbr.ambient", 0.30f},
            .ambientSpecular {"backend.pbr.ambientSpecular", 0.04f},
            .fillIntensity {"backend.pbr.fillIntensity", 0.20f},
            .fillDirX {"backend.pbr.fillDirX", 0.39f},
            .fillDirY {"backend.pbr.fillDirY", -0.44f},
            .fillDirZ {"backend.pbr.fillDirZ", -0.81f},
            .diffuseScale {"backend.pbr.diffuseScale", 1.0f},
            .specularScale {"backend.pbr.specularScale", 1.0f},
            .normalStrength {"backend.pbr.normalStrength", 1.0f},
            .flipNormalY {"backend.pbr.flipNormalY", false},
            .invertNormalHandedness {"backend.pbr.invertNormalHandedness", false},
            .skyAmbient {"backend.pbr.skyAmbient", 1.15f},
            .groundAmbient {"backend.pbr.groundAmbient", 0.45f},
            .horizonAmbient {"backend.pbr.horizonAmbient", 0.80f},
            .environmentTint {"backend.pbr.environmentTint", 1.0f},
            .useIbl {"backend.pbr.useIbl", true},
            .iblDiffuseStrength {"backend.pbr.iblDiffuseStrength", 1.0f},
            .iblSpecularStrength {"backend.pbr.iblSpecularStrength", 1.0f},
            .ordonSwordBlade = {
                .roughness {"backend.pbr.ordonSwordBlade.roughness", 0.18f},
                .metallic {"backend.pbr.ordonSwordBlade.metallic", 1.0f},
                .ambientOcclusion {"backend.pbr.ordonSwordBlade.ambientOcclusion", 1.0f},
                .specular {"backend.pbr.ordonSwordBlade.specular", 0.95f},
                .emissiveR {"backend.pbr.ordonSwordBlade.emissiveR", 0.0f},
                .emissiveG {"backend.pbr.ordonSwordBlade.emissiveG", 0.0f},
                .emissiveB {"backend.pbr.ordonSwordBlade.emissiveB", 0.0f},
                .emissiveStrength {"backend.pbr.ordonSwordBlade.emissiveStrength", 0.0f},
                .useRmaosMap {"backend.pbr.ordonSwordBlade.useRmaosMap", true},
                .useLooseMaps {"backend.pbr.ordonSwordBlade.useLooseMaps", true},
                .useNormalMap {"backend.pbr.ordonSwordBlade.useNormalMap", true},
                .useEmissiveMap {"backend.pbr.ordonSwordBlade.useEmissiveMap", true},
            },
            .masterSwordBlade = {
                .roughness {"backend.pbr.masterSwordBlade.roughness", 0.15f},
                .metallic {"backend.pbr.masterSwordBlade.metallic", 1.0f},
                .ambientOcclusion {"backend.pbr.masterSwordBlade.ambientOcclusion", 1.0f},
                .specular {"backend.pbr.masterSwordBlade.specular", 1.0f},
                .emissiveR {"backend.pbr.masterSwordBlade.emissiveR", 0.35f},
                .emissiveG {"backend.pbr.masterSwordBlade.emissiveG", 0.55f},
                .emissiveB {"backend.pbr.masterSwordBlade.emissiveB", 1.0f},
                .emissiveStrength {"backend.pbr.masterSwordBlade.emissiveStrength", 0.035f},
                .useRmaosMap {"backend.pbr.masterSwordBlade.useRmaosMap", true},
                .useLooseMaps {"backend.pbr.masterSwordBlade.useLooseMaps", true},
                .useNormalMap {"backend.pbr.masterSwordBlade.useNormalMap", true},
                .useEmissiveMap {"backend.pbr.masterSwordBlade.useEmissiveMap", true},
            },
        },
        .wasPresetChosen {"backend.wasPresetChosen", false},
        .checkForUpdates {"backend.checkForUpdates", true},
        .cardFileType {"backend.cardFileType", static_cast<int>(CARD_GCIFOLDER)},
        .enableAdvancedSettings {"backend.enableAdvancedSettings", false},
    }
};

UserSettings& getSettings() {
    return g_userSettings;
}

static void registerPbrSwordMaterialSettings(UserSettings::PbrSwordMaterialSettings& settings) {
    Register(settings.roughness);
    Register(settings.metallic);
    Register(settings.ambientOcclusion);
    Register(settings.specular);
    Register(settings.emissiveR);
    Register(settings.emissiveG);
    Register(settings.emissiveB);
    Register(settings.emissiveStrength);
    Register(settings.useRmaosMap);
    Register(settings.useLooseMaps);
    Register(settings.useNormalMap);
    Register(settings.useEmissiveMap);
}

static void registerPbrSettings(UserSettings::PbrSettings& settings) {
    Register(settings.ambient);
    Register(settings.ambientSpecular);
    Register(settings.fillIntensity);
    Register(settings.fillDirX);
    Register(settings.fillDirY);
    Register(settings.fillDirZ);
    Register(settings.diffuseScale);
    Register(settings.specularScale);
    Register(settings.normalStrength);
    Register(settings.flipNormalY);
    Register(settings.invertNormalHandedness);
    Register(settings.skyAmbient);
    Register(settings.groundAmbient);
    Register(settings.horizonAmbient);
    Register(settings.environmentTint);
    Register(settings.useIbl);
    Register(settings.iblDiffuseStrength);
    Register(settings.iblSpecularStrength);
    registerPbrSwordMaterialSettings(settings.ordonSwordBlade);
    registerPbrSwordMaterialSettings(settings.masterSwordBlade);
}

void registerSettings() {
    // Video
    Register(g_userSettings.video.enableFullscreen);
    Register(g_userSettings.video.enableVsync);
    Register(g_userSettings.video.lockAspectRatio);
    Register(g_userSettings.video.enableFpsOverlay);
    Register(g_userSettings.video.fpsOverlayCorner);

    // Audio
    Register(g_userSettings.audio.masterVolume);
    Register(g_userSettings.audio.mainMusicVolume);
    Register(g_userSettings.audio.subMusicVolume);
    Register(g_userSettings.audio.soundEffectsVolume);
    Register(g_userSettings.audio.fanfareVolume);
    Register(g_userSettings.audio.enableReverb);
    Register(g_userSettings.audio.enableHrtf);
    Register(g_userSettings.audio.menuSounds);

    // Game
    Register(g_userSettings.game.language);
    Register(g_userSettings.game.enableQuickTransform);
    Register(g_userSettings.game.hideTvSettingsScreen);
    Register(g_userSettings.game.biggerWallets);
    Register(g_userSettings.game.noReturnRupees);
    Register(g_userSettings.game.disableRupeeCutscenes);
    Register(g_userSettings.game.noSwordRecoil);
    Register(g_userSettings.game.damageMultiplier);
    Register(g_userSettings.game.noHeartDrops);
    Register(g_userSettings.game.instantDeath);
    Register(g_userSettings.game.fastClimbing);
    Register(g_userSettings.game.fastTears);
    Register(g_userSettings.game.no2ndFishForCat);
    Register(g_userSettings.game.instantSaves);
    Register(g_userSettings.game.instantText);
    Register(g_userSettings.game.sunsSong);
    Register(g_userSettings.game.autoSave);
    Register(g_userSettings.game.enableMirrorMode);
    Register(g_userSettings.game.invertCameraXAxis);
    Register(g_userSettings.game.invertCameraYAxis);
    Register(g_userSettings.game.invertFirstPersonXAxis);
    Register(g_userSettings.game.invertFirstPersonYAxis);
    Register(g_userSettings.game.freeCameraSensitivity);
    Register(g_userSettings.game.minimalHUD);
    Register(g_userSettings.game.pauseOnFocusLost);
    Register(g_userSettings.game.bloomMode);
    Register(g_userSettings.game.bloomMultiplier);
    Register(g_userSettings.game.disableWaterRefraction);
    Register(g_userSettings.game.internalResolutionScale);
    Register(g_userSettings.game.shadowResolutionMultiplier);
    Register(g_userSettings.game.enableDepthOfField);
    Register(g_userSettings.game.enableMapBackground);
    Register(g_userSettings.game.enableFastIronBoots);
    Register(g_userSettings.game.canTransformAnywhere);
    Register(g_userSettings.game.freeMagicArmor);
    Register(g_userSettings.game.restoreWiiGlitches);
    Register(g_userSettings.game.enableLinkDollRotation);
    Register(g_userSettings.game.enableAchievementToasts);
    Register(g_userSettings.game.enableControllerToasts);
    Register(g_userSettings.game.noMissClimbing);
    Register(g_userSettings.game.noLowHpSound);
    Register(g_userSettings.game.midnasLamentNonStop);
    Register(g_userSettings.game.enableTurboKeybind);
    Register(g_userSettings.game.speedrunMode);
    Register(g_userSettings.game.liveSplitEnabled);
    Register(g_userSettings.game.recordingMode);
    Register(g_userSettings.game.fastSpinner);
    Register(g_userSettings.game.infiniteHearts);
    Register(g_userSettings.game.infiniteArrows);
    Register(g_userSettings.game.infiniteBombs);
    Register(g_userSettings.game.infiniteOil);
    Register(g_userSettings.game.infiniteOxygen);
    Register(g_userSettings.game.infiniteRupees);
    Register(g_userSettings.game.enableIndefiniteItemDrops);
    Register(g_userSettings.game.moonJump);
    Register(g_userSettings.game.superClawshot);
    Register(g_userSettings.game.alwaysGreatspin);
    Register(g_userSettings.game.enableFrameInterpolation);
    Register(g_userSettings.game.gyroMode);
    Register(g_userSettings.game.enableGyroAim);
    Register(g_userSettings.game.enableGyroRollgoal);
    Register(g_userSettings.game.gyroSensitivityX);
    Register(g_userSettings.game.gyroSensitivityY);
    Register(g_userSettings.game.gyroSensitivityRollgoal);
    Register(g_userSettings.game.gyroDeadband);
    Register(g_userSettings.game.gyroSmoothing);
    Register(g_userSettings.game.gyroInvertPitch);
    Register(g_userSettings.game.gyroInvertYaw);
    Register(g_userSettings.game.freeCamera);
    Register(g_userSettings.game.debugFlyCam);
    Register(g_userSettings.game.debugFlyCamLockEvents);
    Register(g_userSettings.game.allowBackgroundInput);
    Register(g_userSettings.game.enableInputBuffering);

    Register(g_userSettings.backend.isoPath);
    Register(g_userSettings.backend.isoVerification);
    Register(g_userSettings.backend.graphicsBackend);
    Register(g_userSettings.backend.skipPreLaunchUI);
    Register(g_userSettings.backend.showPipelineCompilation);
    Register(g_userSettings.backend.enableExperimentalPbr);
    registerPbrSettings(g_userSettings.backend.pbr);
    Register(g_userSettings.backend.wasPresetChosen);
    Register(g_userSettings.backend.checkForUpdates);
    Register(g_userSettings.backend.cardFileType);
    Register(g_userSettings.backend.enableAdvancedSettings);
}

// Transient settings

static TransientSettings g_transientSettings = {
    .collisionView = {
        .enableTerrainView = false,
        .enableWireframe = false,
        .enableAtView = false,
        .enableTgView = false,
        .enableCoView = false,
        .terrainViewOpacity = 50.0f,
        .colliderViewOpacity = 50.0f,
        .drawRange = 100.0f,
    },
    .skipFrameRateLimit = false,
};

TransientSettings& getTransientSettings() {
    return g_transientSettings;
}

}
