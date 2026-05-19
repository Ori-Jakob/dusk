#ifndef DUSK_IMGUI_TEXTURE_PREVIEW_HPP
#define DUSK_IMGUI_TEXTURE_PREVIEW_HPP

#include "imgui.h"

#include <cstdint>
#include <string>

class J3DTexture;

namespace dusk {
struct ImGuiTexturePreview {
    ImTextureID texture = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::string error;
};

ImGuiTexturePreview makeJ3DTexturePreview(J3DTexture* texture, std::uint16_t textureIndex);
}

#endif
