#include "dusk/terrain_texture_sampling.h"

#if TARGET_PC

#include "dusk/config.hpp"
#include "dusk/data.hpp"
#include "dusk/io.hpp"
#include "dusk/main.h"
#include "dusk/settings.h"

#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include "JSystem/J3DGraphBase/J3DTexture.h"
#include "JSystem/JUtility/JUTNameTab.h"

#include <dolphin/gx/GXEnum.h>
#include <dolphin/gx/GXTextureSampling.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dusk {
namespace {
constexpr const char* kIncludeListFileName = "terrain_texture_variation.json";

std::array<J3DModelData*, 64> s_terrainTextureSamplingModels{};
struct TextureSamplingRule {
    bool included = false;
    bool hasParamOverride = false;
    TerrainTextureSamplingParams params;
};
std::unordered_map<std::string, TextureSamplingRule> s_textureRules;
std::unordered_set<std::string> s_includedMaterials;
bool s_includesLoaded = false;

bool ascii_iequals(char lhs, char rhs) {
    return std::tolower(static_cast<unsigned char>(lhs)) ==
           std::tolower(static_cast<unsigned char>(rhs));
}

bool contains_token(const char* text, std::string_view token) {
    if (text == nullptr || token.empty()) {
        return false;
    }

    for (const char* it = text; *it != '\0'; ++it) {
        size_t i = 0;
        while (i < token.size() && it[i] != '\0' && ascii_iequals(it[i], token[i])) {
            ++i;
        }
        if (i == token.size()) {
            return true;
        }
    }
    return false;
}

bool contains_any_token(const char* text, std::initializer_list<std::string_view> tokens) {
    for (std::string_view token : tokens) {
        if (contains_token(text, token)) {
            return true;
        }
    }
    return false;
}

bool is_repeating_texture(const ResTIMG& timg) {
    return timg.wrapS != GX_CLAMP && timg.wrapT != GX_CLAMP;
}

const char* safe_name(JUTNameTab* names, u16 index) {
    if (names == nullptr) {
        return "";
    }

    const char* name = names->getName(index);
    return name != nullptr ? name : "";
}

bool is_valid_key(const char* name) {
    return name != nullptr && name[0] != '\0';
}

TerrainTextureSamplingParams global_sampling_params() {
    return {
        .cellScale = getSettings().game.stochasticTerrainCellScale.getValue(),
        .jitter = getSettings().game.stochasticTerrainJitter.getValue(),
        .blendWidth = getSettings().game.stochasticTerrainBlendWidth.getValue(),
    };
}

bool name_suggests_grass(const char* name) {
    return contains_any_token(name, {
        "grass",
        "kusa",
        "shiba",
        "siba",
        "sibafu",
        "weed",
        "turf",
        "lawn",
        "clover",
    });
}

bool name_suggests_grass_terrain(const char* name) {
    return name_suggests_grass(name) || contains_any_token(name, {
        "meadow",
    });
}

bool name_suggests_rock_cliff(const char* name) {
    return contains_any_token(name, {
        "cliff",
        "gake",
        "rock",
        "stone",
        "iwa",
        "kabe",
        "wall",
    });
}

bool name_suggests_decorative_vegetation(const char* name) {
    return contains_any_token(name, {
        "flower",
        "flwr",
        "hana",
        "petal",
        "ivy",
        "vine",
        "tsuta",
        "creep",
        "moss",
        "koke",
        "leaf",
        "leaves",
        "foliage",
        "plant",
        "shrub",
        "bush",
        "happa",
    });
}

bool name_suggests_path_road(const char* name) {
    return contains_any_token(name, {
        "path",
        "road",
        "michi",
        "doro",
        "dirt",
        "trail",
        "mud",
        "soil",
        "sand",
        "suna",
        "gravel",
        "jari",
        "track",
    });
}

bool name_suggests_edge_blend(const char* name) {
    return contains_any_token(name, {
        "edge",
        "side",
        "border",
        "blend",
        "grad",
        "bank",
        "shoulder",
    });
}

bool name_suggests_grassy_path_edge(const char* name) {
    return name_suggests_path_road(name) && (name_suggests_grass(name) || name_suggests_edge_blend(name));
}

u32 texture_slot_count(J3DMaterial* material) {
    J3DTevBlock* tevBlock = material->getTevBlock();
    if (tevBlock == nullptr) {
        return 0;
    }

    switch (tevBlock->getType()) {
    case 'TVB1':
        return 1;
    case 'TVB2':
        return 2;
    case 'TVB4':
        return 4;
    case 'TV16':
    case 'TVPT':
        return 8;
    default:
        return 0;
    }
}

bool material_uses_texture(J3DMaterial* material, u16 textureIndex) {
    const u32 slotCount = texture_slot_count(material);
    for (u32 slot = 0; slot < slotCount; ++slot) {
        if (material->getTexNo(slot) == textureIndex) {
            return true;
        }
    }
    return false;
}

using NamePredicate = bool (*)(const char*);

bool texture_used_by_matching_material(J3DModelData* modelData, u16 textureIndex,
                                       NamePredicate predicate) {
    JUTNameTab* materialNames = modelData->getMaterialName();
    if (materialNames == nullptr) {
        return false;
    }

    for (u16 materialIndex = 0; materialIndex < modelData->getMaterialNum(); ++materialIndex) {
        J3DMaterial* material = modelData->getMaterialNodePointer(materialIndex);
        if (material == nullptr || !material_uses_texture(material, textureIndex)) {
            continue;
        }

        if (predicate(safe_name(materialNames, materialIndex))) {
            return true;
        }
    }
    return false;
}

bool texture_or_material_suggests_path_road(J3DModelData* modelData, u16 textureIndex,
                                            const char* textureName) {
    if (name_suggests_path_road(textureName)) {
        return true;
    }

    return texture_used_by_matching_material(modelData, textureIndex, name_suggests_path_road);
}

bool texture_or_material_suggests_grassy_path_edge(J3DModelData* modelData, u16 textureIndex,
                                                   const char* textureName) {
    if (name_suggests_grassy_path_edge(textureName)) {
        return true;
    }

    JUTNameTab* materialNames = modelData->getMaterialName();
    if (materialNames == nullptr) {
        return false;
    }

    for (u16 materialIndex = 0; materialIndex < modelData->getMaterialNum(); ++materialIndex) {
        J3DMaterial* material = modelData->getMaterialNodePointer(materialIndex);
        if (material == nullptr || !material_uses_texture(material, textureIndex)) {
            continue;
        }

        const char* materialName = safe_name(materialNames, materialIndex);
        const bool hasPath = name_suggests_path_road(textureName) ||
                             name_suggests_path_road(materialName);
        const bool hasGrassEdge = name_suggests_grass(textureName) ||
                                  name_suggests_grass(materialName) ||
                                  name_suggests_edge_blend(textureName) ||
                                  name_suggests_edge_blend(materialName);
        if (hasPath && hasGrassEdge) {
            return true;
        }
    }
    return false;
}

bool heuristic_candidate(J3DModelData* modelData, J3DTexture* texture, JUTNameTab* textureNames,
                         u16 textureIndex) {
    ResTIMG* timg = texture->getResTIMG(textureIndex);
    if (timg == nullptr || timg->width == 0 || timg->height == 0 || !is_repeating_texture(*timg)) {
        return false;
    }

    const char* textureName = safe_name(textureNames, textureIndex);
    const bool grassTerrain = name_suggests_grass(textureName) ||
                              texture_used_by_matching_material(modelData, textureIndex,
                                                                name_suggests_grass_terrain);
    const bool grassyPathEdge = texture_or_material_suggests_grassy_path_edge(modelData, textureIndex,
                                                                              textureName);
    const bool pathRoadTerrain = texture_or_material_suggests_path_road(modelData, textureIndex,
                                                                        textureName);
    const bool decorativeVegetation = name_suggests_decorative_vegetation(textureName) ||
                                      texture_used_by_matching_material(modelData, textureIndex,
                                                                        name_suggests_decorative_vegetation);
    const bool rockCliff = name_suggests_rock_cliff(textureName) ||
                           texture_used_by_matching_material(modelData, textureIndex,
                                                             name_suggests_rock_cliff);

    if (decorativeVegetation && !grassyPathEdge) {
        return false;
    }
    if (rockCliff && !grassyPathEdge && !pathRoadTerrain) {
        return false;
    }
    return grassTerrain || grassyPathEdge || pathRoadTerrain;
}

void ensure_includes_loaded() {
    if (!s_includesLoaded) {
        loadTerrainTextureSamplingIncludes();
    }
}

bool texture_used_by_included_material(J3DModelData* modelData, u16 textureIndex) {
    ensure_includes_loaded();

    JUTNameTab* materialNames = modelData->getMaterialName();
    if (materialNames == nullptr) {
        return false;
    }

    for (u16 materialIndex = 0; materialIndex < modelData->getMaterialNum(); ++materialIndex) {
        J3DMaterial* material = modelData->getMaterialNodePointer(materialIndex);
        const char* materialName = safe_name(materialNames, materialIndex);
        if (material == nullptr || !is_valid_key(materialName) ||
            !material_uses_texture(material, textureIndex))
        {
            continue;
        }

        if (s_includedMaterials.contains(materialName)) {
            return true;
        }
    }
    return false;
}

TextureSamplingRule* find_texture_rule(const char* textureName) {
    if (!is_valid_key(textureName)) {
        return nullptr;
    }

    auto it = s_textureRules.find(textureName);
    return it != s_textureRules.end() ? &it->second : nullptr;
}

const TextureSamplingRule* find_texture_rule_const(const char* textureName) {
    if (!is_valid_key(textureName)) {
        return nullptr;
    }

    auto it = s_textureRules.find(textureName);
    return it != s_textureRules.end() ? &it->second : nullptr;
}

bool should_stochastically_sample(J3DModelData* modelData, J3DTexture* texture, JUTNameTab* textureNames,
                                  u16 textureIndex) {
    ensure_includes_loaded();

    ResTIMG* timg = texture->getResTIMG(textureIndex);
    if (timg == nullptr || timg->width == 0 || timg->height == 0 || !is_repeating_texture(*timg)) {
        return false;
    }

    const char* textureName = safe_name(textureNames, textureIndex);
    const TextureSamplingRule* rule = find_texture_rule_const(textureName);
    return (rule != nullptr && rule->included) ||
           texture_used_by_included_material(modelData, textureIndex);
}

template <typename Callback>
void read_name_array(const nlohmann::json& source, const char* key, Callback callback) {
    if (!source.contains(key) || !source[key].is_array()) {
        return;
    }

    for (const auto& entry : source[key]) {
        if (entry.is_string()) {
            const std::string name = entry.get<std::string>();
            if (!name.empty()) {
                callback(name);
            }
            continue;
        }

        if (!entry.is_object() || !entry.value("include", true)) {
            continue;
        }

        const char* nameKey = std::string_view{key} == "textures" ? "texture" : "material";
        if (entry.contains(nameKey) && entry[nameKey].is_string()) {
            const std::string name = entry[nameKey].get<std::string>();
            if (!name.empty()) {
                callback(name);
            }
        }
    }
}

void read_texture_array(const nlohmann::json& source) {
    if (!source.contains("textures") || !source["textures"].is_array()) {
        return;
    }

    for (const auto& entry : source["textures"]) {
        if (entry.is_string()) {
            const std::string name = entry.get<std::string>();
            if (!name.empty()) {
                s_textureRules[name].included = true;
            }
            continue;
        }

        if (!entry.is_object() || !entry.contains("texture") || !entry["texture"].is_string()) {
            continue;
        }

        const std::string name = entry["texture"].get<std::string>();
        if (name.empty()) {
            continue;
        }

        TextureSamplingRule rule;
        rule.included = entry.value("include", true);
        rule.hasParamOverride = entry.value("overrideParams", false);
        if (rule.hasParamOverride) {
            rule.params.cellScale = entry.value("cellScale", rule.params.cellScale);
            rule.params.jitter = entry.value("jitter", rule.params.jitter);
            rule.params.blendWidth = entry.value("blendWidth", rule.params.blendWidth);
        }

        if (rule.included || rule.hasParamOverride) {
            s_textureRules[name] = rule;
        }
    }
}
} // namespace

