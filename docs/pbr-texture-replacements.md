# Experimental PBR Texture Replacements

This feature extends Dusk's existing texture replacement workflow with optional PBR sidecar maps. It is experimental and
is currently intended for development builds and material-authoring tests.

## Enabling

PBR material overrides are disabled by default.

1. In the RmlUi settings menu, enable `Enable Advanced Settings`.
2. Open the ImGui developer menu with `Shift+F1`.
3. Go to `Debug > Graphics Settings`.
4. Toggle `PBR Material Rendering`.

When the option is off, Aurora uses the normal GX/TEV rendering path and ignores PBR sidecar maps.

## Texture Folder

Texture replacements are loaded recursively from:

```text
%APPDATA%\TwilitRealm\Dusk\texture_replacements
```

PBR maps must sit next to the base replacement texture they belong to. They use the same stem and file extension as the
base texture with a suffix appended before `.dds`.

Example:

```text
texture_replacements\
  field\
    tex1_128x128_0123456789abcdef_6.dds
    tex1_128x128_0123456789abcdef_6_rmaos.dds
    tex1_128x128_0123456789abcdef_6_roughness.dds
    tex1_128x128_0123456789abcdef_6_metallic.dds
    tex1_128x128_0123456789abcdef_6_ao.dds
    tex1_128x128_0123456789abcdef_6_specular.dds
    tex1_128x128_0123456789abcdef_6_normal.dds
    tex1_128x128_0123456789abcdef_6_emissive.dds
```

PBR sidecar files are not treated as standalone texture replacements.

## Required And Optional Maps

The base replacement texture is still required and follows the existing Dolphin-style replacement filename format.

The PBR override activates when the base replacement has at least one matching PBR sidecar. Use `_rmaos` for packed
authoring, or use loose channel maps when you want to author individual channels separately.

| Map | Required | Accepted suffixes | Notes |
| --- | --- | --- | --- |
| Base color | Yes | none | Standard replacement texture. Its alpha is preserved in the final material output. |
| RMAOS | No | `_rmaos` | Packed roughness/metallic/ambient occlusion/specular map. |
| Roughness | No | `_roughness`, `_rough` | Loose map override for roughness. Samples the red channel. |
| Metallic | No | `_metallic`, `_metal` | Loose map override for metallic. Samples the red channel. |
| Ambient occlusion | No | `_ao` | Loose map override for AO. Samples the red channel. |
| Specular | No | `_specular`, `_spec` | Loose map override for specular strength. Samples the red channel. |
| Normal | No | `_normal`, `_n` | Tangent-space normal override. |
| Emissive | No | `_emissive`, `_e` | RGB is added after direct lighting. |

All PBR maps must be DDS files. Sidecar maps are loaded without separate `_mipN` sidecar chains. Use the same dimensions
as the base color texture unless you are deliberately testing lower-resolution material maps.

## RMAOS Channel Packing

The `_rmaos.dds` texture uses normalized channels:

| Channel | Meaning | Runtime range |
| --- | --- | --- |
| R | Roughness | Clamped to `0.04-1.0` |
| G | Metallic | Clamped to `0.0-1.0` |
| B | Ambient occlusion | Clamped to `0.0-1.0` |
| A | Specular strength | Clamped to `0.0-1.0` |

If a packed channel or loose map is missing, the shader uses these defaults:

```text
Roughness: 0.5
Metallic: 0.0
AO:        1.0
Specular:  0.5
```

## Rendering Behavior

Aurora chooses the first TEV stage that samples a texture with a loaded PBR sidecar and uses that texture coordinate set
and color channel for the material override. The generated shader samples:

- the base replacement as albedo
- the optional `_rmaos` map for packed roughness, metallic, AO, and specular strength
- optional loose roughness, metallic, AO, and specular maps, which override the corresponding packed/default values
- the optional normal map, when present
- the optional emissive map, when present

The override writes the material result into the TEV `prev` color before fog and alpha compare. This means fog and alpha
test behavior still run after the PBR override, but the detailed original TEV color-combiner result is replaced for that
material.

## Built-In Material Overrides

