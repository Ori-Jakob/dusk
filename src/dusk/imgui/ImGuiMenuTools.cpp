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

}  // namespace

namespace dusk {
    ImGuiMenuTools::ImGuiMenuTools() {}

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
                bool enableExperimentalPbr = getSettings().backend.enableExperimentalPbr;
                if (ImGui::Checkbox("Experimental PBR material override", &enableExperimentalPbr)) {
                    getSettings().backend.enableExperimentalPbr.setValue(enableExperimentalPbr);
                    pbr_settings::apply();
                    config::Save();
                }
                if (enableExperimentalPbr) {
                    auto& pbr = getSettings().backend.pbr;

                    ImGui::Separator();
                    ImGui::TextDisabled("PBR Lighting");
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
                        pbr_settings::apply_lighting();
                        config::Save();
                    }
                    if (ImGui::Button("Reset Lighting##pbr")) {
                        pbr_settings::reset_lighting();
                        config::Save();
                    }

                    ImGui::Separator();
                    ImGui::TextDisabled("PBR Material Response");
                    float pbrDiffuseScale = pbr.diffuseScale;
                    float pbrSpecularScale = pbr.specularScale;
                    bool materialChanged = false;
                    materialChanged |= ImGui::SliderFloat("Diffuse Scale##pbr", &pbrDiffuseScale, 0.0f, 4.0f);
                    materialChanged |= ImGui::SliderFloat("Specular Scale##pbr", &pbrSpecularScale, 0.0f, 8.0f);
                    if (materialChanged) {
                        pbr.diffuseScale.setValue(pbrDiffuseScale);
                        pbr.specularScale.setValue(pbrSpecularScale);
                        pbr_settings::apply_material_scales();
                        config::Save();
                    }
                    if (ImGui::Button("Reset Material Response##pbr")) {
                        pbr_settings::reset_material_scales();
                        config::Save();
                    }

                    ImGui::Separator();
                    ImGui::TextDisabled("PBR Ambient Gradient");
                    float pbrSkyAmbient = pbr.skyAmbient;
                    float pbrGroundAmbient = pbr.groundAmbient;
                    float pbrHorizonAmbient = pbr.horizonAmbient;
                    float pbrEnvironmentTint = pbr.environmentTint;
                    bool ambientGradientChanged = false;
                    ambientGradientChanged |= ImGui::SliderFloat("Sky Ambient##pbr", &pbrSkyAmbient, 0.0f, 4.0f);
                    ambientGradientChanged |=
                        ImGui::SliderFloat("Ground Ambient##pbr", &pbrGroundAmbient, 0.0f, 4.0f);
                    ambientGradientChanged |=
                        ImGui::SliderFloat("Horizon Ambient##pbr", &pbrHorizonAmbient, 0.0f, 4.0f);
                    ambientGradientChanged |=
                        ImGui::SliderFloat("Environment Tint##pbr", &pbrEnvironmentTint, 0.0f, 1.0f);
                    if (ambientGradientChanged) {
                        pbr.skyAmbient.setValue(pbrSkyAmbient);
                        pbr.groundAmbient.setValue(pbrGroundAmbient);
                        pbr.horizonAmbient.setValue(pbrHorizonAmbient);
                        pbr.environmentTint.setValue(pbrEnvironmentTint);
                        pbr_settings::apply_ambient_gradient();
                        config::Save();
                    }
                    if (ImGui::Button("Reset Ambient Gradient##pbr")) {
                        pbr_settings::reset_ambient_gradient();
                        config::Save();
                    }

                    ImGui::Separator();
                    ImGui::TextDisabled("PBR IBL");
                    bool pbrUseIbl = pbr.useIbl;
                    float pbrIblDiffuseStrength = pbr.iblDiffuseStrength;
                    float pbrIblSpecularStrength = pbr.iblSpecularStrength;
                    bool iblChanged = false;
                    iblChanged |= ImGui::Checkbox("Use IBL##pbr", &pbrUseIbl);
                    iblChanged |= ImGui::SliderFloat("IBL Diffuse##pbr", &pbrIblDiffuseStrength, 0.0f, 4.0f);
                    iblChanged |= ImGui::SliderFloat("IBL Specular##pbr", &pbrIblSpecularStrength, 0.0f, 4.0f);
                    if (iblChanged) {
                        pbr.useIbl.setValue(pbrUseIbl);
                        pbr.iblDiffuseStrength.setValue(pbrIblDiffuseStrength);
                        pbr.iblSpecularStrength.setValue(pbrIblSpecularStrength);
                        pbr_settings::apply_ibl();
                        config::Save();
                    }
                    if (ImGui::Button("Reset IBL##pbr")) {
                        pbr_settings::reset_ibl();
                        config::Save();
                    }

