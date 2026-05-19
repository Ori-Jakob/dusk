#include "ImGuiMenuTools.hpp"

#include "ImGuiConsole.hpp"
#include "dusk/config.hpp"
#include "dusk/io.hpp"
#include "dusk/settings.h"
#include "dusk/terrain_texture_sampling.h"

#include "JSystem/J3DGraphBase/J3DTexture.h"
#include "JSystem/JUtility/JUTTexture.h"

#include <aurora/imgui.h>
#include <dolphin/gx/GXTexture.h>
#include <dolphin/gx/GXTextureReplacement.h>

#include "fmt/format.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace dusk {
namespace {
struct Selection {
    J3DModelData* modelData = nullptr;
    J3DTexture* texture = nullptr;
    std::uint16_t materialIndex = 0;
    std::uint16_t textureIndex = 0;
    std::uint16_t textureSlot = 0;
};

struct TexturePreview {
    J3DTexture* texture = nullptr;
    std::uint16_t textureIndex = 0;
    ResTIMG* timg = nullptr;
    ImTextureID imguiTexture = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::string error;
};

std::array<char, 128> s_filter{};
Selection s_selection;
std::vector<TexturePreview> s_previewCache;
std::string s_status;

struct RGBA8 {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0xff;
};

void collect_use(const TerrainTextureSamplingUse& use, void* userData) {
    static_cast<std::vector<TerrainTextureSamplingUse>*>(userData)->push_back(use);
}

std::string lower_ascii(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

bool text_matches_filter(const TerrainTextureSamplingUse& use) {
    if (s_filter[0] == '\0') {
        return true;
    }

    const std::string needle = lower_ascii(s_filter.data());
    std::string haystack;
    haystack.reserve(128);
    haystack += use.materialName != nullptr ? use.materialName : "";
    haystack += ' ';
    haystack += use.textureName != nullptr ? use.textureName : "";
    haystack = lower_ascii(std::move(haystack));
    return haystack.find(needle) != std::string::npos;
}

bool same_selection(const TerrainTextureSamplingUse& use, const Selection& selection) {
    return use.modelData == selection.modelData &&
           use.texture == selection.texture &&
           use.materialIndex == selection.materialIndex &&
           use.textureIndex == selection.textureIndex &&
           use.textureSlot == selection.textureSlot;
}

Selection make_selection(const TerrainTextureSamplingUse& use) {
    return {
        .modelData = use.modelData,
        .texture = use.texture,
        .materialIndex = use.materialIndex,
        .textureIndex = use.textureIndex,
        .textureSlot = use.textureSlot,
    };
}

const char* texture_format_name(std::uint8_t format) {
    switch (format) {
    case GX_TF_I4:
        return "I4";
    case GX_TF_I8:
        return "I8";
    case GX_TF_IA4:
        return "IA4";
    case GX_TF_IA8:
        return "IA8";
    case GX_TF_RGB565:
        return "RGB565";
    case GX_TF_RGB5A3:
        return "RGB5A3";
    case GX_TF_RGBA8:
        return "RGBA8";
    case GX_TF_CMPR:
        return "CMPR";
    case GX_TF_C4:
        return "C4";
    case GX_TF_C8:
        return "C8";
    case GX_TF_C14X2:
        return "C14X2";
#ifdef _GX_TF_PC
    case GX_TF_R8_PC:
        return "R8 PC";
    case GX_TF_RGBA8_PC:
        return "RGBA8 PC";
#endif
    default:
        return "Unknown";
    }
}

const char* wrap_name(std::uint8_t wrap) {
    switch (wrap) {
    case GX_CLAMP:
        return "Clamp";
    case GX_REPEAT:
        return "Repeat";
    case GX_MIRROR:
        return "Mirror";
    default:
        return "?";
    }
}

bool can_decode_preview(std::uint8_t format) {
    switch (format) {
    case GX_TF_I4:
    case GX_TF_I8:
    case GX_TF_IA4:
    case GX_TF_IA8:
    case GX_TF_RGB565:
    case GX_TF_RGB5A3:
    case GX_TF_RGBA8:
    case GX_TF_CMPR:
    case GX_TF_C4:
    case GX_TF_C8:
#ifdef _GX_TF_PC
    case GX_TF_R8_PC:
    case GX_TF_RGBA8_PC:
#endif
        return true;
    default:
        return false;
    }
}

std::uint8_t expand_to_8(std::uint32_t value, std::uint32_t bits) {
    if (bits == 0) {
        return 0;
    }
    if (bits >= 8) {
        return static_cast<std::uint8_t>(value);
    }
    const std::uint32_t maxValue = (1u << bits) - 1u;
    return static_cast<std::uint8_t>((value * 255u + maxValue / 2u) / maxValue);
}

std::uint16_t read_be16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8) | data[1]);
}

