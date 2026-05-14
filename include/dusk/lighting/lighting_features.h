#pragma once

namespace dusk {
enum class PbrEnhancedShadowMode : int;
}

namespace dusk::lighting {

bool pbr_material_rendering_enabled();
bool experimental_lighting_enabled();
bool enhanced_direct_lights_enabled();
bool enhanced_shadows_enabled();
PbrEnhancedShadowMode effective_enhanced_shadow_mode();
bool aurora_shadow_maps_enabled();

}  // namespace dusk::lighting
