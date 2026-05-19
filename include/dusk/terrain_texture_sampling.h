#ifndef DUSK_TERRAIN_TEXTURE_SAMPLING_H
#define DUSK_TERRAIN_TEXTURE_SAMPLING_H

#include <cstdint>
#include <filesystem>

class J3DModelData;
class J3DTexture;

namespace dusk {
struct TerrainTextureSamplingParams {
    float cellScale = 1.0f;
    float jitter = 1.0f;
    float blendWidth = 0.25f;
};

struct TerrainTextureSamplingUse {
    J3DModelData* modelData = nullptr;
    J3DTexture* texture = nullptr;
    std::uint16_t materialIndex = 0;
    std::uint16_t textureIndex = 0;
    std::uint16_t textureSlot = 0;
    const char* materialName = "";
    const char* textureName = "";
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint8_t format = 0;
    std::uint8_t wrapS = 0;
    std::uint8_t wrapT = 0;
    bool repeating = false;
    bool candidate = false;
    bool textureIncluded = false;
    bool materialIncluded = false;
    bool textureParamOverride = false;
    TerrainTextureSamplingParams textureParams;
    bool applied = false;
};

using TerrainTextureSamplingUseVisitor = void (*)(const TerrainTextureSamplingUse& use, void* userData);

void applyTerrainTextureSamplingSettings();
void applyTerrainTextureSampling(J3DModelData* modelData);
void registerTerrainTextureSamplingModel(J3DModelData* modelData);
void unregisterTerrainTextureSamplingModel(J3DModelData* modelData);
void reapplyTerrainTextureSampling();
void visitTerrainTextureSamplingUses(TerrainTextureSamplingUseVisitor visitor, void* userData);

std::filesystem::path terrainTextureSamplingIncludePath();
bool loadTerrainTextureSamplingIncludes();
bool saveTerrainTextureSamplingIncludes();
bool isTerrainTextureSamplingTextureIncluded(const char* textureName);
bool isTerrainTextureSamplingMaterialIncluded(const char* materialName);
void setTerrainTextureSamplingTextureIncluded(const char* textureName, bool included);
void setTerrainTextureSamplingMaterialIncluded(const char* materialName, bool included);
bool getTerrainTextureSamplingTextureParams(const char* textureName, TerrainTextureSamplingParams& params);
void setTerrainTextureSamplingTextureParams(const char* textureName, const TerrainTextureSamplingParams& params);
void clearTerrainTextureSamplingTextureParams(const char* textureName);
}

#endif