std::uint8_t s3tc_blend(std::uint32_t a, std::uint32_t b) {
    return static_cast<std::uint8_t>((((a << 1) + a) + ((b << 2) + b)) >> 3);
}

std::uint8_t half_blend(std::uint8_t a, std::uint8_t b) {
    return static_cast<std::uint8_t>((static_cast<std::uint32_t>(a) + b) >> 1);
}

void write_pixel(std::vector<std::uint8_t>& pixels, std::uint32_t width, std::uint32_t height,
                 std::uint32_t x, std::uint32_t y, const RGBA8& color) {
    if (x >= width || y >= height) {
        return;
    }

    const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
    pixels[offset] = color.r;
    pixels[offset + 1] = color.g;
    pixels[offset + 2] = color.b;
    pixels[offset + 3] = color.a;
}

RGBA8 decode_rgb565(std::uint16_t texel) {
    return {
        .r = expand_to_8((texel >> 11) & 0x1f, 5),
        .g = expand_to_8((texel >> 5) & 0x3f, 6),
        .b = expand_to_8(texel & 0x1f, 5),
        .a = 0xff,
    };
}

RGBA8 decode_rgb5a3(std::uint16_t texel) {
    if ((texel & 0x8000) != 0) {
        return {
            .r = expand_to_8((texel >> 10) & 0x1f, 5),
            .g = expand_to_8((texel >> 5) & 0x1f, 5),
            .b = expand_to_8(texel & 0x1f, 5),
            .a = 0xff,
        };
    }

    return {
        .r = expand_to_8((texel >> 8) & 0xf, 4),
        .g = expand_to_8((texel >> 4) & 0xf, 4),
        .b = expand_to_8(texel & 0xf, 4),
        .a = expand_to_8((texel >> 12) & 0x7, 3),
    };
}

RGBA8 decode_ia8(const std::uint8_t* data) {
    std::uint16_t texel = 0;
    std::memcpy(&texel, data, sizeof(texel));
    const std::uint8_t intensity = texel >> 8;
    return {
        .r = intensity,
        .g = intensity,
        .b = intensity,
        .a = static_cast<std::uint8_t>(texel & 0xff),
    };
}

RGBA8 decode_palette_entry(const ResTIMG& timg, const std::uint8_t* tlutData, std::uint16_t index) {
    if (tlutData == nullptr || index >= timg.numColors) {
        return {.a = 0};
    }

    const std::uint8_t* entry = tlutData + static_cast<size_t>(index) * 2;
    switch (timg.colorFormat) {
    case GX_TL_IA8:
        return decode_ia8(entry);
    case GX_TL_RGB565:
        return decode_rgb565(read_be16(entry));
    case GX_TL_RGB5A3:
        return decode_rgb5a3(read_be16(entry));
    default:
        return {.a = 0};
    }
}

template <typename DecodePixel>
void decode_tiled(std::vector<std::uint8_t>& pixels, const std::uint8_t* data,
                  std::uint32_t width, std::uint32_t height, std::uint32_t blockWidth,
                  std::uint32_t blockHeight, std::uint32_t blockBytes,
                  DecodePixel decodePixel) {
    size_t sourceOffset = 0;
    for (std::uint32_t by = 0; by < height; by += blockHeight) {
        for (std::uint32_t bx = 0; bx < width; bx += blockWidth) {
            const std::uint8_t* block = data + sourceOffset;
            for (std::uint32_t y = 0; y < blockHeight; ++y) {
                for (std::uint32_t x = 0; x < blockWidth; ++x) {
                    if (bx + x < width && by + y < height) {
                        write_pixel(pixels, width, height, bx + x, by + y, decodePixel(block, x, y));
                    }
                }
            }
            sourceOffset += blockBytes;
        }
    }
}

