#include "fmt/format.h"
#include "imgui.h"
#include "aurora/gfx.h"

#include "ImGuiConfig.hpp"
#include "dusk/config.hpp"
#include "dusk/hotkeys.h"
#include "dusk/pbr_material_override.h"
#include "dusk/pbr_settings.h"
#include "dusk/settings.h"
#include "ImGuiConsole.hpp"
#include "ImGuiMenuTools.hpp"

#include "ImGuiEngine.hpp"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_horse.h"
#include "d/d_com_inf_game.h"
#include "dusk/dusk.h"
#include "dusk/main.h"
#include "m_Do/m_Do_main.h"

#include <aurora/lib/internal.hpp>
#include <SDL3/SDL_misc.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace aurora::gx {
extern bool enableLodBias;
}

namespace {

constexpr const char* PbrDebugModeLabels[] = {
    "Off",
    "Albedo",
    "Roughness",
    "Metallic",
    "AO",
    "Specular",
    "Normal",
    "GX Light Tint",
    "Direct Diffuse",
    "Direct Specular",
    "IBL Diffuse",
    "IBL Specular",
};

const char* PbrIblSourceLabel(AuroraPbrIblSource source) {
    switch (source) {
    case AURORA_PBR_IBL_SOURCE_PROBE:
        return "Runtime Probe";
    case AURORA_PBR_IBL_SOURCE_AUTHORED:
        return "Authored Assets";
    case AURORA_PBR_IBL_SOURCE_FALLBACK:
        return "Fallback";
    }

    return "Unknown";
}

const char* EnabledLabel(bool enabled) {
    return enabled ? "yes" : "no";
}

void DrawPbrIblStatusContents() {
    const AuroraPbrIblStatus* status = aurora_get_pbr_ibl_status();
    if (status == nullptr) {
        ImGui::TextUnformatted("IBL status unavailable");
        return;
    }

    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("PBR enabled:       {}\n"), EnabledLabel(status->pbrEnabled)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("IBL enabled:       {}\n"), EnabledLabel(status->iblEnabled)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Requested source:  {}\n"), PbrIblSourceLabel(status->requestedSource)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Active source:     {}\n"), PbrIblSourceLabel(status->activeSource)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Active mips:       {}\n"), status->activePrefilterMipCount));

    ImGui::SeparatorText("Runtime Probe");
    const char* probeState = "idle";
    if (status->probeFilterPending) {
        probeState = "filter pending";
    } else if (status->probeCaptureInProgress) {
        probeState = "capturing";
    } else if (status->probeRefreshPending) {
        probeState = "refresh pending";
    }

    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("State:             {}\n"), probeState));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Available:         {}\n"), EnabledLabel(status->probeAvailable)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Camera valid:      {}\n"), EnabledLabel(status->probeCameraValid)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Resources ready:   {}\n"), EnabledLabel(status->probeCaptureResourcesReady)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Auto refresh:      {}\n"), EnabledLabel(status->probeAutoRefresh)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Capture face:      {}/6\n"), status->probeCaptureFace + 1));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Delay frames:      {}\n"), status->probeCaptureDelayFrames));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Frames since:      {}\n"), status->probeFramesSinceRefresh));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Raw cube:          {}x{}\n"), status->probeCubeSize,
                                    status->probeCubeSize));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Irradiance cube:   {}x{}\n"), status->probeIrradianceSize,
                                    status->probeIrradianceSize));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Prefilter mips:    {}\n"), status->probePrefilterMipCount));

    ImGui::SeparatorText("Authored IBL");
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Available:         {}\n"), EnabledLabel(status->authoredAvailable)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Global loaded:     {}\n"), EnabledLabel(status->authoredGlobalLoaded)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Stage loaded:      {}\n"), EnabledLabel(status->authoredStageLoaded)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Room loaded:       {}\n"), EnabledLabel(status->authoredRoomLoaded)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Stage:             {}\n"),
                                    status->authoredStage[0] != '\0' ? status->authoredStage : "(none)"));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Room:              {}\n"), status->authoredRoom));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Scene key:         {}\n"),
                                    status->authoredSceneKey[0] != '\0' ? status->authoredSceneKey : "(none)"));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Root:              {}\n"),
                                    status->authoredRoot[0] != '\0' ? status->authoredRoot : "(unset)"));
}

