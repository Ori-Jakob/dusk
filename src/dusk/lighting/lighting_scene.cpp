#include "dusk/lighting/lighting_scene.h"

#include "d/d_kankyo.h"
#include "d/d_com_inf_game.h"
#include "dusk/lighting/light_tuning.h"

#include <algorithm>
#include <cmath>

namespace dusk::lighting {
namespace {

constexpr float BaseLightSyntheticRadius = 60000.0f;
constexpr float BaseLightDefaultAmbientScale = 0.08f;
constexpr float BaseLightDefaultDirectScale = 0.35f;
constexpr float BaseLightDefaultScoreScale = 0.35f;

SceneLightRegistry sSceneLights{};

float square(float value) {
    return value * value;
}

float clamp_color_component(s16 value) {
    return std::clamp(static_cast<float>(value) / 255.0f, 0.0f, 4.0f);
}

float luminance(float r, float g, float b) {
    return r * 0.2126f + g * 0.7152f + b * 0.0722f;
}

float length_squared(const cXyz& value) {
    return square(value.x) + square(value.y) + square(value.z);
}

cXyz normalize_or_default(const cXyz& value, const cXyz& fallback) {
    const float lenSq = length_squared(value);
    if (lenSq <= 0.000001f) {
        return fallback;
    }

    const float invLen = 1.0f / std::sqrt(lenSq);
    return cXyz(value.x * invLen, value.y * invLen, value.z * invLen);
}

void accumulate_reference_contribution(SceneLight& light, const cXyz& referencePos) {
    if (light.type == SceneLightType::Directional) {
        light.referenceAmbientStrength = std::max(light.tuningAmbientScale, 0.0f);
    } else {
        light.referenceAmbientStrength = scene_light_soft_radius_strength(light, referencePos) *
                                         std::max(light.tuningAmbientScale, 0.0f);
    }
    light.referenceAmbientColor[0] = light.rawColor[0] * light.referenceAmbientStrength;
    light.referenceAmbientColor[1] = light.rawColor[1] * light.referenceAmbientStrength;
    light.referenceAmbientColor[2] = light.rawColor[2] * light.referenceAmbientStrength;
    light.referenceAmbientLuminance = light.luminance * light.referenceAmbientStrength;

    sSceneLights.accumulatedAmbientStrength += light.referenceAmbientStrength;
    sSceneLights.accumulatedAmbientLuminance += light.referenceAmbientLuminance;
    sSceneLights.accumulatedAmbientColor[0] += light.referenceAmbientColor[0];
    sSceneLights.accumulatedAmbientColor[1] += light.referenceAmbientColor[1];
    sSceneLights.accumulatedAmbientColor[2] += light.referenceAmbientColor[2];

    const float centroidWeight = light.referenceAmbientLuminance * (light.priority ? 2.0f : 1.0f);
    if (centroidWeight > 0.0f) {
        sSceneLights.accumulatedAmbientCentroid.x += light.worldPosition.x * centroidWeight;
        sSceneLights.accumulatedAmbientCentroid.y += light.worldPosition.y * centroidWeight;
        sSceneLights.accumulatedAmbientCentroid.z += light.worldPosition.z * centroidWeight;
        sSceneLights.accumulatedAmbientWeight += centroidWeight;
    }

    if (light.referenceAmbientLuminance > 0.0f &&
        (sSceneLights.dominantAmbientLightIndex < 0 ||
         light.referenceAmbientLuminance >
             sSceneLights.lights[sSceneLights.dominantAmbientLightIndex].referenceAmbientLuminance))
    {
        sSceneLights.dominantAmbientLightIndex = static_cast<int32_t>(sSceneLights.lightCount - 1);
    }

    if (light.shadowScore > 0.0f &&
        (sSceneLights.shadowLightIndex < 0 ||
         light.shadowScore > sSceneLights.lights[sSceneLights.shadowLightIndex].shadowScore))
    {
        sSceneLights.shadowLightIndex = static_cast<int32_t>(sSceneLights.lightCount - 1);
    }
}

void add_light(const LIGHT_INFLUENCE* influence, SceneLightSource source, uint32_t sourceIndex,
               const cXyz& referencePos, const LightingTuning& tuning) {
    if (influence == nullptr || influence->mPow <= 0.01f) {
        ++sSceneLights.rejectedLightCount;
        return;
    }

    const LightTuningScale lightTuning =
        light_tuning_for_source(tuning, scene_light_source_name(source), sourceIndex);
    if (!lightTuning.enabled) {
        ++sSceneLights.rejectedLightCount;
        return;
    }

    const float r = clamp_color_component(influence->mColor.r) * lightTuning.colorScale[0];
    const float g = clamp_color_component(influence->mColor.g) * lightTuning.colorScale[1];
    const float b = clamp_color_component(influence->mColor.b) * lightTuning.colorScale[2];
    const float lightLuma = luminance(r, g, b);
    if (lightLuma <= 0.0001f) {
        ++sSceneLights.rejectedLightCount;
        return;
    }

    cXyz worldPosition = influence->mPosition;
    if (lightTuning.hasPositionOverride) {
        worldPosition = cXyz(lightTuning.position[0], lightTuning.position[1], lightTuning.position[2]);
    }

    const float radius = std::max((influence->mPow + 1000.0f) * lightTuning.radiusScale, 1.0f);
    const float dist2 =
        square(worldPosition.x - referencePos.x) + square(worldPosition.y - referencePos.y) +
        square(worldPosition.z - referencePos.z);
    const float dist = std::sqrt(dist2);
    const float selectionRadius = (radius * tuning.selectionRadiusScale) + tuning.selectionRadiusPadding;
    if (dist > selectionRadius) {
        ++sSceneLights.rejectedLightCount;
        return;
    }

    if (sSceneLights.lightCount >= MaxSceneLights) {
        ++sSceneLights.rejectedLightCount;
        return;
    }

    SceneLight& light = sSceneLights.lights[sSceneLights.lightCount++];
    light = {};
    light.stableId = reinterpret_cast<uintptr_t>(influence);
    light.source = source;
    light.type = SceneLightType::Point;
    light.sourceIndex = sourceIndex;
    light.sourcePointer = influence;
    light.worldPosition = worldPosition;
    light.color[0] = r;
    light.color[1] = g;
    light.color[2] = b;
    light.rawColor[0] = std::clamp(static_cast<float>(influence->mColor.r) * lightTuning.colorScale[0], 0.0f,
                                   1020.0f);
    light.rawColor[1] = std::clamp(static_cast<float>(influence->mColor.g) * lightTuning.colorScale[1], 0.0f,
                                   1020.0f);
    light.rawColor[2] = std::clamp(static_cast<float>(influence->mColor.b) * lightTuning.colorScale[2], 0.0f,
                                   1020.0f);
    light.radius = radius;
    light.power = std::max(influence->mPow * lightTuning.powerScale, 1.0f);
    light.fluctuation = influence->mFluctuation;
    light.luminance = lightLuma;
    light.distance = dist;
    light.tuningAmbientScale = lightTuning.ambientScale;
    light.tuningDirectScale = lightTuning.directScale;
    light.positionOverridden = lightTuning.hasPositionOverride;
    light.isFire = lightTuning.isFire;
    light.castsShadow = lightTuning.castsShadows;
    light.shadowType = light.castsShadow ? lightTuning.shadowType : LightShadowType::None;
    light.shadowPriority = lightTuning.shadowPriority;
    light.priority = influence->mIndex < 0;

    const float radiusSq = radius * radius;
    const float distanceWeight = radiusSq / std::max(dist2 + radiusSq, 1.0f);
    const float selectionFade = std::clamp(1.0f - (dist / selectionRadius), 0.0f, 1.0f);
    light.selectionScore = lightLuma * distanceWeight * selectionFade * selectionFade *
                           (light.priority ? 2.0f : 1.0f) * lightTuning.scoreScale;
    light.shadowScore = light.castsShadow && light.shadowType != LightShadowType::None
                            ? light.selectionScore * std::max(light.shadowPriority, 0.0f)
                            : 0.0f;
    accumulate_reference_contribution(light, referencePos);

    if (source == SceneLightSource::Base) {
        ++sSceneLights.baseLightCount;
    } else if (source == SceneLightSource::Point) {
        ++sSceneLights.pointLightCount;
    } else if (source == SceneLightSource::Effect) {
        ++sSceneLights.effectLightCount;
    }
}

void add_base_light(const LIGHT_INFLUENCE& influence, const cXyz& referencePos, const LightingTuning& tuning) {
    const LightTuningScale lightTuning =
        light_tuning_for_source(tuning, scene_light_source_name(SceneLightSource::Base), 0);
    if (!lightTuning.enabled) {
        ++sSceneLights.rejectedLightCount;
        return;
    }

    const float r = clamp_color_component(influence.mColor.r) * lightTuning.colorScale[0];
    const float g = clamp_color_component(influence.mColor.g) * lightTuning.colorScale[1];
    const float b = clamp_color_component(influence.mColor.b) * lightTuning.colorScale[2];
    const float lightLuma = luminance(r, g, b);
    if (lightLuma <= 0.0001f || sSceneLights.lightCount >= MaxSceneLights) {
        ++sSceneLights.rejectedLightCount;
        return;
    }

    cXyz direction(influence.mPosition.x - referencePos.x, influence.mPosition.y - referencePos.y,
                   influence.mPosition.z - referencePos.z);
    direction = normalize_or_default(direction, cXyz(-0.45f, 0.65f, 0.62f));

    SceneLight& light = sSceneLights.lights[sSceneLights.lightCount++];
    light = {};
    light.stableId = reinterpret_cast<uintptr_t>(&influence);
    light.source = SceneLightSource::Base;
    light.type = SceneLightType::Directional;
    light.sourceIndex = 0;
    light.sourcePointer = &influence;
    light.worldDirection = direction;
    light.worldPosition = cXyz(referencePos.x + direction.x * BaseLightSyntheticRadius,
                               referencePos.y + direction.y * BaseLightSyntheticRadius,
                               referencePos.z + direction.z * BaseLightSyntheticRadius);
    light.color[0] = r;
    light.color[1] = g;
    light.color[2] = b;
    light.rawColor[0] = std::clamp(static_cast<float>(influence.mColor.r) * lightTuning.colorScale[0], 0.0f,
                                   1020.0f);
    light.rawColor[1] = std::clamp(static_cast<float>(influence.mColor.g) * lightTuning.colorScale[1], 0.0f,
                                   1020.0f);
    light.rawColor[2] = std::clamp(static_cast<float>(influence.mColor.b) * lightTuning.colorScale[2], 0.0f,
                                   1020.0f);
    light.radius = BaseLightSyntheticRadius * lightTuning.radiusScale;
    light.power = BaseLightSyntheticRadius * lightTuning.powerScale;
    light.fluctuation = influence.mFluctuation;
    light.luminance = lightLuma;
    light.distance = 0.0f;
    light.tuningAmbientScale = lightTuning.ambientScale * BaseLightDefaultAmbientScale;
    light.tuningDirectScale = lightTuning.directScale * BaseLightDefaultDirectScale;
    light.castsShadow = false;
    light.shadowType = LightShadowType::None;
    light.shadowPriority = 0.0f;
    light.priority = true;
    light.selectionScore = lightLuma * std::max(lightTuning.scoreScale, 0.0f) * BaseLightDefaultScoreScale;
    light.shadowScore = 0.0f;
    accumulate_reference_contribution(light, referencePos);
    ++sSceneLights.baseLightCount;
}

}  // namespace

void update_scene_lights(const cXyz& referencePos) {
    sSceneLights = {};
    sSceneLights.referencePosition = referencePos;
    sSceneLights.valid = true;

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr) {
        stage = "";
    }
    const LightingTuning& tuning = current_lighting_tuning(stage, dComIfGp_roomControl_getStayNo());