std::vector<std::uint8_t> decode_rgba8(const std::uint8_t* data, std::uint32_t width, std::uint32_t height) {
    std::vector<std::uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    size_t sourceOffset = 0;
    for (std::uint32_t by = 0; by < height; by += 4) {
        for (std::uint32_t bx = 0; bx < width; bx += 4) {
            const std::uint8_t* block = data + sourceOffset;
            for (std::uint32_t y = 0; y < 4; ++y) {
                for (std::uint32_t x = 0; x < 4; ++x) {
                    const size_t ar = static_cast<size_t>(y) * 8 + x * 2;
                    const size_t gb = 32 + static_cast<size_t>(y) * 8 + x * 2;
                    write_pixel(pixels, width, height, bx + x, by + y, {
                        .r = block[ar + 1],
                        .g = block[gb],
                        .b = block[gb + 1],
                        .a = block[ar],
                    });
                }
            }
            sourceOffset += 64;
        }
    }
    return pixels;
}

std::vector<std::uint8_t> decode_cmpr(const std::uint8_t* data, std::uint32_t width, std::uint32_t height) {
    std::vector<std::uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    const std::uint8_t* src = data;
    for (std::uint32_t yy = 0; yy < height; yy += 8) {
        for (std::uint32_t xx = 0; xx < width; xx += 8) {
            for (std::uint32_t yb = 0; yb < 8; yb += 4) {
                for (std::uint32_t xb = 0; xb < 8; xb += 4) {
                    const std::uint16_t color1 = read_be16(src);
                    const std::uint16_t color2 = read_be16(src + 2);
                    src += 4;

                    std::array<std::uint8_t, 16> colorTable{};
                    const RGBA8 c1 = decode_rgb565(color1);
                    const RGBA8 c2 = decode_rgb565(color2);
                    colorTable[0] = c1.r;
                    colorTable[1] = c1.g;
                    colorTable[2] = c1.b;
                    colorTable[3] = c1.a;
                    colorTable[4] = c2.r;
                    colorTable[5] = c2.g;
                    colorTable[6] = c2.b;
                    colorTable[7] = c2.a;
                    if (color1 > color2) {
                        colorTable[8] = s3tc_blend(colorTable[4], colorTable[0]);
                        colorTable[9] = s3tc_blend(colorTable[5], colorTable[1]);
                        colorTable[10] = s3tc_blend(colorTable[6], colorTable[2]);
                        colorTable[11] = 0xff;
                        colorTable[12] = s3tc_blend(colorTable[0], colorTable[4]);
                        colorTable[13] = s3tc_blend(colorTable[1], colorTable[5]);
                        colorTable[14] = s3tc_blend(colorTable[2], colorTable[6]);
                        colorTable[15] = 0xff;
                    } else {
                        colorTable[8] = half_blend(colorTable[0], colorTable[4]);
                        colorTable[9] = half_blend(colorTable[1], colorTable[5]);
                        colorTable[10] = half_blend(colorTable[2], colorTable[6]);
                        colorTable[11] = 0xff;
                        colorTable[12] = colorTable[8];
                        colorTable[13] = colorTable[9];
                        colorTable[14] = colorTable[10];
                        colorTable[15] = 0;
                    }

                    for (std::uint32_t y = 0; y < 4; ++y) {
                        std::uint8_t bits = src[y];
                        for (std::uint32_t x = 0; x < 4; ++x) {
                            const std::uint8_t* color = &colorTable[static_cast<size_t>((bits >> 6) & 3) * 4];
                            write_pixel(pixels, width, height, xx + xb + x, yy + yb + y, {
                                .r = color[0],
                                .g = color[1],
                                .b = color[2],
                                .a = color[3],
                            });
                            bits <<= 2;
                        }
                    }
                    src += 4;
                }
            }
        }
    }
    return pixels;
}

std::vector<std::uint8_t> decode_texture_preview(const ResTIMG& timg, const std::uint8_t* imageData,
                                                 const std::uint8_t* tlutData) {
    const std::uint32_t width = timg.width;
    const std::uint32_t height = timg.height;
    std::vector<std::uint8_t> pixels(static_cast<size_t>(width) * height * 4);

#ifdef _GX_TF_PC
    if (timg.format == GX_TF_RGBA8_PC) {
        std::memcpy(pixels.data(), imageData, pixels.size());
        return pixels;
    }
    if (timg.format == GX_TF_R8_PC) {
        for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
            const std::uint8_t intensity = imageData[i];
            pixels[i * 4] = intensity;
            pixels[i * 4 + 1] = intensity;
            pixels[i * 4 + 2] = intensity;
            pixels[i * 4 + 3] = intensity;
        }
        return pixels;
    }
