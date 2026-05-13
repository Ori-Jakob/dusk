#include "dusk/enhanced_lighting.h"

#include "d/d_com_inf_game.h"
#include "dusk/lighting/lighting_features.h"
#include "dusk/lighting/light_tuning.h"
#include "dusk/lighting/lighting_scene.h"
#include "dusk/settings.h"
#include "m_Do/m_Do_mtx.h"

#include <aurora/gfx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace dusk::enhanced_lighting {
namespace {

constexpr uint32_t MaxLights = 64;
constexpr float LightBlendAlpha = 0.35f;
constexpr float LightFadeOut = 0.82f;
constexpr float MinTrackedIntensity = 0.01f;

struct CandidateLight {
    const void* source = nullptr;
    uintptr_t stableId = 0;
    lighting::SceneLightSource sourceKind = lighting::SceneLightSource::Point;
    uint32_t sourceIndex = 0;
    cXyz worldPosition{};
    float color[3] = {};
    float radius = 1.0f;
    float power = 1.0f;
    float fluctuation = 0.0f;
    float intensity = 1.0f;
    float score = 0.0f;
};

struct TrackedLight {
    const void* source = nullptr;
    uintptr_t stableId = 0;
    lighting::SceneLightSource sourceKind = lighting::SceneLightSource::Point;
    uint32_t sourceIndex = 0;
    cXyz worldPosition{};
    float color[3] = {};
    float radius = 1.0f;
    float power = 1.0f;
    float fluctuation = 0.0f;
    float intensity = 0.0f;
    float score = 0.0f;
    bool active = false;
    bool matched = false;
};

std::array<TrackedLight, MaxLights> sTrackedLights{};
bool sTrackedLightsInitialized = false;
char sTrackedStage[16] = {};
int sTrackedRoom = -999;

float luminance(float r, float g, float b) {
    return r * 0.2126f + g * 0.7152f + b * 0.0722f;
}

float lerp(float current, float target, float alpha) {
    return current + ((target - current) * alpha);
}

void lerp_position(cXyz& current, const cXyz& target, float alpha) {
    current.x = lerp(current.x, target.x, alpha);
    current.y = lerp(current.y, target.y, alpha);
    current.z = lerp(current.z, target.z, alpha);
}

void reset_tracked_lights() {
    sTrackedLights = {};
    sTrackedLightsInitialized = false;
}

void reset_tracked_lights_on_scene_change() {
    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr) {
        stage = "";
    }

    const int room = dComIfGp_roomControl_getStayNo();
    if (std::strncmp(sTrackedStage, stage, sizeof(sTrackedStage) - 1) == 0 && sTrackedRoom == room) {
        return;
    }

    std::strncpy(sTrackedStage, stage, sizeof(sTrackedStage) - 1);
    sTrackedStage[sizeof(sTrackedStage) - 1] = '\0';
    sTrackedRoom = room;
    reset_tracked_lights();
}

void insert_candidate(std::array<CandidateLight, MaxLights>& candidates, uint32_t& count, CandidateLight candidate) {
    if (candidate.score <= 0.0f) {
        return;
    }

    const uint32_t insertLimit = std::min<uint32_t>(count, candidates.size());
    uint32_t insertAt = insertLimit;
    for (uint32_t i = 0; i < insertLimit; ++i) {
        if (candidate.score > candidates[i].score) {
            insertAt = i;
            break;
        }
    }

    if (insertAt >= candidates.size()) {
        return;
    }

    const uint32_t moveEnd = std::min<uint32_t>(count, candidates.size() - 1);
    for (uint32_t i = moveEnd; i > insertAt; --i) {
        candidates[i] = candidates[i - 1];
    }

    candidates[insertAt] = candidate;
    count = std::min<uint32_t>(count + 1, candidates.size());
}

CandidateLight make_candidate(const lighting::SceneLight& sceneLight) {
    CandidateLight candidate{};
    candidate.source = sceneLight.sourcePointer;
    candidate.stableId = sceneLight.stableId;
    candidate.sourceKind = sceneLight.source;
    candidate.sourceIndex = sceneLight.sourceIndex;
    candidate.worldPosition = sceneLight.worldPosition;
    candidate.radius = sceneLight.radius;
    candidate.power = sceneLight.power;
    candidate.fluctuation = sceneLight.fluctuation;
    candidate.color[0] = sceneLight.color[0];
    candidate.color[1] = sceneLight.color[1];
    candidate.color[2] = sceneLight.color[2];
    candidate.intensity = sceneLight.tuningDirectScale;
    candidate.score = sceneLight.selectionScore;
    return candidate;
}

