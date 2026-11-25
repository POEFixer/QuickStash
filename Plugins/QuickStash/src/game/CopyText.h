// The lines of an item's Ctrl+C copy text that TAKE's own search can rebuild from what the plugin
// reads (Rarity, Item Level, Waystone Tier, the level requirement, Unidentified), in the game's own
// words, and the game's search filters ("ilvl:80", "tier:15"). Used when the game's own highlight
// result cannot be read; the game's result covers everything else (properties, attributes, ...).
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#pragma once

#include "TextMatch.h"

#include <string>
#include <string_view>

namespace QuickStashGame {

// What the plugin knows about one item for these lines.
struct ItemFacts {
    std::string classId;     // ItemClasses Id ("Helmet"); "" when the class table does not know the base
    bool modsRead = false;   // the host answered the item's mods read (the fields below are set)
    int  rarity = 0;         // Mods rarity: 0 Normal, 1 Magic, 2 Rare, 3 Unique
    int  itemLevel = 0;      // 0: the item has no Mods component (currency and the like)
    int  requiredLevel = 0;
    bool identified = true;
    int  waystoneTier = 0;   // from the metadata path ("MapKeyTier15"); 0 = none
};

// Item classes that carry no Mods component (stackable currency, omens, augments, uncut gems, ...):
// the game's copy text calls their rarity "Currency", and reading their mods only costs the host a
// failed lookup.
bool ClassWithoutMods(const std::string& classId);

// False when the class alone tells every line AppendFactLines can write (gems, divination cards,
// quest items, fragments, breachstones, sentinels, ClassWithoutMods): no mods read is needed.
bool FactsNeedModsRead(const std::string& classId);

// "Metadata/Items/Maps/MapKeyTier15" -> 15; 0 when the path names no tier.
int ParseWaystoneTier(std::string_view metadataPath);

// Appends, as '\n'-led lines, what the game's copy text says for these facts: "Rarity: Rare",
// "Item Level: 82", "Waystone Tier: 15", "Requires: Level 80", "Unidentified", in English, then in
// the table column `languageColumn` when it is another one (-1 or 0: English only). The rarity word
// follows the game's frame type order: gem classes "Gem", divination cards "Divination Card", quest
// items "Quest", map fragments, breachstones and sentinels "Normal", ClassWithoutMods "Currency",
// else the Mods rarity of an item whose mods were read with an item level (no word otherwise).
void AppendFactLines(std::string& s, const ItemFacts& facts, int languageColumn);

// The search filters' words in English plus the table column `languageColumn` (-1 or 0: English).
SearchFilters MakeSearchFilters(int languageColumn);

} // namespace QuickStashGame
