// Finds the backpack and the open storage tab in one inventory listing.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#pragma once

#include "../Settings.h"
#include "../../../../POEFixer/plugin_sdk/PluginSDK.h"

#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace QuickStashGame {

using PluginSDK::IsGuildStashInventory;

inline bool StartsWith(const char* name, const char* prefix) {
    return name && std::strncmp(name, prefix, std::strlen(prefix)) == 0;
}

// The player's own 12x5 backpack. Prefers the known names, then any visible
// 12x5 grid that is not the guild stash.
inline const PluginSDK::Inventory* FindMainInventory(const PluginSDK::Context* ctx,
                                                     const std::vector<PluginSDK::Inventory>& all) {
    if (!ctx) return nullptr;
    static const char* const kNames[] = {"MainInventory1", "Main Inventory", "Backpack", "Player Inventory"};
    for (const char* want : kNames) {
        for (const auto& inv : all) {
            const char* name = ctx->Inventory.GetName(inv.InventoryId);
            if (name && std::strcmp(name, want) == 0) return &inv;
        }
    }
    for (const auto& inv : all) {
        if (!inv.Grid.Valid || IsGuildStashInventory(inv)) continue;
        if (inv.TotalBoxesX == QuickStashConfig::kGridCols && inv.TotalBoxesY == QuickStashConfig::kGridRows)
            return &inv;
    }
    return nullptr;
}

// Equipment, flask/charm belt, cursor: never a storage target.
inline bool IsPlayerSlotName(const char* name) {
    if (!name) return true;
    static const char* const kSlotPrefixes[] = {
        "MainInventory", "BodyArmour", "Weapon", "Offhand", "Helm", "Amulet",
        "Ring", "Gloves", "Boots", "Belt", "Flask", "Cursor", "Trinket", "Charm"};
    for (const char* p : kSlotPrefixes)
        if (StartsWith(name, p)) return true;
    return false;
}

// Windows that hold items but are not a stash (the Ritual "Favours" shop).
inline bool IsNonStashWindowName(const char* name) {
    return StartsWith(name, "Ritual");
}

inline bool GridOnScreen(const PluginSDK::Inventory& inv, float displayW, float displayH) {
    if (!inv.Grid.Valid || inv.Grid.CellSize <= 0.0f) return false;
    if (displayW <= 0.0f || displayH <= 0.0f) return false;
    const float x = inv.Grid.GridScreenX;
    const float y = inv.Grid.GridScreenY;
    const float margin = 4.0f;
    return x >= -margin && y >= -margin && x < displayW && y < displayH;
}

inline bool ItemOnScreen(const PluginSDK::InventoryItem& item, float displayW, float displayH) {
    if (!item.ScreenValid) return false;
    if (item.ScreenW <= 0.0f || item.ScreenH <= 0.0f) return false;
    if (displayW <= 0.0f || displayH <= 0.0f) return false;
    const float cx = item.ScreenX + item.ScreenW * 0.5f;
    const float cy = item.ScreenY + item.ScreenH * 0.5f;
    return cx >= 0.0f && cy >= 0.0f && cx < displayW && cy < displayH;
}

inline bool TabOnScreen(const PluginSDK::Inventory& inv, float displayW, float displayH) {
    if (GridOnScreen(inv, displayW, displayH)) return true;
    for (const auto& it : inv.Items)
        if (ItemOnScreen(it, displayW, displayH)) return true;
    return false;
}

// The open storage tab TAKE reads from: the on-screen, non-player inventory
// with the most items (guild stash included). Empty tabs are skipped (nothing
// to take).
inline const PluginSDK::Inventory* FindOpenStashAny(const PluginSDK::Context* ctx,
                                                    const std::vector<PluginSDK::Inventory>& all,
                                                    int mainInventoryId, float displayW, float displayH) {
    if (!ctx) return nullptr;
    const PluginSDK::Inventory* best = nullptr;
    for (const auto& inv : all) {
        if (inv.InventoryId == mainInventoryId) continue;
        const bool guild = IsGuildStashInventory(inv);
        if (!guild) {
            const char* name = ctx->Inventory.GetName(inv.InventoryId);
            if (IsPlayerSlotName(name) || IsNonStashWindowName(name)) continue;
        }
        const long long area = static_cast<long long>(inv.TotalBoxesX) * inv.TotalBoxesY;
        if (area < 10 && !guild) continue;
        if (inv.Items.empty()) continue;
        if (!TabOnScreen(inv, displayW, displayH)) continue;
        if (!best || inv.Items.size() > best->Items.size()) best = &inv;
    }
    return best;
}

// Grid-only detection (diagnostics): the on-screen non-player grid with the
// most items, ties broken by the larger grid.
inline const PluginSDK::Inventory* FindOpenStashGrid(const PluginSDK::Context* ctx,
                                                     const std::vector<PluginSDK::Inventory>& all,
                                                     int mainInventoryId, float displayW, float displayH) {
    if (!ctx) return nullptr;
    const PluginSDK::Inventory* best = nullptr;
    for (const auto& inv : all) {
        if (inv.InventoryId == mainInventoryId) continue;
        if (!IsGuildStashInventory(inv) && IsPlayerSlotName(ctx->Inventory.GetName(inv.InventoryId))) continue;
        if (!GridOnScreen(inv, displayW, displayH)) continue;
        if (!best) { best = &inv; continue; }
        const long long invArea = static_cast<long long>(inv.TotalBoxesX) * inv.TotalBoxesY;
        const long long bestArea = static_cast<long long>(best->TotalBoxesX) * best->TotalBoxesY;
        if (inv.Items.size() > best->Items.size() || (inv.Items.size() == best->Items.size() && invArea > bestArea))
            best = &inv;
    }
    return best;
}

} // namespace QuickStashGame