void DrawPbrSwordMaterialControls(const char* label, dusk::pbr_material_override::SwordMaterialKind kind) {
    using namespace dusk::pbr_material_override;

    if (!ImGui::TreeNode(label)) {
        return;
    }

    SwordBladeMaterial& material = sword_blade_material(kind);

    bool changed = false;
    changed |= ImGui::SliderFloat("Roughness", &material.roughness, 0.04f, 1.0f, "%.3f");
    changed |= ImGui::SliderFloat("Metallic", &material.metallic, 0.0f, 1.0f, "%.3f");
    changed |= ImGui::SliderFloat("Ambient Occlusion", &material.ambientOcclusion, 0.0f, 1.0f, "%.3f");
    changed |= ImGui::SliderFloat("Specular", &material.specular, 0.0f, 1.0f, "%.3f");
    changed |= ImGui::ColorEdit3("Emissive Color", material.emissiveColor);
    changed |= ImGui::SliderFloat("Emissive Strength", &material.emissiveStrength, 0.0f, 1.0f, "%.4f");
    changed |= ImGui::Checkbox("Use Replacement RMAOS", &material.useRmaosMap);
    changed |= ImGui::Checkbox("Use Replacement Loose Maps", &material.useLooseMaps);
    changed |= ImGui::Checkbox("Use Replacement Normal Map", &material.useNormalMap);
    changed |= ImGui::Checkbox("Use Replacement Emissive Map", &material.useEmissiveMap);

    if (ImGui::Button("Reset to Defaults")) {
        dusk::pbr_settings::reset_sword_material(kind);
        dusk::config::Save();
    } else if (changed) {
        dusk::pbr_settings::save_sword_material(kind);
        dusk::config::Save();
    }

    ImGui::TreePop();
}

void ApplyFogOverrideSettings() {
    const auto& backend = dusk::getSettings().backend;
    aurora_set_fog_override(backend.gxFogOverrideEnabled.getValue(), backend.gxFogExposure.getValue(),
                            backend.gxFogOpacity.getValue(), backend.gxFogColorR.getValue(),
                            backend.gxFogColorG.getValue(), backend.gxFogColorB.getValue());
}

void ResetFogOverrideSettings() {
    auto& backend = dusk::getSettings().backend;
    backend.gxFogOverrideEnabled.setValue(false);
    backend.gxFogExposure.setValue(1.0f);
    backend.gxFogOpacity.setValue(1.0f);
    backend.gxFogColorR.setValue(0.85f);
    backend.gxFogColorG.setValue(0.90f);
    backend.gxFogColorB.setValue(1.0f);
    ApplyFogOverrideSettings();
    dusk::config::Save();
}

void DrawFogOverrideControls() {
    auto& backend = dusk::getSettings().backend;

    bool enabled = backend.gxFogOverrideEnabled;
    if (ImGui::Checkbox("GX Fog Override", &enabled)) {
        backend.gxFogOverrideEnabled.setValue(enabled);
        ApplyFogOverrideSettings();
        dusk::config::Save();
    }

    if (!enabled) {
        return;
    }

    float exposure = backend.gxFogExposure;
    float opacity = backend.gxFogOpacity;
    float color[3] = {
        backend.gxFogColorR,
        backend.gxFogColorG,
        backend.gxFogColorB,
    };

    bool changed = false;
    changed |= ImGui::SliderFloat("Fog Exposure", &exposure, 0.0f, 4.0f, "%.2f");
    changed |= ImGui::SliderFloat("Fog Opacity", &opacity, 0.0f, 1.0f, "%.2f");
    changed |= ImGui::ColorEdit3("Fog Color", color);

    if (changed) {
        backend.gxFogExposure.setValue(exposure);
        backend.gxFogOpacity.setValue(opacity);
        backend.gxFogColorR.setValue(color[0]);
        backend.gxFogColorG.setValue(color[1]);
        backend.gxFogColorB.setValue(color[2]);
        ApplyFogOverrideSettings();
        dusk::config::Save();
    }

    if (ImGui::Button("Reset GX Fog Override")) {
        ResetFogOverrideSettings();
    }
}

void ResetCameraFogOverrideSettings() {
    auto& backend = dusk::getSettings().backend;
    backend.cameraFogOverrideEnabled.setValue(false);
    backend.cameraFogExposure.setValue(1.0f);
    backend.cameraFogOpacity.setValue(1.0f);
    backend.cameraFogColorR.setValue(1.0f);
    backend.cameraFogColorG.setValue(1.0f);
    backend.cameraFogColorB.setValue(1.0f);
    dusk::config::Save();
}

