#include "fmt/format.h"
#include "imgui.h"
#include "aurora/gfx.h"

#include "ImGuiConfig.hpp"
#include "dusk/config.hpp"
#include "dusk/hotkeys.h"
#include "dusk/io.hpp"
#include "dusk/lighting/lighting_features.h"
#include "dusk/settings.h"
#include "ImGuiConsole.hpp"
#include "ImGuiLightingTools.hpp"
#include "ImGuiMenuTools.hpp"
#include "ImGuiPbrTools.hpp"

#include "ImGuiEngine.hpp"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_horse.h"
#include "d/d_com_inf_game.h"
#include "dusk/data.hpp"
#include "dusk/dusk.h"
#include "dusk/main.h"
#include "m_Do/m_Do_main.h"

#include <aurora/lib/internal.hpp>
#include <SDL3/SDL_misc.h>

#include <algorithm>
#include <cmath>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace aurora::gx {
extern bool enableLodBias;
}

namespace {

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
            ImGui::MenuItem("State Share", hotkeys::SHOW_STATE_SHARE, &m_showStateShare);

            ImGui::EndDisabled();

            if (!dusk::IsGameLaunched) {
                ImGui::EndDisabled();
            }

#if DUSK_CAN_OPEN_DATA_FOLDER
            ImGui::Separator();
            if (ImGui::MenuItem("Open Data Folder")) {
                data::open_data_path();
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
                if (ImGui::BeginMenu("Fog")) {
                    DrawFogOverrideControls();
                    ImGui::Separator();
                    DrawCameraFogOverrideControls();
                    ImGui::EndMenu();
                }

                ImGui::MenuItem("Bloom", nullptr, &m_showBloomWindow);

                ImGui::Separator();
                if (ImGui::BeginMenu("Experimental Rendering")) {
                    dusk::imgui_pbr::DrawExperimentalPbrToggle();
                    dusk::imgui_lighting::DrawExperimentalLightingToggle();

                    ImGui::Separator();
                    ImGui::MenuItem("PBR Materials", nullptr, &m_showPbrWindow);
                    ImGui::MenuItem("Lighting And Shadows", nullptr, &m_showPbrEnhancedLightingWindow);
                    ImGui::MenuItem("Light Editor", nullptr, &m_showLightingSceneEditorWindow);
                    ImGui::MenuItem("IBL Status Overlay", nullptr, &m_showPbrIblOverlay);
                    ImGui::MenuItem("Lighting Scene Overlay", nullptr, &m_showLightingSceneOverlay);
                    ImGui::EndMenu();
                }
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
            ImGui::MenuItem("Heap Viewer", hotkeys::SHOW_HEAP_VIEWER, &m_showHeapOverlay);
            ImGui::MenuItem("Player Info", hotkeys::SHOW_PLAYER_INFO, &m_showPlayerInfo);
            ImGui::MenuItem("Debug Camera", hotkeys::SHOW_DEBUG_CAMERA, &m_showCameraOverlay);
            ImGui::MenuItem("Audio Debug", hotkeys::SHOW_AUDIO_DEBUG, &m_showAudioDebug);
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
        dusk::imgui_pbr::DrawMaterialOverrideWindow(m_showPbrWindow);
    }

    void ImGuiMenuTools::ShowPbrEnhancedLightingWindow() {
        dusk::imgui_pbr::DrawLightingWindow(m_showPbrEnhancedLightingWindow);
    }

    void ImGuiMenuTools::ShowLightingSceneEditorWindow() {
        dusk::imgui_lighting::DrawSceneEditorWindow(m_showLightingSceneEditorWindow);
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
        if (!getSettings().backend.enableAdvancedSettings || !m_showPbrIblOverlay ||
            !dusk::lighting::pbr_material_rendering_enabled())
        {
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
            dusk::imgui_lighting::DrawIblStatusContents();
            ShowCornerContextMenu(m_pbrIblOverlayCorner, m_debugOverlayCorner);
        }
        ImGui::End();

        ImGui::PopFont();
    }

    void ImGuiMenuTools::ShowLightingSceneOverlay() {
        if (!getSettings().backend.enableAdvancedSettings || !m_showLightingSceneOverlay ||
            !dusk::lighting::experimental_lighting_enabled())
        {
            return;
        }

        dusk::imgui_lighting::DrawSceneOverlayContents();
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
