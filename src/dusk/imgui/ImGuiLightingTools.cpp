#include "ImGuiLightingTools.hpp"

#include "fmt/format.h"
#include "imgui.h"
#include "aurora/gfx.h"

#include "ImGuiConfig.hpp"
#include "ImGuiConsole.hpp"
#include "d/d_com_inf_game.h"
#include "dusk/config.hpp"
#include "dusk/io.hpp"
#include "dusk/lighting/light_tuning.h"
#include "dusk/lighting/lighting_features.h"
#include "dusk/lighting/lighting_scene.h"
#include "dusk/pbr_settings.h"
#include "dusk/settings.h"
#include "m_Do/m_Do_graphic.h"
#include "m_Do/m_Do_lib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace dusk::imgui_lighting {

constexpr const char* PbrEnhancedLightFalloffLabels[] = {
    "Legacy Radius",
    "Inverse Square",
};

struct ShadowModeOption {
    dusk::PbrEnhancedShadowMode mode;
    const char* label;
};

constexpr ShadowModeOption PbrEnhancedShadowModeOptions[] = {
    {dusk::PbrEnhancedShadowMode::AuroraShadowMaps, "Aurora Shadow Maps"},
    {dusk::PbrEnhancedShadowMode::Hybrid, "Hybrid Contact Shadows"},
    {dusk::PbrEnhancedShadowMode::OverrideDirection, "Override Original Direction"},
    {dusk::PbrEnhancedShadowMode::DisableGameShadows, "Suppress Original Shadows"},
};

struct LightShadowTypeOption {
    dusk::lighting::LightShadowType type;
    const char* label;
};

constexpr LightShadowTypeOption LightShadowTypeOptions[] = {
    {dusk::lighting::LightShadowType::None, "None"},
    {dusk::lighting::LightShadowType::LocalProjected, "Local Projected"},
    {dusk::lighting::LightShadowType::Directional, "Directional"},
    {dusk::lighting::LightShadowType::Point, "Point Cubemap Later"},
};

int ShadowModeOptionIndex(dusk::PbrEnhancedShadowMode mode) {
    for (int i = 0; i < IM_ARRAYSIZE(PbrEnhancedShadowModeOptions); ++i) {
        if (PbrEnhancedShadowModeOptions[i].mode == mode) {
            return i;
        }
    }

    return 0;
}

int LightShadowTypeOptionIndex(dusk::lighting::LightShadowType type) {
    for (int i = 0; i < IM_ARRAYSIZE(LightShadowTypeOptions); ++i) {
        if (LightShadowTypeOptions[i].type == type) {
            return i;
        }
    }

    return 1;
}

const char* LightShadowTypeLabel(dusk::lighting::LightShadowType type) {
    return LightShadowTypeOptions[LightShadowTypeOptionIndex(type)].label;
}

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

const char* AuroraSceneLightSourceLabel(uint32_t source) {
    switch (source) {
    case AURORA_SCENE_LIGHT_SOURCE_GAME_ENVIRONMENT:
        return "environment";
    case AURORA_SCENE_LIGHT_SOURCE_GAME_POINT:
        return "point";
    case AURORA_SCENE_LIGHT_SOURCE_GAME_EFFECT:
        return "effect";
    case AURORA_SCENE_LIGHT_SOURCE_AUTHORED:
        return "authored";
    default:
        return "unknown";
    }
}

const char* AuroraShadowTypeLabel(uint32_t type) {
    switch (type) {
    case AURORA_PBR_SHADOW_TYPE_NONE:
        return "none";
    case AURORA_PBR_SHADOW_TYPE_LOCAL_PROJECTED:
        return "local projected";
    case AURORA_PBR_SHADOW_TYPE_DIRECTIONAL:
        return "directional";
    case AURORA_PBR_SHADOW_TYPE_POINT:
        return "point cubemap";
    default:
        return "unknown";
    }
}

struct LightEditorState {
    std::string sceneKey;
    std::string selectedSource;
    int selectedIndex = -1;
    dusk::lighting::LightTuningRule selectedRule{};
    float selectionRadiusScale = dusk::lighting::DefaultSelectionRadiusScale;
    float selectionRadiusPadding = dusk::lighting::DefaultSelectionRadiusPadding;
    bool hasSelection = false;
    std::string status;
};

LightEditorState sLightEditor{};

bool DrawExperimentalLightingToggle();

const char* CurrentStageName() {
    const char* stage = dComIfGp_getStartStageName();
    return stage != nullptr ? stage : "";
}

int CurrentRoomNo() {
    return dComIfGp_roomControl_getStayNo();
}

void ResetLightEditorForScene(const dusk::lighting::LightingTuning& tuning) {
    if (sLightEditor.sceneKey == tuning.sceneKey) {
        return;
    }

    sLightEditor.sceneKey = tuning.sceneKey;
    sLightEditor.selectionRadiusScale = tuning.selectionRadiusScale;
    sLightEditor.selectionRadiusPadding = tuning.selectionRadiusPadding;
    sLightEditor.selectedSource.clear();
    sLightEditor.selectedIndex = -1;
    sLightEditor.selectedRule = {};
    sLightEditor.hasSelection = false;
    sLightEditor.status.clear();
}

void SelectLightForEditing(const dusk::lighting::SceneLight& light,
                           const dusk::lighting::LightingTuning& tuning) {
    const char* source = dusk::lighting::scene_light_source_name(light.source);
    sLightEditor.selectedSource = source;
    sLightEditor.selectedIndex = static_cast<int>(light.sourceIndex);
    sLightEditor.selectedRule = {};
    sLightEditor.selectedRule.source = source;
    sLightEditor.selectedRule.sourceIndex = static_cast<int>(light.sourceIndex);
    sLightEditor.selectedRule.setEnabled = true;
    sLightEditor.selectedRule.setCastsShadows = true;
    sLightEditor.selectedRule.scale =
        dusk::lighting::light_tuning_for_source(tuning, source, light.sourceIndex);
    sLightEditor.selectedRule.scale.shadowType = light.shadowType;
    sLightEditor.selectedRule.setPositionOverride = sLightEditor.selectedRule.scale.hasPositionOverride;
    if (!sLightEditor.selectedRule.scale.hasPositionOverride) {
        sLightEditor.selectedRule.scale.position = {light.worldPosition.x, light.worldPosition.y,
                                                    light.worldPosition.z};
    }
    sLightEditor.selectedRule.setIsFire = sLightEditor.selectedRule.scale.isFire;
    sLightEditor.hasSelection = true;
    sLightEditor.status.clear();
}

