#include "dusk/pbr_settings.h"

#include "dusk/lighting/lighting_features.h"
#include "dusk/settings.h"

#include <aurora/gfx.h>

namespace dusk::pbr_settings {
namespace {

template <typename T>
void reset(config::ConfigVar<T>& setting) {
    setting.setValue(setting.getDefaultValue());
}

UserSettings::PbrSwordMaterialSettings& sword_settings(pbr_material_override::SwordMaterialKind kind) {
    auto& settings = getSettings().backend.pbr;
    switch (kind) {
    case pbr_material_override::SwordMaterialKind::Ordon:
        return settings.ordonSwordBlade;
    case pbr_material_override::SwordMaterialKind::Master:
        return settings.masterSwordBlade;
    }

    return settings.masterSwordBlade;
}

void reset_sword_settings(UserSettings::PbrSwordMaterialSettings& settings) {
    reset(settings.roughness);
    reset(settings.metallic);
    reset(settings.ambientOcclusion);
    reset(settings.specular);
    reset(settings.emissiveR);
    reset(settings.emissiveG);
    reset(settings.emissiveB);
    reset(settings.emissiveStrength);
    reset(settings.useRmaosMap);
    reset(settings.useLooseMaps);
    reset(settings.useNormalMap);
    reset(settings.useEmissiveMap);
}

}  // namespace

void apply_lighting() {
    const auto& pbr = getSettings().backend.pbr;
    aurora_set_pbr_light_params(pbr.ambient.getValue(), pbr.ambientSpecular.getValue(), pbr.fillIntensity.getValue());
}

void apply_material_scales() {
    const auto& pbr = getSettings().backend.pbr;
    aurora_set_pbr_material_params(pbr.diffuseScale.getValue(), pbr.specularScale.getValue());
}

void apply_normal() {
    const auto& pbr = getSettings().backend.pbr;
    aurora_set_pbr_normal_params(pbr.normalStrength.getValue(), pbr.flipNormalY.getValue(),
                                 pbr.invertNormalHandedness.getValue());
}

void apply_ambient_gradient() {
    const auto& pbr = getSettings().backend.pbr;
    aurora_set_pbr_ambient_gradient_params(pbr.skyAmbient.getValue(), pbr.groundAmbient.getValue(),
                                           pbr.horizonAmbient.getValue(), pbr.environmentTint.getValue());
}

void apply_indirect_occlusion() {
    const auto& pbr = getSettings().backend.pbr;
    aurora_set_pbr_indirect_occlusion_params(pbr.indirectOcclusionStrength.getValue(),
                                             pbr.indirectOcclusionHorizon.getValue(),
                                             pbr.indirectOcclusionSpecular.getValue());
}

void apply_dynamic_gi() {
    const auto& pbr = getSettings().backend.pbr;
    aurora_set_pbr_dynamic_gi_params(pbr.dynamicGi.getValue(), pbr.dynamicGiStrength.getValue(),
                                     pbr.dynamicGiNormalWrap.getValue(), pbr.dynamicGiAlbedoInfluence.getValue());
}

void apply_ibl() {
    const auto& pbr = getSettings().backend.pbr;
    aurora_set_pbr_ibl_source(pbr.useAuthoredIbl.getValue() ? AURORA_PBR_IBL_SOURCE_AUTHORED
                                                            : AURORA_PBR_IBL_SOURCE_PROBE);
    aurora_set_pbr_probe_auto_refresh(pbr.periodicProbeRefresh.getValue());
    aurora_set_pbr_local_probe_gi(pbr.localProbeGi.getValue());
    aurora_set_pbr_probe_cache(pbr.probeCache.getValue());
    aurora_set_pbr_probe_nearest_cache(pbr.nearestProbeCache.getValue(), pbr.nearestProbeMaxDistance.getValue());
    aurora_set_pbr_probe_spatial_blending(pbr.spatialProbeBlend.getValue(),
                                          pbr.spatialProbeBlendMaxDistance.getValue());
    aurora_set_pbr_probe_blending(pbr.probeBlending.getValue(), static_cast<uint32_t>(pbr.probeBlendFrames.getValue()));
    aurora_set_pbr_ibl_params(pbr.useIbl.getValue(), pbr.iblDiffuseStrength.getValue(),
                              pbr.iblSpecularStrength.getValue());
}

void apply_fill_direction() {
    const auto& pbr = getSettings().backend.pbr;
    aurora_set_pbr_fill_dir(pbr.fillDirX.getValue(), pbr.fillDirY.getValue(), pbr.fillDirZ.getValue());
}

void apply_debug() {
    const auto& pbr = getSettings().backend.pbr;
    aurora_set_pbr_debug_mode(static_cast<AuroraPbrDebugMode>(pbr.debugMode.getValue()));
}

void apply_enhanced_lighting() {
    const auto& pbr = getSettings().backend.pbr;
    const auto falloff = pbr.enhancedLightFalloff.getValue() == PbrEnhancedLightFalloff::InverseSquare
                             ? AURORA_PBR_ENHANCED_LIGHT_FALLOFF_INVERSE_SQUARE
                             : AURORA_PBR_ENHANCED_LIGHT_FALLOFF_LEGACY_RADIUS;
    aurora_set_pbr_enhanced_lighting(lighting::enhanced_direct_lights_enabled(), falloff,
                                     static_cast<uint32_t>(pbr.enhancedLightCount.getValue()),
                                     pbr.enhancedLightIntensity.getValue(), pbr.enhancedLightDebug.getValue());
}

void apply_enhanced_shadows() {
    const auto& pbr = getSettings().backend.pbr;
    const bool auroraShadowMaps = lighting::aurora_shadow_maps_enabled();
    aurora_set_pbr_shadow_map_params(auroraShadowMaps, static_cast<uint32_t>(pbr.enhancedShadowMapSize.getValue()),
                                     pbr.enhancedShadowStrength.getValue(), pbr.enhancedShadowBias.getValue());
}

void apply_sword_material(pbr_material_override::SwordMaterialKind kind) {
    auto& material = pbr_material_override::sword_blade_material(kind);
    const auto& settings = sword_settings(kind);
    material.roughness = settings.roughness.getValue();
    material.metallic = settings.metallic.getValue();
    material.ambientOcclusion = settings.ambientOcclusion.getValue();
    material.specular = settings.specular.getValue();
    material.emissiveColor[0] = settings.emissiveR.getValue();
    material.emissiveColor[1] = settings.emissiveG.getValue();
    material.emissiveColor[2] = settings.emissiveB.getValue();
    material.emissiveStrength = settings.emissiveStrength.getValue();
    material.useRmaosMap = settings.useRmaosMap.getValue();
    material.useLooseMaps = settings.useLooseMaps.getValue();
    material.useNormalMap = settings.useNormalMap.getValue();
    material.useEmissiveMap = settings.useEmissiveMap.getValue();
}

void save_sword_material(pbr_material_override::SwordMaterialKind kind) {
    const auto& material = pbr_material_override::sword_blade_material(kind);
    auto& settings = sword_settings(kind);
    settings.roughness.setValue(material.roughness);
    settings.metallic.setValue(material.metallic);
    settings.ambientOcclusion.setValue(material.ambientOcclusion);
    settings.specular.setValue(material.specular);
    settings.emissiveR.setValue(material.emissiveColor[0]);
    settings.emissiveG.setValue(material.emissiveColor[1]);
    settings.emissiveB.setValue(material.emissiveColor[2]);
    settings.emissiveStrength.setValue(material.emissiveStrength);
    settings.useRmaosMap.setValue(material.useRmaosMap);
    settings.useLooseMaps.setValue(material.useLooseMaps);
    settings.useNormalMap.setValue(material.useNormalMap);
    settings.useEmissiveMap.setValue(material.useEmissiveMap);
}

void apply() {
    aurora_enable_pbr(lighting::pbr_material_rendering_enabled());
    apply_lighting();
    apply_material_scales();
    apply_normal();
    apply_ambient_gradient();
    apply_indirect_occlusion();
    apply_dynamic_gi();
    apply_ibl();
    apply_fill_direction();
    apply_debug();
    apply_enhanced_lighting();
    apply_enhanced_shadows();
    apply_sword_material(pbr_material_override::SwordMaterialKind::Ordon);
    apply_sword_material(pbr_material_override::SwordMaterialKind::Master);
}

void reset_lighting() {
    auto& pbr = getSettings().backend.pbr;
    reset(pbr.ambient);
    reset(pbr.ambientSpecular);
    reset(pbr.fillIntensity);
    apply_lighting();
}

void reset_material_scales() {
    auto& pbr = getSettings().backend.pbr;
    reset(pbr.diffuseScale);
    reset(pbr.specularScale);
    apply_material_scales();
}

void reset_normal() {
    auto& pbr = getSettings().backend.pbr;
    reset(pbr.normalStrength);
    reset(pbr.flipNormalY);
    reset(pbr.invertNormalHandedness);
    apply_normal();
}

void reset_ambient_gradient() {
    auto& pbr = getSettings().backend.pbr;
    reset(pbr.skyAmbient);
    reset(pbr.groundAmbient);
    reset(pbr.horizonAmbient);
    reset(pbr.environmentTint);
    apply_ambient_gradient();
}

void reset_indirect_occlusion() {
    auto& pbr = getSettings().backend.pbr;
    reset(pbr.indirectOcclusionStrength);
    reset(pbr.indirectOcclusionHorizon);
    reset(pbr.indirectOcclusionSpecular);
    apply_indirect_occlusion();
}

void reset_dynamic_gi() {
    auto& pbr = getSettings().backend.pbr;
    reset(pbr.dynamicGi);
    reset(pbr.dynamicGiStrength);
    reset(pbr.dynamicGiNormalWrap);
    reset(pbr.dynamicGiAlbedoInfluence);
    apply_dynamic_gi();
}

void reset_ibl() {
    auto& pbr = getSettings().backend.pbr;
    reset(pbr.useIbl);
    reset(pbr.useAuthoredIbl);
    reset(pbr.periodicProbeRefresh);
    reset(pbr.localProbeGi);
    reset(pbr.probeCache);
    reset(pbr.nearestProbeCache);
    reset(pbr.nearestProbeMaxDistance);
    reset(pbr.spatialProbeBlend);
    reset(pbr.spatialProbeBlendMaxDistance);
    reset(pbr.probeBlending);
    reset(pbr.probeBlendFrames);
    reset(pbr.iblDiffuseStrength);
    reset(pbr.iblSpecularStrength);
    apply_ibl();
}

void reset_fill_direction() {
    auto& pbr = getSettings().backend.pbr;
    reset(pbr.fillDirX);
    reset(pbr.fillDirY);
    reset(pbr.fillDirZ);
    apply_fill_direction();
}

void reset_debug() {
    auto& pbr = getSettings().backend.pbr;
    reset(pbr.debugMode);
    apply_debug();
}

void reset_enhanced_lighting() {
    auto& pbr = getSettings().backend.pbr;
    reset(pbr.enhancedLights);
    reset(pbr.enhancedLightCount);
    reset(pbr.enhancedLightFalloff);
    reset(pbr.enhancedLightIntensity);
    reset(pbr.enhancedLightDebug);
    apply_enhanced_lighting();
}

void reset_enhanced_shadows() {
    auto& pbr = getSettings().backend.pbr;
    reset(pbr.enhancedShadows);
    reset(pbr.enhancedShadowMode);
    reset(pbr.enhancedShadowMapSize);
    reset(pbr.enhancedShadowStrength);
    reset(pbr.enhancedShadowBias);
    apply_enhanced_shadows();
}

void reset_sword_material(pbr_material_override::SwordMaterialKind kind) {
    reset_sword_settings(sword_settings(kind));
    apply_sword_material(kind);
}

}  // namespace dusk::pbr_settings
