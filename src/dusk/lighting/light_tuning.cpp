#include "dusk/lighting/light_tuning.h"

#include "aurora/lib/logging.hpp"
#include "dusk/io.hpp"
#include "dusk/main.h"

#include "fmt/format.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <string_view>
#include <utility>
#include <vector>

namespace dusk::lighting {
namespace {

using json = nlohmann::json;

aurora::Module LightTuningLog("dusk::lighting::tuning");

LightingTuning sTuning{};
std::string sCachedStage;
int sCachedRoom = -999999;
bool sCacheValid = false;

struct RuntimeLightRule {
    std::string sceneKey;
    LightTuningRule rule;
};

struct RuntimeSelectionRadius {
    std::string sceneKey;
    bool valid = false;
    float scale = DefaultSelectionRadiusScale;
    float padding = DefaultSelectionRadiusPadding;
};

std::vector<RuntimeLightRule> sRuntimeLightRules;
RuntimeSelectionRadius sRuntimeSelectionRadius;

float json_float_range(const json& object, std::string_view key, float fallback, float min, float max) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number()) {
        return fallback;
    }

    return std::clamp(it->get<float>(), min, max);
}

float json_float(const json& object, std::string_view key, float fallback) {
    return json_float_range(object, key, fallback, 0.0f, 16.0f);
}

int json_int(const json& object, std::string_view key, int fallback) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number_integer()) {
        return fallback;
    }

    return it->get<int>();
}

std::string json_string(const json& object, std::string_view key, std::string fallback) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) {
        return fallback;
    }

    return it->get<std::string>();
}

std::array<float, 3> json_float3(const json& object, std::string_view key, std::array<float, 3> fallback) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_array() || it->size() != 3) {
        return fallback;
    }

    std::array<float, 3> values = fallback;
    for (size_t i = 0; i < values.size(); ++i) {
        if (!(*it)[i].is_number()) {
            return fallback;
        }
        values[i] = std::clamp((*it)[i].get<float>(), 0.0f, 16.0f);
    }

    return values;
}

std::array<float, 3> json_float3_range(const json& object, std::string_view key, std::array<float, 3> fallback,
                                       float min, float max) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_array() || it->size() != 3) {
        return fallback;
    }

    std::array<float, 3> values = fallback;
    for (size_t i = 0; i < values.size(); ++i) {
        if (!(*it)[i].is_number()) {
            return fallback;
        }
        values[i] = std::clamp((*it)[i].get<float>(), min, max);
    }

    return values;
}