Some original GameCube materials do not sample a texture, so there is no texture filename that can receive a PBR sidecar.
The experimental PBR toggle also enables a small built-in material override path for these cases.

Currently, Dusk registers the visible blade material for:

- equipped Ordon Sword
- equipped Master Sword
- pedestal/world Master Sword

These blade overrides use the material's existing TEV result as albedo and then apply constant PBR values. Because that
TEV result has already been affected by GX lighting, the shader removes the current lighting tint before feeding the
blade color into the PBR pass. This keeps the built-in sword materials closer to texture sidecar behavior instead of
applying scene color twice. The Ordon Sword blade is treated as smooth metal. The Master Sword blade is treated as smooth
metal with a very slight blue emissive contribution.

The sword blade overrides can also use standalone named PBR files from the texture replacement folder, so they do not
need a base replacement texture. Use one of these material names:

- `ordon_sword_blade`
- `master_sword_blade`

The same suffixes from Required And Optional Maps are supported:

| Map | Ordon Sword example | Master Sword example |
| --- | --- | --- |
| RMAOS | `ordon_sword_blade_rmaos.dds` | `master_sword_blade_rmaos.dds` |
| Roughness | `ordon_sword_blade_roughness.dds`, `ordon_sword_blade_rough.dds` | `master_sword_blade_roughness.dds`, `master_sword_blade_rough.dds` |
| Metallic | `ordon_sword_blade_metallic.dds`, `ordon_sword_blade_metal.dds` | `master_sword_blade_metallic.dds`, `master_sword_blade_metal.dds` |
| Ambient occlusion | `ordon_sword_blade_ao.dds` | `master_sword_blade_ao.dds` |
| Specular | `ordon_sword_blade_specular.dds`, `ordon_sword_blade_spec.dds` | `master_sword_blade_specular.dds`, `master_sword_blade_spec.dds` |
| Normal | `ordon_sword_blade_normal.dds`, `ordon_sword_blade_n.dds` | `master_sword_blade_normal.dds`, `master_sword_blade_n.dds` |
| Emissive | `ordon_sword_blade_emissive.dds`, `ordon_sword_blade_e.dds` | `master_sword_blade_emissive.dds`, `master_sword_blade_e.dds` |

The files are found by the same recursive `texture_replacements` scan, and can be placed in any subfolder. The scan runs
at startup, so restart the game after adding or renaming one of these files. The ImGui developer menu has per-sword
toggles for RMAOS, loose maps, normal maps, and emissive maps. If a matching file is present and the matching toggle is
enabled, the map is sampled with the blade's first available texture coordinate. If the model has no usable texture
coordinate, the shader falls back to a generated view-space coordinate.

PBR lighting uses the selected TEV stage's GX color channel (`rast0` or `rast1`) when one is available. The shader
normalizes this color into a hue tint with a brightness floor, so dark environment lighting can color the material without
fully crushing the PBR result. When that channel has GX lighting enabled, direct PBR diffuse and specular terms are
accumulated from the channel's active GX lights, light colors, and attenuation settings. If the channel is unlit, the
shader falls back to a simple fixed direct-light approximation.

Ambient lighting uses a cheap hemispherical gradient. Up-facing normals receive the sky multiplier, down-facing normals
receive the ground multiplier, and side-facing normals receive the horizon multiplier. The gradient is tinted by the same
GX environment/raster lighting color used by the rest of the PBR path.

Indirect occlusion is applied after ambient/IBL evaluation and before direct lights are added. It is a cheap shader-local
GI approximation, not screen-space AO: material AO and downward-facing normal influence can darken indirect diffuse,
indirect specular, and the fill-light term while leaving direct diffuse/specular lighting untouched.

`Dynamic Local GI` adds an experimental first-bounce term driven by the same light list used by enhanced direct lighting.
It estimates bounced diffuse contribution per PBR fragment from local light color, attenuation, material albedo, and a
normal-wrap lobe. This is not ray-traced/path-traced GI and does not know about hidden geometry yet, but it is the first
dynamic indirect lighting path: moving or changing enhanced lights can now affect PBR indirect diffuse instead of only
direct diffuse/specular.