#endif

    switch (timg.format) {
    case GX_TF_I4:
        decode_tiled(pixels, imageData, width, height, 8, 8, 32,
            [](const std::uint8_t* block, std::uint32_t x, std::uint32_t y) {
                const std::uint8_t packed = block[(y * 8 + x) / 2];
                const std::uint8_t intensity = expand_to_8((x & 1) != 0 ? packed & 0xf : packed >> 4, 4);
                return RGBA8{intensity, intensity, intensity, intensity};
            });
        break;
    case GX_TF_I8:
        decode_tiled(pixels, imageData, width, height, 8, 4, 32,
            [](const std::uint8_t* block, std::uint32_t x, std::uint32_t y) {
                const std::uint8_t intensity = block[y * 8 + x];
                return RGBA8{intensity, intensity, intensity, intensity};
            });
        break;
    case GX_TF_IA4:
        decode_tiled(pixels, imageData, width, height, 8, 4, 32,
            [](const std::uint8_t* block, std::uint32_t x, std::uint32_t y) {
                const std::uint8_t value = block[y * 8 + x];
                const std::uint8_t intensity = expand_to_8(value & 0xf, 4);
                return RGBA8{intensity, intensity, intensity, expand_to_8(value >> 4, 4)};
            });
        break;
    case GX_TF_IA8:
        decode_tiled(pixels, imageData, width, height, 4, 4, 32,
            [](const std::uint8_t* block, std::uint32_t x, std::uint32_t y) {
                return decode_ia8(block + (y * 4 + x) * 2);
            });
        break;
    case GX_TF_RGB565:
        decode_tiled(pixels, imageData, width, height, 4, 4, 32,
            [](const std::uint8_t* block, std::uint32_t x, std::uint32_t y) {
                return decode_rgb565(read_be16(block + (y * 4 + x) * 2));
            });
        break;
    case GX_TF_RGB5A3:
        decode_tiled(pixels, imageData, width, height, 4, 4, 32,
            [](const std::uint8_t* block, std::uint32_t x, std::uint32_t y) {
                return decode_rgb5a3(read_be16(block + (y * 4 + x) * 2));
            });
        break;
    case GX_TF_RGBA8:
        return decode_rgba8(imageData, width, height);
    case GX_TF_CMPR:
        return decode_cmpr(imageData, width, height);
    case GX_TF_C4:
        decode_tiled(pixels, imageData, width, height, 8, 8, 32,
            [&](const std::uint8_t* block, std::uint32_t x, std::uint32_t y) {
                const std::uint8_t packed = block[(y * 8 + x) / 2];
                const std::uint16_t index = (x & 1) != 0 ? packed & 0xf : packed >> 4;
                return decode_palette_entry(timg, tlutData, index);
            });
        break;
    case GX_TF_C8:
        decode_tiled(pixels, imageData, width, height, 8, 4, 32,
            [&](const std::uint8_t* block, std::uint32_t x, std::uint32_t y) {
                return decode_palette_entry(timg, tlutData, block[y * 8 + x]);
            });
        break;
    default:
        pixels.clear();
        break;
    }

    return pixels;
}