const dusk::lighting::SceneLight* FindSelectedLight(const dusk::lighting::SceneLightRegistry& registry) {
    if (!sLightEditor.hasSelection) {
        return nullptr;
    }

    for (uint32_t i = 0; i < registry.lightCount; ++i) {
        const auto& light = registry.lights[i];
        if (sLightEditor.selectedSource == dusk::lighting::scene_light_source_name(light.source) &&
            sLightEditor.selectedIndex == static_cast<int>(light.sourceIndex))
        {
            return &light;
        }
    }

    return nullptr;
}

void ApplySelectedLightRuleLive() {
    if (!sLightEditor.hasSelection) {
        return;
    }

    dusk::lighting::set_runtime_light_tuning_rule(CurrentStageName(), CurrentRoomNo(), sLightEditor.selectedRule);
}

void SetLightEditorStatus(bool success, const std::string& path, const std::string& error) {
    if (success) {
        sLightEditor.status = fmt::format(FMT_STRING("Saved {}"), path);
        return;
    }

    sLightEditor.status = fmt::format(FMT_STRING("Save failed: {}"), error.empty() ? "unknown error" : error);
}

void DrawPbrEnhancedLightingStatusContents() {
    const AuroraPbrEnhancedLightingStatus* status = aurora_get_pbr_enhanced_lighting_status();
    if (status == nullptr) {
        ImGui::TextUnformatted("Enhanced lighting status unavailable");
        return;
    }

    const char* falloff = status->falloff == AURORA_PBR_ENHANCED_LIGHT_FALLOFF_INVERSE_SQUARE
                              ? "Inverse Square"
                              : "Legacy Radius";
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Enabled:          {}\n"), EnabledLabel(status->enabled)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Debug enabled:    {}\n"), EnabledLabel(status->debugEnabled)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Active GPU lights:{}/{}\n"), status->lightCount,
                                    status->maxLightCount));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Scene API:        {} ({} submitted)\n"),
                    EnabledLabel(status->sceneLightApiBacked), status->submittedSceneLightCount));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("GPU storage:      {} ({} bytes)\n"),
                                    EnabledLabel(status->storageBacked), status->storageByteSize));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Falloff:          {}\n"), falloff));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Intensity scale:  {:.3f}\n"), status->intensityScale));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Dynamic GI:       {}\n"), EnabledLabel(status->dynamicGiEnabled)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("GI strength:      {:.3f}\n"), status->dynamicGiStrength));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("GI normal wrap:   {:.3f}\n"), status->dynamicGiNormalWrap));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("GI albedo bleed:  {:.3f}\n"), status->dynamicGiAlbedoInfluence));

    if (status->lightCount > 0) {
        const auto& light = status->strongestLight;
        dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Primary radius:   {:.1f}\n"), light.radius));
        dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Primary strength: {:.3f}\n"), light.intensity));
        dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Primary pos VS:   {:.1f}, {:.1f}, {:.1f}\n"),
                                        light.position[0], light.position[1], light.position[2]));
        dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Primary color:    {:.3f}, {:.3f}, {:.3f}\n"),
                                        light.color[0], light.color[1], light.color[2]));
    }
}

void DrawLightingSceneInventoryContents() {
    const auto& registry = dusk::lighting::scene_lights();
    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr) {
        stage = "";
    }
    const auto& tuning = dusk::lighting::current_lighting_tuning(stage, dComIfGp_roomControl_getStayNo());
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Registry valid:   {}\n"), EnabledLabel(registry.valid)));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Lights:           {} base, {} point, {} effect, {} total\n"),
                    registry.baseLightCount, registry.pointLightCount, registry.effectLightCount,
                    registry.lightCount));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Rejected/culling: {}\n"), registry.rejectedLightCount));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Reference:        {:.1f}, {:.1f}, {:.1f}\n"),
                                    registry.referencePosition.x, registry.referencePosition.y,
                                    registry.referencePosition.z));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Tuning scene:     {}\n"), tuning.sceneKey));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Tuning files:     {}\n"), tuning.sourceSummary));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Tuning scales:    {:.3f} ambient, {:.3f} direct\n"),
                                    tuning.enhancedAmbientScale, tuning.enhancedDirectScale));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Tuning rules:     {}\n"), tuning.lightRules.size()));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Ambient strength: {:.3f} total, {:.3f} luma\n"),
                    registry.accumulatedAmbientStrength, registry.accumulatedAmbientLuminance));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Ambient color:    {:.1f}, {:.1f}, {:.1f}\n"),
                                    registry.accumulatedAmbientColor[0], registry.accumulatedAmbientColor[1],
                                    registry.accumulatedAmbientColor[2]));
    if (registry.accumulatedAmbientWeight > 0.0f) {
        dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Ambient centroid: {:.1f}, {:.1f}, {:.1f}\n"),
                                        registry.accumulatedAmbientCentroid.x,
                                        registry.accumulatedAmbientCentroid.y,
                                        registry.accumulatedAmbientCentroid.z));
    }
    if (registry.dominantAmbientLightIndex >= 0 &&
        static_cast<uint32_t>(registry.dominantAmbientLightIndex) < registry.lightCount)
    {
        const auto& dominant = registry.lights[registry.dominantAmbientLightIndex];
        dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Dominant ambient: {} {} ({:.3f})\n"),
                                        dusk::lighting::scene_light_source_name(dominant.source),
                                        dominant.sourceIndex, dominant.referenceAmbientLuminance));
    }
    if (registry.shadowLightIndex >= 0 && static_cast<uint32_t>(registry.shadowLightIndex) < registry.lightCount) {
        const auto& shadow = registry.lights[registry.shadowLightIndex];
        dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Shadow source:    {} {} ({:.3f})\n"),
                                        dusk::lighting::scene_light_source_name(shadow.source),
                                        shadow.sourceIndex, shadow.shadowScore));
    }

    if (!ImGui::BeginTable("LightingSceneInventoryTable", 18,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX |
                               ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
                           ImVec2(0.0f, 280.0f)))
    {
        return;
    }

    ImGui::TableSetupColumn("Source");
    ImGui::TableSetupColumn("Slot");
    ImGui::TableSetupColumn("ID");
    ImGui::TableSetupColumn("Distance");
    ImGui::TableSetupColumn("Power");
    ImGui::TableSetupColumn("Amb");
    ImGui::TableSetupColumn("Amb Scale");
    ImGui::TableSetupColumn("Direct Scale");
    ImGui::TableSetupColumn("Shadow");
    ImGui::TableSetupColumn("Shadow Type");
    ImGui::TableSetupColumn("Shadow Score");
    ImGui::TableSetupColumn("Amb Luma");
    ImGui::TableSetupColumn("Amb RGB");
    ImGui::TableSetupColumn("Score");
    ImGui::TableSetupColumn("Color");
    ImGui::TableSetupColumn("Priority");
    ImGui::TableSetupColumn("Type");
    ImGui::TableSetupColumn("Position");
    ImGui::TableHeadersRow();

    for (uint32_t i = 0; i < registry.lightCount; ++i) {
        const auto& light = registry.lights[i];
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(dusk::lighting::scene_light_source_name(light.source));
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%u", light.sourceIndex);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("0x%llx", static_cast<unsigned long long>(light.stableId));
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.1f", light.distance);
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%.1f", light.power);
        ImGui::TableSetColumnIndex(5);
        ImGui::Text("%.3f", light.referenceAmbientStrength);
        ImGui::TableSetColumnIndex(6);
        ImGui::Text("%.2f", light.tuningAmbientScale);
        ImGui::TableSetColumnIndex(7);
        ImGui::Text("%.2f", light.tuningDirectScale);
        ImGui::TableSetColumnIndex(8);
        ImGui::TextUnformatted(EnabledLabel(light.castsShadow));
        ImGui::TableSetColumnIndex(9);
        ImGui::TextUnformatted(LightShadowTypeLabel(light.shadowType));
        ImGui::TableSetColumnIndex(10);
        ImGui::Text("%.4f", light.shadowScore);
        ImGui::TableSetColumnIndex(11);
        ImGui::Text("%.4f", light.referenceAmbientLuminance);
        ImGui::TableSetColumnIndex(12);
        ImGui::Text("%.1f %.1f %.1f", light.referenceAmbientColor[0], light.referenceAmbientColor[1],
                    light.referenceAmbientColor[2]);
        ImGui::TableSetColumnIndex(13);
        ImGui::Text("%.4f", light.selectionScore);
        ImGui::TableSetColumnIndex(14);
        ImGui::Text("%.2f %.2f %.2f", light.color[0], light.color[1], light.color[2]);
        ImGui::TableSetColumnIndex(15);
        ImGui::TextUnformatted(EnabledLabel(light.priority));
        ImGui::TableSetColumnIndex(16);
        ImGui::TextUnformatted(light.type == dusk::lighting::SceneLightType::Directional ? "directional" : "point");
        ImGui::TableSetColumnIndex(17);
        ImGui::Text("%.0f %.0f %.0f", light.worldPosition.x, light.worldPosition.y, light.worldPosition.z);
    }

    ImGui::EndTable();
}

