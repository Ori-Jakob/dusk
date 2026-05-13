#pragma once

#include "SSystem/SComponent/c_xyz.h"

#include <cstdint>

namespace dusk::lighting {

constexpr uint32_t MaxSceneLights = 128;

enum class SceneLightSource : uint8_t {
    Point,
    Effect,
};

struct SceneLight {
    uintptr_t stableId = 0;
    SceneLightSource source = SceneLightSource::Point;
    uint32_t sourceIndex = 0;
    const void* sourcePointer = nullptr;
    cXyz worldPosition{};
    float color[3] = {};
    float rawColor[3] = {};
    float radius = 1.0f;
    float power = 1.0f;
    float fluctuation = 0.0f;
    float luminance = 0.0f;
    float distance = 0.0f;
    float selectionScore = 0.0f;
    float referenceAmbientStrength = 0.0f;
    float referenceAmbientLuminance = 0.0f;
    float referenceAmbientColor[3] = {};
    float tuningAmbientScale = 1.0f;
    float tuningDirectScale = 1.0f;
    bool castsShadow = true;
    float shadowPriority = 1.0f;
    float shadowScore = 0.0f;
    bool priority = false;
};

struct SceneLightRegistry {
    SceneLight lights[MaxSceneLights]{};
    uint32_t lightCount = 0;
    uint32_t pointLightCount = 0;
    uint32_t effectLightCount = 0;
    uint32_t rejectedLightCount = 0;
    cXyz referencePosition{};
    float accumulatedAmbientStrength = 0.0f;
    float accumulatedAmbientLuminance = 0.0f;
    float accumulatedAmbientColor[3] = {};
    cXyz accumulatedAmbientCentroid{};
    float accumulatedAmbientWeight = 0.0f;
    int32_t dominantAmbientLightIndex = -1;
    int32_t shadowLightIndex = -1;
    bool valid = false;
};

void update_scene_lights(const cXyz& referencePos);
void clear_scene_lights();
const SceneLightRegistry& scene_lights();

const char* scene_light_source_name(SceneLightSource source);
float scene_light_distance_squared(const SceneLight& light, const cXyz& receiverPos);
float scene_light_soft_radius_strength(const SceneLight& light, const cXyz& receiverPos);

}  // namespace dusk::lighting
