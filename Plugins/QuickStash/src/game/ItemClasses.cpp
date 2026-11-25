// The item class of a stash item, from the generated table.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "ItemClasses.h"

#include "ItemClasses.generated.h"

#include <algorithm>
#include <cstring>

namespace QuickStashGame {

namespace {

// The table's key form of a path: ASCII lower case, each non-ASCII UTF-16 unit '?' (the host lists
// paths that way, GameData::WideToNarrow). A UTF-8 path gets the same key: a lead byte below 0xF0
// is one UTF-16 unit, one from 0xF0 on two; continuation bytes add nothing.
std::string KeyOf(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (const char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c < 0x80) out += (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : ch;
        else if (c >= 0xF0) out += "??";
        else if (c >= 0xC0) out += '?';
    }
    return out;
}

// "<label>: <name>" in one table column, as the game's copy text writes it; "" when the class has
// no name there.
std::string ClassLine(const ItemClassText& cls, size_t column) {
    const char* name = cls.name[column];
    const char* label = ItemClassData::kTexts[column].itemClass;
    if (!name || !*name || !label || !*label) return {};
    std::string line(label);
    line += ": ";
    line += name;
    return line;
}

} // namespace

const ItemClassText* FindItemClass(std::string_view metadataPath) {
    const size_t at = metadataPath.find('@');
    if (at != std::string_view::npos) metadataPath = metadataPath.substr(0, at);
    const size_t slash = metadataPath.rfind('/');
    if (slash == std::string_view::npos || slash == 0 || slash + 1 >= metadataPath.size()) return nullptr;
    const std::string folder = KeyOf(metadataPath.substr(0, slash));
    const std::string leaf = KeyOf(metadataPath.substr(slash + 1));

    const auto& folders = ItemClassData::kFolders;
    const auto f = std::lower_bound(folders.begin(), folders.end(), folder,
        [](const ItemClassData::Folder& a, const std::string& b) { return std::strcmp(a.path, b.c_str()) < 0; });
    if (f == folders.end() || folder != f->path) return nullptr;

    const auto* first = ItemClassData::kBases.data() + f->first;
    const auto* last = first + f->count;
    const auto* b = std::lower_bound(first, last, leaf,
        [](const ItemClassData::Base& a, const std::string& l) { return std::strcmp(a.leaf, l.c_str()) < 0; });
    if (b == last || leaf != b->leaf) return nullptr;
    if (b->itemClass >= ItemClassData::kClasses.size()) return nullptr;
    return &ItemClassData::kClasses[b->itemClass];
}

int ItemClassLanguageColumn(const char* hostName) {
    if (!hostName || !*hostName) return -1;
    for (int i = 0; i < ItemClassData::kLanguageCount; ++i)
        if (std::strcmp(ItemClassData::kLanguages[static_cast<size_t>(i)], hostName) == 0) return i;
    return -1;
}

void AppendItemClass(std::string& s, const ItemClassText& cls, int languageColumn) {
    const std::string english = ClassLine(cls, 0);
    if (!english.empty()) {
        s += '\n';
        s += english;
    }
    if (languageColumn > 0 && languageColumn < ItemClassData::kLanguageCount) {
        const std::string local = ClassLine(cls, static_cast<size_t>(languageColumn));
        if (!local.empty() && local != english) {
            s += '\n';
            s += local;
        }
    }
}

const char* ItemClassId(const ItemClassText& cls) {
    return cls.id;
}

} // namespace QuickStashGame
