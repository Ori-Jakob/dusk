#include "dusk/lighting/lighting_features.h"

#include "dusk/settings.h"

namespace dusk::lighting {

bool pbr_material_rendering_enabled() {
    return getSettings().backend.enableExperimentalPbr.getValue();
}

bool experimental_lighting_enabled() {
    return getSettings().backend.enableExperimentalLighting.getValue();
}

bool enhanced_direct_lights_enabled() {
    const auto& settings = getSettings();
    return settings.backend.enableExperimentalLighting.getValue() && settings.backend.pbr.enhancedLights.getValue();
}

bool enhanced_shadows_enabled() {
    const auto& settings = getSettings();
    return settings.backend.enableExperimentalLighting.getValue() && settings.backend.pbr.enhancedShadows.getValue();
}

PbrEnhancedShadowMode effective_enhanced_shadow_mode() {
    const PbrEnhancedShadowMode mode = getSettings().backend.pbr.enhancedShadowMode.getValue();
    if (mode == PbrEnhancedShadowMode::Original) {
        return PbrEnhancedShadowMode::AuroraShadowMaps;
    }

    return mode;
}

bool aurora_shadow_maps_enabled() {
    const auto& settings = getSettings();
    return settings.backend.enableExperimentalLighting.getValue() && settings.backend.pbr.enhancedShadows.getValue() &&
           effective_enhanced_shadow_mode() == PbrEnhancedShadowMode::AuroraShadowMaps;
}

}  // namespace dusk::lighting
