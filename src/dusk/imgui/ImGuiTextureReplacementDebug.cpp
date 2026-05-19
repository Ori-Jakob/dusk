#include "ImGuiMenuTools.hpp"

#include "ImGuiTexturePreview.hpp"
#include "dusk/texture_replacement_debug.h"

#include "JSystem/J3DGraphBase/J3DTexture.h"

#include <aurora/gfx.h>
#include <dolphin/gx/GXTexture.h>
#include <dolphin/gx/GXTextureReplacement.h>

#include "fmt/format.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace dusk {
namespace {
struct ReplacementRow {
    std::uint32_t index = 0;
    AuroraTextureReplacementEntryInfo info{};
    std::string name;
    std::string path;
    std::string searchText;
    std::string baseSearchText;
    std::string loadedTextureNames;
    std::uint32_t loadedUseCount = 0;
    bool loadedReplacementInfo = false;
    bool failedReplacementInfo = false;
};

struct OriginalPreviewCache {
    J3DTexture* texture = nullptr;
    std::uint16_t textureIndex = 0;
    ResTIMG* timg = nullptr;
    ImGuiTexturePreview preview;
};

std::array<char, 256> s_filter{};
std::vector<ReplacementRow> s_rows;
std::vector<TextureReplacementObservedTexture> s_loadedTextures;
std::vector<OriginalPreviewCache> s_originalPreviewCache;
std::string s_rootPath;
std::string s_status;
bool s_loadedOnly = false;
bool s_needsRefresh = true;
std::uint32_t s_selectedIndex = UINT32_MAX;

using ReplacementStringReader = u32 (*)(u32, char*, u32);

std::string lower_ascii(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

std::string read_aurora_string(u32 (*reader)(char*, u32)) {
    const u32 size = reader(nullptr, 0);
    if (size == 0) {
        return {};
    }

    std::vector<char> buffer(static_cast<size_t>(size) + 1);
    reader(buffer.data(), static_cast<u32>(buffer.size()));
    return buffer.data();
}

std::string read_replacement_string(ReplacementStringReader reader, std::uint32_t index) {
    const u32 size = reader(index, nullptr, 0);
    if (size == 0) {
        return {};
    }

    std::vector<char> buffer(static_cast<size_t>(size) + 1);
    reader(index, buffer.data(), static_cast<u32>(buffer.size()));
    return buffer.data();
}

const char* texture_format_name(std::uint32_t format) {
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

std::string replacement_size_text(const AuroraTextureReplacementEntryInfo& info) {
    if (!info.has_replacement_info) {
        return "Select row";
    }
    return fmt::format("{}x{}", info.replacement_width, info.replacement_height);
}

std::string replacement_size_text(const ReplacementRow& row) {
    if (row.info.has_replacement_info) {
        return replacement_size_text(row.info);
    }
    return row.failedReplacementInfo ? "Failed" : "Select row";
}

void rebuild_search_text(ReplacementRow& row) {
    row.baseSearchText = lower_ascii(fmt::format(
        "{} {} {}x{} {}",
        row.name,
        row.path,
        row.info.original_width,
        row.info.original_height,
        texture_format_name(row.info.original_format)));
    row.searchText = row.baseSearchText;
    if (!row.loadedTextureNames.empty()) {
        row.searchText += ' ';
        row.searchText += lower_ascii(row.loadedTextureNames);
    }
}

void refresh_loaded_matches() {
    s_loadedTextures = collectTextureReplacementObservedTextures();
    s_originalPreviewCache.clear();

    struct LoadedMatch {
        std::uint32_t count = 0;
        std::string names;
        std::unordered_set<std::string> uniqueNames;
    };

    std::unordered_map<std::string, LoadedMatch> matchesByPath;
    for (const TextureReplacementObservedTexture& texture : s_loadedTextures) {
        if (texture.replacementPath.empty()) {
            continue;
        }

        LoadedMatch& match = matchesByPath[texture.replacementPath];
        ++match.count;

        const std::string name = texture.textureName.empty() ? "(unnamed)" : texture.textureName;
        if (match.uniqueNames.insert(name).second) {
            if (!match.names.empty()) {
                match.names += ", ";
            }
            match.names += name;
        }
    }

    for (ReplacementRow& row : s_rows) {
        row.loadedUseCount = 0;
        row.loadedTextureNames.clear();

        const auto match = matchesByPath.find(row.path);
        if (match != matchesByPath.end()) {
            row.loadedUseCount = match->second.count;
            row.loadedTextureNames = match->second.names;
        }

        row.searchText = row.baseSearchText;
        if (!row.loadedTextureNames.empty()) {
            row.searchText += ' ';
            row.searchText += lower_ascii(row.loadedTextureNames);
        }
    }
}

void refresh_rows() {
    s_rootPath = read_aurora_string(AuroraGetTextureReplacementRootPath);

    s_rows.clear();
    s_selectedIndex = UINT32_MAX;
    const u32 count = AuroraGetTextureReplacementEntryCount();
    s_rows.reserve(count);

    for (u32 i = 0; i < count; ++i) {
        ReplacementRow row{
            .index = i,
            .name = read_replacement_string(AuroraGetTextureReplacementEntryName, i),
            .path = read_replacement_string(AuroraGetTextureReplacementEntryPath, i),
        };

        if (AuroraGetTextureReplacementEntryInfo(i, &row.info) == GX_FALSE) {
            continue;
        }

        rebuild_search_text(row);
        s_rows.push_back(std::move(row));
    }

    refresh_loaded_matches();
    s_status = fmt::format(
        "Indexed {} replacement texture{}; observed {} loaded J3D texture{}.",
        s_rows.size(),
        s_rows.size() == 1 ? "" : "s",
        s_loadedTextures.size(),
        s_loadedTextures.size() == 1 ? "" : "s");
    s_needsRefresh = false;
}

bool row_matches_filter(const ReplacementRow& row) {
    if (s_loadedOnly && row.loadedUseCount == 0) {
        return false;
    }

    if (s_filter[0] == '\0') {
        return true;
    }

    const std::string needle = lower_ascii(s_filter.data());
    return row.searchText.find(needle) != std::string::npos;
}

bool load_replacement_info(ReplacementRow& row) {
    if (row.loadedReplacementInfo) {
        return row.info.has_replacement_info;
    }

    if (AuroraLoadTextureReplacementEntryInfo(row.index, &row.info) == GX_FALSE) {
        row.failedReplacementInfo = true;
        s_status = fmt::format("Failed to load replacement metadata for {}.", row.name);
        return false;
    }

    row.loadedReplacementInfo = true;
    row.failedReplacementInfo = !row.info.has_replacement_info;
    rebuild_search_text(row);

    if (row.info.has_replacement_info) {
        s_status = fmt::format("Loaded replacement metadata for {}.", row.name);
        return true;
    }

    s_status = fmt::format("Replacement metadata unavailable for {}.", row.name);
    return false;
}

std::vector<ReplacementRow*> filtered_rows() {
    std::vector<ReplacementRow*> out;
    out.reserve(s_rows.size());
    for (ReplacementRow& row : s_rows) {
        if (row_matches_filter(row)) {
            out.push_back(&row);
        }
    }
    return out;
}

ReplacementRow* selected_row() {
    if (s_selectedIndex == UINT32_MAX) {
        return nullptr;
    }

    for (ReplacementRow& row : s_rows) {
        if (row.index == s_selectedIndex) {
            return &row;
        }
    }

    return nullptr;
}

TextureReplacementObservedTexture* first_loaded_texture(const ReplacementRow& row) {
    for (TextureReplacementObservedTexture& texture : s_loadedTextures) {
        if (texture.replacementPath == row.path) {
            return &texture;
        }
    }
    return nullptr;
}

const ImGuiTexturePreview& original_preview_for(const TextureReplacementObservedTexture& texture) {
    ResTIMG* timg = texture.texture != nullptr ? texture.texture->getResTIMG(texture.textureIndex) : nullptr;
    for (OriginalPreviewCache& cache : s_originalPreviewCache) {
        if (cache.texture == texture.texture &&
            cache.textureIndex == texture.textureIndex &&
            cache.timg == timg)
        {
            return cache.preview;
        }
    }

    OriginalPreviewCache cache{
        .texture = texture.texture,
        .textureIndex = texture.textureIndex,
        .timg = timg,
        .preview = makeJ3DTexturePreview(texture.texture, texture.textureIndex),
    };
    s_originalPreviewCache.push_back(std::move(cache));
    return s_originalPreviewCache.back().preview;
}

void draw_toolbar() {
    if (ImGui::Button("Refresh")) {
        aurora_reload_texture_replacements();
        refresh_rows();
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh Loaded")) {
        refresh_loaded_matches();
        s_status = fmt::format(
            "Observed {} loaded J3D texture{}.",
            s_loadedTextures.size(),
            s_loadedTextures.size() == 1 ? "" : "s");
    }
    ImGui::SameLine();
    ImGui::InputTextWithHint("Filter", "replacement, original name, path, size, or format", s_filter.data(), s_filter.size());

    ImGui::SameLine();
    ImGui::Checkbox("Loaded in scene only", &s_loadedOnly);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Filters to replacement files currently used by loaded J3D model textures.");
    }
}

void draw_root_path() {
    ImGui::TextWrapped("Folder: %s", s_rootPath.empty() ? "(not initialized)" : s_rootPath.c_str());
    if (!s_rootPath.empty()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Copy Folder")) {
            ImGui::SetClipboardText(s_rootPath.c_str());
        }
    }

    if (!s_status.empty()) {
        ImGui::TextDisabled("%s", s_status.c_str());
    }
}

void draw_rows_table(const std::vector<ReplacementRow*>& rows, ImVec2 size) {
    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_ScrollY;

    if (!ImGui::BeginTable("texture_replacement_debug_entries", 9, flags, size)) {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Replacement Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Loaded", ImGuiTableColumnFlags_WidthFixed, 72.0f);
    ImGui::TableSetupColumn("Original Names", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Original Size", ImGuiTableColumnFlags_WidthFixed, 96.0f);
    ImGui::TableSetupColumn("Replacement Size", ImGuiTableColumnFlags_WidthFixed, 118.0f);
    ImGui::TableSetupColumn("Format", ImGuiTableColumnFlags_WidthFixed, 72.0f);
    ImGui::TableSetupColumn("Mips", ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Copy", ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableHeadersRow();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            ReplacementRow& row = *rows[static_cast<size_t>(i)];
            const bool isSelected = s_selectedIndex == row.index;

            ImGui::PushID(static_cast<int>(row.index));
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            if (ImGui::Selectable(row.name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                s_selectedIndex = row.index;
                load_replacement_info(row);
            }

            ImGui::TableNextColumn();
            if (row.loadedUseCount > 0) {
                ImGui::Text("%u", row.loadedUseCount);
            } else {
                ImGui::TextUnformatted("-");
            }

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(row.loadedTextureNames.empty() ? "-" : row.loadedTextureNames.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%ux%u", row.info.original_width, row.info.original_height);

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(replacement_size_text(row).c_str());

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(texture_format_name(row.info.original_format));

            ImGui::TableNextColumn();
            if (row.info.has_replacement_info) {
                ImGui::Text("%u", row.info.replacement_mips);
            } else {
                ImGui::TextUnformatted("-");
            }

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(row.path.c_str());

            ImGui::TableNextColumn();
            if (ImGui::SmallButton("Path")) {
                ImGui::SetClipboardText(row.path.c_str());
            }
            ImGui::PopID();
        }
    }

    ImGui::EndTable();
}

void draw_preview_image(const ImGuiTexturePreview& preview) {
    if (!preview.error.empty()) {
        ImGui::TextDisabled("%s", preview.error.c_str());
        return;
    }

    if (preview.texture == 0 || preview.width == 0 || preview.height == 0) {
        ImGui::TextDisabled("No preview available.");
        return;
    }

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float maxWidth = std::min(avail.x, 320.0f);
    const float maxHeight = 180.0f;
    const float scale = std::min(maxWidth / preview.width, maxHeight / preview.height);
    const ImVec2 size(preview.width * scale, preview.height * scale);
    ImGui::Image(preview.texture, size);
    ImGui::TextDisabled("%ux%u", preview.width, preview.height);
}

void draw_selected_details() {
    ReplacementRow* row = selected_row();
    if (row == nullptr) {
        ImGui::TextDisabled("Select a texture replacement to inspect it.");
        return;
    }

    ImGui::TextUnformatted(row->name.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy Replacement Path")) {
        ImGui::SetClipboardText(row->path.c_str());
    }

    TextureReplacementObservedTexture* loadedTexture = first_loaded_texture(*row);
    if (loadedTexture == nullptr) {
        ImGui::TextDisabled("Original preview unavailable because this replacement is not currently loaded by a J3D model.");
        return;
    }

    ImGui::Text("Original: %s", loadedTexture->textureName.empty() ? "(unnamed)" : loadedTexture->textureName.c_str());
    const ImGuiTexturePreview& preview = original_preview_for(*loadedTexture);
    draw_preview_image(preview);
}
} // namespace

void ImGuiMenuTools::ShowTextureReplacementDebug() {
    if (!m_showTextureReplacementDebug) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(1180.0f, 720.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Texture Replacements", &m_showTextureReplacementDebug)) {
        ImGui::End();
        return;
    }

    if (s_needsRefresh) {
        refresh_rows();
    }

    draw_toolbar();
    draw_root_path();
    ImGui::Separator();

    const auto rows = filtered_rows();
    const float previewHeight = 240.0f;
    const float tableHeight = std::max(180.0f, ImGui::GetContentRegionAvail().y - previewHeight);
    draw_rows_table(rows, ImVec2(0.0f, tableHeight));
    ImGui::Separator();
    draw_selected_details();

    ImGui::End();
}
} // namespace dusk
