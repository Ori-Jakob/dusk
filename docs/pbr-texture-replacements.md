# Experimental PBR Texture Replacements

This feature extends Dusk's existing texture replacement workflow with optional PBR sidecar maps. It is experimental and
is currently intended for development builds and material-authoring tests.

## Enabling

PBR material overrides are disabled by default.

1. In the RmlUi settings menu, enable `Enable Advanced Settings`.
2. Open the ImGui developer menu with `Shift+F1`.
3. Go to `Debug > Graphics Settings`.
4. Toggle `Experimental PBR material override`.

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

When IBL is enabled, the shader samples a cube irradiance map for indirect diffuse, a prefiltered specular cube with
roughness-driven mip selection, and a split-sum BRDF LUT for indirect specular. Aurora creates neutral fallback IBL
textures at startup and tints them with the same GX environment/raster lighting color. Authored IBL assets can override
those fallback textures globally, per stage, or per room.

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
strength, environment tint strength, IBL diffuse/specular strength, normal strength, normal green-channel flipping,
tangent handedness inversion, the built-in sword blade material values, and the per-sword map toggles. These controls are
stored under `backend.pbr.*` in the normal Dusk config file, and each group has a reset button that restores its default
authoring values.

Current limitations:

- Only one PBR material sidecar set is bound per draw.
- Authored IBL uses static DDS face files. Runtime scene probe capture is not implemented yet; that requires re-rendering
  the scene six times from a probe camera and feeding the result through an irradiance/prefilter pipeline.
- Built-in untextured material overrides are limited to known material targets.
- Normal maps use derivative-based tangent reconstruction; vertex-provided GX NBT tangents/binormals are not supported
  yet.

## Future Implementations

- Runtime probe capture for scene-specific cubemaps.
- Better preservation of TEV/material color behavior for materials that rely on original color-combiner output.
- Debug visualization modes for albedo, roughness, metallic, AO, specular, normal, GX light tint, direct diffuse, and
  direct specular, IBL diffuse, and IBL specular.

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
