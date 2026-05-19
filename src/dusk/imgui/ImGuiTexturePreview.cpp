#include "ImGuiTexturePreview.hpp"

#include "JSystem/J3DGraphBase/J3DTexture.h"
#include "JSystem/JUtility/JUTTexture.h"

#include <aurora/imgui.h>
#include <dolphin/gx/GXEnum.h>

#include "fmt/format.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <vector>

namespace dusk {
namespace {
struct RGBA8 {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0xff;
};

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
} // namespace

ImGuiTexturePreview makeJ3DTexturePreview(J3DTexture* texture, std::uint16_t textureIndex) {
    ImGuiTexturePreview preview;

    if (texture == nullptr) {
        preview.error = "No texture object.";
        return preview;
    }

    ResTIMG* timg = texture->getResTIMG(textureIndex);
    if (timg == nullptr || timg->width == 0 || timg->height == 0) {
        preview.error = "No texture image.";
        return preview;
    }

    preview.width = timg->width;
    preview.height = timg->height;

    if (!can_decode_preview(timg->format)) {
        preview.error = fmt::format("Preview does not support {}.", texture_format_name(timg->format));
        return preview;
    }

    const auto pixelCount = static_cast<size_t>(timg->width) * static_cast<size_t>(timg->height);
    const std::uint8_t* imageData = texture->getImgDataPtr(textureIndex);
    if (imageData == nullptr) {
        preview.error = "Texture image data is unavailable.";
        return preview;
    }

    try {
        const std::uint8_t* tlutData = timg->indexTexture ? texture->getTlutDataPtr(textureIndex) : nullptr;
        if (timg->indexTexture && (tlutData == nullptr || timg->numColors == 0)) {
            preview.error = "Texture palette data is unavailable.";
            return preview;
        }

        std::vector<std::uint8_t> pixels = decode_texture_preview(*timg, imageData, tlutData);
        if (pixels.size() < pixelCount * 4) {
            preview.error = "Texture conversion returned no preview pixels.";
            return preview;
        }

        preview.texture = aurora_imgui_add_texture(timg->width, timg->height, pixels.data());
    } catch (const std::exception& e) {
        preview.error = e.what();
    }

    return preview;
}
} // namespace dusk