std::string lower_ascii(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

bool source_matches(std::string_view ruleSource, std::string_view lightSource) {
    const std::string rule = lower_ascii(ruleSource);
    const std::string source = lower_ascii(lightSource);
    if (rule.empty() || rule == "any" || rule == "all") {
        return true;
    }
    if (rule == source) {
        return true;
    }
    if (source == "point" && (rule == "pointlight" || rule == "game_point")) {
        return true;
    }
    if (source == "effect" && (rule == "efplight" || rule == "game_effect")) {
        return true;
    }
    if (source == "base" &&
        (rule == "base_light" || rule == "environment" || rule == "environment_key" || rule == "key" ||
         rule == "sun" || rule == "moon"))
    {
        return true;
    }

    return false;
}

LightShadowType shadow_type_from_string(std::string_view text, LightShadowType fallback) {
    const std::string value = lower_ascii(text);
    if (value.empty()) {
        return fallback;
    }
    if (value == "none" || value == "off" || value == "disabled") {
        return LightShadowType::None;
    }
    if (value == "local" || value == "local_projected" || value == "projected" || value == "spot") {
        return LightShadowType::LocalProjected;
    }
    if (value == "directional" || value == "sun" || value == "key") {
        return LightShadowType::Directional;
    }
    if (value == "point" || value == "cubemap" || value == "cube") {
        return LightShadowType::Point;
    }

    return fallback;
}

const char* shadow_type_to_string(LightShadowType type) {
    switch (type) {
    case LightShadowType::None:
        return "none";
    case LightShadowType::LocalProjected:
        return "local_projected";
    case LightShadowType::Directional:
        return "directional";
    case LightShadowType::Point:
        return "point";
    }

    return "local_projected";
}

void apply_light_rules(const json& document, LightingTuning& tuning) {
    const auto it = document.find("lights");
    if (it == document.end()) {
        return;
    }
    if (!it->is_array()) {
        LightTuningLog.warn("Lighting tuning 'lights' member is not an array");
        return;
    }

    for (const json& item : *it) {
        if (!item.is_object()) {
            continue;
        }

        LightTuningRule rule{};
        rule.source = json_string(item, "source", rule.source);
        rule.sourceIndex = json_int(item, "index", json_int(item, "sourceIndex", rule.sourceIndex));
        rule.scale.ambientScale = json_float(item, "ambientScale", rule.scale.ambientScale);
        rule.scale.directScale = json_float(item, "directScale", rule.scale.directScale);
        rule.scale.radiusScale = json_float_range(item, "radiusScale", rule.scale.radiusScale, 0.01f, 16.0f);
        rule.scale.powerScale = json_float_range(item, "powerScale", rule.scale.powerScale, 0.01f, 16.0f);
        rule.scale.scoreScale = json_float(item, "scoreScale", rule.scale.scoreScale);
        rule.scale.shadowPriority = json_float(item, "shadowPriority", rule.scale.shadowPriority);
        if (item.contains("shadowType")) {
            rule.setShadowType = true;
            if (item["shadowType"].is_string()) {
                rule.scale.shadowType =
                    shadow_type_from_string(item["shadowType"].get<std::string>(), rule.scale.shadowType);
            } else if (item["shadowType"].is_number_integer()) {
                const int type = std::clamp(item["shadowType"].get<int>(), 0, 3);
                rule.scale.shadowType = static_cast<LightShadowType>(type);
            }
        }
        rule.scale.colorScale = json_float3(item, "colorScale", rule.scale.colorScale);
        if (item.contains("position") && item["position"].is_array()) {
            rule.setPositionOverride = true;
            rule.scale.hasPositionOverride = true;
            rule.scale.position = json_float3_range(item, "position", rule.scale.position, -500000.0f, 500000.0f);
        }
        if (item.contains("isFire") && item["isFire"].is_boolean()) {
            rule.setIsFire = true;
            rule.scale.isFire = item["isFire"].get<bool>();
        }

        const bool hasEnabled = item.contains("enabled") && item["enabled"].is_boolean();
        const bool hasDisabled = item.contains("disabled") && item["disabled"].is_boolean();
        rule.setEnabled = hasEnabled || hasDisabled;
        if (hasEnabled) {
            rule.scale.enabled = item["enabled"].get<bool>();
        }
        if (hasDisabled) {
            rule.scale.enabled = !item["disabled"].get<bool>();
        }
        const bool hasCastsShadows = item.contains("castsShadows") && item["castsShadows"].is_boolean();
        const bool hasCastShadows = item.contains("castShadows") && item["castShadows"].is_boolean();
        const bool hasShadowCaster = item.contains("shadowCaster") && item["shadowCaster"].is_boolean();
        rule.setCastsShadows = hasCastsShadows || hasCastShadows || hasShadowCaster;
        if (hasCastsShadows) {
            rule.scale.castsShadows = item["castsShadows"].get<bool>();
        }
        if (hasCastShadows) {
            rule.scale.castsShadows = item["castShadows"].get<bool>();
        }
        if (hasShadowCaster) {
            rule.scale.castsShadows = item["shadowCaster"].get<bool>();
        }

        tuning.lightRules.push_back(std::move(rule));
    }
}

void apply_runtime_selection_radius(LightingTuning& tuning) {
    if (!sRuntimeSelectionRadius.valid || sRuntimeSelectionRadius.sceneKey != tuning.sceneKey) {
        return;
    }

    tuning.selectionRadiusScale = sRuntimeSelectionRadius.scale;
    tuning.selectionRadiusPadding = sRuntimeSelectionRadius.padding;
}

void apply_rule_to_scale(const LightTuningRule& rule, LightTuningScale& scale, bool replace) {
    if (rule.setEnabled) {
        scale.enabled = rule.scale.enabled;
    }
    if (rule.setCastsShadows) {
        scale.castsShadows = rule.scale.castsShadows;
    }
    if (rule.setShadowType) {
        scale.shadowType = rule.scale.shadowType;
        if (scale.shadowType == LightShadowType::None) {
            scale.castsShadows = false;
        }
    }
    if (rule.setPositionOverride) {
        scale.hasPositionOverride = rule.scale.hasPositionOverride;
        scale.position = rule.scale.position;
    }
    if (rule.setIsFire) {
        scale.isFire = rule.scale.isFire;
    }

    if (replace) {
        scale.ambientScale = rule.scale.ambientScale;
        scale.directScale = rule.scale.directScale;
        scale.radiusScale = rule.scale.radiusScale;
        scale.powerScale = rule.scale.powerScale;
        scale.scoreScale = rule.scale.scoreScale;
        scale.shadowPriority = rule.scale.shadowPriority;
        scale.shadowType = rule.scale.shadowType;
        scale.colorScale = rule.scale.colorScale;
        scale.hasPositionOverride = rule.scale.hasPositionOverride;
        scale.position = rule.scale.position;
        scale.isFire = rule.scale.isFire;
        return;
    }

    scale.ambientScale *= rule.scale.ambientScale;
    scale.directScale *= rule.scale.directScale;
    scale.radiusScale *= rule.scale.radiusScale;
    scale.powerScale *= rule.scale.powerScale;
    scale.scoreScale *= rule.scale.scoreScale;
    scale.shadowPriority *= rule.scale.shadowPriority;
    for (size_t i = 0; i < scale.colorScale.size(); ++i) {
        scale.colorScale[i] *= rule.scale.colorScale[i];
    }
}

bool rule_matches(const LightTuningRule& rule, std::string_view source, uint32_t sourceIndex) {
    if (!source_matches(rule.source, source)) {
        return false;
    }
    if (rule.sourceIndex >= 0 && static_cast<uint32_t>(rule.sourceIndex) != sourceIndex) {
        return false;
    }

    return true;
}

std::filesystem::path room_tuning_path_for_stage(const char* stageName, int roomNo) {
    const std::string stage = stageName != nullptr ? stageName : "";
    return ConfigPath / "texture_replacements" / "lighting" / stage / fmt::format("room_{}.json", roomNo);
}

json load_tuning_document(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec)) {
        return json::object();
    }

    const auto data = io::FileStream::ReadAllBytes(path);
    json document = json::parse(data);
    if (!document.is_object()) {
        return json::object();
    }

    return document;
}