void DrawCameraFogOverrideControls() {
    auto& backend = dusk::getSettings().backend;

    bool enabled = backend.cameraFogOverrideEnabled;
    if (ImGui::Checkbox("Camera Fog Override", &enabled)) {
        backend.cameraFogOverrideEnabled.setValue(enabled);
        dusk::config::Save();
    }

    if (!enabled) {
        return;
    }

    float exposure = backend.cameraFogExposure;
    float opacity = backend.cameraFogOpacity;
    float color[3] = {
        backend.cameraFogColorR,
        backend.cameraFogColorG,
        backend.cameraFogColorB,
    };

    bool changed = false;
    changed |= ImGui::SliderFloat("Camera Fog Exposure", &exposure, 0.0f, 4.0f, "%.2f");
    changed |= ImGui::SliderFloat("Camera Fog Opacity", &opacity, 0.0f, 1.0f, "%.2f");
    changed |= ImGui::ColorEdit3("Camera Fog Color", color);

    if (changed) {
        backend.cameraFogExposure.setValue(exposure);
        backend.cameraFogOpacity.setValue(opacity);
        backend.cameraFogColorR.setValue(color[0]);
        backend.cameraFogColorG.setValue(color[1]);
        backend.cameraFogColorB.setValue(color[2]);
        dusk::config::Save();
    }

    if (ImGui::Button("Reset Camera Fog Override")) {
        ResetCameraFogOverrideSettings();
    }
}