    add_base_light(g_env_light.base_light, referencePos, tuning);
    for (uint32_t i = 0; i < 100; ++i) {
        add_light(g_env_light.pointlight[i], SceneLightSource::Point, i, referencePos, tuning);
    }
    for (uint32_t i = 0; i < 5; ++i) {
        add_light(g_env_light.efplight[i], SceneLightSource::Effect, i, referencePos, tuning);
    }

    if (sSceneLights.accumulatedAmbientWeight > 0.0f) {
        const float invWeight = 1.0f / sSceneLights.accumulatedAmbientWeight;
        sSceneLights.accumulatedAmbientCentroid.x *= invWeight;
        sSceneLights.accumulatedAmbientCentroid.y *= invWeight;
        sSceneLights.accumulatedAmbientCentroid.z *= invWeight;
    }
}

void clear_scene_lights() {
    sSceneLights = {};
}

const SceneLightRegistry& scene_lights() {
    return sSceneLights;
}

const char* scene_light_source_name(SceneLightSource source) {
    switch (source) {
    case SceneLightSource::Base:
        return "base";
    case SceneLightSource::Point:
        return "point";
    case SceneLightSource::Effect:
        return "effect";
    }

    return "unknown";
}

float scene_light_distance_squared(const SceneLight& light, const cXyz& receiverPos) {
    return square(light.worldPosition.x - receiverPos.x) + square(light.worldPosition.y - receiverPos.y) +
           square(light.worldPosition.z - receiverPos.z);
}

float scene_light_soft_radius_strength(const SceneLight& light, const cXyz& receiverPos) {
    const float distance = std::sqrt(scene_light_distance_squared(light, receiverPos));
    const float power = std::max(light.power, 1.0f);
    const float t = std::clamp(1.0f - (distance / power), 0.0f, 1.0f);
    return t * t * (3.0f - (2.0f * t));
}

}  // namespace dusk::lighting