json light_rule_to_json(const LightTuningRule& rule) {
    json object = json::object();
    object["source"] = rule.source;
    object["index"] = rule.sourceIndex;
    if (rule.setEnabled) {
        object["enabled"] = rule.scale.enabled;
    }
    object["ambientScale"] = rule.scale.ambientScale;
    object["directScale"] = rule.scale.directScale;
    object["radiusScale"] = rule.scale.radiusScale;
    object["powerScale"] = rule.scale.powerScale;
    object["scoreScale"] = rule.scale.scoreScale;
    if (rule.setCastsShadows) {
        object["castsShadows"] = rule.scale.castsShadows;
    }
    object["shadowPriority"] = rule.scale.shadowPriority;
    if (rule.setShadowType) {
        object["shadowType"] = shadow_type_to_string(rule.scale.shadowType);
    }
    object["colorScale"] = {rule.scale.colorScale[0], rule.scale.colorScale[1], rule.scale.colorScale[2]};
    if (rule.setPositionOverride && rule.scale.hasPositionOverride) {
        object["position"] = {rule.scale.position[0], rule.scale.position[1], rule.scale.position[2]};
    }
    if (rule.setIsFire) {
        object["isFire"] = rule.scale.isFire;
    }
    return object;
}

int light_rule_index(const json& document, const LightTuningRule& rule) {
    const auto it = document.find("lights");
    if (it == document.end() || !it->is_array()) {
        return -1;
    }

    for (size_t i = 0; i < it->size(); ++i) {
        const json& item = (*it)[i];
        if (!item.is_object()) {
            continue;
        }

        const int itemIndex = json_int(item, "index", json_int(item, "sourceIndex", -1));
        if (itemIndex != rule.sourceIndex) {
            continue;
        }
        if (!source_matches(json_string(item, "source", "any"), rule.source)) {
            continue;
        }

        return static_cast<int>(i);
    }

    return -1;
}