void applyTerrainTextureSamplingSettings() {
    AuroraSetStochasticSamplingEnabled(
        getSettings().game.enableStochasticTerrainTextures.getValue() ? GX_TRUE : GX_FALSE);
    AuroraSetStochasticSamplingParams(
        getSettings().game.stochasticTerrainCellScale,
        getSettings().game.stochasticTerrainJitter,
        getSettings().game.stochasticTerrainBlendWidth);
    reapplyTerrainTextureSampling();
}

void applyTerrainTextureSampling(J3DModelData* modelData) {
    if (modelData == nullptr) {
        return;
    }

    J3DTexture* texture = modelData->getTexture();
    if (texture == nullptr) {
        return;
    }

    JUTNameTab* textureNames = modelData->getTextureName();
    for (u16 i = 0; i < texture->getNum(); ++i) {
        TGXTexObj* texObj = texture->getTexObj(i);
        const char* textureName = safe_name(textureNames, i);
        const TextureSamplingRule* rule = find_texture_rule_const(textureName);
        AuroraSetTexObjStochasticSampling(
            texObj,
            should_stochastically_sample(modelData, texture, textureNames, i) ? GX_TRUE : GX_FALSE);
        if (rule != nullptr && rule->hasParamOverride) {
            AuroraSetTexObjStochasticSamplingParams(
                texObj, rule->params.cellScale, rule->params.jitter, rule->params.blendWidth);
        } else {
            AuroraClearTexObjStochasticSamplingParams(texObj);
        }
    }
}

