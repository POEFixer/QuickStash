// Transfer: which backpack items to Ctrl+click and where their slots are.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#pragma once

#include "../Settings.h"
#include "../../../../POEFixer/plugin_sdk/PluginSDK.h"

#include <algorithm>
#include <vector>

namespace QuickStashGame {

struct ClickTarget {
    int slotX = 0;
    int slotY = 0;
};

struct ScreenPoint {
    int x = 0;
    int y = 0;
};

inline float SlotCenterX(const PluginSDK::Inventory& inv, int slotX) {
    return inv.Grid.GridScreenX + (static_cast<float>(slotX) + 0.5f) * inv.Grid.CellSize;
}

inline float SlotCenterY(const PluginSDK::Inventory& inv, int slotY) {
    return inv.Grid.GridScreenY + (static_cast<float>(slotY) + 0.5f) * inv.Grid.CellSize;
}

// An item covering any excluded cell stays in the bag. Sizes are clamped to
// the grid so a garbage width/height can never spin this loop.
inline bool ItemTouchesIgnoredCell(const PluginSDK::InventoryItem& item,
                                   const QuickStashConfig::Settings& settings) {
    const int w = std::clamp(item.Width, 1, QuickStashConfig::kGridCols);
    const int h = std::clamp(item.Height, 1, QuickStashConfig::kGridRows);
    for (int dy = 0; dy < h; ++dy)
        for (int dx = 0; dx < w; ++dx)
            if (settings.IsCellIgnored(item.SlotX + dx, item.SlotY + dy)) return true;
    return false;
}

// Items to Ctrl+click, row by row from the top-left.
inline std::vector<ClickTarget> BuildClickQueue(const PluginSDK::Inventory& inv,
                                                const QuickStashConfig::Settings& settings) {
    std::vector<ClickTarget> queue;
    if (!inv.Grid.Valid) return queue;
    queue.reserve(inv.Items.size());
    for (const auto& item : inv.Items) {
        if (ItemTouchesIgnoredCell(item, settings)) continue;
        queue.push_back(ClickTarget{item.SlotX, item.SlotY});
    }
    std::sort(queue.begin(), queue.end(), [](const ClickTarget& a, const ClickTarget& b) {
        if (a.slotY != b.slotY) return a.slotY < b.slotY;
        return a.slotX < b.slotX;
    });
    return queue;
}

inline int CountTransferable(const PluginSDK::Inventory& inv, const QuickStashConfig::Settings& settings) {
    int n = 0;
    for (const auto& item : inv.Items)
        if (!ItemTouchesIgnoredCell(item, settings)) ++n;
    return n;
}

// Backpack cells not covered by any item.
inline int FreeCells(const PluginSDK::Inventory& inv) {
    bool used[QuickStashConfig::kGridRows][QuickStashConfig::kGridCols] = {};
    for (const auto& item : inv.Items) {
        const int w = std::clamp(item.Width, 1, QuickStashConfig::kGridCols);
        const int h = std::clamp(item.Height, 1, QuickStashConfig::kGridRows);
        for (int dy = 0; dy < h; ++dy)
            for (int dx = 0; dx < w; ++dx) {
                const int x = item.SlotX + dx, y = item.SlotY + dy;
                if (x >= 0 && x < QuickStashConfig::kGridCols && y >= 0 && y < QuickStashConfig::kGridRows)
                    used[y][x] = true;
            }
    }
    int free = 0;
    for (const auto& row : used)
        for (bool u : row) free += u ? 0 : 1;
    return free;
}

} // namespace QuickStashGame
