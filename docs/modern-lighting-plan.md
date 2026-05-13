# Modern Lighting System Plan

This document lays out a staged plan for moving Dusk's PC lighting path toward a modern engine-style renderer while
preserving the original GameCube renderer as the default behavior. The plan assumes the existing experimental PBR path,
texture sidecars, runtime IBL probes, enhanced direct lights, and enhanced shadow toggles already exist.

The goal is not to replace the entire game renderer in one pass. The goal is to build a parallel lighting pipeline that
can consume the game's original light/environment data, expose modern rendering features through Aurora, and let Dusk
opt into those features safely.

## Target Outcome

A modernized lighting path should eventually provide:

- Many local lights affecting the same object at the same time.
- Physically plausible PBR direct lighting with stable attenuation and color handling.
- Directional, point, and spot light support with per-light shadow options.
- Shadow maps owned by Aurora instead of relying on the original single-source real-shadow path.
- Stable ambient lighting that does not switch based on the nearest light.
- Probe-based indirect diffuse and specular lighting with authored and runtime data.
- Scene/room-aware probe volumes instead of one active global room probe.
- Better occlusion: material AO, ambient occlusion, contact shadows, and eventually dynamic GI occlusion.
- Debug views for every major lighting input and output.
- Per-stage/per-room tuning hooks so the system can be calibrated gradually.

## Non-Goals

These are intentionally out of scope for the first pass:

- A full path tracer as the main gameplay renderer.
- Perfect physical unit conversion from GameCube light values.
- Replacing every original TEV material with PBR.
- Replacing the original renderer for non-PBR materials.
- Real-time GI that is fully accurate with arbitrary moving geometry.

The path should be modern and extensible, but it should stay practical for this codebase.

## Current Baseline

The current experimental lighting work already has useful pieces:

- PBR material sidecars for albedo plus roughness, metallic, AO, specular, normal, and emissive.
- Built-in PBR overrides for known untextured sword blade materials.
- Aurora shader support for PBR direct diffuse/specular, IBL diffuse/specular, BRDF LUT, debug views, and dynamic local GI.
- Storage-backed enhanced PBR light upload, so the current direct-light list is submitted once per rendered view and
  consumed from GPU storage by PBR shaders.
- Runtime probe capture, filtering, caching, scene/room reuse, nearest cached probe fallback, and probe transition blending.
- Dusk-side enhanced light extraction from `g_env_light.pointlight` and `g_env_light.efplight`.
- A first `dusk::lighting` scene registry that stores active point/effect lights with stable source IDs, source metadata,
  reference distance, selection score, receiver-relative ambient contribution metrics, accumulated ambient color, a
  compatibility centroid, and ImGui inventory display.
- An initial Aurora `aurora_set_scene_lights(...)` API bridge. Dusk now submits the full tracked enhanced scene-light
  list with source metadata, while Aurora clamps that feed to the active GPU light budget and adapts it into the existing
  storage-backed PBR light buffer until the clustered/Forward+ path is ready.
- A first file-backed lighting tuning layer loaded from `texture_replacements/lighting`. It supports neutral-by-default
  global, stage, and room JSON overrides for enhanced ambient scale, enhanced direct-light scale, scene-light activation
  distance, and per-light color/intensity/radius rules.
- A first ImGui light editor for the current stage/room. It can select detected scene lights, apply per-light tuning live,
  and save the selected light rule or activation distance policy to the room JSON file under
  `texture_replacements/lighting/<stage>/room_<room>.json`.
- Per-light shadow-caster policy can now be authored through the same tuning path using `castsShadows` and
  `shadowPriority`. Override-direction shadows use the chosen shadow source when one is available, and the lighting
  overlay/inventory expose that source.
- Dusk now submits the selected shadow light to Aurora as a shadow-map request containing source metadata, world-space
  position, target position, color, radius, score, and priority. This is the handoff the future Aurora shadow atlas will
  consume.
- A first world-space lighting scene overlay that projects active scene lights over the game view, draws their influence
  radii, highlights priority/dominant ambient lights, and marks the accumulated ambient centroid.
- Experimental additive ambient-light override for the original point-light influence path.
- ImGui controls for PBR, enhanced lights, enhanced shadows, IBL, dynamic GI, and debug overlays.
- A separate `backend.enableExperimentalLighting` gate now owns modern lighting, light-editor, and shadow features so
  stock/non-PBR materials can opt into enhanced scene lighting without enabling PBR material replacement.