void DrawSceneEditorWindow(bool& open) {
    if (!open) {
        return;
    }

    if (!ImGui::Begin("Lighting Scene Editor", &open)) {
        ImGui::End();
        return;
    }

    const bool enableExperimentalLighting = DrawExperimentalLightingToggle();
    if (!enableExperimentalLighting) {
        ImGui::End();
        return;
    }

    const auto& registry = dusk::lighting::scene_lights();
    const char* stage = CurrentStageName();
    const int room = CurrentRoomNo();
    const auto& tuning = dusk::lighting::current_lighting_tuning(stage, room);
    ResetLightEditorForScene(tuning);

    const std::string path = dusk::io::fs_path_to_string(dusk::lighting::room_lighting_tuning_path(stage, room));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Scene: {}\n"), tuning.sceneKey));
    ImGui::TextWrapped("Room file: %s", path.c_str());

    ImGui::SeparatorText("Activation Distance");
    bool radiusChanged = false;
    radiusChanged |= ImGui::SliderFloat("Radius Multiplier##lightEditor", &sLightEditor.selectionRadiusScale,
                                        0.1f, 8.0f, "%.2f");
    radiusChanged |= ImGui::SliderFloat("Extra Distance##lightEditor", &sLightEditor.selectionRadiusPadding,
                                        0.0f, 30000.0f, "%.0f");
    ImGui::TextUnformatted("Detection radius = light radius * multiplier + extra distance.");
    if (radiusChanged) {
        dusk::lighting::set_runtime_lighting_selection_radius(stage, room, sLightEditor.selectionRadiusScale,
                                                              sLightEditor.selectionRadiusPadding);
    }

    if (ImGui::Button("Save Activation Distance##lightEditor")) {
        std::string savedPath;
        std::string error;
        const bool saved = dusk::lighting::save_room_lighting_selection_radius(
            stage, room, sLightEditor.selectionRadiusScale, sLightEditor.selectionRadiusPadding, &savedPath, &error);
        SetLightEditorStatus(saved, savedPath, error);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Activation Distance##lightEditor")) {
        sLightEditor.selectionRadiusScale = dusk::lighting::DefaultSelectionRadiusScale;
        sLightEditor.selectionRadiusPadding = dusk::lighting::DefaultSelectionRadiusPadding;
        dusk::lighting::set_runtime_lighting_selection_radius(stage, room, sLightEditor.selectionRadiusScale,
                                                              sLightEditor.selectionRadiusPadding);
    }

    ImGui::SeparatorText("Detected Lights");
    if (!registry.valid) {
        ImGui::TextUnformatted("Lighting registry is not active yet.");
    } else if (registry.lightCount == 0) {
        ImGui::TextUnformatted("No detected lights. Increase activation distance or move closer to a light.");
    } else if (ImGui::BeginTable("LightingSceneEditorLightTable", 13,
                                 ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                     ImGuiTableFlags_SizingFixedFit,
                                 ImVec2(0.0f, 220.0f)))
    {
        ImGui::TableSetupColumn("Light");
        ImGui::TableSetupColumn("Distance");
        ImGui::TableSetupColumn("Power");
        ImGui::TableSetupColumn("Amb");
        ImGui::TableSetupColumn("Direct");
        ImGui::TableSetupColumn("Shadow");
        ImGui::TableSetupColumn("Shadow Type");
        ImGui::TableSetupColumn("Shadow Score");
        ImGui::TableSetupColumn("Fire");
        ImGui::TableSetupColumn("Override");
        ImGui::TableSetupColumn("Score");
        ImGui::TableSetupColumn("Color");
        ImGui::TableSetupColumn("Position");
        ImGui::TableHeadersRow();

        for (uint32_t i = 0; i < registry.lightCount; ++i) {
            const auto& light = registry.lights[i];
            const char* source = dusk::lighting::scene_light_source_name(light.source);
            const bool selected = sLightEditor.hasSelection && sLightEditor.selectedSource == source &&
                                  sLightEditor.selectedIndex == static_cast<int>(light.sourceIndex);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const std::string label = fmt::format(FMT_STRING("{} {}##lightEditor{}"), source, light.sourceIndex, i);
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                SelectLightForEditing(light, tuning);
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.1f", light.distance);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f", light.power);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f", light.tuningAmbientScale);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.2f", light.tuningDirectScale);
            ImGui::TableSetColumnIndex(5);
            ImGui::TextUnformatted(EnabledLabel(light.castsShadow));
            ImGui::TableSetColumnIndex(6);
            ImGui::TextUnformatted(LightShadowTypeLabel(light.shadowType));
            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%.3f", light.shadowScore);
            ImGui::TableSetColumnIndex(8);
            ImGui::TextUnformatted(EnabledLabel(light.isFire));
            ImGui::TableSetColumnIndex(9);
            ImGui::TextUnformatted(EnabledLabel(light.positionOverridden));
            ImGui::TableSetColumnIndex(10);
            ImGui::Text("%.3f", light.selectionScore);
            ImGui::TableSetColumnIndex(11);
            ImGui::Text("%.2f %.2f %.2f", light.color[0], light.color[1], light.color[2]);
            ImGui::TableSetColumnIndex(12);
            ImGui::Text("%.0f %.0f %.0f", light.worldPosition.x, light.worldPosition.y, light.worldPosition.z);
        }

        ImGui::EndTable();
    }

    ImGui::SeparatorText("Selected Light Rule");
    if (!sLightEditor.hasSelection) {
        ImGui::TextUnformatted("Select a detected light to edit its room tuning rule.");
    } else {
        dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Editing: {} {}\n"), sLightEditor.selectedSource,
                                              sLightEditor.selectedIndex));

        auto& rule = sLightEditor.selectedRule;
        bool ruleChanged = false;
        ruleChanged |= ImGui::Checkbox("Enabled##lightEditor", &rule.scale.enabled);
        rule.setEnabled = true;
        ruleChanged |= ImGui::SliderFloat("Ambient Scale##lightEditor", &rule.scale.ambientScale, 0.0f, 8.0f,
                                          "%.3f");
        ruleChanged |= ImGui::SliderFloat("Direct Scale##lightEditor", &rule.scale.directScale, 0.0f, 8.0f,
                                          "%.3f");
        ruleChanged |= ImGui::SliderFloat("Radius Scale##lightEditor", &rule.scale.radiusScale, 0.01f, 8.0f,
                                          "%.3f");
        ruleChanged |= ImGui::SliderFloat("Power Scale##lightEditor", &rule.scale.powerScale, 0.01f, 8.0f,
                                          "%.3f");
        ruleChanged |= ImGui::SliderFloat("Score Scale##lightEditor", &rule.scale.scoreScale, 0.0f, 8.0f,
                                          "%.3f");
        ruleChanged |= ImGui::Checkbox("Casts Shadows##lightEditor", &rule.scale.castsShadows);
        rule.setCastsShadows = true;
        int shadowTypeIndex = LightShadowTypeOptionIndex(rule.scale.shadowType);
        if (ImGui::Combo("Shadow Type##lightEditor", &shadowTypeIndex,
                         [](void*, int index, const char** outText) {
                             if (index < 0 || index >= IM_ARRAYSIZE(LightShadowTypeOptions)) {
                                 return false;
                             }
                             *outText = LightShadowTypeOptions[index].label;
                             return true;
                         },
                         nullptr, IM_ARRAYSIZE(LightShadowTypeOptions)))
        {
            rule.scale.shadowType = LightShadowTypeOptions[shadowTypeIndex].type;
            rule.scale.castsShadows = rule.scale.shadowType != dusk::lighting::LightShadowType::None;
            rule.setShadowType = true;
            rule.setCastsShadows = true;
            ruleChanged = true;
        }
        ruleChanged |= ImGui::SliderFloat("Shadow Priority##lightEditor", &rule.scale.shadowPriority, 0.0f, 8.0f,
                                          "%.3f");
        ruleChanged |= ImGui::ColorEdit3("Color Scale##lightEditor", rule.scale.colorScale.data());

        bool isFire = rule.scale.isFire;
        if (ImGui::Checkbox("Is Fire##lightEditor", &isFire)) {
            rule.scale.isFire = isFire;
            rule.setIsFire = true;
            ruleChanged = true;
        }

        bool positionOverride = rule.scale.hasPositionOverride;
        if (ImGui::Checkbox("Position Override##lightEditor", &positionOverride)) {
            rule.scale.hasPositionOverride = positionOverride;
            rule.setPositionOverride = true;
            if (positionOverride) {
                const dusk::lighting::SceneLight* selectedLight = FindSelectedLight(registry);
                if (selectedLight != nullptr) {
                    rule.scale.position = {selectedLight->worldPosition.x, selectedLight->worldPosition.y,
                                           selectedLight->worldPosition.z};
                }
            }
            ruleChanged = true;
        }
        if (rule.scale.hasPositionOverride) {
            std::array<float, 3> position = rule.scale.position;
            if (ImGui::DragFloat3("Position XYZ##lightEditor", position.data(), 10.0f, -500000.0f, 500000.0f,
                                  "%.1f"))
            {
                rule.scale.position = position;
                rule.setPositionOverride = true;
                ruleChanged = true;
            }
            if (ImGui::Button("Use Detected Position##lightEditor")) {
                const dusk::lighting::SceneLight* selectedLight = FindSelectedLight(registry);
                if (selectedLight != nullptr) {
                    rule.scale.position = {selectedLight->worldPosition.x, selectedLight->worldPosition.y,
                                           selectedLight->worldPosition.z};
                    rule.setPositionOverride = true;
                    ruleChanged = true;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Position Override##lightEditor")) {
                rule.scale.hasPositionOverride = false;
                rule.setPositionOverride = true;
                ruleChanged = true;
            }
        }

        if (ruleChanged) {
            ApplySelectedLightRuleLive();
        }

        if (ImGui::Button("Save Selected Light##lightEditor")) {
            ApplySelectedLightRuleLive();
            std::string savedPath;
            std::string error;
            const bool saved =
                dusk::lighting::save_room_light_tuning_rule(stage, room, sLightEditor.selectedRule, &savedPath, &error);
            SetLightEditorStatus(saved, savedPath, error);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Selected Light##lightEditor")) {
            sLightEditor.selectedRule.setEnabled = true;
            sLightEditor.selectedRule.setCastsShadows = true;
            sLightEditor.selectedRule.setShadowType = true;
            sLightEditor.selectedRule.setPositionOverride = true;
            sLightEditor.selectedRule.setIsFire = true;
            sLightEditor.selectedRule.scale = {};
            ApplySelectedLightRuleLive();
        }
    }

    if (!sLightEditor.status.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", sLightEditor.status.c_str());
    }

    ImGui::End();
}