void registerTerrainTextureSamplingModel(J3DModelData* modelData) {
    if (modelData == nullptr) {
        return;
    }

    ensure_includes_loaded();

    J3DModelData** freeSlot = nullptr;
    for (J3DModelData*& registeredModelData : s_terrainTextureSamplingModels) {
        if (registeredModelData == modelData) {
            return;
        }
        if (registeredModelData == nullptr && freeSlot == nullptr) {
            freeSlot = &registeredModelData;
        }
    }

    if (freeSlot != nullptr) {
        *freeSlot = modelData;
    }
}

void unregisterTerrainTextureSamplingModel(J3DModelData* modelData) {
    if (modelData == nullptr) {
        return;
    }

    for (J3DModelData*& registeredModelData : s_terrainTextureSamplingModels) {
        if (registeredModelData == modelData) {
            registeredModelData = nullptr;
        }
    }
}

void reapplyTerrainTextureSampling() {
    for (J3DModelData* modelData : s_terrainTextureSamplingModels) {
        applyTerrainTextureSampling(modelData);
    }
}

void visitTerrainTextureSamplingUses(TerrainTextureSamplingUseVisitor visitor, void* userData) {
    if (visitor == nullptr) {
        return;
    }

    ensure_includes_loaded();

    for (J3DModelData* modelData : s_terrainTextureSamplingModels) {
        if (modelData == nullptr) {
            continue;
        }

        J3DTexture* texture = modelData->getTexture();
        if (texture == nullptr) {
            continue;
        }

        JUTNameTab* materialNames = modelData->getMaterialName();
        JUTNameTab* textureNames = modelData->getTextureName();
        const u16 textureCount = texture->getNum();
        for (u16 materialIndex = 0; materialIndex < modelData->getMaterialNum(); ++materialIndex) {
            J3DMaterial* material = modelData->getMaterialNodePointer(materialIndex);
            if (material == nullptr) {
                continue;
            }

            const u32 slotCount = texture_slot_count(material);
            for (u16 slot = 0; slot < slotCount; ++slot) {
                const u16 textureIndex = material->getTexNo(slot);
                if (textureIndex == 0xffff || textureIndex >= textureCount) {
                    continue;
                }

                ResTIMG* timg = texture->getResTIMG(textureIndex);
                const char* materialName = safe_name(materialNames, materialIndex);
                const char* textureName = safe_name(textureNames, textureIndex);

                TerrainTextureSamplingUse use;
                use.modelData = modelData;
                use.texture = texture;
                use.materialIndex = materialIndex;
                use.textureIndex = textureIndex;
                use.textureSlot = slot;
                use.materialName = materialName;
                use.textureName = textureName;
                if (timg != nullptr) {
                    use.width = timg->width;
                    use.height = timg->height;
                    use.format = timg->format;
                    use.wrapS = timg->wrapS;
                    use.wrapT = timg->wrapT;
                    use.repeating = is_repeating_texture(*timg);
                }
                use.candidate = heuristic_candidate(modelData, texture, textureNames, textureIndex);
                if (const TextureSamplingRule* rule = find_texture_rule_const(textureName)) {
                    use.textureIncluded = rule->included;
                    use.textureParamOverride = rule->hasParamOverride;
                    use.textureParams = rule->hasParamOverride ? rule->params : global_sampling_params();
                } else {
                    use.textureParams = global_sampling_params();
                }
                use.materialIncluded = is_valid_key(materialName) && s_includedMaterials.contains(materialName);
                use.applied = AuroraGetTexObjStochasticSampling(texture->getTexObj(textureIndex)) == GX_TRUE;
                visitor(use, userData);
            }
        }
    }
}