                    ImGui::Separator();
                    ImGui::TextDisabled("PBR Normal Maps");
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
                        pbr_settings::apply_normal();
                        config::Save();
                    }
                    if (ImGui::Button("Reset Normal Maps##pbr")) {
                        pbr_settings::reset_normal();
                        config::Save();
                    }

                    ImGui::Separator();
                    ImGui::TextDisabled("PBR Fill Direction");
                    float pbrFillDir[3] = {pbr.fillDirX, pbr.fillDirY, pbr.fillDirZ};
                    if (ImGui::SliderFloat3("Fill Direction##pbr", pbrFillDir, -1.0f, 1.0f)) {
                        pbr.fillDirX.setValue(pbrFillDir[0]);
                        pbr.fillDirY.setValue(pbrFillDir[1]);
                        pbr.fillDirZ.setValue(pbrFillDir[2]);
                        pbr_settings::apply_fill_direction();
                        config::Save();
                    }
                    if (ImGui::Button("Reset Fill Direction##pbr")) {
                        pbr_settings::reset_fill_direction();
                        config::Save();
                    }

                    ImGui::Separator();
                    ImGui::TextDisabled("Sword Material Overrides");
                    DrawPbrSwordMaterialControls("Ordon Sword Blade", pbr_material_override::SwordMaterialKind::Ordon);
                    DrawPbrSwordMaterialControls("Master Sword Blade", pbr_material_override::SwordMaterialKind::Master);
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
            ImGuiStringViewText(fmt::format(FMT_STRING("FPS: {:.2f}\n"), io.Framerate));
            if (frameUsagePct > 0.f) {
                ImGuiStringViewText(fmt::format(FMT_STRING("Frame usage: {:.1f}%\n"), frameUsagePct));
            }

            ImGui::Separator();

            ImGuiStringViewText(fmt::format(FMT_STRING("Backend: {}\n"), backend_name(aurora_get_backend())));

            ImGui::Separator();

            const auto& stats = lastFrameAuroraStats;

            ImGuiStringViewText(
                fmt::format(FMT_STRING("Queued pipelines:  {}\n"), stats.queuedPipelines));
            ImGuiStringViewText(
                fmt::format(FMT_STRING("Done pipelines:    {}\n"), stats.createdPipelines));
            ImGuiStringViewText(
                fmt::format(FMT_STRING("Draw call count:   {}\n"), stats.drawCallCount));
            ImGuiStringViewText(fmt::format(FMT_STRING("Merged draw calls: {}\n"),
                stats.mergedDrawCallCount));
            ImGuiStringViewText(fmt::format(FMT_STRING("Vertex size:       {}\n"),
                BytesToString(stats.lastVertSize)));
            ImGuiStringViewText(fmt::format(FMT_STRING("Uniform size:      {}\n"),
                BytesToString(stats.lastUniformSize)));
            ImGuiStringViewText(fmt::format(FMT_STRING("Index size:        {}\n"),
                BytesToString(stats.lastIndexSize)));
            ImGuiStringViewText(fmt::format(FMT_STRING("Storage size:      {}\n"),
                BytesToString(stats.lastStorageSize)));
            ImGuiStringViewText(fmt::format(FMT_STRING("Tex upload size:   {}\n"),
                BytesToString(stats.lastTextureUploadSize)));
            ImGuiStringViewText(fmt::format(
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
            ImGuiStringViewText(
                player != nullptr
                ? fmt::format("Position: {: .4f}, {: .4f}, {: .4f}\n", player->current.pos.x, player->current.pos.y, player->current.pos.z)
                : "Position: ?, ?, ?\n"
            );

            ImGuiStringViewText(
                player != nullptr
                ? fmt::format("Angle: {0}\n", player->shape_angle.y)
                : "Angle: ?\n"
            );

            ImGuiStringViewText(
                player != nullptr
                ? fmt::format("Speed: {: .4f}\n", player->speedF)
                : "Speed: ?\n"
            );

            ImGui::Separator();
            ImGui::Text("Epona");
            ImGuiStringViewText(
                horse != nullptr
                ? fmt::format("Position: {: .4f}, {: .4f}, {: .4f}\n", horse->current.pos.x, horse->current.pos.y, horse->current.pos.z)
                : "Position: ?, ?, ?\n"
            );

            ImGuiStringViewText(
                horse != nullptr
                ? fmt::format("Angle: {0}\n", horse->shape_angle.y)
                : "Angle: ?\n"
            );

            ImGuiStringViewText(
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
