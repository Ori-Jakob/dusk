#pragma once

#include "dusk/pbr_material_override.h"

namespace dusk::pbr_settings {

void apply();
void apply_lighting();
void apply_material_scales();
void apply_normal();
void apply_ambient_gradient();
void apply_ibl();
void apply_fill_direction();

void reset_lighting();
void reset_material_scales();
void reset_normal();
void reset_ambient_gradient();
void reset_ibl();
void reset_fill_direction();

void apply_sword_material(pbr_material_override::SwordMaterialKind kind);
void save_sword_material(pbr_material_override::SwordMaterialKind kind);
void reset_sword_material(pbr_material_override::SwordMaterialKind kind);

}  // namespace dusk::pbr_settings