bool write_tuning_document(const std::filesystem::path& path, const json& document, std::string* outError) {
    try {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            if (outError != nullptr) {
                *outError = ec.message();
            }
            return false;
        }

        io::FileStream::WriteAllText(path, document.dump(4));
        return true;
    } catch (const std::exception& e) {
        if (outError != nullptr) {
            *outError = e.what();
        }
        return false;
    }
}

bool apply_tuning_file(const std::filesystem::path& path, LightingTuning& tuning) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec)) {
        return false;
    }

    try {
        const auto data = io::FileStream::ReadAllBytes(path);
        const json document = json::parse(data);
        if (!document.is_object()) {
            LightTuningLog.error("Lighting tuning file '{}' is not a JSON object", io::fs_path_to_string(path));
            return false;
        }

        tuning.enhancedAmbientScale =
            json_float(document, "enhancedAmbientScale", tuning.enhancedAmbientScale);
        tuning.enhancedDirectScale =
            json_float(document, "enhancedDirectScale", tuning.enhancedDirectScale);
        tuning.selectionRadiusScale =
            json_float_range(document, "selectionRadiusScale", tuning.selectionRadiusScale, 0.1f, 16.0f);
        tuning.selectionRadiusPadding =
            json_float_range(document, "selectionRadiusPadding", tuning.selectionRadiusPadding, 0.0f, 50000.0f);
        apply_light_rules(document, tuning);

        if (!tuning.sourceSummary.empty()) {
            tuning.sourceSummary += ", ";
        }
        tuning.sourceSummary += io::fs_path_to_string(path.filename());
        return true;
    } catch (const std::exception& e) {
        LightTuningLog.error("Failed to load lighting tuning file '{}': {}", io::fs_path_to_string(path), e.what());
        return false;
    }
}

std::string make_scene_key(const char* stageName, int roomNo) {
    return fmt::format("{}:{}", stageName != nullptr ? stageName : "", roomNo);
}

}  // namespace

const LightingTuning& current_lighting_tuning(const char* stageName, int roomNo) {
    const std::string stage = stageName != nullptr ? stageName : "";
    if (sCacheValid && sCachedStage == stage && sCachedRoom == roomNo) {
        return sTuning;
    }

    sCachedStage = stage;
    sCachedRoom = roomNo;
    sCacheValid = true;

    sTuning = {};
    sTuning.sceneKey = make_scene_key(stage.c_str(), roomNo);

    const std::filesystem::path root = ConfigPath / "texture_replacements" / "lighting";
    sTuning.globalLoaded = apply_tuning_file(root / "global.json", sTuning);
    if (!stage.empty()) {
        sTuning.stageLoaded = apply_tuning_file(root / (stage + ".json"), sTuning);
        sTuning.roomLoaded = apply_tuning_file(root / stage / fmt::format("room_{}.json", roomNo), sTuning);
        sTuning.roomLoaded = apply_tuning_file(root / fmt::format("{}_room_{}.json", stage, roomNo), sTuning) ||
                              sTuning.roomLoaded;
    }

    apply_runtime_selection_radius(sTuning);

    if (sTuning.sourceSummary.empty()) {
        sTuning.sourceSummary = "(defaults)";
    }

    return sTuning;
}