std::filesystem::path terrainTextureSamplingIncludePath() {
    const std::filesystem::path basePath =
        ConfigPath.empty() ? data::configured_data_path() : ConfigPath;
    return basePath / kIncludeListFileName;
}

bool loadTerrainTextureSamplingIncludes() {
    s_textureRules.clear();
    s_includedMaterials.clear();
    s_includesLoaded = true;

    const std::filesystem::path path = terrainTextureSamplingIncludePath();
    if (!std::filesystem::exists(path)) {
        return true;
    }

    try {
        const auto data = io::FileStream::ReadAllBytes(path);
        const auto json = nlohmann::json::parse(data);
        if (!json.is_object()) {
            return false;
        }

        read_texture_array(json);
        read_name_array(json, "materials", [](const std::string& name) {
            s_includedMaterials.insert(name);
        });
        reapplyTerrainTextureSampling();
        return true;
    } catch (...) {
        return false;
    }
}

bool saveTerrainTextureSamplingIncludes() {
    ensure_includes_loaded();

    std::vector<std::string> textureNames;
    textureNames.reserve(s_textureRules.size());
    for (const auto& [name, rule] : s_textureRules) {
        if (rule.included || rule.hasParamOverride) {
            textureNames.push_back(name);
        }
    }
    std::vector<std::string> materials(s_includedMaterials.begin(), s_includedMaterials.end());
    std::sort(textureNames.begin(), textureNames.end());
    std::sort(materials.begin(), materials.end());

    nlohmann::json textures = nlohmann::json::array();
    for (const std::string& textureName : textureNames) {
        const TextureSamplingRule& rule = s_textureRules[textureName];
        if (!rule.hasParamOverride && rule.included) {
            textures.push_back(textureName);
            continue;
        }

        nlohmann::json entry = {
            {"texture", textureName},
            {"include", rule.included},
            {"overrideParams", rule.hasParamOverride},
        };
        if (rule.hasParamOverride) {
            entry["cellScale"] = rule.params.cellScale;
            entry["jitter"] = rule.params.jitter;
            entry["blendWidth"] = rule.params.blendWidth;
        }
        textures.push_back(std::move(entry));
    }

    nlohmann::json json = {
        {"version", 1},
        {"textures", textures},
        {"materials", materials},
    };

    try {
        io::FileStream::WriteAllText(terrainTextureSamplingIncludePath(), json.dump(4));
        return true;
    } catch (...) {
        return false;
    }
}

