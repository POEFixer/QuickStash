// The item class line of a stash item ("Item Class: Helmets" for an Ancestral Tiara), for TAKE's
// search: the game's "Highlight Items" box matches the item's Ctrl+C copy text, which starts with
// that line, and a base type name often does not carry the class.
// The table is the game's own (ItemClasses.generated.h, tools/quick_stash_item_classes/gen.py).
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#pragma once

#include <string>
#include <string_view>

namespace QuickStashGame {

// One row of the generated table (ItemClasses.generated.h): the class Id and its names.
namespace ItemClassData { struct ItemClass; }
using ItemClassText = ItemClassData::ItemClass;

// The class of a base type by the item's metadata path (its BaseItemTypes Id; anything from the
// first '@' on is ignored, ASCII case does not matter, and a non-ASCII character may come as UTF-8
// or as the host listing's '?'); nullptr for a path the table does not know (a base type newer
// than the plugin's table, or one whose class the game does not name).
const ItemClassText* FindItemClass(std::string_view metadataPath);

// The table column of a game language named as GameLanguageHostName names it ("Russian",
// "Traditional Chinese"); -1 for nullptr, "" and a language the game ships no tables for
// (Simplified Chinese).
int ItemClassLanguageColumn(const char* hostName);

// Appends the copy text's class line as a '\n'-led line, "<label>: <name>" ("Item Class: Helmets"),
// in English, then in the table column `languageColumn` when it is another one and the class has a
// name there (-1 or 0: English only). A language without a name for the class adds nothing.
void AppendItemClass(std::string& s, const ItemClassText& cls, int languageColumn);

// The item class Id ("Helmet"; diagnostics).
const char* ItemClassId(const ItemClassText& cls);

} // namespace QuickStashGame
