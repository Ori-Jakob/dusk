#pragma once

#include "JSystem/J3DGraphBase/J3DStruct.h"
#include "SSystem/SComponent/c_xyz.h"

namespace dusk::enhanced_lighting {

void update_lights_for_current_view();
bool find_blended_light_influence(const cXyz& receiverPos, cXyz& outLightPos, GXColorS10& outLightColor,
                                  float& outLightDistance, float& outLightPower, float& outLightFluctuation);
bool find_shadow_override_position(const cXyz& receiverPos, cXyz& outLightPos);
bool should_suppress_real_shadows();
bool should_suppress_simple_shadows();

}  // namespace dusk::enhanced_lighting