bool isTerrainTextureSamplingTextureIncluded(const char* textureName) {
    ensure_includes_loaded();
    const TextureSamplingRule* rule = find_texture_rule_const(textureName);
    return rule != nullptr && rule->included;
}

bool isTerrainTextureSamplingMaterialIncluded(const char* materialName) {
    ensure_includes_loaded();
    return is_valid_key(materialName) && s_includedMaterials.contains(materialName);
}

void setTerrainTextureSamplingTextureIncluded(const char* textureName, bool included) {
    ensure_includes_loaded();
    if (!is_valid_key(textureName)) {
        return;
    }

    TextureSamplingRule* rule = nullptr;
    if (included) {
        rule = &s_textureRules[textureName];
        rule->included = true;
    } else {
        rule = find_texture_rule(textureName);
        if (rule == nullptr) {
            return;
        }
        rule->included = false;
        if (!rule->hasParamOverride) {
            s_textureRules.erase(textureName);
        }
    }
}

void setTerrainTextureSamplingMaterialIncluded(const char* materialName, bool included) {
    ensure_includes_loaded();
    if (!is_valid_key(materialName)) {
        return;
    }

    if (included) {
        s_includedMaterials.insert(materialName);
    } else {
        s_includedMaterials.erase(materialName);
    }
}

