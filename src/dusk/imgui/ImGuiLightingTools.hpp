#pragma once

namespace dusk::imgui_lighting {

bool DrawExperimentalLightingToggle();
void DrawEnhancedLightingControls();
void DrawEnhancedShadowControls();
void DrawSceneEditorWindow(bool& open);
void DrawSceneOverlayContents();
void DrawIblStatusContents();

}  // namespace dusk::imgui_lighting