TexturePreview make_preview(const TerrainTextureSamplingUse& use) {
    TexturePreview preview;
    preview.texture = use.texture;
    preview.textureIndex = use.textureIndex;
    preview.width = use.width;
    preview.height = use.height;

    if (use.texture == nullptr) {
        preview.error = "No texture object.";
        return preview;
    }

    ResTIMG* timg = use.texture->getResTIMG(use.textureIndex);
    preview.timg = timg;
    if (timg == nullptr || timg->width == 0 || timg->height == 0) {
        preview.error = "No texture image.";
        return preview;
    }
    if (!can_decode_preview(timg->format)) {
        preview.error = fmt::format("Preview does not support {}.", texture_format_name(timg->format));
        return preview;
    }

    const auto pixelCount = static_cast<size_t>(timg->width) * static_cast<size_t>(timg->height);
    u8* imageData = use.texture->getImgDataPtr(use.textureIndex);
    if (imageData == nullptr) {
        preview.error = "Texture image data is unavailable.";
        return preview;
    }

    try {
        u8* tlutData = timg->indexTexture ? use.texture->getTlutDataPtr(use.textureIndex) : nullptr;
        if (timg->indexTexture && (tlutData == nullptr || timg->numColors == 0)) {
            preview.error = "Texture palette data is unavailable.";
            return preview;
        }

        std::vector<std::uint8_t> pixels = decode_texture_preview(*timg, imageData, tlutData);
        if (pixels.size() < pixelCount * 4) {
            preview.error = "Texture conversion returned no preview pixels.";
            return preview;
        }

        preview.imguiTexture = aurora_imgui_add_texture(
            timg->width,
            timg->height,
            pixels.data());
    } catch (const std::exception& e) {
        preview.error = e.what();
    }

    return preview;
}

TexturePreview& preview_for(const TerrainTextureSamplingUse& use) {
    ResTIMG* timg = use.texture != nullptr ? use.texture->getResTIMG(use.textureIndex) : nullptr;
    for (TexturePreview& preview : s_previewCache) {
        if (preview.texture == use.texture &&
            preview.textureIndex == use.textureIndex &&
            preview.timg == timg)
        {
            return preview;
        }
    }

    s_previewCache.push_back(make_preview(use));
    return s_previewCache.back();
}

using TextureStringReader = u32 (*)(const GXTexObj*, char*, u32);

std::string read_texture_string(const GXTexObj* texObj, TextureStringReader reader) {
    if (texObj == nullptr || reader == nullptr) {
        return {};
    }

    const u32 length = reader(texObj, nullptr, 0);
    if (length == 0) {
        return {};
    }

    std::vector<char> buffer(static_cast<size_t>(length) + 1);
    reader(texObj, buffer.data(), static_cast<u32>(buffer.size()));
    return buffer.data();
}

GXTexObj* selected_tex_obj(const TerrainTextureSamplingUse& use) {
    if (use.texture == nullptr) {
        return nullptr;
    }
    return use.texture->getTexObj(use.textureIndex);
}

void apply_and_save_config() {
    config::Save();
    applyTerrainTextureSamplingSettings();
}

void include_candidate_textures(const std::vector<TerrainTextureSamplingUse>& rows) {
    for (const TerrainTextureSamplingUse& row : rows) {
        if (row.candidate && row.repeating && row.textureName != nullptr && row.textureName[0] != '\0') {
            setTerrainTextureSamplingTextureIncluded(row.textureName, true);
        }
    }
    reapplyTerrainTextureSampling();
}

void draw_global_controls() {
    bool enabled = getSettings().game.enableStochasticTerrainTextures.getValue();
    if (ImGui::Checkbox("Enable terrain variation", &enabled)) {
        getSettings().game.enableStochasticTerrainTextures.setValue(enabled);
        apply_and_save_config();
    }

    float cellScale = getSettings().game.stochasticTerrainCellScale.getValue();
    float jitter = getSettings().game.stochasticTerrainJitter.getValue();
    float blendWidth = getSettings().game.stochasticTerrainBlendWidth.getValue();
    bool paramsChanged = false;
    paramsChanged |= ImGui::SliderFloat("Cell scale", &cellScale, 0.25f, 16.0f, "%.2fx");
    paramsChanged |= ImGui::SliderFloat("Jitter", &jitter, 0.0f, 2.5f, "%.2fx");
    paramsChanged |= ImGui::SliderFloat("Blend", &blendWidth, 0.0f, 4.0f, "%.2fx");
    if (paramsChanged) {
        getSettings().game.stochasticTerrainCellScale.setValue(cellScale);
        getSettings().game.stochasticTerrainJitter.setValue(jitter);
        getSettings().game.stochasticTerrainBlendWidth.setValue(blendWidth);
        apply_and_save_config();
    }
}