void collect_lights(std::array<CandidateLight, MaxLights>& candidates, uint32_t& candidateCount) {
    const lighting::SceneLightRegistry& registry = lighting::scene_lights();
    for (uint32_t i = 0; i < registry.lightCount; ++i) {
        insert_candidate(candidates, candidateCount, make_candidate(registry.lights[i]));
    }
}

int find_tracked_light(const void* source) {
    for (int i = 0; i < static_cast<int>(sTrackedLights.size()); ++i) {
        if (sTrackedLights[i].active && sTrackedLights[i].source == source) {
            return i;
        }
    }

    return -1;
}

int find_replacement_tracked_light() {
    int lowestScoreIndex = 0;
    float lowestScore = std::numeric_limits<float>::max();
    for (int i = 0; i < static_cast<int>(sTrackedLights.size()); ++i) {
        if (!sTrackedLights[i].active) {
            return i;
        }

        const float score = sTrackedLights[i].score * sTrackedLights[i].intensity;
        if (score < lowestScore) {
            lowestScore = score;
            lowestScoreIndex = i;
        }
    }

    return lowestScoreIndex;
}

void apply_candidate_to_tracked_light(TrackedLight& tracked, const CandidateLight& candidate, bool immediate) {
    tracked.source = candidate.source;
    tracked.stableId = candidate.stableId;
    tracked.sourceKind = candidate.sourceKind;
    tracked.sourceIndex = candidate.sourceIndex;
    tracked.active = true;
    tracked.matched = true;

    if (immediate) {
        tracked.worldPosition = candidate.worldPosition;
        tracked.color[0] = candidate.color[0];
        tracked.color[1] = candidate.color[1];
        tracked.color[2] = candidate.color[2];
        tracked.radius = candidate.radius;
        tracked.power = candidate.power;
        tracked.fluctuation = candidate.fluctuation;
        tracked.intensity = candidate.intensity;
        tracked.score = candidate.score;
        return;
    }

    lerp_position(tracked.worldPosition, candidate.worldPosition, LightBlendAlpha);
    tracked.color[0] = lerp(tracked.color[0], candidate.color[0], LightBlendAlpha);
    tracked.color[1] = lerp(tracked.color[1], candidate.color[1], LightBlendAlpha);
    tracked.color[2] = lerp(tracked.color[2], candidate.color[2], LightBlendAlpha);
    tracked.radius = lerp(tracked.radius, candidate.radius, LightBlendAlpha);
    tracked.power = lerp(tracked.power, candidate.power, LightBlendAlpha);
    tracked.fluctuation = lerp(tracked.fluctuation, candidate.fluctuation, LightBlendAlpha);
    tracked.intensity = lerp(tracked.intensity, candidate.intensity, LightBlendAlpha);
    tracked.score = lerp(tracked.score, candidate.score, LightBlendAlpha);
}

void update_tracked_lights(const std::array<CandidateLight, MaxLights>& candidates, uint32_t candidateCount) {
    const bool immediate = !sTrackedLightsInitialized;
    for (TrackedLight& tracked : sTrackedLights) {
        tracked.matched = false;
    }

    for (uint32_t i = 0; i < candidateCount; ++i) {
        const CandidateLight& candidate = candidates[i];
        int trackedIndex = find_tracked_light(candidate.source);
        bool newTrackedLight = false;
        if (trackedIndex < 0) {
            trackedIndex = find_replacement_tracked_light();
            sTrackedLights[trackedIndex] = {};
            newTrackedLight = true;
        }

        if (newTrackedLight && !immediate) {
            TrackedLight& tracked = sTrackedLights[trackedIndex];
            tracked.source = candidate.source;
            tracked.stableId = candidate.stableId;
            tracked.sourceKind = candidate.sourceKind;
            tracked.sourceIndex = candidate.sourceIndex;
            tracked.worldPosition = candidate.worldPosition;
            tracked.color[0] = candidate.color[0];
            tracked.color[1] = candidate.color[1];
            tracked.color[2] = candidate.color[2];
            tracked.radius = candidate.radius;
            tracked.power = candidate.power;
            tracked.fluctuation = candidate.fluctuation;
            tracked.active = true;
        }

        apply_candidate_to_tracked_light(sTrackedLights[trackedIndex], candidate, immediate);
    }

    for (TrackedLight& tracked : sTrackedLights) {
        if (!tracked.active || tracked.matched) {
            continue;
        }

        tracked.intensity *= LightFadeOut;
        tracked.score *= LightFadeOut;
        if (tracked.intensity < MinTrackedIntensity || tracked.score <= 0.0001f) {
            tracked = {};
        }
    }

    sTrackedLightsInitialized = true;
}

