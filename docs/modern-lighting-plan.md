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
- Dusk-side enhanced light extraction from `g_env_light.base_light`, `g_env_light.pointlight`, and
  `g_env_light.efplight`. The base light is treated as a conservative synthetic directional environment/key light
  because the original game stores it as a direction anchor with zero power.
- A first `dusk::lighting` scene registry that stores active point/effect lights with stable source IDs, source metadata,
  reference distance, selection score, receiver-relative ambient contribution metrics, accumulated ambient color, a
  compatibility centroid, light type metadata, and ImGui inventory display.
- An initial Aurora `aurora_set_scene_lights(...)` API bridge. Dusk now submits the full tracked enhanced scene-light
  list with source metadata, while Aurora clamps that feed to the active GPU light budget and adapts it into the existing
  storage-backed PBR light buffer until the clustered/Forward+ path is ready. The bridge now preserves point versus
  directional light type and uploads directional vectors for the synthetic environment key.
- A first file-backed lighting tuning layer loaded from `texture_replacements/lighting`. It supports neutral-by-default
  global, stage, and room JSON overrides for enhanced ambient scale, enhanced direct-light scale, scene-light activation
  distance, and per-light color/intensity/radius rules.
- A first ImGui light editor for the current stage/room. It can select detected scene lights, apply per-light tuning live,
  and save the selected light rule or activation distance policy to the room JSON file under
  `texture_replacements/lighting/<stage>/room_<room>.json`.
- Per-light `isFire` metadata authored through the light editor now drives optional runtime fire flicker for enhanced
  scene lights. The effect is deterministic per stable light ID, scales by the original light fluctuation value when one
  exists, and is controlled by global strength/speed debug-menu settings.
- Per-light shadow-caster policy can now be authored through the same tuning path using `castsShadows` and
  `shadowPriority`. It also supports `shadowType` (`none`, `local_projected`, `directional`, or `point`). The current
  `local_projected` mode should be treated as a projector/spot-style approximation, not a physically correct point-light
  shadow. Torch-like omni lights should default to `none` or `contact_only` until true point-light shadows exist.
  Override-direction shadows use the chosen shadow source when one is available, and the lighting overlay/inventory expose
  that source.
- Enhanced shadows now have a single ownership rule: disabled means the original game shadow renderer is untouched,
  while enabled selects an enhanced path such as Aurora shadow maps, hybrid contact shadows, direction override, or full
  original-shadow suppression. Legacy `original` config values are accepted but normalized to Aurora shadow maps when the
  enhanced-shadow toggle is on.
- Dusk now submits the selected shadow light to Aurora as a shadow-map request containing source metadata, world-space
  position, target position, color, radius, score, and priority. This is the handoff the future Aurora shadow atlas will
  consume.
- Dusk also submits a ranked list of shadow requests. Aurora stores the list as atlas-facing descriptors and renders
  renderable non-point requests into separate layers of a small shadow texture array. Receivers can sample the matching
  captured slot for each enhanced light.
- Aurora shadow capture uses a shadow-safe material texture bind group. It preserves normal material texture bindings for
  depth-only/alpha-tested caster replay, but replaces the receiver shadow-map bindings with the fallback shadow view.
  This is required by WebGPU: the same shadow depth texture cannot be written as a render attachment and sampled as a
  texture binding in the same command encoder synchronization scope.
- Aurora shadow-map status now reports request, captured, and pending masks plus the per-slot request metadata used by
  the atlas. The ImGui shadow controls also have a manual refresh button so stale or edited light requests can be forced
  back through the atlas capture path without restarting.
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

### Shadow Research Notes

Modern engines generally do not solve every light with one shadow model. They split shadowing by light type, then cache
and filter each class differently:

- Directional/key lights use cascaded or virtualized shadow maps so nearby geometry gets more precision while far scenery
  stays stable.
- Spot/projector lights use one perspective shadow map and fit cleanly into an atlas.
- Point/omni lights are much more expensive because a physically correct shadow needs six views, usually a cubemap or
  cubemap-array slot. They should be limited to important authored lights.