LightTuningScale light_tuning_for_source(const LightingTuning& tuning, std::string_view source, uint32_t sourceIndex) {
    LightTuningScale scale{};
    for (const LightTuningRule& rule : tuning.lightRules) {
        if (!rule_matches(rule, source, sourceIndex)) {
            continue;
        }

        apply_rule_to_scale(rule, scale, false);
    }

    for (const RuntimeLightRule& runtimeRule : sRuntimeLightRules) {
        if (runtimeRule.sceneKey != tuning.sceneKey || !rule_matches(runtimeRule.rule, source, sourceIndex)) {
            continue;
        }

        apply_rule_to_scale(runtimeRule.rule, scale, true);
    }

    scale.ambientScale = std::clamp(scale.ambientScale, 0.0f, 16.0f);
    scale.directScale = std::clamp(scale.directScale, 0.0f, 16.0f);
    scale.radiusScale = std::clamp(scale.radiusScale, 0.01f, 16.0f);
    scale.powerScale = std::clamp(scale.powerScale, 0.01f, 16.0f);
    scale.scoreScale = std::clamp(scale.scoreScale, 0.0f, 16.0f);
    scale.shadowPriority = std::clamp(scale.shadowPriority, 0.0f, 16.0f);
    for (float& colorScale : scale.colorScale) {
        colorScale = std::clamp(colorScale, 0.0f, 16.0f);
    }

    return scale;
}

std::filesystem::path room_lighting_tuning_path(const char* stageName, int roomNo) {
    return room_tuning_path_for_stage(stageName, roomNo);
}

void set_runtime_light_tuning_rule(const char* stageName, int roomNo, const LightTuningRule& rule) {
    const std::string sceneKey = make_scene_key(stageName, roomNo);
    for (RuntimeLightRule& runtimeRule : sRuntimeLightRules) {
        if (runtimeRule.sceneKey == sceneKey && runtimeRule.rule.source == rule.source &&
            runtimeRule.rule.sourceIndex == rule.sourceIndex)
        {
            runtimeRule.rule = rule;
            return;
        }
    }

    sRuntimeLightRules.push_back({sceneKey, rule});
}

void set_runtime_lighting_selection_radius(const char* stageName, int roomNo, float scale, float padding) {
    sRuntimeSelectionRadius.sceneKey = make_scene_key(stageName, roomNo);
    sRuntimeSelectionRadius.valid = true;
    sRuntimeSelectionRadius.scale = std::clamp(scale, 0.1f, 16.0f);
    sRuntimeSelectionRadius.padding = std::clamp(padding, 0.0f, 50000.0f);

    if (sCacheValid && sTuning.sceneKey == sRuntimeSelectionRadius.sceneKey) {
        apply_runtime_selection_radius(sTuning);
    }
}

bool save_room_light_tuning_rule(const char* stageName, int roomNo, const LightTuningRule& rule,
                                 std::string* outPath, std::string* outError) {
    const std::filesystem::path path = room_tuning_path_for_stage(stageName, roomNo);
    if (outPath != nullptr) {
        *outPath = io::fs_path_to_string(path);
    }

    try {
        json document = load_tuning_document(path);
        if (!document.contains("lights") || !document["lights"].is_array()) {
            document["lights"] = json::array();
        }

        const int index = light_rule_index(document, rule);
        if (index >= 0) {
            document["lights"][static_cast<size_t>(index)] = light_rule_to_json(rule);
        } else {
            document["lights"].push_back(light_rule_to_json(rule));
        }

        if (!write_tuning_document(path, document, outError)) {
            return false;
        }
        invalidate_lighting_tuning_cache();
        return true;
    } catch (const std::exception& e) {
        if (outError != nullptr) {
            *outError = e.what();
        }
        return false;
    }
}

bool save_room_lighting_selection_radius(const char* stageName, int roomNo, float scale, float padding,
                                         std::string* outPath, std::string* outError) {
    const std::filesystem::path path = room_tuning_path_for_stage(stageName, roomNo);
    if (outPath != nullptr) {
        *outPath = io::fs_path_to_string(path);
    }

    try {
        json document = load_tuning_document(path);
        document["selectionRadiusScale"] = std::clamp(scale, 0.1f, 16.0f);
        document["selectionRadiusPadding"] = std::clamp(padding, 0.0f, 50000.0f);
        if (!write_tuning_document(path, document, outError)) {
            return false;
        }
        invalidate_lighting_tuning_cache();
        return true;
    } catch (const std::exception& e) {
        if (outError != nullptr) {
            *outError = e.what();
        }
        return false;
    }
}

void invalidate_lighting_tuning_cache() {
    sCacheValid = false;
}

}  // namespace dusk::lighting