- Initial Aurora shadow-map API/settings/status scaffolding.
- Initial Aurora shadow-light request API/status plumbing.

The biggest remaining architectural problem is that the original game was designed around selecting one nearby point
light for color influence and shadow direction. A modern path needs to treat that original behavior as an input source,
not as the final lighting model.

## Design Principles

1. Keep the original renderer intact.

All modern lighting features should remain behind PC-only experimental toggles until they are proven stable. Original
GX/TEV behavior should still be available and should remain the fallback.

2. Extract once, render many.

Dusk should collect original lights, environment colors, room state, and actor-local light hints into a PC-side lighting
scene. Aurora should consume this scene for PBR direct lighting, shadows, GI, debug overlays, and future renderer paths.

3. Keep Dusk's CPU work shallow.

Dusk is already CPU bound, so the CPU side should avoid per-draw lighting decisions wherever possible. CPU extraction
should gather coarse scene data once per native frame, upload compact buffers, and let Aurora/GPU stages handle light
culling, clustered assignment, probe blending, shadow filtering, AO, and GI detail work.

4. Separate lighting data from lighting policy.

The extractor should gather raw lights. Separate policy layers should decide attenuation, priority, shadow casting,
probe placement, and per-stage tuning.

5. Avoid nearest-light switches.

Nearest-light logic can be useful for compatibility or fallback, but modern direct lighting and ambient lighting should
sum or cluster many contributors. Shadows can use priority/selection, but selection should be stable and debounced.

6. Prefer incremental renderer upgrades.

Forward+/clustered lighting, a shadow atlas, and probe volumes are more realistic near-term targets than a full deferred
renderer rewrite.

## Proposed Architecture

### Dusk-Side Lighting Scene

Add a PC-only lighting layer under `src/dusk/lighting`:

- `LightRegistry`: stores the current frame's extracted lights with stable IDs.
- `LightExtractor`: reads original sources such as `g_env_light.pointlight`, `g_env_light.efplight`, base light, room
  light vectors, actor-local lights, boss lights, scripted demo lights, and environment state.
- `LightPolicy`: applies per-stage rules, intensity scale, color scale, attenuation mode, shadow-caster rules, and
  culling thresholds.
- `ProbeManager`: owns scene/room probe metadata, authored probe volume definitions, runtime capture requests, and probe
  selection weights.
- `LightingDebug`: exports ImGui tables and overlays for active lights, shadows, probes, and per-frame budgets.

The registry should use stable IDs where possible:

- Pointer identity for persistent `LIGHT_INFLUENCE` objects.
- Stage/room/index identifiers for stage-authored lights.
- Actor process ID plus local light index for actor-owned lights.
- Synthetic IDs for environment and fallback lights.

Stable IDs matter because fades, shadow caching, probe influence, and debug selection need continuity.

CPU-side responsibilities should stop at extraction, coarse filtering, stable IDs, and authoring/debug metadata. Avoid
expensive per-draw light selection, per-pixel approximations, repeated room scans, or synchronous GPU readbacks.

### Aurora-Side Lighting Renderer

Add an Aurora lighting layer under `extern/aurora/lib/gx` or a new `lib/lighting` area:

- `lighting_scene`: GPU buffers for lights, environment parameters, and debug metadata.
- `clustered_lighting`: optional Forward+ light bins for efficient many-light shading.
- `shadow_atlas`: depth textures, view/projection matrices, per-light shadow descriptors, and cache state.
- `probe_volume`: irradiance/prefilter probe textures, probe metadata buffers, and blend weights.
- `lighting_debug`: debug views for direct light contribution, shadow maps, probe weights, and GI terms.

Aurora should expose C APIs for Dusk:

- `aurora_set_scene_lights(...)`
- `aurora_set_environment_lighting(...)`
- `aurora_set_shadow_requests(...)`
- `aurora_set_probe_volume(...)`
- `aurora_get_lighting_status()`

The existing enhanced PBR APIs can either wrap these or be migrated onto them.

### CPU-Bound Strategy

Because Dusk is CPU bound, prefer GPU-side work even if the GPU solution is slightly more complex:

- Upload all plausible lights for the current scene/room to a GPU storage buffer once per native frame.
- Keep CPU culling coarse: stage, room, enabled flag, max distance, and obvious invalid lights.
- Do fine light selection on the GPU with Forward+/clustered bins or shader-side loops over compact index lists.
- Build light lists from depth on the GPU when the depth buffer is available.
- Filter probes, generate prefiltered mips, run AO, and evaluate contact shadows on the GPU.
- Keep shadow-map rendering and filtering in Aurora; Dusk should only provide light/caster policy hints.
- Avoid per-material CPU lighting work unless it is cached or only used for compatibility with original TEV behavior.
- Avoid CPU readbacks in gameplay. Debug readbacks should be opt-in and clearly marked as expensive.

Good CPU work:

- Extract raw game light data.
- Assign stable source IDs.
- Load authoring/tuning files.
- Decide high-level feature toggles and budgets.
- Submit compact buffers and requests to Aurora.

Bad CPU work:

- Per-draw nearest-light searches.
- Per-object probe selection on the CPU for every draw.
- Rebuilding shadow caster lists every presentation frame.
- Recomputing light influence for debug UI when the overlay is hidden.
- Polling GPU results synchronously.

## Phase 1: Lighting Data Inventory

Purpose: understand every source of game lighting before replacing policy.

Tasks:

- Catalog `g_env_light.pointlight`, `g_env_light.efplight`, `base_light`, `dungeonlight`, `BOSS_LIGHT`, room light
  vectors, actor-local `LIGHT_INFLUENCE` members, demo lights, and special-case environment colors.
- Add an ImGui "Lighting Scene" window listing all detected lights with source type, position, color, radius/power,
  room/stage, stable ID, and current contribution.
- Add world/debug overlays for light spheres, labels, dominant ambient selection, and eventual shadow-caster selection.
- Log per-stage light counts and identify stages with unusual light values.

Acceptance criteria:

- In any room, we can see all active original point/effect lights in ImGui.
- We can tell which lights are stage-authored, actor-authored, effect-authored, or fallback/environment.
- No rendering behavior changes are required in this phase.

## Phase 2: Replace One-Light Ambient Policy

Purpose: stop the original nearest-light ambient handoff from controlling modern lighting.

Tasks:

- Keep the current additive point/effect ambient override, but move it into the new lighting scene/policy layer.
- Let all active nearby ambient lights contribute with soft attenuation.
- Add per-stage scale controls for accumulated ambient color to prevent overbright rooms.
- Keep `mLightPosWorld` as a weighted centroid only for compatibility with the old real-shadow path.
- Add a debug view for "Original nearest light" versus "Enhanced accumulated ambient".

Acceptance criteria:

- Moving between two torches does not visibly switch ambient tint from one torch to the other.
- Multiple nearby lights visibly combine.
- Disabling enhanced lighting restores the original nearest-light behavior.

## Phase 3: Modern Direct Lighting

Purpose: make PBR direct lighting independent of original GX light-slot limits.

Recommended approach: clustered forward lighting rather than deferred rendering.

Why clustered forward:

- Dusk/Aurora already emits many GX-style draw calls with material-specific shaders.
- Forward PBR fits the existing shader-generation model.
- Transparency and original TEV behavior are easier to preserve.
- A full deferred rewrite would require G-buffer ownership, material remapping, and major changes to draw ordering.

Tasks:

- Replace the fixed 8-light PBR uniform block with a storage buffer of scene lights.
- Upload all plausible scene/room lights to the GPU with only coarse CPU filtering.
- Add a simple GPU light-list path before doing expensive CPU-side per-draw culling.
- Add Forward+ clusters early: build a screen/depth cluster grid and assign light indices to clusters on the GPU.
- Keep CPU-culling only as a fallback/debug path, not the target architecture.
- Support light types:
  - Directional light for sun/moon/environment key.
  - Point lights from original point/effect lights.
  - Spot lights for future authored overrides.
  - Optional capsule/area approximations for large glowing objects.
- Add physically plausible attenuation options:
  - Legacy radius for compatibility.
  - Inverse square with radius fade for modern PBR.
  - Per-stage intensity calibration.
- Keep a direct-light debug mode showing selected/contributing lights per pixel.

Acceptance criteria:

- More than 8 scene lights can exist without changing shader uniform layout.
- PBR materials receive multiple local light contributions without snapping.
- CPU cost does not scale with draw count for normal PBR lighting.
- Performance remains stable in light-heavy rooms.

## Phase 4: Shadow System Replacement

Purpose: replace the original single-source real-shadow path with Aurora-owned shadow maps.

Tasks:

- Implement a shadow atlas:
  - One atlas texture or a small set of depth textures.
  - Per-light atlas rectangles.
  - Per-light view/projection matrices.
  - PCF sampling and bias controls.
- Keep shadow caster selection coarse on CPU and let Aurora own depth rendering, atlas allocation, filtering, and receiver
  sampling.
- Start with spot/directional-style local shadows:
  - One selected high-priority light.
  - Depth-only caster replay.
  - Receiver sampling in PBR shaders.
- Expand to multiple lights:
  - Stable shadow-caster selection.
  - Shadow cache with update throttling.
  - Per-light priority based on contribution, screen relevance, and explicit flags.
- Add directional shadows:
  - Cascaded shadow maps for sun/moon/key light.
  - Stage/room tuning for cascade distance and bias.
- Move cascade split selection, PCF/EVSM-style filtering, and receiver-space shadow tests to GPU shader code.
- Add point-light shadows later:
  - Cubemap shadows are expensive because they require six faces per light.
  - Use only for important lights.
- Add contact shadows:
  - Screen-space contact shadows for short-range grounding.
  - Optional only for PBR materials at first.

Acceptance criteria:

- Enhanced shadow mode can render at least one Aurora shadow map on PBR receivers.
- Old real shadows can be suppressed without leaving characters completely ungrounded.
- Shadow source changes are stable, debounced, and visible in debug UI.

## Phase 5: Environment, Sky, Fog, And Exposure

Purpose: make lighting feel coherent even before full GI is available.

Tasks:

- Formalize environment lighting parameters:
  - Sky color.
  - Ground color.
  - Horizon color.
  - Key light direction.
  - Fog color/density.
  - Exposure/tonemap.
- Pull values from the original environment system where possible.
- Add per-stage overrides for problem rooms.
- Add optional color temperature controls for authored lights.
- Add filmic tonemapping or a configurable tonemap curve for PBR output.
- Ensure fog is applied consistently after PBR lighting and debug modes.

Acceptance criteria:

- PBR materials do not look detached from the original scene palette.
- Indoor/outdoor transitions preserve believable ambient color.
- Bright lights can be intense without clipping harshly.

## Phase 6: Probe-Based Indirect Lighting

Purpose: move from one active room probe to a modern probe-volume model.

Tasks:

- Define authored probe volumes:
  - JSON or TOML files under `texture_replacements/pbr_ibl` or a new `lighting` folder.
  - Probe position, radius, room/stage, priority, capture source, and blend shape.
- Support multiple active probes:
  - Blend by distance/volume weight.
  - Prefer same room, but allow cross-room blending near transitions.
  - Keep fallback/environment probe as a low-priority contributor.
- Store probe metadata in a GPU buffer so object/pixel probe weights can be evaluated in Aurora instead of per draw on
  the Dusk CPU.
- Add runtime probe capture for authored probe positions:
  - Capture at explicit probe locations rather than always at the camera.
  - Capture only when requested, on scene change, or through tools.
- Add probe bake/export tooling:
  - Save runtime captured probes as authored assets.
  - Inspect probe faces, irradiance, and prefilter mips.
- Add per-object probe selection:
  - Sample probes by object/world position instead of camera position only.
  - Prefer GPU-side probe weighting for PBR draws; CPU-side selection should be a compatibility/debug fallback.

Acceptance criteria:

- A room can have more than one indirect lighting region.
- Moving through a doorway blends between probes instead of swapping.
- Runtime captured probes can be saved and reused as authored IBL assets.

## Phase 7: Ambient Occlusion And Contact Grounding

Purpose: give indirect lighting geometric grounding.

Tasks:

- Add GTAO or SSAO as a screen-space pass.
- Feed AO into PBR indirect diffuse/specular and optional non-PBR ambient tint.
- Add contact shadows from direct lights or screen-space ray marching.
- Keep material AO from sidecars as a separate multiplier from screen-space AO.
- Add debug modes for material AO, screen AO, contact shadows, and final indirect occlusion.

Acceptance criteria:

- Characters and props stop looking like they float under strong IBL.
- AO does not over-darken foggy or already dark scenes.
- The effect scales with resolution and has performance controls.

## Phase 8: Dynamic GI Experiments

Purpose: add real dynamic bounce behavior after the stable direct/shadow/probe foundation exists.

Candidate approaches:

- Improved local light bounce:
  - Current dynamic local GI is shader-local.
  - Extend it with better surface heuristics and occlusion.
- Screen-space GI:
  - Uses depth/color buffers.
  - Good for visible bounce, misses off-screen geometry.
  - Best as an optional enhancement over probes.
- DDGI-style probe grid:
  - Dynamic irradiance probes updated over time.
  - More stable than pure screen-space GI.
  - Requires probe placement, ray/capture strategy, relocation/visibility, and update budgets.
- Voxel/SDF GI:
  - Expensive to build for this renderer.
  - Useful only if a scene voxelization path emerges.
- Hardware ray tracing or path tracing:
  - Future research only.
  - Requires backend support, acceleration structures, material mapping, and fallback paths.

Recommended first dynamic GI target:

1. Probe-volume GI with authored/runtime probes.
2. Screen-space diffuse bounce as an optional detail layer.
3. DDGI only after probe volumes and shadow maps are stable.

Acceptance criteria:

- Dynamic GI improves lighting in motion without causing frame hitches.
- GI update cost is visible in the debug overlay.
- Disabling dynamic GI leaves stable probe-based indirect lighting intact.

## Phase 9: Tooling And Tuning

Purpose: make the system authorable instead of hardcoded.

Tasks:

- Add a lighting debug window:
  - Scene light list.
  - Selected PBR lights.
  - Shadow atlas entries.
  - Probe list and weights.
  - Per-stage override status.
  - GPU/CPU timing.
- Add debug render modes:
  - Light influence.
  - Direct diffuse.
  - Direct specular.
  - Ambient/IBL diffuse.
  - IBL specular.
  - Shadow factor.
  - AO/contact shadows.
  - Probe weights.
- Add authoring files:
  - `lighting/global.json`
  - `lighting/<stage>.json`
  - `lighting/<stage>/room_<room>.json`
- Add hot reload for lighting overrides.
- Add export buttons for runtime probes and captured light inventories.

Acceptance criteria:

- A room can be tuned without recompiling.
- Debug overlays explain why a pixel is lit a certain way.
- Captured probes and stage overrides can be iterated quickly.

## Suggested File Layout

Dusk:

```text
include/dusk/lighting/
  lighting_features.h
  lighting_scene.h
  light_extractor.h
  light_policy.h
  probe_manager.h

src/dusk/lighting/
  lighting_features.cpp
  lighting_scene.cpp
  light_extractor.cpp
  light_policy.cpp
  probe_manager.cpp
  lighting_debug.cpp

src/dusk/imgui/
  ImGuiLightingTools.cpp/.hpp
  ImGuiPbrTools.cpp/.hpp
```

Aurora:

```text
extern/aurora/lib/gx/
  lighting.hpp
  lighting.cpp
  shadow_atlas.hpp
  shadow_atlas.cpp
  probe_volume.hpp
  probe_volume.cpp
```

The existing `src/dusk/enhanced_lighting.cpp` can become the first implementation slice of this new layout or remain as
the compatibility wrapper while the new modules are built.

## Roadmap

### Milestone 1: Stabilize Current Enhanced Lighting

- Finish additive ambient light behavior.
- Add debug display for every contributing ambient light.
- Add a toggle to compare original nearest light versus enhanced additive ambient.
- Add per-stage ambient scale. `texture_replacements/lighting/global.json`, `<stage>.json`, `<stage>/room_<room>.json`,
  and `<stage>_room_<room>.json` can set `enhancedAmbientScale` and `enhancedDirectScale`.

Example:

```json
{
  "enhancedAmbientScale": 0.85,
  "enhancedDirectScale": 1.15,
  "selectionRadiusScale": 2.5,
  "selectionRadiusPadding": 1500.0,
  "lights": [
    {
      "source": "point",
      "index": 3,
      "ambientScale": 0.5,
      "directScale": 1.4,
      "castsShadows": true,
      "shadowPriority": 2.0,
      "colorScale": [1.0, 0.85, 0.7]
    },
    {
      "source": "effect",
      "index": 1,
      "disabled": true
    }
  ]
}
```