- Contact shadows or screen-space shadows are commonly used as a cheap grounding layer, especially when full local
  shadow maps are too expensive or too unstable.
- Shadow caches and update policies matter as much as resolution. Static casters can remain cached, dynamic casters can
  update on a smaller budget, and moving lights should invalidate only the slots they actually affect.

For Dusk, this means the current `local_projected` path should not be treated as the default answer for torch-like point
lights. It is useful as a directed/projector approximation, but it will produce wrong-looking rectangular or directionally
snapping shadows when used as an omni light. The next shadow work should prove caster/receiver correctness and authoring
semantics before increasing the number of active shadow maps.

Recommended shadow type semantics:

- `none`: the light contributes direct/ambient lighting but does not cast a mapped shadow.
- `contact_only`: the light may contribute short-range grounding through the contact-shadow/AO path, but no shadow map is
  allocated.
- `local_projected`: a spot/projector-style local shadow with an explicit direction, cone/box bounds, and receiver fade.
  Use for authored fixtures where a projected beam is believable.
- `directional`: a cascaded key-light shadow for sun/moon/environment lighting.
- `point`: future true point-light shadows using six faces or a lower-cost substitute. This should stay opt-in and
  budgeted.

Near-term shadow debugging requirements:

- Atlas/depth debug views that can show each active slot, layer, or atlas rectangle.
- Light frustum/projection overlays in the world view.
- Caster inclusion diagnostics, especially for skinned actors like Link and alpha-tested geometry.
- Receiver-coordinate debug modes for projected UV, depth comparison, bias, and final shadow factor.
- Per-slot update counters that distinguish static cached draws from dynamic refreshed draws.

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

- In any room, we can see the active base/environment key plus original point/effect lights in ImGui.
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

- Replace the fixed 8-light PBR uniform block with a storage buffer of scene lights. A storage-backed buffer exists for
  the current enhanced light feed; the remaining work is to promote it from a flat shader loop into clustered/Forward+
  assignment.
- Upload all plausible scene/room lights to the GPU with only coarse CPU filtering.
- Add a simple GPU light-list path before doing expensive CPU-side per-draw culling.
- Add Forward+ clusters early: build a screen/depth cluster grid and assign light indices to clusters on the GPU.
- Keep CPU-culling only as a fallback/debug path, not the target architecture.
- Support light types:
  - Directional light for sun/moon/environment key. Initial directional upload exists for `base_light`.
  - Point lights from original point/effect lights. Initial point upload exists for `pointlight` and `efplight`.
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

- Keep the atlas/array foundation, but make the next pass correctness-first:
  - Show every active atlas slot/layer in the debug UI.
  - Draw the selected light projection/frustum over the scene.
  - Expose projected UV, receiver depth, comparison depth, bias, and final shadow factor debug modes.
  - Add caster inclusion diagnostics so it is obvious whether static geometry, alpha-tested geometry, and skinned actors
    are present in a slot.
- Treat `local_projected` as a spot/projector shadow, not a default point-light shadow:
  - Require an explicit authored direction or target.
  - Use projection edge fade and depth fade only as cleanup, not as the core correctness fix.
  - Default torch-like omni lights to `contact_only` or `none` until point shadows exist.
- Keep CPU shadow work coarse and stable:
  - Dusk submits high-level light/caster policy hints and stable IDs.
  - Aurora owns depth rendering, atlas allocation, filtering, receiver sampling, and cache invalidation.
  - Avoid rebuilding caster lists every presentation frame.
- Add a shadow cache policy before raising the shadow count:
  - Static casters can remain cached until scene/room/light policy changes.
  - Dynamic/skinned casters refresh on a small per-frame budget.
  - Moving lights invalidate only their own slots.
  - Empty captures retry slowly and do not thrash the cache.
- Add a caster taxonomy:
  - Opaque static geometry.
  - Alpha-tested static geometry.
  - Dynamic/skinned actors.
  - Transparent and effect geometry excluded by default.
