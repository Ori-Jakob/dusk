#include "ImGuiTextureReplacementDebug.hpp"

#include "aurora/gfx.h"
#include "aurora/texture_replacement_debug.hpp"
#include "imgui.h"

#include "dusk/config.hpp"
#include "dusk/settings.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace dusk::imgui_texture_replacement_debug {
namespace {
using aurora::gfx::texture_replacement::DebugInventoryStats;
using aurora::gfx::texture_replacement::DebugMapSlot;
using aurora::gfx::texture_replacement::DebugMaterialInfo;
using aurora::gfx::texture_replacement::DebugMaterialKind;
using aurora::gfx::texture_replacement::DebugTexturePreview;

struct MapSlotDef {
    DebugMapSlot slot;
    const char* label;
    const char* shortLabel;
};

constexpr std::array<MapSlotDef, static_cast<size_t>(DebugMapSlot::Count)> MapSlots{{
    {DebugMapSlot::Base, "Base", "B"},
    {DebugMapSlot::Rmaos, "RMAOS", "R"},
    {DebugMapSlot::Roughness, "Roughness", "Ro"},
    {DebugMapSlot::Metallic, "Metallic", "M"},
    {DebugMapSlot::Ao, "AO", "AO"},
    {DebugMapSlot::Specular, "Specular", "S"},
    {DebugMapSlot::Normal, "Normal", "N"},
    {DebugMapSlot::Emissive, "Emissive", "E"},
}};

struct WindowState {
    std::vector<DebugMaterialInfo> materials;
    DebugInventoryStats stats;
    std::string selectedId;
    char search[256] = {};
    int typeFilter = 0;
    int mapFilter = 0;
    bool needsRefresh = true;
};

WindowState s_state;

std::string FsPathToString(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return {reinterpret_cast<const char*>(text.c_str())};
}

size_t SlotIndex(DebugMapSlot slot) {
    return static_cast<size_t>(slot);
}

const std::filesystem::path& MapPath(const DebugMaterialInfo& material, DebugMapSlot slot) {
    return material.maps[SlotIndex(slot)];
}

std::string ToLower(std::string_view text) {
    std::string out{text};
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

std::string RelativePath(const std::filesystem::path& path, const DebugInventoryStats& stats) {
    if (path.empty()) {
        return {};
    }
    if (!stats.replacementRoot.empty()) {
        const auto root = FsPathToString(stats.replacementRoot.lexically_normal());
        const auto full = FsPathToString(path.lexically_normal());
        const auto rootLower = ToLower(root);
        const auto fullLower = ToLower(full);
        if (fullLower.size() > rootLower.size() && fullLower.starts_with(rootLower) &&
            (full[root.size()] == '/' || full[root.size()] == '\\'))
        {
            size_t offset = root.size();
            while (offset < full.size() && (full[offset] == '/' || full[offset] == '\\')) {
                ++offset;
            }
            return full.substr(offset);
        }
    }
    return FsPathToString(path);
}

std::string PathKey(const std::filesystem::path& path) {
    return ToLower(FsPathToString(path.lexically_normal()));
}

std::string MaterialId(const DebugMaterialInfo& material) {
    for (const auto& slot : MapSlots) {
        const auto& path = MapPath(material, slot.slot);
        if (!path.empty()) {
            return std::string(material.kind == DebugMaterialKind::Replacement ? "replacement:" :
                                                                                 "named:") +
                   PathKey(path);
        }
    }
    return std::string(
               material.kind == DebugMaterialKind::Replacement ? "replacement:" : "named:") +
           material.name;
}

const char* MaterialKindLabel(DebugMaterialKind kind) {
    switch (kind) {
    case DebugMaterialKind::Replacement:
        return "Replacement";
    case DebugMaterialKind::NamedPbr:
        return "Named PBR";
    }
    return "Unknown";
}

uint32_t PbrMapCount(const DebugMaterialInfo& material) {
    uint32_t count = 0;
    for (const auto& slot : MapSlots) {
        if (slot.slot != DebugMapSlot::Base && !MapPath(material, slot.slot).empty()) {
            ++count;
        }
    }
    return count;
}

uint32_t TotalMapCount(const DebugMaterialInfo& material) {
    uint32_t count = 0;
    for (const auto& slot : MapSlots) {
        if (!MapPath(material, slot.slot).empty()) {
            ++count;
        }
    }
    return count;
}

bool HasLooseMaterialMap(const DebugMaterialInfo& material) {
    return !MapPath(material, DebugMapSlot::Roughness).empty() ||
           !MapPath(material, DebugMapSlot::Metallic).empty() ||
           !MapPath(material, DebugMapSlot::Ao).empty() ||
           !MapPath(material, DebugMapSlot::Specular).empty();
}

bool PassesMapFilter(const DebugMaterialInfo& material) {
    switch (s_state.mapFilter) {
    case 1:
        return PbrMapCount(material) > 0;
    case 2:
        return !MapPath(material, DebugMapSlot::Rmaos).empty();
    case 3:
        return HasLooseMaterialMap(material);
    case 4:
        return !MapPath(material, DebugMapSlot::Normal).empty();
    case 5:
        return !MapPath(material, DebugMapSlot::Emissive).empty();
    case 6:
        return material.kind == DebugMaterialKind::Replacement && PbrMapCount(material) == 0;
    case 7:
        return material.hasMips;
    case 8:
        return material.textureHashWildcard || material.tlutHashWildcard;
    default:
        return true;
    }
}

bool PassesTypeFilter(const DebugMaterialInfo& material) {
    switch (s_state.typeFilter) {
    case 1:
        return material.kind == DebugMaterialKind::Replacement;
    case 2:
        return material.kind == DebugMaterialKind::Replacement && PbrMapCount(material) > 0;
    case 3:
        return material.kind == DebugMaterialKind::NamedPbr;
    case 4:
        return material.cached;
    case 5:
        return material.failed || material.reportedMissing;
    default:
        return true;
    }
}

bool PassesSearch(const DebugMaterialInfo& material, std::string_view searchLower) {
    if (searchLower.empty()) {
        return true;
    }

    std::string blob = ToLower(material.name);
    blob += ' ';
    blob += ToLower(FsPathToString(material.directory));
    blob += ' ';
    blob += MaterialKindLabel(material.kind);
    blob += ' ';
    blob += std::to_string(material.width);
    blob += 'x';
    blob += std::to_string(material.height);
    blob += ' ';
    blob += std::to_string(material.format);
    for (const auto& slot : MapSlots) {
        const auto& path = MapPath(material, slot.slot);
        if (!path.empty()) {
            blob += ' ';
            blob += ToLower(FsPathToString(path.filename()));
            blob += ' ';
            blob += ToLower(FsPathToString(path));
        }
    }
    return blob.find(searchLower) != std::string::npos;
}

std::vector<size_t> BuildFilteredRows() {
    std::vector<size_t> rows;
    const std::string searchLower = ToLower(s_state.search);
    rows.reserve(s_state.materials.size());
    for (size_t i = 0; i < s_state.materials.size(); ++i) {
        const auto& material = s_state.materials[i];
        if (PassesTypeFilter(material) && PassesMapFilter(material) &&
            PassesSearch(material, searchLower))
        {
            rows.push_back(i);
        }
    }
    return rows;
}

std::string FormatBytes(uint64_t bytes) {
    constexpr std::array<const char*, 5> units = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < units.size()) {
        value /= 1024.0;
        ++unit;
    }
    char buffer[64];
    if (unit == 0) {
        std::snprintf(
            buffer, sizeof(buffer), "%llu %s", static_cast<unsigned long long>(bytes), units[unit]);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.2f %s", value, units[unit]);
    }
    return buffer;
}

std::string MapSummary(const DebugMaterialInfo& material) {
    std::string out;
    for (const auto& slot : MapSlots) {
        if (!MapPath(material, slot.slot).empty()) {
            if (!out.empty()) {
                out += ' ';
            }
            out += slot.shortLabel;
        }
    }
    return out.empty() ? "-" : out;
}

std::string SizeSummary(const DebugMaterialInfo& material) {
    if (material.kind == DebugMaterialKind::NamedPbr) {
        return "-";
    }
    return std::to_string(material.width) + "x" + std::to_string(material.height) + " fmt " +
           std::to_string(material.format);
}

std::string CacheSummary(const DebugMaterialInfo& material) {
    if (material.failed) {
        return "failed";
    }
    if (material.cached) {
        return "cached";
    }
    if (material.reportedMissing) {
        return "missed";
    }
    return "-";
}

void RefreshInventory() {
    s_state.materials = aurora::gfx::texture_replacement::debug_collect_materials();
    s_state.stats = aurora::gfx::texture_replacement::debug_inventory_stats();
    s_state.needsRefresh = false;
}

const DebugMaterialInfo* FindSelectedMaterial() {
    if (s_state.selectedId.empty()) {
        return nullptr;
    }
    for (const auto& material : s_state.materials) {
        if (MaterialId(material) == s_state.selectedId) {
            return &material;
        }
    }
    return nullptr;
}

DebugTexturePreview LoadPreview(const std::filesystem::path& path) {
    return aurora::gfx::texture_replacement::debug_load_texture_preview(path);
}

ImTextureID TextureId(const DebugTexturePreview& preview) {
    return preview.textureId;
}

void SortRows(std::vector<size_t>& rows, const ImGuiTableSortSpecs* sortSpecs) {
    if (sortSpecs == nullptr || sortSpecs->SpecsCount == 0) {
        return;
    }

    const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[0];
    const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
    std::stable_sort(rows.begin(), rows.end(), [&](size_t lhsIndex, size_t rhsIndex) {
        const auto& lhs = s_state.materials[lhsIndex];
        const auto& rhs = s_state.materials[rhsIndex];
        int result = 0;
        switch (spec.ColumnUserID) {
        case 1:
            result = std::strcmp(MaterialKindLabel(lhs.kind), MaterialKindLabel(rhs.kind));
            break;
        case 2: {
            const uint64_t lhsPixels = static_cast<uint64_t>(lhs.width) * lhs.height;
            const uint64_t rhsPixels = static_cast<uint64_t>(rhs.width) * rhs.height;
            result = lhsPixels < rhsPixels ? -1 : (lhsPixels > rhsPixels ? 1 : 0);
            break;
        }
        case 3: {
            const uint32_t lhsMaps = TotalMapCount(lhs);
            const uint32_t rhsMaps = TotalMapCount(rhs);
            result = lhsMaps < rhsMaps ? -1 : (lhsMaps > rhsMaps ? 1 : 0);
            break;
        }
        case 4:
            result = std::strcmp(CacheSummary(lhs).c_str(), CacheSummary(rhs).c_str());
            break;
        case 5:
            result = FsPathToString(lhs.directory).compare(FsPathToString(rhs.directory));
            break;
        default:
            result = lhs.name.compare(rhs.name);
            break;
        }
        if (result == 0) {
            result = lhs.name.compare(rhs.name);
        }
        return ascending ? result < 0 : result > 0;
    });
}

void DrawTopBar() {
    ImGui::Text("Indexed: %u replacements, %u PBR-ready, %u named PBR groups",
        s_state.stats.indexedReplacementCount, s_state.stats.pbrReplacementCount,
        s_state.stats.namedPbrMaterialCount);
    ImGui::SameLine();
    ImGui::TextDisabled("Cache: %u entries, %s", s_state.stats.cachedReplacementCount,
        FormatBytes(s_state.stats.cachedReplacementBytes).c_str());

    ImGui::TextWrapped("Root: %s", FsPathToString(s_state.stats.replacementRoot).c_str());

    if (ImGui::Button("Refresh Inventory")) {
        RefreshInventory();
    }
    ImGui::SameLine();
    if (ImGui::Button("Rebuild Aurora Index")) {
        aurora_refresh_texture_replacements();
        aurora::gfx::texture_replacement::debug_clear_preview_cache();
        RefreshInventory();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Preview Cache")) {
        aurora::gfx::texture_replacement::debug_clear_preview_cache();
    }
    ImGui::SameLine();
    bool autoRefresh = dusk::getSettings().backend.textureReplacementAutoRefresh;
    if (ImGui::Checkbox("Auto Refresh", &autoRefresh)) {
        dusk::getSettings().backend.textureReplacementAutoRefresh.setValue(autoRefresh);
        aurora_set_texture_replacement_auto_refresh(autoRefresh);
        dusk::config::Save();
        s_state.stats.autoRefreshEnabled = autoRefresh;
    }
}

void DrawFilters(size_t visibleCount) {
    ImGui::SetNextItemWidth(std::min(420.0f, ImGui::GetContentRegionAvail().x));
    ImGui::InputTextWithHint("##TextureReplacementSearch",
        "Search textures, folders, hashes, formats", s_state.search, sizeof(s_state.search));
    ImGui::SameLine();
    ImGui::TextDisabled("%zu / %zu", visibleCount, s_state.materials.size());

    constexpr std::array<const char*, 6> typeFilters = {
        "All", "Replacements", "PBR replacements", "Named PBR", "Cached", "Issues"};
    ImGui::SetNextItemWidth(170.0f);
    ImGui::Combo(
        "Type", &s_state.typeFilter, typeFilters.data(), static_cast<int>(typeFilters.size()));
    ImGui::SameLine();

    constexpr std::array<const char*, 9> mapFilters = {"Any map", "Has PBR", "RMAOS", "Loose maps",
        "Normal", "Emissive", "No PBR sidecars", "Mip chain", "Wildcard hash"};
    ImGui::SetNextItemWidth(170.0f);
    ImGui::Combo("Map", &s_state.mapFilter, mapFilters.data(), static_cast<int>(mapFilters.size()));
}

void DrawMaterialTable(std::vector<size_t>& rows) {
    const ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                       ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                                       ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY |
                                       ImGuiTableFlags_SizingStretchProp;
    const float tableHeight = std::max(220.0f, ImGui::GetContentRegionAvail().y * 0.42f);
    if (!ImGui::BeginTable("TextureReplacementDebugTable", 6, tableFlags, ImVec2(0, tableHeight))) {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0.35f, 0);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 105.0f, 1);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 120.0f, 2);
    ImGui::TableSetupColumn("Maps", ImGuiTableColumnFlags_WidthFixed, 120.0f, 3);
    ImGui::TableSetupColumn("Cache", ImGuiTableColumnFlags_WidthFixed, 80.0f, 4);
    ImGui::TableSetupColumn("Directory", ImGuiTableColumnFlags_WidthStretch, 0.35f, 5);
    ImGui::TableHeadersRow();

    SortRows(rows, ImGui::TableGetSortSpecs());

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const auto& material = s_state.materials[rows[row]];
            const std::string id = MaterialId(material);
            const bool selected = id == s_state.selectedId;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(id.c_str());
            if (ImGui::Selectable(
                    material.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
            {
                s_state.selectedId = id;
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(MaterialKindLabel(material.kind));
            ImGui::TableSetColumnIndex(2);
            const std::string size = SizeSummary(material);
            ImGui::TextUnformatted(size.c_str());
            ImGui::TableSetColumnIndex(3);
            const std::string maps = MapSummary(material);
            ImGui::TextUnformatted(maps.c_str());
            ImGui::TableSetColumnIndex(4);
            const std::string cache = CacheSummary(material);
            ImGui::TextUnformatted(cache.c_str());
            ImGui::TableSetColumnIndex(5);
            const std::string dir = FsPathToString(material.directory);
            ImGui::TextUnformatted(dir.c_str());
        }
    }
    ImGui::EndTable();
}