void DrawPbrMaterialOverrideWindow(bool& open) {
    if (!open) {
        return;
    }

    if (!ImGui::Begin("PBR Material Override", &open)) {
        ImGui::End();
        return;
    }

    bool enableExperimentalPbr = dusk::getSettings().backend.enableExperimentalPbr;
    if (ImGui::Checkbox("Experimental PBR material override", &enableExperimentalPbr)) {
        dusk::getSettings().backend.enableExperimentalPbr.setValue(enableExperimentalPbr);
        dusk::pbr_settings::apply();
        dusk::config::Save();
    }

    if (!enableExperimentalPbr) {
        ImGui::End();
        return;
    }

    auto& pbr = dusk::getSettings().backend.pbr;

    ImGui::SeparatorText("PBR Lighting");
    float pbrAmbient = pbr.ambient;
    float pbrAmbientSpec = pbr.ambientSpecular;
    float pbrFillIntensity = pbr.fillIntensity;
    bool lightingChanged = false;
    lightingChanged |= ImGui::SliderFloat("Ambient##pbr", &pbrAmbient, 0.0f, 1.0f);
    lightingChanged |= ImGui::SliderFloat("Ambient Specular##pbr", &pbrAmbientSpec, 0.0f, 0.2f);
    lightingChanged |= ImGui::SliderFloat("Fill Light##pbr", &pbrFillIntensity, 0.0f, 1.0f);
    if (lightingChanged) {
        pbr.ambient.setValue(pbrAmbient);
        pbr.ambientSpecular.setValue(pbrAmbientSpec);
        pbr.fillIntensity.setValue(pbrFillIntensity);
        dusk::pbr_settings::apply_lighting();
        dusk::config::Save();
    }
    if (ImGui::Button("Reset Lighting##pbr")) {
        dusk::pbr_settings::reset_lighting();
        dusk::config::Save();
    }

    ImGui::SeparatorText("PBR Material Response");
    float pbrDiffuseScale = pbr.diffuseScale;
    float pbrSpecularScale = pbr.specularScale;
    bool materialChanged = false;
    materialChanged |= ImGui::SliderFloat("Diffuse Scale##pbr", &pbrDiffuseScale, 0.0f, 4.0f);
    materialChanged |= ImGui::SliderFloat("Specular Scale##pbr", &pbrSpecularScale, 0.0f, 8.0f);
    if (materialChanged) {
        pbr.diffuseScale.setValue(pbrDiffuseScale);
        pbr.specularScale.setValue(pbrSpecularScale);
        dusk::pbr_settings::apply_material_scales();
        dusk::config::Save();
    }
    if (ImGui::Button("Reset Material Response##pbr")) {
        dusk::pbr_settings::reset_material_scales();
        dusk::config::Save();
    }

    ImGui::SeparatorText("PBR Ambient Gradient");
    float pbrSkyAmbient = pbr.skyAmbient;
    float pbrGroundAmbient = pbr.groundAmbient;
    float pbrHorizonAmbient = pbr.horizonAmbient;
    float pbrEnvironmentTint = pbr.environmentTint;
    bool ambientGradientChanged = false;
    ambientGradientChanged |= ImGui::SliderFloat("Sky Ambient##pbr", &pbrSkyAmbient, 0.0f, 4.0f);
    ambientGradientChanged |= ImGui::SliderFloat("Ground Ambient##pbr", &pbrGroundAmbient, 0.0f, 4.0f);
    ambientGradientChanged |= ImGui::SliderFloat("Horizon Ambient##pbr", &pbrHorizonAmbient, 0.0f, 4.0f);
    ambientGradientChanged |= ImGui::SliderFloat("Environment Tint##pbr", &pbrEnvironmentTint, 0.0f, 1.0f);
    if (ambientGradientChanged) {
        pbr.skyAmbient.setValue(pbrSkyAmbient);
        pbr.groundAmbient.setValue(pbrGroundAmbient);
        pbr.horizonAmbient.setValue(pbrHorizonAmbient);
        pbr.environmentTint.setValue(pbrEnvironmentTint);
        dusk::pbr_settings::apply_ambient_gradient();
        dusk::config::Save();
    }
    if (ImGui::Button("Reset Ambient Gradient##pbr")) {
        dusk::pbr_settings::reset_ambient_gradient();
        dusk::config::Save();
    }

    ImGui::SeparatorText("PBR IBL");
    bool pbrUseIbl = pbr.useIbl;
    bool pbrUseAuthoredIbl = pbr.useAuthoredIbl;
    bool pbrAutoUpdateProbeIbl = pbr.autoUpdateProbeIbl;
    float pbrIblDiffuseStrength = pbr.iblDiffuseStrength;
    float pbrIblSpecularStrength = pbr.iblSpecularStrength;
    bool iblChanged = false;
    iblChanged |= ImGui::Checkbox("Use IBL##pbr", &pbrUseIbl);
    iblChanged |= ImGui::Checkbox("Use Authored IBL Assets##pbr", &pbrUseAuthoredIbl);
    iblChanged |= ImGui::Checkbox("Auto Update Runtime Probe##pbr", &pbrAutoUpdateProbeIbl);
    iblChanged |= ImGui::SliderFloat("IBL Diffuse##pbr", &pbrIblDiffuseStrength, 0.0f, 4.0f);
    iblChanged |= ImGui::SliderFloat("IBL Specular##pbr", &pbrIblSpecularStrength, 0.0f, 4.0f);
    if (iblChanged) {
        pbr.useIbl.setValue(pbrUseIbl);
        pbr.useAuthoredIbl.setValue(pbrUseAuthoredIbl);
        pbr.autoUpdateProbeIbl.setValue(pbrAutoUpdateProbeIbl);
        pbr.iblDiffuseStrength.setValue(pbrIblDiffuseStrength);
        pbr.iblSpecularStrength.setValue(pbrIblSpecularStrength);
        dusk::pbr_settings::apply_ibl();
        dusk::config::Save();
    }
    if (ImGui::Button("Refresh Runtime Probe##pbr")) {
        aurora_request_pbr_probe_refresh();
    }
    if (ImGui::Button("Reset IBL##pbr")) {
        dusk::pbr_settings::reset_ibl();
        dusk::config::Save();
    }
    if (ImGui::TreeNode("IBL Runtime Status##pbr")) {
        DrawPbrIblStatusContents();
        ImGui::TreePop();
    }

    ImGui::SeparatorText("PBR Normal Maps");
    float pbrNormalStrength = pbr.normalStrength;
    bool pbrFlipNormalY = pbr.flipNormalY;
    bool pbrInvertNormalHandedness = pbr.invertNormalHandedness;
    bool normalChanged = false;
    normalChanged |= ImGui::SliderFloat("Normal Strength##pbr", &pbrNormalStrength, 0.0f, 4.0f);
    normalChanged |= ImGui::Checkbox("Flip Normal Y##pbr", &pbrFlipNormalY);
    normalChanged |= ImGui::Checkbox("Invert Normal Handedness##pbr", &pbrInvertNormalHandedness);
    if (normalChanged) {
        pbr.normalStrength.setValue(pbrNormalStrength);
        pbr.flipNormalY.setValue(pbrFlipNormalY);
        pbr.invertNormalHandedness.setValue(pbrInvertNormalHandedness);
        dusk::pbr_settings::apply_normal();
        dusk::config::Save();
    }
    if (ImGui::Button("Reset Normal Maps##pbr")) {
        dusk::pbr_settings::reset_normal();
        dusk::config::Save();
    }

    ImGui::SeparatorText("PBR Fill Direction");
    float pbrFillDir[3] = {pbr.fillDirX, pbr.fillDirY, pbr.fillDirZ};
    if (ImGui::SliderFloat3("Fill Direction##pbr", pbrFillDir, -1.0f, 1.0f)) {
        pbr.fillDirX.setValue(pbrFillDir[0]);
        pbr.fillDirY.setValue(pbrFillDir[1]);
        pbr.fillDirZ.setValue(pbrFillDir[2]);
        dusk::pbr_settings::apply_fill_direction();
        dusk::config::Save();
    }
    if (ImGui::Button("Reset Fill Direction##pbr")) {
        dusk::pbr_settings::reset_fill_direction();
        dusk::config::Save();
    }

    ImGui::SeparatorText("PBR Debug Visualization");
    int pbrDebugMode = pbr.debugMode;
    if (ImGui::Combo("Debug Mode##pbr", &pbrDebugMode, PbrDebugModeLabels, IM_ARRAYSIZE(PbrDebugModeLabels))) {
        pbr.debugMode.setValue(pbrDebugMode);
        dusk::pbr_settings::apply_debug();
        dusk::config::Save();
    }
    if (ImGui::Button("Reset Debug Visualization##pbr")) {
        dusk::pbr_settings::reset_debug();
        dusk::config::Save();
    }

    ImGui::SeparatorText("Sword Material Overrides");
    DrawPbrSwordMaterialControls("Ordon Sword Blade", dusk::pbr_material_override::SwordMaterialKind::Ordon);
    DrawPbrSwordMaterialControls("Master Sword Blade", dusk::pbr_material_override::SwordMaterialKind::Master);

    ImGui::End();
}

}  // namespace

