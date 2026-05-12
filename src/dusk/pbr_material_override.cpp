#include "dusk/pbr_material_override.h"

#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include <aurora/gfx.h>

#include <algorithm>
#include <vector>

namespace dusk::pbr_material_override {
namespace {

struct SwordModelOverride {
    const J3DModel* model;
    SwordMaterialKind kind;
};

std::vector<SwordModelOverride> sSwordModelOverrides;

constexpr SwordBladeMaterial kDefaultOrdonBladeMaterial = {
    0.18f, // roughness
    1.0f,  // metallic
    1.0f,  // ambient occlusion
    0.95f, // specular
    {0.0f, 0.0f, 0.0f},
    0.0f,
    true,
    true,
    true,
    true,
};

constexpr SwordBladeMaterial kDefaultMasterBladeMaterial = {
    0.15f, // roughness
    1.0f,  // metallic
    1.0f,  // ambient occlusion
    1.0f,  // specular
    {0.35f, 0.55f, 1.0f},
    0.035f,
    true,
    true,
    true,
    true,
};

SwordBladeMaterial sOrdonBladeMaterial = kDefaultOrdonBladeMaterial;
SwordBladeMaterial sMasterBladeMaterial = kDefaultMasterBladeMaterial;

auto find_sword_model(const J3DModel* model) {
    return std::find_if(sSwordModelOverrides.begin(), sSwordModelOverrides.end(),
                        [model](const SwordModelOverride& entry) { return entry.model == model; });
}

SwordBladeMaterial& mutable_blade_material(SwordMaterialKind kind) {
    switch (kind) {
    case SwordMaterialKind::Ordon:
        return sOrdonBladeMaterial;
    case SwordMaterialKind::Master:
        return sMasterBladeMaterial;
    }

    return sMasterBladeMaterial;
}

const SwordBladeMaterial& default_blade_material(SwordMaterialKind kind) {
    switch (kind) {
    case SwordMaterialKind::Ordon:
        return kDefaultOrdonBladeMaterial;
    case SwordMaterialKind::Master:
        return kDefaultMasterBladeMaterial;
    }

    return kDefaultMasterBladeMaterial;
}

const char* blade_material_name(SwordMaterialKind kind) {
    switch (kind) {
    case SwordMaterialKind::Ordon:
        return "ordon_sword_blade";
    case SwordMaterialKind::Master:
        return "master_sword_blade";
    }

    return "";
}

bool is_sword_blade_material(const J3DMaterial* material) {
    if (material == nullptr) {
        return false;
    }

    // AL_SWA / AL_SWM and O_AL_SWM use material 0 for the visible sword blade draw.
    // This catches untextured blade geometry while leaving sheaths and unrelated held items alone.
    return material->getIndex() == 0;
}

void set_blade_override(SwordMaterialKind kind, const SwordBladeMaterial& material) {
    aurora_set_pbr_constant_material_override(
        material.roughness,
        material.metallic,
        material.ambientOcclusion,
        material.specular,
        material.emissiveColor[0],
        material.emissiveColor[1],
        material.emissiveColor[2],
        material.emissiveStrength);
    aurora_set_pbr_constant_material_override_maps(
        blade_material_name(kind),
        material.useRmaosMap,
        material.useLooseMaps,
        material.useNormalMap,
        material.useEmissiveMap);
}

}  // namespace

void register_sword_model(const J3DModel* model, SwordMaterialKind kind) {
    if (model == nullptr) {
        return;
    }

    const auto it = find_sword_model(model);
    if (it != sSwordModelOverrides.end()) {
        it->kind = kind;
        return;
    }

    sSwordModelOverrides.push_back({model, kind});
}

void unregister_model(const J3DModel* model) {
    if (model == nullptr) {
        return;
    }

    const auto it = find_sword_model(model);
    if (it != sSwordModelOverrides.end()) {
        sSwordModelOverrides.erase(it);
    }
}

SwordBladeMaterial& sword_blade_material(SwordMaterialKind kind) {
    return mutable_blade_material(kind);
}

const SwordBladeMaterial& default_sword_blade_material(SwordMaterialKind kind) {
    return default_blade_material(kind);
}

void reset_sword_blade_material(SwordMaterialKind kind) {
    mutable_blade_material(kind) = default_blade_material(kind);
}

bool begin_j3d_material_draw(const J3DModel* model, const J3DMaterial* material) {
    if (!aurora_pbr_enabled() || !is_sword_blade_material(material)) {
        return false;
    }

    const auto it = find_sword_model(model);
    if (it == sSwordModelOverrides.end()) {
        return false;
    }

    switch (it->kind) {
    case SwordMaterialKind::Ordon:
        set_blade_override(it->kind, sOrdonBladeMaterial);
        break;
    case SwordMaterialKind::Master:
        set_blade_override(it->kind, sMasterBladeMaterial);
        break;
    }

    return true;
}

void end_j3d_material_draw(bool override_was_active) {
    if (override_was_active) {
        aurora_clear_pbr_constant_material_override();
    }
}

}  // namespace dusk::pbr_material_override
