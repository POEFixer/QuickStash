// Item search for TAKE: Unicode case folding and the game's "Highlight Items"
// query rules (space-separated terms that must all match, "quoted phrases",
// a leading ! to exclude, a|b alternatives, regular expressions, and the
// filters "ilvl:80" / "tier:15"). Like the game, every term is tested against
// each line of the item's text on its own.
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

// The words of the game's search filters, in every language the search covers (English and the
// game language): a token "<alias>:<value>" whose alias is one of `aliases` becomes the line
// "<label>: <value>" for each of `labels` (a whole line for a number, a regular expression for a
// [bracketed] value), as the game rewrites "ilvl:80" into "Item Level: 80".
struct SearchFilter {
    std::vector<std::string> aliases;  // "ilvl", "item level", ...
    std::vector<std::string> labels;   // "Item Level", ...
};
struct SearchFilters {
    SearchFilter itemLevel;     // ClientStrings SearchFilterItemLevel / ItemDisplayStringItemLevel
    SearchFilter waystoneTier;  // ClientStrings SearchFilterMapTier / ItemDisplayMapTier
};

class SearchQuery {
public:
    SearchQuery() = default;
    explicit SearchQuery(const std::string& raw, const SearchFilters* filters = nullptr);

    // No terms: every item matches (an empty search box takes the whole tab).
    bool Empty() const { return m_terms.empty(); }

    // `foldedHaystack` is the item's searchable text after FoldCase, one
    // name or mod line per '\n'-separated line. A term matches when one line
    // has it; a regular expression never spans two lines.
    bool Matches(const std::string& foldedHaystack) const;

    const std::string& Raw() const { return m_raw; }

private:
    struct Alternative {
        std::string literal;                  // folded literal text
        bool wholeLine = false;               // the literal must be the whole line (a filter number)
        std::shared_ptr<const std::regex> re; // set for a regular-expression term
    };
    struct Term {
        bool negate = false;
        std::vector<Alternative> alternatives;
    };
    static bool HasRegexSyntax(std::string_view token);
    // A filter token ("ilvl:80", "tier:[1-9]"): its alternatives, or false when it is none.
    static bool ParseFilter(const std::string& body, const SearchFilters& filters, std::vector<Alternative>& out);

    std::string m_raw;
    std::vector<Term> m_terms;
};

} // namespace QuickStashGame