void DrawSelectedKeyDetails(const DebugMaterialInfo& material) {
    ImGui::Text("%s", material.name.c_str());
    ImGui::TextDisabled(
        "%s in %s", MaterialKindLabel(material.kind), FsPathToString(material.directory).c_str());
    if (material.kind == DebugMaterialKind::Replacement) {
        ImGui::Text("Runtime key: %ux%u format %u%s", material.width, material.height,
            material.format, material.hasTlut ? " TLUT" : "");
        ImGui::Text("Texture hash: %016llx%s",
            static_cast<unsigned long long>(material.textureHash),
            material.textureHashWildcard ? " wildcard" : "");
        if (material.hasTlut) {
            ImGui::Text("TLUT hash:    %016llx%s",
                static_cast<unsigned long long>(material.tlutHash),
                material.tlutHashWildcard ? " wildcard" : "");
        }
        if (material.hasMips) {
            ImGui::SameLine();
            ImGui::TextDisabled("mip chain");
        }
    }

    const auto& primary = MapPath(material, DebugMapSlot::Base).empty() ?
                              MapPath(material, DebugMapSlot::Rmaos) :
                              MapPath(material, DebugMapSlot::Base);
    if (!primary.empty()) {
        if (ImGui::Button("Copy Primary Path")) {
            const std::string path = FsPathToString(primary);
            ImGui::SetClipboardText(path.c_str());
        }
    }
}