ImU32 SceneLightOverlayColor(const dusk::lighting::SceneLight& light, int alpha) {
    const float maxComponent = std::max({light.color[0], light.color[1], light.color[2], 1.0f});
    const int r = static_cast<int>(std::clamp(light.color[0] / maxComponent, 0.0f, 1.0f) * 255.0f);
    const int g = static_cast<int>(std::clamp(light.color[1] / maxComponent, 0.0f, 1.0f) * 255.0f);
    const int b = static_cast<int>(std::clamp(light.color[2] / maxComponent, 0.0f, 1.0f) * 255.0f);
    return IM_COL32(r, g, b, alpha);
}

bool ProjectLightingOverlayPoint(const cXyz& worldPosition, ImVec2& screenPosition) {
    if (dComIfGd_getView() == nullptr) {
        return false;
    }

    Vec world = {worldPosition.x, worldPosition.y, worldPosition.z};
    Vec camera = {};
    mDoLib_pos2camera(&world, &camera);
    if (camera.z >= -1.0f) {
        return false;
    }

    Vec projected = {};
    mDoLib_project(&world, &projected);
    if (!std::isfinite(projected.x) || !std::isfinite(projected.y)) {
        return false;
    }

    const float sourceMinX = mDoGph_gInf_c::getMinXF();
    const float sourceMinY = mDoGph_gInf_c::getMinYF();
    const float sourceWidth = std::max(mDoGph_gInf_c::getWidthF(), 1.0f);
    const float sourceHeight = std::max(mDoGph_gInf_c::getHeightF(), 1.0f);
    const float normalizedX = (projected.x - sourceMinX) / sourceWidth;
    const float normalizedY = (projected.y - sourceMinY) / sourceHeight;

    constexpr float OverlayMargin = 0.15f;
    if (normalizedX < -OverlayMargin || normalizedX > 1.0f + OverlayMargin ||
        normalizedY < -OverlayMargin || normalizedY > 1.0f + OverlayMargin)
    {
        return false;
    }

    const ImGuiIO& io = ImGui::GetIO();
    screenPosition = ImVec2(normalizedX * io.DisplaySize.x, normalizedY * io.DisplaySize.y);
    return true;
}