bool getTerrainTextureSamplingTextureParams(const char* textureName,
                                            TerrainTextureSamplingParams& params) {
    ensure_includes_loaded();
    const TextureSamplingRule* rule = find_texture_rule_const(textureName);
    if (rule == nullptr || !rule->hasParamOverride) {
        params = global_sampling_params();
        return false;
    }

    params = rule->params;
    return true;
}

void setTerrainTextureSamplingTextureParams(const char* textureName,
                                            const TerrainTextureSamplingParams& params) {
    ensure_includes_loaded();
    if (!is_valid_key(textureName)) {
        return;
    }

    TextureSamplingRule& rule = s_textureRules[textureName];
    rule.hasParamOverride = true;
    rule.params = params;
}

void clearTerrainTextureSamplingTextureParams(const char* textureName) {
    ensure_includes_loaded();
    TextureSamplingRule* rule = find_texture_rule(textureName);
    if (rule == nullptr) {
        return;
    }

    rule->hasParamOverride = false;
    rule->params = {};
    if (!rule->included) {
        s_textureRules.erase(textureName);
    }
}
} // namespace dusk

#else

namespace dusk {
void applyTerrainTextureSamplingSettings() {}
void applyTerrainTextureSampling(J3DModelData*) {}
void registerTerrainTextureSamplingModel(J3DModelData*) {}
void unregisterTerrainTextureSamplingModel(J3DModelData*) {}
void reapplyTerrainTextureSampling() {}
void visitTerrainTextureSamplingUses(TerrainTextureSamplingUseVisitor, void*) {}
std::filesystem::path terrainTextureSamplingIncludePath() { return {}; }
bool loadTerrainTextureSamplingIncludes() { return true; }
bool saveTerrainTextureSamplingIncludes() { return true; }
bool isTerrainTextureSamplingTextureIncluded(const char*) { return false; }
bool isTerrainTextureSamplingMaterialIncluded(const char*) { return false; }
void setTerrainTextureSamplingTextureIncluded(const char*, bool) {}
void setTerrainTextureSamplingMaterialIncluded(const char*, bool) {}
bool getTerrainTextureSamplingTextureParams(const char*, TerrainTextureSamplingParams&) { return false; }
void setTerrainTextureSamplingTextureParams(const char*, const TerrainTextureSamplingParams&) {}
void clearTerrainTextureSamplingTextureParams(const char*) {}
}

#endif
