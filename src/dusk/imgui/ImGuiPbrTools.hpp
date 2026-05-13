#pragma once

namespace dusk::imgui_pbr {

bool DrawExperimentalPbrToggle();
void DrawLightingWindow(bool& open);
void DrawMaterialOverrideWindow(bool& open);

}  // namespace dusk::imgui_pbr