- Add bias and filtering controls by shadow type:
  - Constant bias.
  - Slope/normal bias.
  - Receiver-plane or depth-clamp helpers where practical.
  - Small PCF first; EVSM/VSM only after depth-map correctness is proven.
- Add contact shadows as the first broad grounding fix:
  - Screen-space or depth-based short-range shadows for actors and props.
  - Works as a fallback for omni/fire lights where a projected local shadow is wrong.
  - Integrates with both PBR and regular GX/TEV receivers when enhanced lighting is enabled.
- Add directional shadows after contact shadows:
  - Cascaded shadow maps for sun/moon/key/environment light.
  - Stage/room tuning for cascade distance and bias.
  - Keep cascade split selection and filtering in Aurora/GPU code.
- Add true point-light shadows later:
  - Cubemap or cubemap-array shadows need six faces per light.
  - Restrict to explicitly authored important lights.
  - Prefer lower resolution, update throttling, and per-light budgets.
- Expand to multiple mapped lights only after the above diagnostics prove the one-light and atlas-slot paths:
  - Stable shadow source assignment.
  - Shadow cache with update throttling.
  - Per-light priority based on contribution, screen relevance, authored flags, and shadow type.

Acceptance criteria:

- Disabling enhanced shadows leaves original game shadows untouched; enabling enhanced shadows activates an enhanced
  shadow path and can render at least one Aurora shadow map on PBR receivers.
- Old real shadows can be suppressed without leaving characters completely ungrounded.
- Shadow source changes are stable, debounced, and visible in debug UI.
- The debug UI can explain why a receiver is shadowed, which slot it samples, and which caster geometry was rendered.
- `local_projected` lights do not masquerade as omni point shadows; point-light shadows remain an explicit future path.

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
      "shadowType": "local_projected",
      "colorScale": [1.0, 0.85, 0.7],
      "position": [-375.0, 2305.0, 17250.0],
      "isFire": true
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

- Done: create the first Aurora-owned shadow depth target, comparison sampler, selected-light request, light-space
  matrix, and status overlay.
- Done: render a depth-only caster replay pass for one selected high-priority light.
- Done: sample the first Aurora shadow map in PBR receivers with a small GPU-side PCF filter.
- Done: associate the first shadow map with the selected enhanced scene light so only that light's direct PBR contribution
  is shadowed when enhanced lights are active.
- Done: cache the first shadow map and refresh it only when the selected light/target moves enough or settings change.
  Zero-draw captures retry slowly so empty frames do not cause a constant recapture loop.
- Done: add a regular GX/TEV receiver bridge. Lit non-PBR materials preserve their TEV output, then sample the Aurora
  shadow map through a local light-weighted darkening pass and receive a restrained enhanced-light diffuse lift.
- Done: promote the one-light shadow handoff into an atlas-facing request list. Dusk ranks up to four shadow requests
  from per-light tuning, including shadow type and atlas slot metadata.
- Done: start the real Aurora shadow atlas. Aurora now allocates a four-layer depth texture array, creates per-layer
  render views, renders each queued non-point request into its own slot, and exposes captured-slot counts in the debug
  status. The current receiver shader still samples the active slot only.
- Done: add receiver-side atlas slot selection for enhanced lights. Aurora binds the shadow target as a depth texture
  array, uploads one shadow matrix per atlas slot, tags enhanced lights with their captured slot, and samples the matching
  layer in PBR and regular GX/TEV receiver paths. Legacy fallback lighting still uses the active slot.
- Done: add shadow-capture synchronization safety. Shadow caster replay now uses a separate shadow-safe bind group that
  keeps material textures available but binds fallback views for the PBR shadow receiver slots, avoiding WebGPU
  `TextureBinding|RenderAttachment` validation errors when a shadow map is refreshed and sampled in the same frame.
- Done: add first atlas diagnostics. The shadow status panel lists every active atlas slot, whether it has a request,
  whether it has been captured, whether it is pending refresh, and which source/type/score/priority produced it. A
  manual `Refresh Shadow Atlas` button requeues all active shadow slots.