namespace dusk {
    ImGuiMenuTools::ImGuiMenuTools() {}

    void ImGuiMenuTools::ApplyGraphicsDebugSettings() {
        ApplyFogOverrideSettings();
    }

    void ImGuiMenuTools::draw() {
        if (ImGui::BeginMenu("Tools")) {
            if (!dusk::IsGameLaunched) {
                ImGui::BeginDisabled();
            }

            ImGui::BeginDisabled(getSettings().game.speedrunMode);

            ImGui::MenuItem("Save Editor", hotkeys::SHOW_SAVE_EDITOR, &m_showSaveEditor);
            ImGui::MenuItem("Map Loader", hotkeys::SHOW_MAP_LOADER, &m_showMapLoader);
            ImGui::MenuItem("State Share", hotkeys::SHOW_STATE_SHARE, &m_showStateShare);

            ImGui::EndDisabled();

            if (!dusk::IsGameLaunched) {
                ImGui::EndDisabled();
            }

            ImGui::Separator();
            ImGui::Checkbox("Show Input Viewer", &m_showInputViewer);

#if DUSK_CAN_OPEN_DATA_FOLDER
            ImGui::Separator();
            if (ImGui::MenuItem("Open Data Folder")) {
                OpenDataFolder();
            }
#endif

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug")) {
            ImGui::BeginDisabled(getSettings().game.speedrunMode);

            bool developmentMode = mDoMain::developmentMode == 1;
            if (ImGui::Checkbox("Development Mode", &developmentMode)) {
                mDoMain::developmentMode = developmentMode ? 1 : -1;
            }

            ImGui::Separator();

            auto& collisionView = getTransientSettings().collisionView;
            if (ImGui::BeginMenu("Graphics Settings")) {
                bool disableWaterRefraction = getSettings().game.disableWaterRefraction;
                if (ImGui::Checkbox("Disable Water Refraction", &disableWaterRefraction)) {
                    getSettings().game.disableWaterRefraction.setValue(disableWaterRefraction);
                    config::Save();
                }
                ImGui::Checkbox("Enable LOD Bias", &aurora::gx::enableLodBias);
                ImGui::Separator();
                DrawFogOverrideControls();
                ImGui::Separator();
                DrawCameraFogOverrideControls();

                ImGui::Separator();
                ImGui::MenuItem("PBR Material Override", nullptr, &m_showPbrWindow);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Collision View")) {
                ImGui::Checkbox("Enable Terrain view", &collisionView.enableTerrainView);
                ImGui::Checkbox("Enable wireframe view", &collisionView.enableWireframe);
                ImGui::SliderFloat("Opacity##terrain", &collisionView.terrainViewOpacity, 0.0f, 100.0f);
                ImGui::SliderFloat("Draw Range", &collisionView.drawRange, 0.0f, 1000.0f);
                ImGui::Separator();
                ImGui::Checkbox("Enable Attack Collider view", &collisionView.enableAtView);
                ImGui::Checkbox("Enable Target Collider view", &collisionView.enableTgView);
                ImGui::Checkbox("Enable Push Collider view", &collisionView.enableCoView);
                ImGui::SliderFloat("Opacity##colliders", &collisionView.colliderViewOpacity, 0.0f, 100.0f);
                ImGui::EndMenu();
            }

            if (!dusk::IsGameLaunched) {
                ImGui::BeginDisabled();
            }

            ImGui::MenuItem("Process Management", hotkeys::SHOW_PROCESS_MANAGEMENT, &m_showProcessManagement);
            ImGui::MenuItem("Debug Overlay", hotkeys::SHOW_DEBUG_OVERLAY, &m_showDebugOverlay);
            ImGui::MenuItem("PBR IBL Overlay", nullptr, &m_showPbrIblOverlay);
            ImGui::MenuItem("Heap Viewer", hotkeys::SHOW_HEAP_VIEWER, &m_showHeapOverlay);
            ImGui::MenuItem("Player Info", hotkeys::SHOW_PLAYER_INFO, &m_showPlayerInfo);
            ImGui::MenuItem("Debug Camera", hotkeys::SHOW_DEBUG_CAMERA, &m_showCameraOverlay);
            ImGui::MenuItem("Audio Debug", hotkeys::SHOW_AUDIO_DEBUG, &m_showAudioDebug);
            ImGui::MenuItem("Bloom", nullptr, &m_showBloomWindow);
            ImGui::MenuItem("Stub Log", nullptr, &m_showStubLog);
            ImGui::MenuItem("Actor Spawner", nullptr, &m_showActorSpawner);

            if (!dusk::IsGameLaunched) {
                ImGui::EndDisabled();
            }

            ImGui::MenuItem("OSReport Force", nullptr, &OSReportReallyForceEnable);

            ImGui::EndDisabled();

            ImGui::EndMenu();
        }
    }

