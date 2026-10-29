// TAKE: which items of the open tab match the search, and where they are.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#pragma once

#include "Languages.h"
#include "TextMatch.h"
#include "TransferPlanner.h"
#include "../../../../POEFixer/plugin_sdk/PluginSDK.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace QuickStashGame {

struct ScreenRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

inline ScreenPoint RectCenter(const ScreenRect& r) {
    return ScreenPoint{static_cast<int>(r.x + r.w * 0.5f + 0.5f), static_cast<int>(r.y + r.h * 0.5f + 0.5f)};
}

// Per-item reads per pass. The worker repeats the pass sooner while items are
// still unread, so a full tab is covered within a few hundred milliseconds.
inline constexpr int    kMaxNameReadsPerPass = 200;
inline constexpr int    kMaxModReadsPerPass  = 120;
inline constexpr size_t kItemTextCacheMax    = 4000;

// Waystone "final from map" stats that carry no mod line of their own.
inline constexpr int kAggItemRarity           = 8206;
inline constexpr int kAggPackSize             = 8207;
inline constexpr int kAggMonsterRarity        = 8208;
inline constexpr int kAggMonsterEffectiveness = 8209;
inline constexpr int kAggWaystoneDropChance   = 8210;

const char* AggregatedLabel(int key);

// The Stats.dat Id of an aggregated stat (FormatStatLocalized words it in the game language);
// nullptr for any other key.
inline const char* AggregatedStatId(int key) {
    switch (key) {
        case kAggItemRarity:           return "map_item_drop_rarity_+%_final_from_map";
        case kAggPackSize:             return "map_pack_size_+%_final_from_map";
        case kAggMonsterRarity:
            return "map_number_of_magic_and_rare_packs_+%_final_and_rare_monster_modifiers_chance_+%_final_from_map";
        case kAggMonsterEffectiveness: return "map_monster_potency_+%_final_from_map";
        case kAggWaystoneDropChance:   return "map_map_item_drop_chance_+%_final_from_map";
        default:                       return nullptr;
    }
}

// The game language mod text is searchable in besides English.
struct ModLanguage {
    const char* hostName = nullptr;  // FormatStatLocalized's name; nullptr or "English": English only
    std::string corrupted;           // the game's "Corrupted" in that language (ItemPopupCorrupted), "" = none

    bool Localized() const {
        return hostName && *hostName && std::string_view(hostName) != "English";
    }
    bool operator==(const ModLanguage& o) const {
        return std::string_view(hostName ? hostName : "") == std::string_view(o.hostName ? o.hostName : "") &&
               corrupted == o.corrupted;
    }
};

// The language mod text is searchable in besides English: the player's choice (`setting`, a
// GameLanguage), Automatic = the host's game language, else `labelLanguage` (the search label's
// spelling, older hosts); with the game's own "Corrupted" in that language.
ModLanguage ResolveModLanguage(const PluginSDK::Context* ctx, int setting, GameLanguage labelLanguage);

// A "Corrupted" line for the English search, plus the game's own word when it is another one.
inline void AppendCorrupted(std::string& s, const ModLanguage& language) {
    s += "\nCorrupted";
    if (!language.corrupted.empty() && language.corrupted != "Corrupted") {
        s += '\n';
        s += language.corrupted;
    }
}

// Base type and unique name as the inventory listing gives them.
std::string BuildNameSearchText(const PluginSDK::InventoryItem& item);

// Names read straight from the item when they differ from the listing's
// (helps clients whose listing names are not localized).
std::string BuildDirectNameText(const PluginSDK::Context* ctx, const PluginSDK::InventoryItem& item);

struct ModTextResult {
    std::string text;
    bool        pending = false;  // localized text still loading on the host: ask again later
};

// Mod names, affix names, formatted stat lines, "Corrupted" and the waystone reward stats, in
// English plus `language` when it is not English.
ModTextResult BuildModText(const PluginSDK::Context* ctx, const PluginSDK::InventoryItem& item,
                           const ModLanguage& language);

// Searchable text per item, folded for matching, cached by item address and
// metadata path across passes. Owned and used by one thread.
class ItemTextCache {
public:
    void BeginPass();
    void Clear() { m_map.clear(); }
    size_t Size() const { return m_map.size(); }

    // Folded searchable text. `complete` is false while a name or mod read for
    // this item is still waiting for this pass's budget.
    const std::string& Text(const PluginSDK::Context* ctx, const PluginSDK::InventoryItem& item,
                            bool readMods, const ModLanguage& language, bool& complete);

private:
    struct Entry {
        std::string path;
        std::string listSource;  // BaseTypeName + '\n' + UniqueName the fold was built from
        std::string listFolded;
        std::string namesFolded;
        std::string modsFolded;
        std::string combined;
        bool namesRead = false;
        bool modsRead = false;
        bool combinedMods = false;
        bool combinedValid = false;
    };
    std::unordered_map<uintptr_t, Entry> m_map;
    int m_nameBudget = 0;
    int m_modBudget = 0;
};

// Screen rectangle of an item on the open tab: its own rect on special tabs,
// grid math on normal ones. nullopt when it is not on screen.
std::optional<ScreenRect> ResolveItemRect(const PluginSDK::Inventory& inv, const PluginSDK::InventoryItem& item,
                                          float displayW, float displayH);

struct WithdrawSelection {
    std::vector<ScreenRect> rects;   // in click order (row by row)
    int  totalQty = 0;               // stack sizes summed (1 for unstacked items)
    bool pending = false;            // some items' text not read yet: the count may still change
};

WithdrawSelection SelectWithdrawTargets(const PluginSDK::Context* ctx, const PluginSDK::Inventory& inv,
                                        float displayW, float displayH, ItemTextCache& cache,
                                        const SearchQuery& query, bool readMods, const ModLanguage& language);

} // namespace QuickStashGame
