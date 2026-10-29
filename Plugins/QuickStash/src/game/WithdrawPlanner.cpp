// TAKE: item search text, matching, and click targets.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "WithdrawPlanner.h"

#include "PanelDetector.h"

#include <algorithm>
#include <cstring>

namespace QuickStashGame {

const char* AggregatedLabel(int key) {
    switch (key) {
        case kAggItemRarity:           return "Item Rarity";
        case kAggPackSize:             return "Monster Pack Size";
        case kAggMonsterRarity:        return "Monster Rarity";
        case kAggMonsterEffectiveness: return "Monster Effectiveness";
        case kAggWaystoneDropChance:   return "Waystone Drop Chance";
        default:                       return nullptr;
    }
}

ModLanguage ResolveModLanguage(const PluginSDK::Context* ctx, int setting, GameLanguage labelLanguage) {
    GameLanguage host = GameLanguage::Auto;
    if (ctx) {
        const PluginSDK::GameLanguageInfo info = ctx->Lang.GameLanguage();
        if (info.known) host = GameLanguageFromCsdName(info.csdName);
    }
    const GameLanguage effective = EffectiveGameLanguage(setting, host, labelLanguage);
    ModLanguage language;
    language.hostName = GameLanguageHostName(effective);
    if (ctx && language.Localized())
        language.corrupted = ctx->Lang.GameString("ItemPopupCorrupted", GameLanguageGameCode(effective));
    return language;
}

std::string BuildNameSearchText(const PluginSDK::InventoryItem& item) {
    std::string s;
    s.reserve(item.BaseTypeName.size() + item.UniqueName.size() + 1);
    s += item.BaseTypeName;
    s += '\n';
    s += item.UniqueName;
    return s;
}

std::string BuildDirectNameText(const PluginSDK::Context* ctx, const PluginSDK::InventoryItem& item) {
    std::string s;
    if (!ctx || !item.Address) return s;
    const std::string base = ctx->Inventory.ReadItemBaseTypeName(item.Address);
    if (base.empty()) return s;  // the item is gone (moved or freed)
    if (base != item.BaseTypeName) {
        s += '\n';
        s += base;
    }
    const std::string unique = ctx->Inventory.ReadItemUniqueName(item.Address);
    if (!unique.empty() && unique != item.UniqueName) {
        s += '\n';
        s += unique;
    }
    return s;
}

ModTextResult BuildModText(const PluginSDK::Context* ctx, const PluginSDK::InventoryItem& item,
                           const ModLanguage& language) {
    ModTextResult r;
    if (!ctx || !item.Address) return r;
    // Liveness check first: an item that moved since the listing reads empty.
    if (ctx->Inventory.ReadItemBaseTypeName(item.Address).empty()) return r;

    const bool localized = language.Localized();
    const auto mods = ctx->Inventory.ReadItemMods(item.Address);
    std::string& s = r.text;
    if (mods.Valid && mods.IsCorrupted) AppendCorrupted(s, language);
    const std::vector<PluginSDK::Mod>* groups[] = {
        &mods.ImplicitMods, &mods.ExplicitMods, &mods.EnchantMods, &mods.HellscapeMods, &mods.CrucibleMods};
    for (const auto* g : groups) {
        for (const auto& m : *g) {
            // Mod.Name is a stat id (not display text) and stays out: it made
            // short searches match items the game does not highlight.
            if (!m.AffixName.empty()) { s += '\n'; s += m.AffixName; }
            if (m.StatKey.empty()) continue;
            const std::string line = ctx->Inventory.FormatStat(m.StatKey, m.Value0, m.Value1);
            if (!line.empty()) { s += '\n'; s += line; }
            if (localized) {
                bool pending = false;
                const std::string loc = ctx->Inventory.FormatStatLocalized(m.StatKey, m.Value0, m.Value1,
                                                                           language.hostName, &pending);
                if (pending) r.pending = true;
                if (!loc.empty() && loc != line) { s += '\n'; s += loc; }
            }
        }
    }
    for (const auto& kv : ctx->Inventory.ReadItemAggregatedStats(item.Address)) {
        if (kv.second == 0) continue;
        if (const char* lbl = AggregatedLabel(kv.first)) {
            s += '\n';
            s += lbl;
            // In a non-English game, also the line the game itself shows for the stat.
            const char* statId = AggregatedStatId(kv.first);
            if (localized && statId) {
                bool pending = false;
                const std::string loc = ctx->Inventory.FormatStatLocalized(
                    statId, static_cast<float>(kv.second), 0.0f, language.hostName, &pending);
                if (pending) r.pending = true;
                if (!loc.empty()) { s += '\n'; s += loc; }
            }
        }
    }
    return r;
}

void ItemTextCache::BeginPass() {
    m_nameBudget = kMaxNameReadsPerPass;
    m_modBudget = kMaxModReadsPerPass;
    if (m_map.size() > kItemTextCacheMax) m_map.clear();
}

const std::string& ItemTextCache::Text(const PluginSDK::Context* ctx, const PluginSDK::InventoryItem& item,
                                       bool readMods, const ModLanguage& language, bool& complete) {
    Entry& e = m_map[item.Address];
    if (e.path != item.Path) {
        e = Entry{};
        e.path = item.Path;
    }
    const std::string source = BuildNameSearchText(item);
    if (e.listSource != source) {
        e.listSource = source;
        e.listFolded = FoldCase(source);
        e.combinedValid = false;
    }
    complete = true;
    if (!e.namesRead) {
        if (m_nameBudget > 0) {
            --m_nameBudget;
            e.namesFolded = FoldCase(BuildDirectNameText(ctx, item));
            e.namesRead = true;
            e.combinedValid = false;
        } else {
            complete = false;
        }
    }
    if (readMods && !e.modsRead) {
        if (m_modBudget > 0) {
            --m_modBudget;
            ModTextResult mods = BuildModText(ctx, item, language);
            e.modsFolded = FoldCase(mods.text);
            e.modsRead = !mods.pending;  // localized text still loading: read again next pass
            if (mods.pending) complete = false;
            e.combinedValid = false;
        } else {
            complete = false;
        }
    }
    const bool withMods = readMods && !e.modsFolded.empty();
    if (!e.combinedValid || e.combinedMods != withMods) {
        e.combined = e.listFolded;
        e.combined += e.namesFolded;
        if (withMods) e.combined += e.modsFolded;
        e.combinedMods = withMods;
        e.combinedValid = true;
    }
    return e.combined;
}

namespace {

bool GridLayoutPlausible(const PluginSDK::Inventory& inv, float displayW) {
    if (inv.TotalBoxesY < 6) return false;
    if (inv.Grid.CellSize > 0.0f && static_cast<float>(inv.TotalBoxesX) * inv.Grid.CellSize > displayW)
        return false;
    return true;
}

struct Hit {
    int slotY = 0;
    int slotX = 0;
    ScreenRect rect;
};

} // namespace

std::optional<ScreenRect> ResolveItemRect(const PluginSDK::Inventory& inv, const PluginSDK::InventoryItem& item,
                                          float displayW, float displayH) {
    if (ItemOnScreen(item, displayW, displayH))
        return ScreenRect{item.ScreenX, item.ScreenY, item.ScreenW, item.ScreenH};
    if (inv.Grid.Valid && GridOnScreen(inv, displayW, displayH) && inv.Grid.CellSize > 0.0f &&
        GridLayoutPlausible(inv, displayW)) {
        const float cell = inv.Grid.CellSize;
        const int w = std::max(item.Width, 1);
        const int h = std::max(item.Height, 1);
        ScreenRect r{inv.Grid.GridScreenX + static_cast<float>(item.SlotX) * cell,
                     inv.Grid.GridScreenY + static_cast<float>(item.SlotY) * cell,
                     static_cast<float>(w) * cell, static_cast<float>(h) * cell};
        const float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
        if (cx < 0.0f || cy < 0.0f || cx >= displayW || cy >= displayH) return std::nullopt;
        return r;
    }
    return std::nullopt;
}

WithdrawSelection SelectWithdrawTargets(const PluginSDK::Context* ctx, const PluginSDK::Inventory& inv,
                                        float displayW, float displayH, ItemTextCache& cache,
                                        const SearchQuery& query, bool readMods, const ModLanguage& language) {
    WithdrawSelection sel;
    cache.BeginPass();
    std::vector<Hit> hits;
    hits.reserve(inv.Items.size());
    int qty = 0;
    for (const auto& item : inv.Items) {
        const auto rect = ResolveItemRect(inv, item, displayW, displayH);
        if (!rect) continue;
        if (!query.Empty()) {
            bool complete = true;
            const std::string& text = cache.Text(ctx, item, readMods, language, complete);
            if (!complete) sel.pending = true;
            if (!query.Matches(text)) continue;
        }
        hits.push_back(Hit{item.SlotY, item.SlotX, *rect});
        qty += item.StackCount > 0 ? item.StackCount : 1;
    }
    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
        if (a.slotY != b.slotY) return a.slotY < b.slotY;
        return a.slotX < b.slotX;
    });
    sel.rects.reserve(hits.size());
    for (const auto& h : hits) sel.rects.push_back(h.rect);
    sel.totalQty = qty;
    return sel;
}

} // namespace QuickStashGame