Enhanced direct lights are selected with a broader soft relevance radius and are temporally tracked by source, then
faded in and out before being submitted to Aurora. This keeps nearby lights eligible before the player crosses their
original influence radius and prevents Dynamic Local GI from snapping immediately to whichever source is currently
ranked closest.

The PC enhanced-light path also overrides the original nearest-light handoff used by `settingTevStruct_plightcol_plus`.
When enabled, original TEV ambient point-light color is an additive sum of every active nearby point/effect light instead
of the single light returned by `dKy_light_influence_id`. `mLightPosWorld` still has to be represented by one position
for the old real-shadow path, so it uses a weighted centroid of the same contributing lights rather than a nearest-light
winner.

When IBL is enabled, the shader samples a cube irradiance map for indirect diffuse, a prefiltered specular cube with
roughness-driven mip selection, and a split-sum BRDF LUT for indirect specular. Aurora creates neutral fallback IBL
textures at startup and tints them with the same GX environment/raster lighting color. Dusk now defaults to probe-based
IBL, with an ImGui toggle to use authored IBL assets instead. Runtime probes replay the current frame's perspective GX
draws from the active camera position into a double-buffered cubemap. To keep the cost under control, the runtime path
updates one 32x32 face every twelfth native simulation frame only while a refresh is pending, freezes the probe camera for
each six-face refresh cycle, and only replays native sim-frame draws so interpolation presentation frames stay smooth. By
default runtime probes behave as cached scene probes: a refresh is requested when no completed probe is available yet,
when the scene/room changes, or when `Refresh Runtime Probe` is pressed in the developer menu. After all six raw faces are
captured, a GPU filter pass convolves a 16x16 diffuse irradiance cube and a 32x32 roughness-prefiltered specular mip
chain from the captured cube. `Debug Periodic Probe Refresh` can be enabled in ImGui as an experimental stress/testing
option; the normal path does not chase camera movement. `Local Probe GI` keeps the last completed runtime probe active
during stage/room transitions while the new local probe captures, so PBR materials do not temporarily fall back to
neutral IBL. The status overlay marks this as a stale scene probe until the replacement capture finishes. `Runtime Probe
Cache` keeps four processed scene probes alive and reuses a matching stage/room probe immediately when returning to a
recently visited area, then refreshes it in the background. `Nearest Cached Probe` can temporarily use the closest cached
probe from the same stage when the exact room probe has not been captured yet, then replace it once the exact probe
finishes. `Spatial Probe Blend` can also mix the active probe with a nearby same-stage cached probe using camera-distance
weights. This is the first cheap multi-probe GI pass: it does not render new probes, but it softens indirect-light changes
between already captured rooms and near transition areas. `Blend Probe Transitions` crossfades the shader from the
previous active irradiance/prefilter cube to the newly selected runtime probe over a configurable frame count, avoiding a
hard indirect-light pop when a room probe finishes or a cached probe becomes active. Authored IBL assets can override the
runtime probe source globally, per stage, or per room when
`Use Authored IBL Assets` is enabled in the developer menu.

## Authored IBL Assets

Authored IBL assets are loaded from:

```text
texture_replacements\
  pbr_ibl\
    global\
    <stage_name>\
    <stage_name>\room_<room_no>\
```

Aurora loads fallback IBL first, then overlays `global`, then the current stage folder, then the current stage room folder.
That means a room can override only the files it needs while inheriting the rest from stage, global, or fallback assets.

Cube faces use these suffixes:

| Face | Direction |
| --- | --- |
| `px` | Positive X |
| `nx` | Negative X |
| `py` | Positive Y |
| `ny` | Negative Y |
| `pz` | Positive Z |
| `nz` | Negative Z |

Irradiance cube files:

```text
irradiance_px.dds
irradiance_nx.dds
irradiance_py.dds
irradiance_ny.dds
irradiance_pz.dds
irradiance_nz.dds
```

Prefiltered specular cube files support mip chains. Mip 0 can use either plain names or explicit mip names:

```text
prefilter_px.dds
prefilter_mip0_px.dds
prefilter_px_mip0.dds
```

Additional mips use either of these forms:

```text
prefilter_mip1_px.dds
prefilter_px_mip1.dds
```

All six faces for a mip must be present and must share dimensions and format. Each mip after mip 0 should be half the
previous mip size, clamped to `1x1`.

The BRDF LUT is optional:

```text
brdf_lut.dds
brdf.dds
```

DDS files are loaded as 2D face files. The current loader supports the same DDS formats used by texture replacements.
Full DDS cubemap containers are not parsed yet.

The ImGui developer menu exposes experimental controls for diffuse scale, specular scale, sky/ground/horizon ambient
strength, environment tint strength, indirect occlusion, IBL diffuse/specular strength, normal strength, normal
green-channel flipping, tangent handedness inversion, the built-in sword blade material values, and the per-sword map
toggles. These controls are stored under `backend.pbr.*` in the normal Dusk config file, and each group has a reset button
that restores its default authoring values. Indirect occlusion uses
`backend.pbr.indirectOcclusionStrength`, `backend.pbr.indirectOcclusionHorizon`, and
`backend.pbr.indirectOcclusionSpecular`. Dynamic local GI uses `backend.pbr.dynamicGi`,
`backend.pbr.dynamicGiStrength`, `backend.pbr.dynamicGiNormalWrap`, and `backend.pbr.dynamicGiAlbedoInfluence`; it
defaults off because it is an experimental local-bounce approximation. The IBL source toggle is stored as
`backend.pbr.useAuthoredIbl`; its default is
`false`, which requests the runtime probe-based IBL path. Debug periodic probe refresh is stored as
`backend.pbr.periodicProbeRefresh` and defaults to `false` so probes stay cached unless the scene changes or the
developer menu refresh button is used. Local probe GI is stored as `backend.pbr.localProbeGi` and defaults to `true`,
keeping the previous local probe active while a new stage/room probe is being captured. Runtime probe caching is stored as
`backend.pbr.probeCache` and defaults to `true`; it keeps up to four processed runtime probes for recently visited
stage/room keys. Nearest cached probe fallback is stored as `backend.pbr.nearestProbeCache` and defaults to `true`;
`backend.pbr.nearestProbeMaxDistance` defaults to `6000`. Spatial probe blending is stored as
`backend.pbr.spatialProbeBlend` and defaults to `true`; `backend.pbr.spatialProbeBlendMaxDistance` defaults to `8000`.
Probe transition blending is stored as
`backend.pbr.probeBlending` and defaults to `true`; `backend.pbr.probeBlendFrames` defaults to `45`.

The developer menu also has a PBR debug visualization dropdown stored as `backend.pbr.debugMode`. Modes include albedo,
roughness, metallic, AO, specular, normal, GX light tint, direct diffuse/specular, IBL diffuse/specular, indirect
occlusion, and dynamic GI. The debug view only changes draws that are using the experimental PBR path; non-PBR GX draws
continue to render normally.

The developer menu also exposes a `PBR IBL Overlay` under `Debug`. This overlay shows the currently requested and active
IBL source, runtime probe capture state, capture face, refresh/filter status, probe cache usage, active probe key, probe
nearest-cache status, spatial probe blend status, blend factor, replay draw eligibility, probe texture sizes, and which
authored IBL asset layers loaded for the current global/stage/room
selection. `Last cache hit` only becomes `yes` when entering a stage/room key that already has a fully captured processed
probe in one of the cache slots. The same status block is available inside the PBR
Material Override window under `IBL Runtime Status`.

## Enhanced Lighting And Shadows Plan

The original GameCube lighting path intentionally collapses some actor lighting decisions down to one nearby point light.
`dKy_light_influence_id()` searches `g_env_light.pointlight`, and `settingTevStruct_plightcol_plus()` uses the selected
light to update the actor `dKy_tevstr_c` light object, color influence, and `mLightPosWorld`. Real shadows then use
`dDlst_shadowReal_c::set()` and `setShadowRealMtx()` with that same `mLightPosWorld`, so the closest selected light also
drives the shadow direction. Aurora's PBR shader can already loop over multiple GX light slots, but it only receives the
lights the original J3D/GX path installed for the draw. This means PBR direct diffuse/specular inherits the one-light
selection behavior for materials that depend on the point-light influence path.

