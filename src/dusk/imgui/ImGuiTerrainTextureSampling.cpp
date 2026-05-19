#include "ImGuiMenuTools.hpp"

#include "ImGuiConsole.hpp"
#include "ImGuiTexturePreview.hpp"
#include "dusk/config.hpp"
#include "dusk/io.hpp"
#include "dusk/settings.h"
#include "dusk/terrain_texture_sampling.h"

#include "JSystem/J3DGraphBase/J3DTexture.h"
#include "JSystem/JUtility/JUTTexture.h"

#include <dolphin/gx/GXTexture.h>
#include <dolphin/gx/GXTextureReplacement.h>

#include "fmt/format.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cctype>
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
    ImGuiTexturePreview preview;
};

std::array<char, 128> s_filter{};
Selection s_selection;
std::vector<TexturePreview> s_previewCache;
std::string s_status;

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

TexturePreview make_preview(const TerrainTextureSamplingUse& use) {
    TexturePreview preview;
    preview.texture = use.texture;
    preview.textureIndex = use.textureIndex;
    preview.timg = use.texture != nullptr ? use.texture->getResTIMG(use.textureIndex) : nullptr;
    preview.preview = makeJ3DTexturePreview(use.texture, use.textureIndex);
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
    const ImGuiTexturePreview& preview = preview_for(*selected).preview;
    if (preview.texture == 0) {
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
    ImGui::Image(preview.texture,
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