AuroraSceneLightSource make_aurora_scene_light_source(lighting::SceneLightSource source) {
    switch (source) {
    case lighting::SceneLightSource::Point:
        return AURORA_SCENE_LIGHT_SOURCE_GAME_POINT;
    case lighting::SceneLightSource::Effect:
        return AURORA_SCENE_LIGHT_SOURCE_GAME_EFFECT;
    }

    return AURORA_SCENE_LIGHT_SOURCE_UNKNOWN;
}

AuroraSceneLight make_aurora_scene_light(const CandidateLight& candidate, MtxP viewMtx, float directScale) {
    Vec lightPos{candidate.worldPosition.x, candidate.worldPosition.y, candidate.worldPosition.z};
    Vec shaderPos = lightPos;
    if (viewMtx != nullptr) {
        cMtx_multVec(viewMtx, &lightPos, &shaderPos);
    }

    AuroraSceneLight light{};
    light.type = AURORA_SCENE_LIGHT_POINT;
    light.source = make_aurora_scene_light_source(candidate.sourceKind);
    light.sourceIndex = candidate.sourceIndex;
    light.stableId = static_cast<uint64_t>(candidate.stableId);
    light.position[0] = shaderPos.x;
    light.position[1] = shaderPos.y;
    light.position[2] = shaderPos.z;
    light.radius = candidate.radius;
    light.color[0] = candidate.color[0];
    light.color[1] = candidate.color[1];
    light.color[2] = candidate.color[2];
    light.intensity = candidate.intensity * std::max(directScale, 0.0f);
    light.score = candidate.score;
    return light;
}

AuroraPbrShadowLightRequest make_aurora_shadow_light_request(const lighting::SceneLight& sceneLight,
                                                             const cXyz& targetPosition) {
    AuroraPbrShadowLightRequest request{};
    request.valid = true;
    request.source = make_aurora_scene_light_source(sceneLight.source);
    request.sourceIndex = sceneLight.sourceIndex;
    request.stableId = static_cast<uint64_t>(sceneLight.stableId);
    request.position[0] = sceneLight.worldPosition.x;
    request.position[1] = sceneLight.worldPosition.y;
    request.position[2] = sceneLight.worldPosition.z;
    request.target[0] = targetPosition.x;
    request.target[1] = targetPosition.y;
    request.target[2] = targetPosition.z;
    request.color[0] = sceneLight.color[0];
    request.color[1] = sceneLight.color[1];
    request.color[2] = sceneLight.color[2];
    request.radius = sceneLight.radius;
    request.score = sceneLight.shadowScore;
    request.priority = sceneLight.shadowPriority;
    return request;
}

uint32_t collect_tracked_light_outputs(std::array<CandidateLight, MaxLights>& outputs) {
    uint32_t outputCount = 0;
    for (const TrackedLight& tracked : sTrackedLights) {
        if (!tracked.active || tracked.intensity <= MinTrackedIntensity) {
            continue;
        }

        CandidateLight candidate{};
        candidate.source = tracked.source;
        candidate.stableId = tracked.stableId;
        candidate.sourceKind = tracked.sourceKind;
        candidate.sourceIndex = tracked.sourceIndex;
        candidate.worldPosition = tracked.worldPosition;
        candidate.color[0] = tracked.color[0];
        candidate.color[1] = tracked.color[1];
        candidate.color[2] = tracked.color[2];
        candidate.radius = tracked.radius;
        candidate.power = tracked.power;
        candidate.fluctuation = tracked.fluctuation;
        candidate.intensity = tracked.intensity;
        candidate.score = tracked.score * tracked.intensity;
        insert_candidate(outputs, outputCount, candidate);
    }

    return outputCount;
}

}  // namespace

