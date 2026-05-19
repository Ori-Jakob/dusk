#include "ImGuiMenuTools.hpp"

#include <aurora/gfx.h>
#include <dolphin/gx/GXTexture.h>
#include <dolphin/gx/GXTextureReplacement.h>

#include "fmt/format.h"
#include "imgui.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace dusk {
namespace {
struct ReplacementRow {
    std::uint32_t index = 0;
    AuroraTextureReplacementEntryInfo info{};
    std::string name;
    std::string path;
    std::string searchText;
};

std::array<char, 256> s_filter{};
std::vector<ReplacementRow> s_rows;
std::string s_rootPath;
std::string s_status;
bool s_loadedOnly = false;
bool s_needsRefresh = true;

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
        return "?";
    }
    return fmt::format("{}x{}", info.replacement_width, info.replacement_height);
}

void rebuild_search_text(ReplacementRow& row) {
    row.searchText = lower_ascii(fmt::format(
        "{} {} {}x{} {}",
        row.name,
        row.path,
        row.info.original_width,
        row.info.original_height,
        texture_format_name(row.info.original_format)));
}

void refresh_rows() {
    s_rootPath = read_aurora_string(AuroraGetTextureReplacementRootPath);

    s_rows.clear();
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

    s_status = fmt::format("Indexed {} replacement texture{}.", s_rows.size(), s_rows.size() == 1 ? "" : "s");
    s_needsRefresh = false;
}

bool row_matches_filter(const ReplacementRow& row) {
    if (s_filter[0] == '\0') {
        return true;
    }

    const std::string needle = lower_ascii(s_filter.data());
    return row.searchText.find(needle) != std::string::npos;
}

std::vector<const ReplacementRow*> filtered_rows() {
    std::vector<const ReplacementRow*> out;
    out.reserve(s_rows.size());
    for (const ReplacementRow& row : s_rows) {
        if (row_matches_filter(row)) {
            out.push_back(&row);
        }
    }
    return out;
}

void draw_toolbar() {
    if (ImGui::Button("Refresh")) {
        aurora_reload_texture_replacements();
        refresh_rows();
    }
    ImGui::SameLine();
    ImGui::InputTextWithHint("Filter", "name, path, size, or format", s_filter.data(), s_filter.size());

    ImGui::BeginDisabled();
    ImGui::SameLine();
    ImGui::Checkbox("Loaded in scene only", &s_loadedOnly);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("This will be enabled once the game-side texture inventory is added.");
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

void draw_rows_table(const std::vector<const ReplacementRow*>& rows) {
    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_ScrollY;

    if (!ImGui::BeginTable("texture_replacement_debug_entries", 7, flags, ImVec2(0.0f, 0.0f))) {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Replacement Name", ImGuiTableColumnFlags_WidthStretch);
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
            const ReplacementRow& row = *rows[static_cast<size_t>(i)];

            ImGui::PushID(static_cast<int>(row.index));
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(row.name.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%ux%u", row.info.original_width, row.info.original_height);

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(replacement_size_text(row.info).c_str());

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(texture_format_name(row.info.original_format));

            ImGui::TableNextColumn();
            ImGui::Text("%u", row.info.has_replacement_info ? row.info.replacement_mips : 0);

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
    draw_rows_table(rows);

    ImGui::End();
}
} // namespace dusk
