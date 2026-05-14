#include "ImGuiPbrTools.hpp"

#include "imgui.h"
#include "aurora/gfx.h"

#include "dusk/config.hpp"
#include "dusk/pbr_material_override.h"
#include "dusk/pbr_settings.h"
#include "dusk/settings.h"
#include "ImGuiLightingTools.hpp"

namespace dusk::imgui_pbr {
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
    "Indirect Occlusion",
    "Dynamic GI",
    "Shadow Visibility",
};

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


bool DrawExperimentalPbrToggle() {
    bool enableExperimentalPbr = dusk::getSettings().backend.enableExperimentalPbr;
    if (ImGui::Checkbox("PBR Material Rendering", &enableExperimentalPbr)) {
        dusk::getSettings().backend.enableExperimentalPbr.setValue(enableExperimentalPbr);
        dusk::pbr_settings::apply();
        dusk::config::Save();
    }
    return enableExperimentalPbr;
}

void DrawPbrBaseLightingControls() {
    auto& pbr = dusk::getSettings().backend.pbr;

    ImGui::SeparatorText("Base Lighting");
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
    if (ImGui::Button("Reset Base Lighting##pbr")) {
        dusk::pbr_settings::reset_lighting();
        dusk::config::Save();
    }
}

void DrawPbrMaterialResponseControls() {
    auto& pbr = dusk::getSettings().backend.pbr;

    ImGui::SeparatorText("Material Response");
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
}

void DrawPbrAmbientGradientControls() {
    auto& pbr = dusk::getSettings().backend.pbr;

    ImGui::SeparatorText("Ambient Gradient");
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
}

void DrawPbrIndirectOcclusionControls() {
    auto& pbr = dusk::getSettings().backend.pbr;

    ImGui::SeparatorText("Indirect Occlusion");
    float pbrIndirectOcclusionStrength = pbr.indirectOcclusionStrength;
    float pbrIndirectOcclusionHorizon = pbr.indirectOcclusionHorizon;
    float pbrIndirectOcclusionSpecular = pbr.indirectOcclusionSpecular;
    bool indirectOcclusionChanged = false;
    indirectOcclusionChanged |= ImGui::SliderFloat("AO Influence##pbr", &pbrIndirectOcclusionStrength, 0.0f, 1.0f);
    indirectOcclusionChanged |= ImGui::SliderFloat("Horizon Influence##pbr", &pbrIndirectOcclusionHorizon, 0.0f, 1.0f);
    indirectOcclusionChanged |= ImGui::SliderFloat("Specular Influence##pbr", &pbrIndirectOcclusionSpecular, 0.0f, 1.0f);
    if (indirectOcclusionChanged) {
        pbr.indirectOcclusionStrength.setValue(pbrIndirectOcclusionStrength);
        pbr.indirectOcclusionHorizon.setValue(pbrIndirectOcclusionHorizon);
        pbr.indirectOcclusionSpecular.setValue(pbrIndirectOcclusionSpecular);
        dusk::pbr_settings::apply_indirect_occlusion();
        dusk::config::Save();
    }
    if (ImGui::Button("Reset Indirect Occlusion##pbr")) {
        dusk::pbr_settings::reset_indirect_occlusion();
        dusk::config::Save();
    }
}