float ProjectLightingOverlayRadius(const dusk::lighting::SceneLight& light, const ImVec2& center) {
    const float worldRadius = std::clamp(std::max(light.power, light.radius * 0.5f), 50.0f, 6000.0f);
    float projectedRadius = 0.0f;

    cXyz offsets[3] = {light.worldPosition, light.worldPosition, light.worldPosition};
    offsets[0].x += worldRadius;
    offsets[1].y += worldRadius;
    offsets[2].z += worldRadius;

    for (const cXyz& offset : offsets) {
        ImVec2 edge = {};
        if (!ProjectLightingOverlayPoint(offset, edge)) {
            continue;
        }

        const float dx = edge.x - center.x;
        const float dy = edge.y - center.y;
        projectedRadius = std::max(projectedRadius, std::sqrt((dx * dx) + (dy * dy)));
    }

    if (projectedRadius <= 0.0f) {
        projectedRadius = 14.0f + (light.referenceAmbientStrength * 56.0f);
    }

    return std::clamp(projectedRadius, 8.0f, 180.0f);
}

void DrawSceneOverlayContents() {
    const auto& registry = dusk::lighting::scene_lights();
    if (!registry.valid || registry.lightCount == 0) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    for (uint32_t i = 0; i < registry.lightCount; ++i) {
        const auto& light = registry.lights[i];
        ImVec2 center = {};
        if (!ProjectLightingOverlayPoint(light.worldPosition, center)) {
            continue;
        }

        const bool isDominant = registry.dominantAmbientLightIndex == static_cast<int32_t>(i);
        const bool isShadowSource = registry.shadowLightIndex == static_cast<int32_t>(i);
        const float radius = ProjectLightingOverlayRadius(light, center);
        const int alpha = light.referenceAmbientStrength > 0.0f ? 170 : 75;
        const ImU32 color = SceneLightOverlayColor(light, alpha);
        const ImU32 fillColor = SceneLightOverlayColor(light, isDominant ? 230 : 185);
        drawList->AddCircle(center, radius, color, 48, isDominant || isShadowSource ? 2.75f : 1.35f);
        drawList->AddCircleFilled(center, isDominant ? 5.0f : 3.5f, fillColor, 16);

        if (light.priority) {
            const ImVec2 min(center.x - 7.0f, center.y - 7.0f);
            const ImVec2 max(center.x + 7.0f, center.y + 7.0f);
            drawList->AddRect(min, max, IM_COL32(255, 255, 255, 210), 0.0f, 0, 1.5f);
        }

        if (isShadowSource) {
            drawList->AddCircle(center, radius + 5.0f, IM_COL32(255, 130, 80, 235), 48, 2.0f);
            drawList->AddLine(ImVec2(center.x - 8.0f, center.y - 8.0f), ImVec2(center.x + 8.0f, center.y + 8.0f),
                              IM_COL32(255, 130, 80, 235), 2.0f);
            drawList->AddLine(ImVec2(center.x - 8.0f, center.y + 8.0f), ImVec2(center.x + 8.0f, center.y - 8.0f),
                              IM_COL32(255, 130, 80, 235), 2.0f);
        }

        if (isDominant || isShadowSource || light.referenceAmbientStrength > 0.02f || light.selectionScore > 0.02f) {
            const std::string label = fmt::format(
                FMT_STRING("{} {}\namb {:.2f} score {:.2f} shadow {:.2f}"),
                dusk::lighting::scene_light_source_name(light.source), light.sourceIndex,
                light.referenceAmbientStrength, light.selectionScore, light.shadowScore);
            const ImVec2 labelPos(center.x + 8.0f, center.y - 12.0f);
            drawList->AddText(ImVec2(labelPos.x + 1.0f, labelPos.y + 1.0f), IM_COL32(0, 0, 0, 220),
                              label.c_str());
            drawList->AddText(labelPos, isShadowSource
                                            ? IM_COL32(255, 160, 100, 255)
                                            : (isDominant ? IM_COL32(255, 235, 140, 255)
                                                          : IM_COL32(235, 245, 255, 230)),
                              label.c_str());
        }
    }

    ImVec2 reference = {};
    if (ProjectLightingOverlayPoint(registry.referencePosition, reference)) {
        drawList->AddLine(ImVec2(reference.x - 8.0f, reference.y), ImVec2(reference.x + 8.0f, reference.y),
                          IM_COL32(80, 220, 255, 230), 2.0f);
        drawList->AddLine(ImVec2(reference.x, reference.y - 8.0f), ImVec2(reference.x, reference.y + 8.0f),
                          IM_COL32(80, 220, 255, 230), 2.0f);
    }

    if (registry.accumulatedAmbientWeight > 0.0f) {
        ImVec2 centroid = {};
        if (ProjectLightingOverlayPoint(registry.accumulatedAmbientCentroid, centroid)) {
            drawList->AddCircle(centroid, 9.0f, IM_COL32(120, 255, 170, 235), 16, 2.0f);
            drawList->AddText(ImVec2(centroid.x + 10.0f, centroid.y - 9.0f),
                              IM_COL32(120, 255, 170, 235), "ambient centroid");
        }
    }
}