void DrawMapMetadataTable(const DebugMaterialInfo& material) {
    DebugTexturePreview basePreview;
    bool hasBasePreview = false;
    const auto& basePath = MapPath(material, DebugMapSlot::Base);
    if (!basePath.empty()) {
        basePreview = LoadPreview(basePath);
        hasBasePreview = true;
    }

    if (!ImGui::BeginTable("TextureReplacementSelectedMaps", 6,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
    {
        return;
    }

    ImGui::TableSetupColumn("Map", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("File");
    ImGui::TableSetupColumn("Dimensions", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("Format", ImGuiTableColumnFlags_WidthFixed, 130.0f);
    ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_WidthFixed, 85.0f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableHeadersRow();

    for (const auto& slot : MapSlots) {
        const auto& path = MapPath(material, slot.slot);
        if (path.empty()) {
            continue;
        }

        const auto preview = LoadPreview(path);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(slot.label);
        ImGui::TableSetColumnIndex(1);
        const std::string rel = RelativePath(path, s_state.stats);
        ImGui::TextUnformatted(rel.c_str());
        ImGui::TableSetColumnIndex(2);
        if (preview.width != 0 && preview.height != 0) {
            ImGui::Text("%ux%u", preview.width, preview.height);
        } else {
            ImGui::TextUnformatted("-");
        }
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted(preview.formatName.empty() ? "-" : preview.formatName.c_str());
        ImGui::TableSetColumnIndex(4);
        ImGui::TextUnformatted(preview.byteSize == 0 ? "-" : FormatBytes(preview.byteSize).c_str());
        ImGui::TableSetColumnIndex(5);
        if (!preview.error.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", preview.error.c_str());
        } else if (hasBasePreview && slot.slot != DebugMapSlot::Base &&
                   (preview.width != basePreview.width || preview.height != basePreview.height))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.25f, 1.0f), "size differs");
        } else {
            ImGui::TextUnformatted("loaded");
        }
    }

    ImGui::EndTable();
}

void DrawPreviewImage(const MapSlotDef& slot, const std::filesystem::path& path) {
    const auto preview = LoadPreview(path);

    ImGui::Text("%s", slot.label);
    ImGui::TextDisabled("%s", RelativePath(path, s_state.stats).c_str());
    if (!preview.error.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", preview.error.c_str());
        return;
    }

    ImGui::TextDisabled("%ux%u, %s, %s", preview.width, preview.height, preview.formatName.c_str(),
        FormatBytes(preview.byteSize).c_str());

    const ImTextureID textureId = TextureId(preview);
    if (textureId == 0 || preview.width == 0 || preview.height == 0) {
        ImGui::TextUnformatted("No preview texture");
        return;
    }

    const float maxWidth = std::max(180.0f, ImGui::GetContentRegionAvail().x);
    constexpr float maxHeight = 240.0f;
    const float scale = std::clamp(std::min(maxWidth / static_cast<float>(preview.width),
                                       maxHeight / static_cast<float>(preview.height)),
        0.15f, 4.0f);
    ImGui::Image(
        textureId, ImVec2(std::floor(preview.width * scale), std::floor(preview.height * scale)));
}

void DrawPreviewGrid(const DebugMaterialInfo& material) {
    if (!ImGui::BeginTable("TextureReplacementPreviewGrid", 2,
            ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame))
    {
        return;
    }

    int column = 0;
    for (const auto& slot : MapSlots) {
        const auto& path = MapPath(material, slot.slot);
        if (path.empty()) {
            continue;
        }
        if (column == 0) {
            ImGui::TableNextRow();
        }
        ImGui::TableSetColumnIndex(column);
        ImGui::PushID(static_cast<int>(slot.slot));
        DrawPreviewImage(slot, path);
        ImGui::PopID();
        column = (column + 1) % 2;
    }

    ImGui::EndTable();
}

void DrawSelectedDetails(const DebugMaterialInfo* material) {
    ImGui::SeparatorText("Selected Material");
    if (material == nullptr) {
        ImGui::TextUnformatted("Select a texture replacement row to load its preview.");
        return;
    }

    DrawSelectedKeyDetails(*material);
    ImGui::SeparatorText("Maps");
    DrawMapMetadataTable(*material);
    ImGui::SeparatorText("Previews");
    DrawPreviewGrid(*material);
}
}  // namespace

void DrawWindow(bool& open) {
    if (!open) {
        return;
    }

    if (s_state.needsRefresh) {
        RefreshInventory();
    }

    ImGui::SetNextWindowSize(ImVec2(1180.0f, 720.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Texture Replacement PBR Debug", &open)) {
        ImGui::End();
        return;
    }

    DrawTopBar();
    ImGui::Separator();

    auto rows = BuildFilteredRows();
    if (!s_state.selectedId.empty() && FindSelectedMaterial() == nullptr) {
        s_state.selectedId.clear();
    }
    if (s_state.selectedId.empty() && !rows.empty()) {
        s_state.selectedId = MaterialId(s_state.materials[rows.front()]);
    }

    DrawFilters(rows.size());
    DrawMaterialTable(rows);
    DrawSelectedDetails(FindSelectedMaterial());

    ImGui::End();
}

}  // namespace dusk::imgui_texture_replacement_debug