The enhanced path is opt-in from `Debug > Graphics Settings > PBR Enhanced Lighting` in the ImGui developer menu and
guarded by config values. The original renderer continues to run unchanged when the option is disabled.

Implemented settings:

- `backend.pbr.enhancedLights`: enable enhanced direct-light collection for PBR materials.
- `backend.pbr.enhancedLightCount`: maximum enhanced lights to send to Aurora, clamped to the supported sidecar buffer.
- `backend.pbr.enhancedLightFalloff`: select original GX-style attenuation or inverse-square attenuation.
- `backend.pbr.enhancedLightIntensity`: global intensity scale for enhanced direct lights.
- `backend.pbr.enhancedLightDebug`: expose selected light count and strongest-light data in the PBR debug window.
- `backend.pbr.enhancedShadows`: enable enhanced shadow behavior.
- `backend.pbr.enhancedShadowMode`: currently supports `original`, `override_direction`, `disable_game_shadows`,
  `hybrid`, and `aurora_shadow_maps`.
- `backend.pbr.enhancedShadowMapSize`: requested Aurora shadow-map depth texture size.
- `backend.pbr.enhancedShadowStrength`: receiver-side strength for Aurora shadow-map sampling.
- `backend.pbr.enhancedShadowBias`: receiver-side depth bias for Aurora shadow-map sampling.

Implemented direct-light slice:

- Dusk builds a compact sidecar light list from `g_env_light.pointlight` and `g_env_light.efplight` after the original
  environment lighting setup runs.
- The collector ranks enabled lights around the player position, falling back to the active camera eye when no player
  position is available. It transforms selected lights into view space and submits them to Aurora without modifying
  `dKy_tevstr_c`, J3D, or original GX lighting state.
- Aurora exposes a PBR enhanced-light API and uniform buffer. When `enhancedLights` is enabled and at least one sidecar
  light is available, PBR direct diffuse/specular uses that list instead of inheriting the original one-light
  point-influence collapse. The shader still uses original GX/TEV output for environment tint and material-color
  preservation.
- Legacy radius falloff and inverse-square falloff are both available. This is direct-light attenuation only; it is not
  full GI.

Implemented shadow-direction slice:

- When enhanced shadows are enabled and `enhancedShadowMode` is `override_direction`, Dusk keeps the original real-shadow
  renderer but asks the enhanced light collector for the dominant light around the shadow receiver position.
- The selected world-space light position replaces the temporary source passed into `dDlst_shadowReal_c::setShadowRealMtx()`.
  This changes shadow direction without changing the original shadow allocation, receiver polygon gathering, projection
  matrix build, or draw path.

Planned shadow modes:

- `disable_game_shadows`: implemented; suppresses original real shadows and simple/contact shadows while enhanced shadows
  are active.
- `hybrid`: implemented; suppresses original real shadows while keeping original simple/contact shadows.
- `aurora_shadow_maps`: in progress; suppresses original real shadows and routes PBR receivers through Aurora's
  shadow-map slot.

Implementation phases:

1. Done: add a Dusk-side enhanced light collector without changing original lighting state. The current collector covers
   global point lights and effect point lights. A later pass can add room light vectors, sun/moon lights, actor-local
   lights, and per-draw or per-material selection if the camera/player-centered list is not specific enough.
2. Done: add an Aurora enhanced-light buffer/API. Dusk submits selected lights in view space, and Aurora uses this buffer
   for PBR direct diffuse/specular when `enhancedLights` is enabled while still using original GX raster/TEV color as
   environment tint and material-color preservation input.
3. In progress: calibrate inverse-square direct lighting. The mode is exposed with intensity controls, but scene-by-scene
   tuning and better mapping from original radius/power values are still expected before using it as a default.