void update_lights_for_current_view() {
    if (!lighting::experimental_lighting_enabled()) {
        reset_tracked_lights();
        lighting::clear_scene_lights();
        aurora_set_scene_lights(nullptr, 0);
        aurora_set_pbr_shadow_light_request(nullptr);
        return;
    }

    view_class* view = dComIfGd_getView();
    if (view == nullptr) {
        reset_tracked_lights();
        lighting::clear_scene_lights();
        aurora_set_scene_lights(nullptr, 0);
        aurora_set_pbr_shadow_light_request(nullptr);
        return;
    }

    reset_tracked_lights_on_scene_change();

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr) {
        stage = "";
    }
    const int room = dComIfGp_roomControl_getStayNo();
    const lighting::LightingTuning& tuning = lighting::current_lighting_tuning(stage, room);

    cXyz referencePos(view->lookat.eye.x, view->lookat.eye.y, view->lookat.eye.z);
    if (fopAc_ac_c* player = dComIfGp_getPlayer(0)) {
        referencePos = player->current.pos;
    }

    lighting::update_scene_lights(referencePos);

    std::array<CandidateLight, MaxLights> candidates{};
    uint32_t candidateCount = 0;
    collect_lights(candidates, candidateCount);
    update_tracked_lights(candidates, candidateCount);

    std::array<CandidateLight, MaxLights> outputs{};
    const uint32_t outputCount = collect_tracked_light_outputs(outputs);
    std::array<AuroraSceneLight, MaxLights> lights{};
    for (uint32_t i = 0; i < outputCount; ++i) {
        lights[i] = make_aurora_scene_light(outputs[i], view->viewMtx, tuning.enhancedDirectScale);
    }

    aurora_set_scene_lights(lights.data(), outputCount);

    const lighting::SceneLightRegistry& updatedRegistry = lighting::scene_lights();
    if (lighting::enhanced_shadows_enabled() && updatedRegistry.shadowLightIndex >= 0 &&
        static_cast<uint32_t>(updatedRegistry.shadowLightIndex) < updatedRegistry.lightCount)
    {
        const lighting::SceneLight& shadowLight = updatedRegistry.lights[updatedRegistry.shadowLightIndex];
        AuroraPbrShadowLightRequest shadowRequest = make_aurora_shadow_light_request(shadowLight, referencePos);
        aurora_set_pbr_shadow_light_request(&shadowRequest);
    } else {
        aurora_set_pbr_shadow_light_request(nullptr);
    }
}