void DrawIblStatusContents() {
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
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Periodic refresh:  {}\n"), EnabledLabel(status->probeAutoRefresh)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Local probe GI:    {}\n"), EnabledLabel(status->probeLocalGi)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Scene stale:       {}\n"), EnabledLabel(status->probeSceneStale)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Probe cache:       {}\n"), EnabledLabel(status->probeCacheEnabled)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Last cache hit:    {}\n"), EnabledLabel(status->probeCacheHit)));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Nearest cache:     {}\n"), EnabledLabel(status->probeNearestCacheEnabled)));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Nearest active:    {}\n"), EnabledLabel(status->probeNearestCacheActive)));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Nearest distance:  {:.1f}/{:.1f}\n"), status->probeNearestCacheDistance,
                    status->probeNearestCacheMaxDistance));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Spatial blend:     {}\n"), EnabledLabel(status->probeSpatialBlendEnabled)));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Spatial active:    {}\n"), EnabledLabel(status->probeSpatialBlendActive)));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Spatial factor:    {:.3f}/1.000\n"), status->probeSpatialBlendFactor));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Spatial distance:  {:.1f}/{:.1f}\n"), status->probeSpatialBlendDistance,
                    status->probeSpatialBlendMaxDistance));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Spatial probe key: {}\n"),
                                          status->spatialProbeSceneKey[0] != '\0'
                                              ? status->spatialProbeSceneKey
                                              : "(none)"));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Probe blending:    {}\n"), EnabledLabel(status->probeBlendEnabled)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Blend active:      {}\n"), EnabledLabel(status->probeBlendActive)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Blend factor:      {:.3f}/1.000\n"), status->probeBlendFactor));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Blend frames:      {}\n"), status->probeBlendFrames));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Cache slots:       {}/{}\n"), status->probeCacheUsedSlots,
                                    status->probeCacheSlots));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Active probe key:  {}\n"), status->activeProbeSceneKey[0] != '\0'
                                                              ? status->activeProbeSceneKey
                                                              : "(none)"));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Replay draws:      {}\n"), status->probeReplayDraws));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("PBR in replay:     {}\n"), EnabledLabel(status->probeReplayPbrVisible)));
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

