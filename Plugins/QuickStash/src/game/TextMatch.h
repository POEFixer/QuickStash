// Item search for TAKE: Unicode case folding and the game's "Highlight Items"
// query rules (space-separated terms that must all match, "quoted phrases",
// a leading ! to exclude, a|b alternatives, and regular expressions).
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#pragma once

#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace QuickStashGame {

// Lower-cases UTF-8 text for comparison in any client language (Cyrillic,
// Latin with accents, Greek, ...). Text that is not valid UTF-8 is lower-cased
// as ASCII only.
std::string FoldCase(std::string_view utf8);

// True when the needle (already folded) occurs in the folded haystack.
inline bool ContainsFolded(const std::string& foldedHay, const std::string& foldedNeedle) {
    return foldedNeedle.empty() || foldedHay.find(foldedNeedle) != std::string::npos;
}

class SearchQuery {
public:
    SearchQuery() = default;
    explicit SearchQuery(const std::string& raw);

    // No terms: every item matches (an empty search box takes the whole tab).
    bool Empty() const { return m_terms.empty(); }

    // `foldedHaystack` is the item's searchable text after FoldCase, one
    // name or mod line per '\n'-separated line.
    bool Matches(const std::string& foldedHaystack) const;

    const std::string& Raw() const { return m_raw; }

private:
    struct Alternative {
        std::string literal;                 // folded literal text
        std::shared_ptr<const std::regex> re; // set for a regular-expression term
    };
    struct Term {
        bool negate = false;
        std::vector<Alternative> alternatives;
    };
    static bool HasRegexSyntax(std::string_view token);

    std::string m_raw;
    std::vector<Term> m_terms;
};

} // namespace QuickStashGame