void DrawPbrIblControls() {
    auto& pbr = dusk::getSettings().backend.pbr;

    ImGui::SeparatorText("IBL And Probes");
    bool pbrUseIbl = pbr.useIbl;
    bool pbrUseAuthoredIbl = pbr.useAuthoredIbl;
    bool pbrPeriodicProbeRefresh = pbr.periodicProbeRefresh;
    bool pbrLocalProbeGi = pbr.localProbeGi;
    bool pbrProbeCache = pbr.probeCache;
    bool pbrNearestProbeCache = pbr.nearestProbeCache;
    float pbrNearestProbeMaxDistance = pbr.nearestProbeMaxDistance;
    bool pbrSpatialProbeBlend = pbr.spatialProbeBlend;
    float pbrSpatialProbeBlendMaxDistance = pbr.spatialProbeBlendMaxDistance;
    bool pbrProbeBlending = pbr.probeBlending;
    int pbrProbeBlendFrames = pbr.probeBlendFrames;
    float pbrIblDiffuseStrength = pbr.iblDiffuseStrength;
    float pbrIblSpecularStrength = pbr.iblSpecularStrength;
    bool iblChanged = false;
    iblChanged |= ImGui::Checkbox("Use IBL##pbr", &pbrUseIbl);
    iblChanged |= ImGui::Checkbox("Use Authored IBL Assets##pbr", &pbrUseAuthoredIbl);
    iblChanged |= ImGui::Checkbox("Debug Periodic Probe Refresh##pbr", &pbrPeriodicProbeRefresh);
    iblChanged |= ImGui::Checkbox("Local Probe GI##pbr", &pbrLocalProbeGi);
    iblChanged |= ImGui::Checkbox("Runtime Probe Cache##pbr", &pbrProbeCache);
    iblChanged |= ImGui::Checkbox("Nearest Cached Probe##pbr", &pbrNearestProbeCache);
    if (pbrNearestProbeCache) {
        iblChanged |=
            ImGui::SliderFloat("Nearest Probe Max Distance##pbr", &pbrNearestProbeMaxDistance, 0.0f, 20000.0f);
    }
    iblChanged |= ImGui::Checkbox("Spatial Probe Blend##pbr", &pbrSpatialProbeBlend);
    if (pbrSpatialProbeBlend) {
        iblChanged |= ImGui::SliderFloat("Spatial Probe Max Distance##pbr", &pbrSpatialProbeBlendMaxDistance, 0.0f,
                                         30000.0f);
    }
    iblChanged |= ImGui::Checkbox("Blend Probe Transitions##pbr", &pbrProbeBlending);
    if (pbrProbeBlending) {
        iblChanged |= ImGui::SliderInt("Probe Blend Frames##pbr", &pbrProbeBlendFrames, 1, 180);
    }
    iblChanged |= ImGui::SliderFloat("IBL Diffuse##pbr", &pbrIblDiffuseStrength, 0.0f, 4.0f);
    iblChanged |= ImGui::SliderFloat("IBL Specular##pbr", &pbrIblSpecularStrength, 0.0f, 4.0f);
    if (iblChanged) {
        pbr.useIbl.setValue(pbrUseIbl);
        pbr.useAuthoredIbl.setValue(pbrUseAuthoredIbl);
        pbr.periodicProbeRefresh.setValue(pbrPeriodicProbeRefresh);
        pbr.localProbeGi.setValue(pbrLocalProbeGi);
        pbr.probeCache.setValue(pbrProbeCache);
        pbr.nearestProbeCache.setValue(pbrNearestProbeCache);
        pbr.nearestProbeMaxDistance.setValue(pbrNearestProbeMaxDistance);
        pbr.spatialProbeBlend.setValue(pbrSpatialProbeBlend);
        pbr.spatialProbeBlendMaxDistance.setValue(pbrSpatialProbeBlendMaxDistance);
        pbr.probeBlending.setValue(pbrProbeBlending);
        pbr.probeBlendFrames.setValue(pbrProbeBlendFrames);
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
        dusk::imgui_lighting::DrawIblStatusContents();
        ImGui::TreePop();
    }
}

void DrawPbrNormalMapControls() {
    auto& pbr = dusk::getSettings().backend.pbr;

    ImGui::SeparatorText("Normal Maps");
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
}

void DrawPbrFillDirectionControls() {
    auto& pbr = dusk::getSettings().backend.pbr;

    ImGui::SeparatorText("Fill Direction");
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
}

void DrawPbrDebugVisualizationControls() {
    auto& pbr = dusk::getSettings().backend.pbr;

    ImGui::SeparatorText("Debug Visualization");
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
}

void DrawLightingWindow(bool& open) {
    if (!open) {
        return;
    }

    if (!ImGui::Begin("Modern Lighting", &open)) {
        ImGui::End();
        return;
    }

    const bool enableExperimentalLighting = dusk::imgui_lighting::DrawExperimentalLightingToggle();
    if (enableExperimentalLighting) {
        if (ImGui::CollapsingHeader("Direct Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
            dusk::imgui_lighting::DrawEnhancedLightingControls();
        }

        if (ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen)) {
            dusk::imgui_lighting::DrawEnhancedShadowControls();
        }
    }

    ImGui::Separator();
    const bool enableExperimentalPbr = DrawExperimentalPbrToggle();
    if (enableExperimentalPbr) {
        if (ImGui::CollapsingHeader("PBR Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawPbrBaseLightingControls();
            DrawPbrFillDirectionControls();
        }

        if (ImGui::CollapsingHeader("PBR Indirect Lighting And IBL", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawPbrAmbientGradientControls();
            DrawPbrIndirectOcclusionControls();
            DrawPbrIblControls();
        }

        if (ImGui::CollapsingHeader("PBR Diagnostics")) {
            DrawPbrDebugVisualizationControls();
        }
    }

    ImGui::End();
}

void DrawMaterialOverrideWindow(bool& open) {
    if (!open) {
        return;
    }

    if (!ImGui::Begin("PBR Materials", &open)) {
        ImGui::End();
        return;
    }

    const bool enableExperimentalPbr = DrawExperimentalPbrToggle();
    if (!enableExperimentalPbr) {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("Surface Response", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawPbrMaterialResponseControls();
    }

    if (ImGui::CollapsingHeader("Normal Maps", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawPbrNormalMapControls();
    }

    if (ImGui::CollapsingHeader("Sword Material Overrides", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawPbrSwordMaterialControls("Ordon Sword Blade", dusk::pbr_material_override::SwordMaterialKind::Ordon);
        DrawPbrSwordMaterialControls("Master Sword Blade", dusk::pbr_material_override::SwordMaterialKind::Master);
    }

    ImGui::End();
}


}  // namespace dusk::imgui_pbr