- Done: add receiver-side projection edge fading. Local projected shadow-map slots now fade out near their projection
  bounds and near/far depth bounds so the atlas projection itself is less likely to appear as a hard rectangular shadow.
- Done: make the shadow caster pipeline alpha-aware. Depth-only shadow caster variants can now keep a fragment stage when
  GX alpha compare is active, so cutout materials can discard pixels instead of casting solid rectangular silhouettes.
- Done: stabilize shadow request assignment. Active shadow lights keep their relative atlas order while they remain in the
  submitted request set, so similarly scored nearby lights do not constantly swap shadow slots and snap projection
  direction.
- Done: expand atlas diagnostics. The shadow status panel now reports per-slot draw counts, and Aurora labels shadow
  capture render passes with explicit per-slot debug markers for RenderDoc inspection.
- Pause broad multi-shadow expansion until the correctness path is proven:
  - Add atlas/depth debug views for each slot.
  - Add light frustum/projection overlays.
  - Add receiver-coordinate and final-factor debug modes.
  - Add caster inclusion diagnostics for static, alpha-tested, and skinned actor geometry.
- Treat `local_projected` as projector/spot-style only. Add `contact_only`/`none` defaults for omni/fire lights that do
  not have an authored direction.
- Refine caster filtering and receiver bias/quality controls after the debug views can identify whether the bug is in
  capture, projection, comparison, or filtering.
- Add contact shadows before true point-light cubemap shadows so Link/actors have stable grounding even when local shadow
  maps are disabled or projector-only.
- Add directional CSM for environment/key light shadows before expensive point-light cubemap shadows.

Exit criteria:

- `aurora_shadow_maps` produces explainable PBR and GX/TEV receiver shadows without original real shadows.

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

1. Do a shadow correctness/debug pass before adding more shadow features:
   - Atlas/depth views per slot.
   - World-space frustum/projection overlay.
   - Receiver UV/depth/bias/factor debug modes.
   - Caster inclusion status for static, alpha-tested, and skinned actor draws.
2. Update light authoring semantics:
   - `local_projected` means projector/spot-style, not omni point.
   - Torch-like omni/fire lights default to `contact_only` or `none` unless explicitly authored.
   - Add `contact_only` support to the tuning/UI path.
3. Add a screen-space/depth contact-shadow path for actor and prop grounding on both PBR and enhanced GX/TEV receivers.
4. Refine caster filtering, receiver bias, shadow cache invalidation, and update budgets using the new diagnostics.
5. Add directional CSM for the environment/key light.
6. Resume Forward+/clustered GPU light selection once shadow correctness and contact grounding are stable.
7. Revisit true point-light cubemap shadows after the atlas, CSM, contact-shadow, and cache paths are behaving.

This sequence fixes why the current shadows look wrong before scaling the number of shadow-casting lights. It still keeps
Dusk CPU work shallow by pushing projection, filtering, contact shadows, and eventual clustered selection into Aurora/GPU
code.

## Shadow Research References

- Unreal Engine Virtual Shadow Maps: high-resolution virtualized shadow pages, directional clipmaps, page caching,
  invalidation visualization, and shadow-caster debug views:
  https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine
- Unity HDRP shadows: per-light shadow-map counts, separate atlases, update modes, cached shadows, mixed cached static
  and dynamic casters, bias controls, and contact shadows:
  https://docs.unity.cn/Packages/com.unity.render-pipelines.high-definition%4013.1/manual/Shadows-in-HDRP.html
- Unity URP shadows: one common punctual-light atlas, a separate directional atlas, spot lights using one shadow map, and
  point lights using six cubemap faces:
  https://docs.unity.cn/Packages/com.unity.render-pipelines.universal%4015.0/manual/Shadows-in-URP.html
- Godot 3D lights and shadows: PSSM directional shadows, positional shadow atlas allocation, spot versus omni shadow
  costs, and cache/update behavior:
  https://docs.godotengine.org/en/stable/tutorials/3d/lights_and_shadows.html
- Microsoft Cascaded Shadow Maps: frustum partitioning, cascade selection, PCF filtering, blend bands, and common
  artifact causes:
  https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps
