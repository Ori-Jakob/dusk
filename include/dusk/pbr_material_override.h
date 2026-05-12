#pragma once

#include <cstdint>

class J3DMaterial;
class J3DModel;

namespace dusk::pbr_material_override {

enum class SwordMaterialKind : uint8_t {
    Ordon,
    Master,
};

struct SwordBladeMaterial {
    float roughness;
    float metallic;
    float ambientOcclusion;
    float specular;
    float emissiveColor[3];
    float emissiveStrength;
    bool useRmaosMap;
    bool useLooseMaps;
    bool useNormalMap;
    bool useEmissiveMap;
};

void register_sword_model(const J3DModel* model, SwordMaterialKind kind);
void unregister_model(const J3DModel* model);

SwordBladeMaterial& sword_blade_material(SwordMaterialKind kind);
const SwordBladeMaterial& default_sword_blade_material(SwordMaterialKind kind);
void reset_sword_blade_material(SwordMaterialKind kind);

bool begin_j3d_material_draw(const J3DModel* model, const J3DMaterial* material);
void end_j3d_material_draw(bool override_was_active);

}  // namespace dusk::pbr_material_override
