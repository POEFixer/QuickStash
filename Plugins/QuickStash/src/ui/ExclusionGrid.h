// The 12x5 backpack map in the settings: click or drag to choose the cells
// whose items Transfer leaves in the bag.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#pragma once

#include "../Settings.h"
#include <imgui.h>
#include "../../../../POEFixer/plugin_sdk/PluginUi.h"

#include <algorithm>
#include <cmath>

namespace QuickStashUi {

inline constexpr ImU32 kCellKept     = IM_COL32(178, 64, 56, 230);
inline constexpr ImU32 kCellMoved    = IM_COL32(64, 132, 76, 200);

struct ExclusionGridState {
    int paintValue = -1;  // -1: not dragging; 0/1: value painted while the mouse is held
};

inline void LegendSwatch(const char* label, ImU32 color) {
    const float sz = std::floor(ImGui::GetFontSize() * 0.8f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float dy = std::floor((ImGui::GetTextLineHeight() - sz) * 0.5f);
    const ImVec2 p0(std::floor(p.x), std::floor(p.y + dy));
    const ImVec2 p1(p0.x + sz, p0.y + sz);
    dl->AddRectFilled(p0, p1, color, 2.0f);
    dl->AddRect(p0, p1, ImGui::GetColorU32(ImGuiCol_Border), 2.0f);
    ImGui::Dummy(ImVec2(sz, ImGui::GetTextLineHeight()));
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
}

// Returns true when a cell changed.
inline bool DrawExclusionGrid(QuickStashConfig::Settings& settings, ExclusionGridState& state) {
    using QuickStashConfig::kGridCols;
    using QuickStashConfig::kGridRows;
    bool changed = false;

    const float avail = ImGui::GetContentRegionAvail().x;
    const float gap = std::max(1.0f, std::floor(ImGui::GetFontSize() * 0.12f));
    const float maxCell = std::floor(ImGui::GetFrameHeight() * 1.2f);
    const float cell = std::max(8.0f, std::min(maxCell, std::floor((avail - gap * (kGridCols - 1)) / kGridCols)));
    const float gridW = cell * kGridCols + gap * (kGridCols - 1);
    const float gridH = cell * kGridRows + gap * (kGridRows - 1);

    const ImVec2 origin(std::floor(ImGui::GetCursorScreenPos().x), std::floor(ImGui::GetCursorScreenPos().y));
    ImGui::InvisibleButton("##qsExclusionGrid", ImVec2(gridW, gridH));
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    PluginUi::Tip(PluginSDK::I18n::Tr("transfer_grid_tip"));

    int hx = -1, hy = -1;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    if (hovered || active) {
        const float fx = (mouse.x - origin.x) / (cell + gap);
        const float fy = (mouse.y - origin.y) / (cell + gap);
        if (fx >= 0.0f && fy >= 0.0f) {
            hx = static_cast<int>(fx);
            hy = static_cast<int>(fy);
            if (hx >= kGridCols || hy >= kGridRows) hx = hy = -1;
        }
    }
    if (ImGui::IsItemActivated() && hx >= 0) {
        state.paintValue = settings.ignoredCells[static_cast<size_t>(hy)][static_cast<size_t>(hx)] ? 0 : 1;
    }
    if (!active) state.paintValue = -1;
    if (active && state.paintValue >= 0 && hx >= 0) {
        bool& c = settings.ignoredCells[static_cast<size_t>(hy)][static_cast<size_t>(hx)];
        const bool want = state.paintValue == 1;
        if (c != want) {
            c = want;
            changed = true;
        }
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32 hoverCol = ImGui::GetColorU32(ImGuiCol_Text, 0.85f);
    for (int y = 0; y < kGridRows; ++y) {
        for (int x = 0; x < kGridCols; ++x) {
            const ImVec2 p0(origin.x + x * (cell + gap), origin.y + y * (cell + gap));
            const ImVec2 p1(p0.x + cell, p0.y + cell);
            const bool kept = settings.ignoredCells[static_cast<size_t>(y)][static_cast<size_t>(x)];
            dl->AddRectFilled(p0, p1, kept ? kCellKept : kCellMoved, 2.0f);
            const bool isHover = x == hx && y == hy;
            // 1 px frame inside the cell (ImGui 1.93 strokes closed shapes inside).
            dl->AddRect(p0, p1, isHover ? hoverCol : border, 2.0f, 1.0f);
        }
    }
    return changed;
}

} // namespace QuickStashUi