void DrawPbrShadowMapStatusContents() {
    const AuroraPbrShadowMapStatus* status = aurora_get_pbr_shadow_map_status();
    if (status == nullptr) {
        ImGui::TextUnformatted("Shadow map status unavailable");
        return;
    }

    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Enabled:          {}\n"), EnabledLabel(status->enabled)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Resources ready:  {}\n"), EnabledLabel(status->resourcesReady)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Map available:    {}\n"), EnabledLabel(status->mapAvailable)));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Light request:    {}\n"), EnabledLabel(status->lightRequestValid)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Matrix valid:     {}\n"), EnabledLabel(status->matrixValid)));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Caster pass:      {}\n"), EnabledLabel(status->casterPassReady)));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Receiver sample:  {}\n"), EnabledLabel(status->receiverSamplingReady)));
    dusk::ImGuiStringViewText(
        fmt::format(FMT_STRING("Refresh pending:  {}\n"), EnabledLabel(status->refreshPending)));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Size:             {}x{}\n"), status->size, status->size));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Atlas slots:      {} planned\n"), status->atlasSlotCount));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Budget:           {} active, {} refresh/frame\n"),
                                    status->maxActiveSlots, status->slotsPerFrame));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Requests:         {}\n"), status->requestCount));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Request mask:     0x{:x}\n"), status->requestMask));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Captured slots:   {}\n"), status->capturedSlotCount));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Captured mask:    0x{:x}\n"), status->capturedSlotMask));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Pending mask:     0x{:x}\n"), status->pendingSlotMask));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Shadow draws:     {}\n"), status->drawCount));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Slot draws:       {}, {}, {}, {}\n"),
                                    status->slotDrawCounts[0], status->slotDrawCounts[1],
                                    status->slotDrawCounts[2], status->slotDrawCounts[3]));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Strength:         {:.3f}\n"), status->strength));
    dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Bias:             {:.5f}\n"), status->bias));
    if (status->lightRequestValid) {
        const auto& request = status->lightRequest;
        dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Source:           {} {}\n"),
                                        AuroraSceneLightSourceLabel(request.source), request.sourceIndex));
        dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Shadow type:      {}\n"),
                                        AuroraShadowTypeLabel(status->activeShadowType)));
        dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Atlas slot:       {}\n"), status->activeAtlasSlot));
        dusk::ImGuiStringViewText(
            fmt::format(FMT_STRING("Stable ID:        0x{:x}\n"), request.stableId));
        dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Score/priority:   {:.4f} / {:.3f}\n"),
                                        request.score, request.priority));
        dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Radius:           {:.1f}\n"), request.radius));
        dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Position:         {:.1f}, {:.1f}, {:.1f}\n"),
                                        request.position[0], request.position[1], request.position[2]));
        dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Target:           {:.1f}, {:.1f}, {:.1f}\n"),
                                        request.target[0], request.target[1], request.target[2]));
        dusk::ImGuiStringViewText(fmt::format(FMT_STRING("Color:            {:.3f}, {:.3f}, {:.3f}\n"),
                                        request.color[0], request.color[1], request.color[2]));
    }

    if (ImGui::TreeNode("Atlas Slot Requests##pbrShadowStatus")) {
        if (ImGui::BeginTable("PbrShadowAtlasSlots", 9,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Slot");
            ImGui::TableSetupColumn("Req");
            ImGui::TableSetupColumn("Cap");
            ImGui::TableSetupColumn("Pending");
            ImGui::TableSetupColumn("Draws");
            ImGui::TableSetupColumn("Source");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Score");
            ImGui::TableSetupColumn("Priority");
            ImGui::TableHeadersRow();
            const uint32_t slotCount = std::min<uint32_t>(status->atlasSlotCount, 4);
            for (uint32_t slot = 0; slot < slotCount; ++slot) {
                const auto& request = status->requests[slot];
                const bool requested = (status->requestMask & (1u << slot)) != 0;
                const bool captured = (status->capturedSlotMask & (1u << slot)) != 0;
                const bool pending = (status->pendingSlotMask & (1u << slot)) != 0;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%u", slot);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(EnabledLabel(requested && request.valid));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(EnabledLabel(captured));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(EnabledLabel(pending));
                ImGui::TableNextColumn();
                ImGui::Text("%u", status->slotDrawCounts[slot]);
                ImGui::TableNextColumn();
                if (requested && request.valid) {
                    ImGui::Text("%s %u", AuroraSceneLightSourceLabel(request.source), request.sourceIndex);
                } else {
                    ImGui::TextUnformatted("-");
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(requested && request.valid ? AuroraShadowTypeLabel(request.shadowType) : "-");
                ImGui::TableNextColumn();
                if (requested && request.valid) {
                    ImGui::Text("%.3f", request.score);
                } else {
                    ImGui::TextUnformatted("-");
                }
                ImGui::TableNextColumn();
                if (requested && request.valid) {
                    ImGui::Text("%.3f", request.priority);
                } else {
                    ImGui::TextUnformatted("-");
                }
            }
            ImGui::EndTable();
        }
        ImGui::TreePop();
    }
}


void DrawEnhancedLightingControls() {
    auto& pbr = dusk::getSettings().backend.pbr;

    ImGui::SeparatorText("Enhanced Direct Lights");
    bool pbrEnhancedLights = pbr.enhancedLights;
    bool pbrEnhancedLightDebug = pbr.enhancedLightDebug;
    bool pbrEnhancedFireFlicker = pbr.enhancedFireFlicker;
    int pbrEnhancedLightCount = pbr.enhancedLightCount;
    int pbrEnhancedLightFalloff = static_cast<int>(pbr.enhancedLightFalloff.getValue());
    float pbrEnhancedLightIntensity = pbr.enhancedLightIntensity;
    float pbrEnhancedFireFlickerStrength = pbr.enhancedFireFlickerStrength;
    float pbrEnhancedFireFlickerSpeed = pbr.enhancedFireFlickerSpeed;
    bool enhancedLightChanged = false;
    enhancedLightChanged |= ImGui::Checkbox("Enhanced Direct Lights##pbr", &pbrEnhancedLights);
    enhancedLightChanged |= ImGui::SliderInt("Active GPU Lights##pbr", &pbrEnhancedLightCount, 1, 64);
    enhancedLightChanged |= ImGui::Combo("Falloff##pbr", &pbrEnhancedLightFalloff, PbrEnhancedLightFalloffLabels,
                                         IM_ARRAYSIZE(PbrEnhancedLightFalloffLabels));
    enhancedLightChanged |= ImGui::SliderFloat("Intensity##pbr", &pbrEnhancedLightIntensity, 0.0f, 8.0f);
    enhancedLightChanged |= ImGui::Checkbox("Fire Light Flicker##pbr", &pbrEnhancedFireFlicker);
    if (pbrEnhancedFireFlicker) {
        enhancedLightChanged |=
            ImGui::SliderFloat("Fire Flicker Strength##pbr", &pbrEnhancedFireFlickerStrength, 0.0f, 1.0f, "%.3f");
        enhancedLightChanged |=
            ImGui::SliderFloat("Fire Flicker Speed##pbr", &pbrEnhancedFireFlickerSpeed, 0.1f, 4.0f, "%.2f");
    }
    enhancedLightChanged |= ImGui::Checkbox("Debug Overlay Data##pbr", &pbrEnhancedLightDebug);
    if (enhancedLightChanged) {
        pbr.enhancedLights.setValue(pbrEnhancedLights);
        pbr.enhancedLightCount.setValue(pbrEnhancedLightCount);
        pbr.enhancedLightFalloff.setValue(pbrEnhancedLightFalloff == 1 ? dusk::PbrEnhancedLightFalloff::InverseSquare
                                                                       : dusk::PbrEnhancedLightFalloff::LegacyRadius);
        pbr.enhancedLightIntensity.setValue(pbrEnhancedLightIntensity);
        pbr.enhancedFireFlicker.setValue(pbrEnhancedFireFlicker);
        pbr.enhancedFireFlickerStrength.setValue(pbrEnhancedFireFlickerStrength);
        pbr.enhancedFireFlickerSpeed.setValue(pbrEnhancedFireFlickerSpeed);
        pbr.enhancedLightDebug.setValue(pbrEnhancedLightDebug);
        dusk::pbr_settings::apply_enhanced_lighting();
        dusk::config::Save();
    }
    if (ImGui::Button("Reset Enhanced Direct Lights##pbr")) {
        dusk::pbr_settings::reset_enhanced_lighting();
        dusk::config::Save();
    }

    ImGui::SeparatorText("Dynamic Local GI");
    bool pbrDynamicGi = pbr.dynamicGi;
    float pbrDynamicGiStrength = pbr.dynamicGiStrength;
    float pbrDynamicGiNormalWrap = pbr.dynamicGiNormalWrap;
    float pbrDynamicGiAlbedoInfluence = pbr.dynamicGiAlbedoInfluence;
    bool dynamicGiChanged = false;
    dynamicGiChanged |= ImGui::Checkbox("Dynamic GI##pbr", &pbrDynamicGi);
    if (pbrDynamicGi) {
        dynamicGiChanged |= ImGui::SliderFloat("GI Strength##pbr", &pbrDynamicGiStrength, 0.0f, 2.0f);
        dynamicGiChanged |= ImGui::SliderFloat("GI Normal Wrap##pbr", &pbrDynamicGiNormalWrap, 0.0f, 1.0f);
        dynamicGiChanged |= ImGui::SliderFloat("GI Albedo Bleed##pbr", &pbrDynamicGiAlbedoInfluence, 0.0f, 1.0f);
    }
    if (dynamicGiChanged) {
        pbr.dynamicGi.setValue(pbrDynamicGi);
        pbr.dynamicGiStrength.setValue(pbrDynamicGiStrength);
        pbr.dynamicGiNormalWrap.setValue(pbrDynamicGiNormalWrap);
        pbr.dynamicGiAlbedoInfluence.setValue(pbrDynamicGiAlbedoInfluence);
        dusk::pbr_settings::apply_dynamic_gi();
        dusk::config::Save();
    }
    if (ImGui::Button("Reset Dynamic GI##pbr")) {
        dusk::pbr_settings::reset_dynamic_gi();
        dusk::config::Save();
    }

    if (ImGui::TreeNode("Lighting Scene Inventory##pbr")) {
        DrawLightingSceneInventoryContents();
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Enhanced Direct Light Status##pbr")) {
        DrawPbrEnhancedLightingStatusContents();
        ImGui::TreePop();
    }
}

void DrawEnhancedShadowControls() {
    auto& pbr = dusk::getSettings().backend.pbr;

    ImGui::SeparatorText("Enhanced Shadows");
    bool pbrEnhancedShadows = pbr.enhancedShadows;
    dusk::PbrEnhancedShadowMode pbrEnhancedShadowMode = dusk::lighting::effective_enhanced_shadow_mode();
    int pbrEnhancedShadowMapSize = pbr.enhancedShadowMapSize;
    float pbrEnhancedShadowStrength = pbr.enhancedShadowStrength;
    float pbrEnhancedShadowBias = pbr.enhancedShadowBias;
    int pbrEnhancedShadowMaxMaps = pbr.enhancedShadowMaxMaps;
    int pbrEnhancedShadowMapsPerFrame = pbr.enhancedShadowMapsPerFrame;
    bool shadowChanged = false;
    shadowChanged |= ImGui::Checkbox("Use Enhanced Shadows##pbr", &pbrEnhancedShadows);

    if (pbrEnhancedShadows) {
        int shadowModeIndex = ShadowModeOptionIndex(pbrEnhancedShadowMode);
        if (ImGui::BeginCombo("Shadow Path##pbr", PbrEnhancedShadowModeOptions[shadowModeIndex].label)) {
            for (int i = 0; i < IM_ARRAYSIZE(PbrEnhancedShadowModeOptions); ++i) {
                const bool selected = i == shadowModeIndex;
                if (ImGui::Selectable(PbrEnhancedShadowModeOptions[i].label, selected)) {
                    shadowModeIndex = i;
                    pbrEnhancedShadowMode = PbrEnhancedShadowModeOptions[i].mode;
                    shadowChanged = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (pbrEnhancedShadowMode == dusk::PbrEnhancedShadowMode::OverrideDirection) {
            ImGui::TextUnformatted("Original real shadows use the dominant enhanced light for direction.");
        } else if (pbrEnhancedShadowMode == dusk::PbrEnhancedShadowMode::DisableGameShadows) {
            ImGui::TextUnformatted("Original real and simple shadows are suppressed.");
        } else if (pbrEnhancedShadowMode == dusk::PbrEnhancedShadowMode::Hybrid) {
            ImGui::TextUnformatted("Original real shadows are suppressed; simple/contact shadows remain.");
        } else if (pbrEnhancedShadowMode == dusk::PbrEnhancedShadowMode::AuroraShadowMaps) {
            ImGui::TextUnformatted("Original real shadows are suppressed; Aurora owns the PBR shadow-map slot.");
            shadowChanged |= ImGui::SliderInt("Max Shadow Maps##pbr", &pbrEnhancedShadowMaxMaps, 1, 4);
            shadowChanged |= ImGui::SliderInt("Maps Refreshed Per Frame##pbr", &pbrEnhancedShadowMapsPerFrame, 1, 4);
            shadowChanged |= ImGui::SliderInt("Shadow Map Size##pbr", &pbrEnhancedShadowMapSize, 256, 4096);
            shadowChanged |= ImGui::SliderFloat("Shadow Strength##pbr", &pbrEnhancedShadowStrength, 0.0f, 1.0f, "%.3f");
            shadowChanged |= ImGui::SliderFloat("Shadow Bias##pbr", &pbrEnhancedShadowBias, 0.0f, 0.02f, "%.5f");
            if (ImGui::Button("Refresh Shadow Atlas##pbr")) {
                aurora_request_pbr_shadow_map_refresh();
            }
            if (ImGui::TreeNode("Aurora Shadow Map Status##pbr")) {
                DrawPbrShadowMapStatusContents();
                ImGui::TreePop();
            }
        }
    } else {
        ImGui::TextUnformatted("Disabled: the original game shadow renderer is untouched.");
    }

    if (shadowChanged) {
        pbr.enhancedShadows.setValue(pbrEnhancedShadows);
        pbr.enhancedShadowMode.setValue(pbrEnhancedShadowMode);
        pbr.enhancedShadowMapSize.setValue(pbrEnhancedShadowMapSize);
        pbr.enhancedShadowStrength.setValue(pbrEnhancedShadowStrength);
        pbr.enhancedShadowBias.setValue(pbrEnhancedShadowBias);
        pbr.enhancedShadowMaxMaps.setValue(pbrEnhancedShadowMaxMaps);
        pbr.enhancedShadowMapsPerFrame.setValue(pbrEnhancedShadowMapsPerFrame);
        dusk::pbr_settings::apply_enhanced_shadows();
        dusk::config::Save();
    }

    if (ImGui::Button("Reset Enhanced Shadows##pbr")) {
        dusk::pbr_settings::reset_enhanced_shadows();
        dusk::config::Save();
    }
}


bool DrawExperimentalLightingToggle() {
    bool enableExperimentalLighting = dusk::getSettings().backend.enableExperimentalLighting;
    if (ImGui::Checkbox("Modern Lighting And Shadows", &enableExperimentalLighting)) {
        dusk::getSettings().backend.enableExperimentalLighting.setValue(enableExperimentalLighting);
        dusk::pbr_settings::apply_enhanced_lighting();
        dusk::pbr_settings::apply_enhanced_shadows();
        dusk::config::Save();
    }
    return enableExperimentalLighting;
}


}  // namespace dusk::imgui_lighting
