#ifndef DUSK_TEXTURE_REPLACEMENT_DEBUG_H
#define DUSK_TEXTURE_REPLACEMENT_DEBUG_H

#include <cstdint>
#include <string>
#include <vector>

class J3DModelData;
class J3DTexture;

namespace dusk {
struct TextureReplacementObservedTexture {
    J3DModelData* modelData = nullptr;
    J3DTexture* texture = nullptr;
    std::uint16_t textureIndex = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint8_t format = 0;
    std::string textureName;
    std::string replacementName;
    std::string replacementPath;
};

void registerTextureReplacementDebugModel(J3DModelData* modelData);
void unregisterTextureReplacementDebugModel(J3DModelData* modelData);
std::vector<TextureReplacementObservedTexture> collectTextureReplacementObservedTextures();
}

#endif