bool find_blended_light_influence(const cXyz& receiverPos, cXyz& outLightPos, GXColorS10& outLightColor,
                                  float& outLightDistance, float& outLightPower, float& outLightFluctuation) {
    if (!lighting::enhanced_direct_lights_enabled()) {
        return false;
    }

    float directionWeight = 0.0f;
    cXyz blendedPosition(0.0f, 0.0f, 0.0f);
    float accumulatedColor[3] = {};
    float blendedFluctuation = 0.0f;
    float strongestContribution = 0.0f;
    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr) {
        stage = "";
    }
    const lighting::LightingTuning& tuning =
        lighting::current_lighting_tuning(stage, dComIfGp_roomControl_getStayNo());

    const auto accumulateLight = [&](const lighting::SceneLight& light) {
        const float rawR = light.rawColor[0];
        const float rawG = light.rawColor[1];
        const float rawB = light.rawColor[2];
        const float lightLuma = luminance(rawR / 255.0f, rawG / 255.0f, rawB / 255.0f);
        if (lightLuma <= 0.0001f) {
            return;
        }

        const float distance = std::sqrt(lighting::scene_light_distance_squared(light, receiverPos));
        const float power = std::max(light.power, 1.0f);
        const float t = std::clamp(1.0f - (distance / power), 0.0f, 1.0f);
        const float strength = t * t * (3.0f - (2.0f * t));
        if (strength <= 0.0f) {
            return;
        }

        accumulatedColor[0] += rawR * strength;
        accumulatedColor[1] += rawG * strength;
        accumulatedColor[2] += rawB * strength;

        const float weight = lightLuma * strength * (light.priority ? 2.0f : 1.0f);
        blendedPosition.x += light.worldPosition.x * weight;
        blendedPosition.y += light.worldPosition.y * weight;
        blendedPosition.z += light.worldPosition.z * weight;
        blendedFluctuation += light.fluctuation * weight;
        strongestContribution = std::max(strongestContribution, strength);
        directionWeight += weight;
    };

    const lighting::SceneLightRegistry& registry = lighting::scene_lights();
    if (!registry.valid) {
        lighting::update_scene_lights(receiverPos);
    }

    const lighting::SceneLightRegistry& updatedRegistry = lighting::scene_lights();
    for (uint32_t i = 0; i < updatedRegistry.lightCount; ++i) {
        accumulateLight(updatedRegistry.lights[i]);
    }

    if (directionWeight <= 0.0f) {
        return false;
    }

    const float invWeight = 1.0f / directionWeight;
    outLightPos.x = blendedPosition.x * invWeight;
    outLightPos.y = blendedPosition.y * invWeight;
    outLightPos.z = blendedPosition.z * invWeight;
    outLightColor.r =
        static_cast<s16>(std::clamp(accumulatedColor[0] * tuning.enhancedAmbientScale, 0.0f, 1020.0f));
    outLightColor.g =
        static_cast<s16>(std::clamp(accumulatedColor[1] * tuning.enhancedAmbientScale, 0.0f, 1020.0f));
    outLightColor.b =
        static_cast<s16>(std::clamp(accumulatedColor[2] * tuning.enhancedAmbientScale, 0.0f, 1020.0f));
    outLightColor.a = 0xFF;
    outLightFluctuation = blendedFluctuation * invWeight;

    if (strongestContribution <= 0.0f) {
        return false;
    }

    // The original path applies distance/power as a second multiplier. We already summed attenuated light color above,
    // so force the follow-up multiplier to one and let the game's existing env ratios finish the color scaling.
    outLightPower = 1.0f;
    outLightDistance = 0.0f;
    return true;
}

bool find_shadow_override_position(const cXyz& receiverPos, cXyz& outLightPos) {
    const auto& pbr = getSettings().backend.pbr;
    if (!lighting::enhanced_shadows_enabled() ||
        pbr.enhancedShadowMode.getValue() != PbrEnhancedShadowMode::OverrideDirection)
    {
        return false;
    }

    const lighting::SceneLightRegistry& registry = lighting::scene_lights();
    if (!registry.valid || registry.lightCount == 0) {
        return false;
    }

    if (registry.shadowLightIndex >= 0 && static_cast<uint32_t>(registry.shadowLightIndex) < registry.lightCount) {
        const lighting::SceneLight& shadowLight = registry.lights[registry.shadowLightIndex];
        if (shadowLight.castsShadow && shadowLight.shadowScore > 0.0f) {
            outLightPos = shadowLight.worldPosition;
            return true;
        }
    }

    GXColorS10 color{};
    float distance = 0.0f;
    float power = 1.0f;
    float fluctuation = 0.0f;
    return find_blended_light_influence(receiverPos, outLightPos, color, distance, power, fluctuation);
}

bool should_suppress_real_shadows() {
    const auto& pbr = getSettings().backend.pbr;
    if (!lighting::enhanced_shadows_enabled()) {
        return false;
    }

    const PbrEnhancedShadowMode mode = pbr.enhancedShadowMode.getValue();
    return mode == PbrEnhancedShadowMode::DisableGameShadows || mode == PbrEnhancedShadowMode::Hybrid ||
           mode == PbrEnhancedShadowMode::AuroraShadowMaps;
}

bool should_suppress_simple_shadows() {
    const auto& pbr = getSettings().backend.pbr;
    return lighting::enhanced_shadows_enabled() &&
           pbr.enhancedShadowMode.getValue() == PbrEnhancedShadowMode::DisableGameShadows;
}

}  // namespace dusk::enhanced_lighting
