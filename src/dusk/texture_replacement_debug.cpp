#include "dusk/texture_replacement_debug.h"

#if TARGET_PC

#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "JSystem/J3DGraphBase/J3DTexture.h"
#include "JSystem/JUtility/JUTNameTab.h"
#include "JSystem/JUtility/JUTTexture.h"

#include <dolphin/gx/GXTextureReplacement.h>

#include <algorithm>
#include <utility>

namespace dusk {
namespace {
std::vector<J3DModelData*> s_models;

const char* safe_name(JUTNameTab* names, std::uint16_t index) {
    if (names == nullptr) {
        return "";
    }

    const char* name = names->getName(index);
    return name != nullptr ? name : "";
}

std::string read_tex_obj_string(const GXTexObj* texObj, u32 (*reader)(const GXTexObj*, char*, u32)) {
    if (texObj == nullptr || reader == nullptr) {
        return {};
    }

    const u32 size = reader(texObj, nullptr, 0);
    if (size == 0) {
        return {};
    }

    std::vector<char> buffer(static_cast<size_t>(size) + 1);
    reader(texObj, buffer.data(), static_cast<u32>(buffer.size()));
    return buffer.data();
}
} // namespace

void registerTextureReplacementDebugModel(J3DModelData* modelData) {
    if (modelData == nullptr) {
        return;
    }

    if (std::find(s_models.begin(), s_models.end(), modelData) != s_models.end()) {
        return;
    }

    s_models.push_back(modelData);
}

void unregisterTextureReplacementDebugModel(J3DModelData* modelData) {
    if (modelData == nullptr) {
        return;
    }

    s_models.erase(std::remove(s_models.begin(), s_models.end(), modelData), s_models.end());
}

std::vector<TextureReplacementObservedTexture> collectTextureReplacementObservedTextures() {
    std::vector<TextureReplacementObservedTexture> out;

    for (J3DModelData* modelData : s_models) {
        if (modelData == nullptr) {
            continue;
        }

        J3DTexture* texture = modelData->getTexture();
        if (texture == nullptr) {
            continue;
        }

        JUTNameTab* textureNames = modelData->getTextureName();
        for (std::uint16_t i = 0; i < texture->getNum(); ++i) {
            ResTIMG* timg = texture->getResTIMG(i);
            if (timg == nullptr || timg->width == 0 || timg->height == 0) {
                continue;
            }

            TGXTexObj* texObj = texture->getTexObj(i);
            TextureReplacementObservedTexture observed{
                .modelData = modelData,
                .texture = texture,
                .textureIndex = i,
                .width = timg->width,
                .height = timg->height,
                .format = timg->format,
                .textureName = safe_name(textureNames, i),
                .replacementName = read_tex_obj_string(texObj, AuroraGetTexObjTextureReplacementName),
                .replacementPath = read_tex_obj_string(texObj, AuroraGetTexObjTextureReplacementPath),
            };
            out.push_back(std::move(observed));
        }
    }

    return out;
}
} // namespace dusk

#else

namespace dusk {
void registerTextureReplacementDebugModel(J3DModelData*) {}
void unregisterTextureReplacementDebugModel(J3DModelData*) {}
std::vector<TextureReplacementObservedTexture> collectTextureReplacementObservedTextures() {
    return {};
}
} // namespace dusk

#endif