    void ImGuiMenuTools::ShowPbrWindow() {
        DrawPbrMaterialOverrideWindow(m_showPbrWindow);
    }

    void ImGuiMenuTools::ShowDebugOverlay() {
        if (!getSettings().backend.enableAdvancedSettings ||
            !ImGuiConsole::CheckMenuViewToggle(ImGuiKey_F3, m_showDebugOverlay))
        {
            return;
        }

        ImGui::PushFont(ImGuiEngine::fontMono);

        ImGuiIO& io = ImGui::GetIO();
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;
        if (m_debugOverlayCorner != -1) {
            SetOverlayWindowLocation(m_debugOverlayCorner);
            windowFlags |= ImGuiWindowFlags_NoMove;
        }

        ImGui::SetNextWindowBgAlpha(0.65f);
        if (ImGui::Begin("Debug Overlay", nullptr, windowFlags)) {
            dusk::ImGuiStringViewText(fmt::format(FMT_STRING("FPS: {:.2f}\n"), io.Framerate));
            if (frameUsagePct > 0.f) {
                dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Frame usage: {:.1f}%\n"), frameUsagePct));
            }

            ImGui::Separator();

            dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Backend: {}\n"), backend_name(aurora_get_backend())));

            ImGui::Separator();

            const auto& stats = lastFrameAuroraStats;

            dusk::ImGuiStringViewText(
                fmt::format(FMT_STRING("Queued pipelines:  {}\n"), stats.queuedPipelines));
            dusk::ImGuiStringViewText(
                fmt::format(FMT_STRING("Done pipelines:    {}\n"), stats.createdPipelines));
            dusk::ImGuiStringViewText(
                fmt::format(FMT_STRING("Draw call count:   {}\n"), stats.drawCallCount));
            dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Merged draw calls: {}\n"),
                stats.mergedDrawCallCount));
            dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Vertex size:       {}\n"),
                BytesToString(stats.lastVertSize)));
            dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Uniform size:      {}\n"),
                BytesToString(stats.lastUniformSize)));
            dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Index size:        {}\n"),
                BytesToString(stats.lastIndexSize)));
            dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Storage size:      {}\n"),
                BytesToString(stats.lastStorageSize)));
            dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Tex upload size:   {}\n"),
                BytesToString(stats.lastTextureUploadSize)));
            dusk::ImGuiStringViewText(fmt::format(
                FMT_STRING("Total:             {}\n"),
                BytesToString(stats.lastVertSize + stats.lastUniformSize +
                    stats.lastIndexSize + stats.lastStorageSize +
                    stats.lastTextureUploadSize)));

            // TODO: persist to config
            ShowCornerContextMenu(m_debugOverlayCorner, m_cameraOverlayCorner);
        }
        ImGui::End();

        ImGui::PopFont();
    }

    void ImGuiMenuTools::ShowPbrIblOverlay() {
        if (!getSettings().backend.enableAdvancedSettings || !m_showPbrIblOverlay) {
            return;
        }

        ImGui::PushFont(ImGuiEngine::fontMono);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;
        if (m_pbrIblOverlayCorner != -1) {
            SetOverlayWindowLocation(m_pbrIblOverlayCorner);
            windowFlags |= ImGuiWindowFlags_NoMove;
        }

        ImGui::SetNextWindowBgAlpha(0.72f);
        if (ImGui::Begin("PBR IBL Overlay", nullptr, windowFlags)) {
            DrawPbrIblStatusContents();
            ShowCornerContextMenu(m_pbrIblOverlayCorner, m_debugOverlayCorner);
        }
        ImGui::End();

        ImGui::PopFont();
    }

    void ImGuiMenuTools::ShowPlayerInfo() {
        if (!getSettings().backend.enableAdvancedSettings ||
            !ImGuiConsole::CheckMenuViewToggle(ImGuiKey_F5, m_showPlayerInfo))
        {
            return;
        }

        ImGui::PushFont(ImGuiEngine::fontMono);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;
        if (m_playerInfoOverlayCorner != -1) {
            SetOverlayWindowLocation(m_playerInfoOverlayCorner);
            windowFlags |= ImGuiWindowFlags_NoMove;
        }

        ImGui::SetNextWindowBgAlpha(0.65f);

        if (ImGui::Begin("Player Info", nullptr, windowFlags)) {
            daAlink_c* player = (daAlink_c*)dComIfGp_getPlayer(0);
            daHorse_c* horse = dComIfGp_getHorseActor();

            ImGui::Text("Link");
            dusk::ImGuiStringViewText(
                player != nullptr
                ? fmt::format("Position: {: .4f}, {: .4f}, {: .4f}\n", player->current.pos.x, player->current.pos.y, player->current.pos.z)
                : "Position: ?, ?, ?\n"
            );

            dusk::ImGuiStringViewText(
                player != nullptr
                ? fmt::format("Angle: {0}\n", player->shape_angle.y)
                : "Angle: ?\n"
            );

            dusk::ImGuiStringViewText(
                player != nullptr
                ? fmt::format("Speed: {: .4f}\n", player->speedF)
                : "Speed: ?\n"
            );

            ImGui::Separator();
            ImGui::Text("Epona");
            dusk::ImGuiStringViewText(
                horse != nullptr
                ? fmt::format("Position: {: .4f}, {: .4f}, {: .4f}\n", horse->current.pos.x, horse->current.pos.y, horse->current.pos.z)
                : "Position: ?, ?, ?\n"
            );

            dusk::ImGuiStringViewText(
                horse != nullptr
                ? fmt::format("Angle: {0}\n", horse->shape_angle.y)
                : "Angle: ?\n"
            );

            dusk::ImGuiStringViewText(
                horse != nullptr
                ? fmt::format("Speed: {: .4f}\n", horse->speedF)
                : "Speed: ?\n"
            );

            ShowCornerContextMenu(m_playerInfoOverlayCorner, m_debugOverlayCorner);
        }

        ImGui::End();
        ImGui::PopFont();
    }
}
