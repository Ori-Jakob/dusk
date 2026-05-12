#include "dusk/pbr_settings.h"

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

void apply_ibl() {
    const auto& pbr = getSettings().backend.pbr;
    aurora_set_pbr_ibl_params(pbr.useIbl.getValue(), pbr.iblDiffuseStrength.getValue(),
                              pbr.iblSpecularStrength.getValue());
}

void apply_fill_direction() {
    const auto& pbr = getSettings().backend.pbr;
    aurora_set_pbr_fill_dir(pbr.fillDirX.getValue(), pbr.fillDirY.getValue(), pbr.fillDirZ.getValue());
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
    aurora_enable_pbr(getSettings().backend.enableExperimentalPbr.getValue());
    apply_lighting();
    apply_material_scales();
    apply_normal();
    apply_ambient_gradient();
    apply_ibl();
    apply_fill_direction();
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

void reset_ibl() {
    auto& pbr = getSettings().backend.pbr;
    reset(pbr.useIbl);
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

void reset_sword_material(pbr_material_override::SwordMaterialKind kind) {
    reset_sword_settings(sword_settings(kind));
    apply_sword_material(kind);
}

}  // namespace dusk::pbr_settings