Exit criteria:

- The closest-light switch is gone for ambient tint in test rooms.
- No new crashes or startup regressions.

### Milestone 2: Build The Lighting Scene Registry

- Introduce stable light IDs.
- Move point/effect light extraction into a registry.
- Add light source type metadata.
- Add ImGui light inventory.

Exit criteria:

- Enhanced direct lights, ambient override, and shadows all use the same registry.

### Milestone 3: Move PBR Direct Lights To A Storage Buffer

- Replace fixed enhanced-light uniforms with a larger light buffer.
- Upload coarse-filtered scene lights once per native frame.
- Keep a CPU-culling fallback for debugging and low-risk bring-up.
- Add a first clustered/Forward+ GPU prototype immediately after the buffer path is stable.

Exit criteria:

- More than 8 lights can exist and be inspected.
- Enhanced PBR lights are storage-backed instead of embedded in every PBR uniform block.
- PBR material lighting no longer depends on GX light slot count.
- Normal PBR light selection does not add meaningful per-draw CPU cost.

### Milestone 4: Implement Aurora Shadow Maps

- Create shadow atlas resources.
- Render depth-only caster pass for one high-priority light.
- Sample shadow factor in PBR receivers.
- Add atlas debug view.

Exit criteria:

- `aurora_shadow_maps` produces visible shadows without original real shadows.

### Milestone 5: Add Probe Volumes

- Define authored probe metadata.
- Blend multiple probes by object/world position.
- Add runtime probe export.

Exit criteria:

- Multi-probe indirect lighting works in at least one room and one doorway transition.

### Milestone 6: Add AO And Contact Shadows

- Implement GTAO/SSAO.
- Add contact shadow layer.
- Integrate with indirect lighting and debug views.

Exit criteria:

- PBR objects have modern grounding even with bright IBL.

### Milestone 7: Dynamic GI Research Branch

- Prototype screen-space GI or DDGI.
- Measure cost and artifacts.
- Keep probe GI as the stable fallback.

Exit criteria:

- Dynamic GI can be toggled independently and does not destabilize normal play.

## Performance Budgets

Initial targets:

- CPU lighting extraction: less than 0.5 ms in normal rooms.
- CPU-side light upload/coarse filtering: less than 0.25 ms in normal rooms.
- GPU-side light binning/culling: less than 0.5 ms in normal rooms.
- Shadow maps: scalable, ideally less than 2 ms for the first useful mode.
- Probe capture/filter: amortized and never active every frame by default.
- AO/contact shadows: resolution-scaled and independently toggleable.
- Debug overlays: allowed to be expensive only while visible.

CPU scaling rules:

- CPU lighting cost should scale with number of active original light sources, not number of draw calls.
- Per-draw CPU lighting work should be treated as temporary compatibility glue.
- Native simulation frames should own extraction/upload. Interpolation/presentation frames should reuse existing lighting
  buffers unless a debug tool explicitly asks for fresh data.
- GPU debug readbacks should never be required for normal gameplay.

All features should expose status counters before being treated as usable.

## Risk List

- Original TEV materials may depend on color-combiner results that should not be fully replaced.
- Old shadow receivers and modern shadow maps may disagree about caster/receiver geometry.
- Too many additive original lights can over-brighten rooms without stage calibration.
- Point-light cubemap shadows are expensive.
- Runtime probes can hitch if capture/filter scheduling regresses.
- Screen-space GI/AO can conflict with fog, transparency, and low internal resolutions.
- Modern lighting can easily make the original art direction look wrong without tuning tools.
- A CPU-heavy "modern" lighting layer could make the game feel worse even if the visuals improve. Every milestone needs
  CPU timing before it graduates from experimental.

## Immediate Next Steps

1. Add a Forward+/clustered GPU light-selection prototype on top of the scene-light API.
2. Start the Aurora shadow atlas with one depth-only shadow-casting light.
3. Expand the lighting scene overlay with actual Aurora shadow-caster selection once the shadow atlas owns caster choice.
4. Expand lighting tuning files with per-light rules, shadow priority, and color/intensity remaps.

This sequence removes the remaining single-light behavior first, then builds the renderer pieces needed for modern direct
lighting, shadows, and GI.
