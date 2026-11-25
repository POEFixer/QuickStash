// The copy text lines TAKE's own search rebuilds, and the search filters' words.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "CopyText.h"

#include "ItemClasses.generated.h"

#include <array>
#include <vector>

namespace QuickStashGame {
namespace {

using ItemClassData::Texts;

bool ValidColumn(int column) {
    return column >= 0 && column < ItemClassData::kLanguageCount;
}

bool OneOf(const std::string& c, std::initializer_list<const char*> ids) {
    for (const char* id : ids)
        if (c == id) return true;
    return false;
}

bool GemClass(const std::string& c) {
    return OneOf(c, {"Active Skill Gem", "Support Skill Gem", "Meta Skill Gem"});
}

// The rarity word of the game's copy text for these facts; nullptr when it cannot be told.
const char* RarityWord(const Texts& t, const ItemFacts& f) {
    const std::string& c = f.classId;
    if (GemClass(c)) return t.gem;
    if (c == "DivinationCard") return t.divinationCard;
    if (c == "QuestItem") return t.quest;
    if (OneOf(c, {"SentinelDrone", "MapFragment", "Breachstone"})) return t.normal;
    if (ClassWithoutMods(c)) return t.currency;
    // A read that found no item level is no proof of a missing Mods component: no word then.
    if (!f.modsRead || f.itemLevel <= 0) return nullptr;
    switch (f.rarity) {
        case 0: return t.normal;
        case 1: return t.magic;
        case 2: return t.rare;
        case 3: return t.unique;
        default: return nullptr;
    }
}

std::string Labelled(const char* label, const std::string& value) {
    std::string line(label ? label : "");
    line += ": ";
    line += value;
    return line;
}

std::vector<std::string> FactLines(const Texts& t, const ItemFacts& f) {
    std::vector<std::string> lines;
    if (const char* word = RarityWord(t, f)) lines.push_back(Labelled(t.rarity, word));
    if (f.modsRead && f.itemLevel > 0) lines.push_back(Labelled(t.itemLevel, std::to_string(f.itemLevel)));
    if (f.classId == "Map" && f.waystoneTier > 0)
        lines.push_back(Labelled(t.waystoneTier, std::to_string(f.waystoneTier)));
    if (f.modsRead && f.itemLevel > 0 && f.identified && f.requiredLevel > 0) {
        // "Requires: " + "Level" + " " + 80 (the game lists attributes after it; the plugin cannot).
        std::string line(t.requiresLabel);
        line += t.level;
        line += ' ';
        line += std::to_string(f.requiredLevel);
        lines.push_back(std::move(line));
    }
    if (f.modsRead && f.itemLevel > 0 && !f.identified) lines.emplace_back(t.unidentified);
    return lines;
}

std::string TrimSpaces(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && s[a] == ' ') ++a;
    while (b > a && s[b - 1] == ' ') --b;
    return s.substr(a, b - a);
}

// "item level|itemlevel|ilvl" -> its aliases, each trimmed (Traditional Chinese writes "a | b").
std::vector<std::string> SplitAliases(const char* aliases) {
    std::vector<std::string> out;
    std::string part;
    for (const char* p = aliases ? aliases : ""; ; ++p) {
        if (*p == '|' || *p == '\0') {
            part = TrimSpaces(part);
            if (!part.empty()) out.push_back(part);
            part.clear();
            if (*p == '\0') break;
        } else {
            part += *p;
        }
    }
    return out;
}

void AddFilterWords(SearchFilter& filter, const char* aliases, const char* label) {
    for (std::string& a : SplitAliases(aliases)) filter.aliases.push_back(std::move(a));
    if (label && *label) filter.labels.emplace_back(label);
}

} // namespace

bool ClassWithoutMods(const std::string& classId) {
    return OneOf(classId, {"StackableCurrency", "Currency", "SkillGemToken", "Omen", "SoulCore",
                           "UncutSkillGemStackable", "UncutSupportGemStackable", "UncutReservationGemStackable",
                           "IncubatorStackable", "DelveStackableSocketableCurrency", "PinnacleKeyStackable"});
}

bool FactsNeedModsRead(const std::string& classId) {
    if (GemClass(classId) || ClassWithoutMods(classId)) return false;
    return !OneOf(classId, {"DivinationCard", "QuestItem", "SentinelDrone", "MapFragment", "Breachstone"});
}

int ParseWaystoneTier(std::string_view path) {
    std::string lower(path);
    for (char& c : lower)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    size_t pos = lower.find("tier");
    while (pos != std::string::npos) {
        size_t p = pos + 4;
        if (p < lower.size() && lower[p] >= '0' && lower[p] <= '9') {
            int value = 0;
            while (p < lower.size() && lower[p] >= '0' && lower[p] <= '9' && value < 1000) {
                value = value * 10 + (lower[p] - '0');
                ++p;
            }
            return value;
        }
        pos = lower.find("tier", pos + 1);
    }
    return 0;
}

void AppendFactLines(std::string& s, const ItemFacts& facts, int languageColumn) {
    const std::vector<std::string> english = FactLines(ItemClassData::kTexts[0], facts);
    for (const std::string& line : english) {
        s += '\n';
        s += line;
    }
    if (languageColumn <= 0 || !ValidColumn(languageColumn)) return;
    for (const std::string& line : FactLines(ItemClassData::kTexts[static_cast<size_t>(languageColumn)], facts)) {
        bool seen = false;
        for (const std::string& e : english)
            if (e == line) { seen = true; break; }
        if (seen) continue;
        s += '\n';
        s += line;
    }
}

SearchFilters MakeSearchFilters(int languageColumn) {
    SearchFilters f;
    const Texts& en = ItemClassData::kTexts[0];
    AddFilterWords(f.itemLevel, en.filterItemLevel, en.itemLevel);
    AddFilterWords(f.waystoneTier, en.filterWaystoneTier, en.waystoneTier);
    if (languageColumn > 0 && ValidColumn(languageColumn)) {
        const Texts& t = ItemClassData::kTexts[static_cast<size_t>(languageColumn)];
        AddFilterWords(f.itemLevel, t.filterItemLevel, t.itemLevel);
        AddFilterWords(f.waystoneTier, t.filterWaystoneTier, t.waystoneTier);
    }
    return f;
}

} // namespace QuickStashGame
