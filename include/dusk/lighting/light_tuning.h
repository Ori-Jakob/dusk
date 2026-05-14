#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace dusk::lighting {

inline constexpr float DefaultSelectionRadiusScale = 2.5f;
inline constexpr float DefaultSelectionRadiusPadding = 1500.0f;

enum class LightShadowType : uint8_t {
    None,
    LocalProjected,
    Directional,
    Point,
};

struct LightTuningScale {
    bool enabled = true;
    float ambientScale = 1.0f;
    float directScale = 1.0f;
    float radiusScale = 1.0f;
    float powerScale = 1.0f;
    float scoreScale = 1.0f;
    bool castsShadows = true;
    float shadowPriority = 1.0f;
    LightShadowType shadowType = LightShadowType::LocalProjected;
    std::array<float, 3> colorScale = {1.0f, 1.0f, 1.0f};
    bool hasPositionOverride = false;
    std::array<float, 3> position = {};
    bool isFire = false;
};

struct LightTuningRule {
    std::string source = "any";
    int sourceIndex = -1;
    bool setEnabled = false;
    bool setCastsShadows = false;
    bool setShadowType = false;
    bool setPositionOverride = false;
    bool setIsFire = false;
    LightTuningScale scale{};
};

struct LightingTuning {
    float enhancedAmbientScale = 1.0f;
    float enhancedDirectScale = 1.0f;
    float selectionRadiusScale = DefaultSelectionRadiusScale;
    float selectionRadiusPadding = DefaultSelectionRadiusPadding;
    bool globalLoaded = false;
    bool stageLoaded = false;
    bool roomLoaded = false;
    std::string sceneKey;
    std::string sourceSummary;
    std::vector<LightTuningRule> lightRules;
};

const LightingTuning& current_lighting_tuning(const char* stageName, int roomNo);
LightTuningScale light_tuning_for_source(const LightingTuning& tuning, std::string_view source, uint32_t sourceIndex);
std::filesystem::path room_lighting_tuning_path(const char* stageName, int roomNo);
void set_runtime_light_tuning_rule(const char* stageName, int roomNo, const LightTuningRule& rule);
void set_runtime_lighting_selection_radius(const char* stageName, int roomNo, float scale, float padding);
bool save_room_light_tuning_rule(const char* stageName, int roomNo, const LightTuningRule& rule,
                                 std::string* outPath = nullptr, std::string* outError = nullptr);
bool save_room_lighting_selection_radius(const char* stageName, int roomNo, float scale, float padding,
                                         std::string* outPath = nullptr, std::string* outError = nullptr);
void invalidate_lighting_tuning_cache();

}  // namespace dusk::lighting