4. Done: add a shadow-direction override first. When `enhancedShadowMode` is `override_direction`, the original shadow
   renderer chooses the shadow source from the enhanced light ranking around the receiver instead of the single nearest
   point light.
5. Done: add game-shadow suppression modes. `disable_game_shadows` skips original real and simple shadow submission while
   enhanced shadows are active. `hybrid` skips original real shadows but keeps simple/contact shadows.
6. Done: add the Aurora shadow-map handoff. `aurora_shadow_maps` is exposed in the debug menu, persists map size/strength/
   bias settings, suppresses original real shadows, and gives Aurora a uniform matrix, C API, and status overlay. Depth
   texture creation, comparison sampler bindings, and receiver sampling are deferred until the caster pass exists.
7. Next: populate the Aurora shadow map. Start with one dominant directional/spot-style light and a depth-only caster pass,
   then expand to an atlas for multiple lights. Point lights should come later because cubemap shadow maps multiply render
   cost. The runtime probe replay work is a useful reference, but shadow maps still need caster filtering, depth-only
   pipeline variants, light view/projection matrix construction, and cache/update rules.
8. In progress: treat GI as an extension of the existing IBL/probe work. True GI is indirect bounce lighting;
   inverse-square falloff only fixes direct lights. Local-probe GI keeps stale scene probes active during transitions,
   the runtime probe cache keeps four processed stage/room probes available for quick reuse, nearest same-stage probe
   fallback gives uncaptured rooms a better temporary indirect source, probe transition blending avoids hard cubemap
   swaps, spatial probe blending gives the active probe a distance-weighted same-stage neighbor, dynamic local GI adds
   light-driven first-bounce diffuse, and indirect occlusion gives IBL/fill light a low-cost grounding pass. The practical
   path from here is authored probe volumes, then richer per-object or probe-grid weighting, and later screen-space or
   voxel/probe-grid experiments if the cost is acceptable.

Recommended order from here is authored probe volumes, then the Aurora depth-only caster pass, then multi-light shadow
atlases.
The completed direct-light, shadow-direction, suppression, shadow-map handoff, local-probe GI, and runtime probe cache
passes fix the immediate "only the closest light affects PBR/shadows/indirect response" problem before taking on the
larger shadow-map population and multi-probe GI work.

Current limitations:

- Only one PBR material sidecar set is bound per draw.
- Runtime probe filtering uses a fixed-sample GPU approximation, not an offline-quality prefilter.
- Indirect occlusion is material/normal driven only; full SSAO/GTAO or geometric occlusion is not implemented yet.
- Built-in untextured material overrides are limited to known material targets.
- Aurora shadow-map settings/status are wired, but depth resources, receiver sampling, and the caster pass are not active
  yet.
- Dynamic local GI is light-driven and shader-local. It provides dynamic bounce color, but it does not ray march,
  voxelize, or trace scene geometry yet.
- Enhanced shadow direction currently affects real shadows only; simple/contact shadows still use the original path unless
  `disable_game_shadows` is selected.
- Normal maps use vertex-provided GX NBT tangents/binormals when the draw supplies them and fall back to derivative-based
  tangent reconstruction otherwise.

PBR sidecars preserve more of the original GX material behavior by using the TEV/color-combiner output as the material
albedo, then applying the replacement roughness, metallic, AO, specular, normal, and emissive maps on top. This keeps
vertex colors, material colors, raster lighting tint, and multi-stage color-combiner effects from being discarded just
because a PBR sidecar exists.

## Implementation Notes

The shader code is generated at runtime by Aurora's GX shader generator. There are no standalone WGSL shader asset files
to edit for this feature. The relevant implementation is in:

- `extern/aurora/lib/gfx/texture_replacement.cpp`
- `extern/aurora/lib/gx/gx.cpp`
- `extern/aurora/lib/gx/pbr.cpp`
- `extern/aurora/lib/gx/pbr.hpp`
- `extern/aurora/lib/gx/shader.cpp`
- `extern/aurora/lib/gx/shader_info.cpp`
- `extern/aurora/lib/gx/pipeline.hpp`
- `src/dusk/pbr_material_override.cpp`