void draw_texture_preview(const TerrainTextureSamplingUse* selected) {
    ImGui::Separator();
    ImGui::TextUnformatted("Selected Texture");

    if (selected == nullptr) {
        ImGui::TextDisabled("Select a row to preview its texture.");
        return;
    }

    ImGui::Text("Material: %s", selected->materialName);
    ImGui::Text("Texture: %s", selected->textureName);
    ImGui::Text("Size: %ux%u  Format: %s  Wrap: %s / %s",
                selected->width,
                selected->height,
                texture_format_name(selected->format),
                wrap_name(selected->wrapS),
                wrap_name(selected->wrapT));

    ImGui::Spacing();
    ImGui::TextUnformatted("Texture Replacement");
    GXTexObj* texObj = selected_tex_obj(*selected);
    const std::string replacementPath =
        read_texture_string(texObj, AuroraGetTexObjTextureReplacementPath);
    if (!replacementPath.empty()) {
        ImGui::TextWrapped("Path: %s", replacementPath.c_str());
        if (ImGui::Button("Copy Replacement Path")) {
            ImGui::SetClipboardText(replacementPath.c_str());
            s_status = "Copied replacement texture path.";
        }
    } else {
        ImGui::TextDisabled("No indexed replacement texture.");
        const std::string replacementName =
            read_texture_string(texObj, AuroraGetTexObjTextureReplacementName);
        if (!replacementName.empty()) {
            ImGui::TextWrapped("Expected filename: %s", replacementName.c_str());
            if (ImGui::Button("Copy Expected Filename")) {
                ImGui::SetClipboardText(replacementName.c_str());
                s_status = "Copied expected replacement texture filename.";
            }
        }
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Texture Parameters");
    if (selected->textureName == nullptr || selected->textureName[0] == '\0') {
        ImGui::TextDisabled("This texture has no name to save an override against.");
    } else {
        ImGui::PushID(selected->textureName);
        bool useOverride = selected->textureParamOverride;
        TerrainTextureSamplingParams params = selected->textureParams;
        if (ImGui::Checkbox("Use texture override", &useOverride)) {
            if (useOverride) {
                setTerrainTextureSamplingTextureParams(selected->textureName, params);
            } else {
                clearTerrainTextureSamplingTextureParams(selected->textureName);
            }
            reapplyTerrainTextureSampling();
        }

        if (useOverride) {
            bool paramsChanged = false;
            paramsChanged |= ImGui::SliderFloat("Texture cell scale", &params.cellScale, 0.25f, 16.0f, "%.2fx");
            paramsChanged |= ImGui::SliderFloat("Texture jitter", &params.jitter, 0.0f, 2.5f, "%.2fx");
            paramsChanged |= ImGui::SliderFloat("Texture blend", &params.blendWidth, 0.0f, 4.0f, "%.2fx");
            if (paramsChanged) {
                setTerrainTextureSamplingTextureParams(selected->textureName, params);
                reapplyTerrainTextureSampling();
            }
        } else {
            ImGui::TextDisabled("Using global variation parameters.");
        }
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Preview");
    TexturePreview& preview = preview_for(*selected);
    if (preview.imguiTexture == 0) {
        ImGui::TextDisabled("%s", preview.error.empty() ? "Preview unavailable." : preview.error.c_str());
        return;
    }

    const float maxSize = 256.0f;
    float scale = 1.0f;
    if (preview.width > 0 && preview.height > 0) {
        scale = std::min(maxSize / static_cast<float>(preview.width),
                         maxSize / static_cast<float>(preview.height));
        scale = std::clamp(scale, 0.1f, 4.0f);
    }
    ImGui::Image(preview.imguiTexture,
                 ImVec2(static_cast<float>(preview.width) * scale,
                        static_cast<float>(preview.height) * scale));
}

void draw_rows_table(std::vector<TerrainTextureSamplingUse>& rows) {
    std::sort(rows.begin(), rows.end(), [](const auto& lhs, const auto& rhs) {
        const int textureCompare = std::string_view(lhs.textureName).compare(rhs.textureName);
        if (textureCompare != 0) {
            return textureCompare < 0;
        }
        const int materialCompare = std::string_view(lhs.materialName).compare(rhs.materialName);
        if (materialCompare != 0) {
            return materialCompare < 0;
        }
        return lhs.textureSlot < rhs.textureSlot;
    });

    const TerrainTextureSamplingUse* selected = nullptr;
    std::vector<int> visibleRows;
    visibleRows.reserve(rows.size());
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        if (text_matches_filter(rows[i])) {
            visibleRows.push_back(i);
        }
        if (same_selection(rows[i], s_selection)) {
            selected = &rows[i];
        }
    }

    ImGui::Text("%d material texture uses visible, %d total", static_cast<int>(visibleRows.size()),
                static_cast<int>(rows.size()));

    bool shouldReapply = false;
    if (ImGui::BeginTable("terrain_texture_sampling_uses",
                          10,
                          ImGuiTableFlags_Borders |
                              ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(0.0f, 360.0f)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Texture", ImGuiTableColumnFlags_WidthFixed, 64.0f);
        ImGui::TableSetupColumn("Material", ImGuiTableColumnFlags_WidthFixed, 64.0f);
        ImGui::TableSetupColumn("Applied", ImGuiTableColumnFlags_WidthFixed, 62.0f);
        ImGui::TableSetupColumn("Override", ImGuiTableColumnFlags_WidthFixed, 68.0f);
        ImGui::TableSetupColumn("Hint", ImGuiTableColumnFlags_WidthFixed, 44.0f);
        ImGui::TableSetupColumn("Material Name");
        ImGui::TableSetupColumn("Texture Name");
        ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 38.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 78.0f);
        ImGui::TableSetupColumn("Format", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(visibleRows.size()));
        while (clipper.Step()) {
            for (int visibleIndex = clipper.DisplayStart; visibleIndex < clipper.DisplayEnd; ++visibleIndex) {
                TerrainTextureSamplingUse& row = rows[visibleRows[visibleIndex]];
                const bool isSelected = same_selection(row, s_selection);

                ImGui::PushID(visibleIndex);
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                bool textureIncluded = row.textureIncluded;
                if (ImGui::Checkbox("##texture", &textureIncluded)) {
                    setTerrainTextureSamplingTextureIncluded(row.textureName, textureIncluded);
                    shouldReapply = true;
                }

                ImGui::TableNextColumn();
                bool materialIncluded = row.materialIncluded;
                if (ImGui::Checkbox("##material", &materialIncluded)) {
                    setTerrainTextureSamplingMaterialIncluded(row.materialName, materialIncluded);
                    shouldReapply = true;
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row.applied ? "Yes" : "No");

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row.textureParamOverride ? "Yes" : "-");

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row.candidate ? "Yes" : "-");

                ImGui::TableNextColumn();
                if (ImGui::Selectable(row.materialName, isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                    s_selection = make_selection(row);
                    selected = &row;
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row.textureName);

                ImGui::TableNextColumn();
                ImGui::Text("%u", static_cast<unsigned>(row.textureSlot));

                ImGui::TableNextColumn();
                ImGui::Text("%ux%u", static_cast<unsigned>(row.width), static_cast<unsigned>(row.height));

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(texture_format_name(row.format));
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    if (shouldReapply) {
        reapplyTerrainTextureSampling();
    }

    draw_texture_preview(selected);
}
} // namespace

void ImGuiMenuTools::ShowTerrainTextureSampling() {
    if (!m_showTerrainTextureSampling) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(1120.0f, 760.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Terrain Texture Variation", &m_showTerrainTextureSampling)) {
        ImGui::End();
        return;
    }

    draw_global_controls();

    std::vector<TerrainTextureSamplingUse> rows;
    visitTerrainTextureSamplingUses(collect_use, &rows);

    ImGui::Separator();
    ImGui::InputTextWithHint("Filter", "material or texture name", s_filter.data(), s_filter.size());

    if (ImGui::Button("Include Candidate Textures")) {
        include_candidate_textures(rows);
        s_status = "Included current heuristic candidate textures in memory.";
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload JSON")) {
        if (loadTerrainTextureSamplingIncludes()) {
            s_status = "Reloaded inclusion list.";
        } else {
            s_status = "Failed to reload inclusion list.";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save JSON")) {
        if (saveTerrainTextureSamplingIncludes()) {
            s_status = "Saved inclusion list.";
        } else {
            s_status = "Failed to save inclusion list.";
        }
    }

    const std::string path = io::fs_path_to_string(terrainTextureSamplingIncludePath());
    ImGui::TextWrapped("JSON: %s", path.c_str());
    if (!s_status.empty()) {
        ImGui::TextDisabled("%s", s_status.c_str());
    }

    draw_rows_table(rows);

    ImGui::End();
}
} // namespace dusk
