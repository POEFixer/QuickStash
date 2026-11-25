// Item search for TAKE: case folding and query parsing.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "TextMatch.h"

#include <Windows.h>

namespace QuickStashGame {
namespace {

std::string AsciiLower(std::string_view s) {
    std::string out(s);
    for (char& c : out)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return out;
}

bool IsSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

} // namespace

std::string FoldCase(std::string_view utf8) {
    bool ascii = true;
    for (char c : utf8) {
        if (static_cast<unsigned char>(c) >= 0x80) { ascii = false; break; }
    }
    if (ascii || utf8.size() > 0x7FFFFFF) return AsciiLower(utf8);

    const int srcLen = static_cast<int>(utf8.size());
    const int wideLen = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), srcLen, nullptr, 0);
    if (wideLen <= 0) return AsciiLower(utf8);
    std::wstring wide(static_cast<size_t>(wideLen), L'\0');
    ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), srcLen, wide.data(), wideLen);

    const int lowLen = ::LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.c_str(), wideLen,
                                       nullptr, 0, nullptr, nullptr, 0);
    if (lowLen <= 0) return AsciiLower(utf8);
    std::wstring lower(static_cast<size_t>(lowLen), L'\0');
    ::LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.c_str(), wideLen,
                    lower.data(), lowLen, nullptr, nullptr, 0);

    const int outLen = ::WideCharToMultiByte(CP_UTF8, 0, lower.c_str(), lowLen, nullptr, 0, nullptr, nullptr);
    if (outLen <= 0) return AsciiLower(utf8);
    std::string out(static_cast<size_t>(outLen), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, lower.c_str(), lowLen, out.data(), outLen, nullptr, nullptr);
    return out;
}

bool SearchQuery::HasRegexSyntax(std::string_view token) {
    for (char c : token) {
        switch (c) {
            case '.': case '*': case '+': case '?': case '[': case ']':
            case '(': case ')': case '{': case '}': case '^': case '$': case '\\':
                return true;
            default:
                break;
        }
    }
    return false;
}

namespace {

std::string TrimSpaces(std::string_view s) {
    size_t a = 0, b = s.size();
    while (a < b && IsSpace(s[a])) ++a;
    while (b > a && IsSpace(s[b - 1])) --b;
    return std::string(s.substr(a, b - a));
}

std::string EscapeRegex(std::string_view s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '.': case '*': case '+': case '?': case '[': case ']': case '(': case ')': case '{':
            case '}': case '^': case '$': case '\\': case '|': case '/':
                out += '\\';
                break;
            default:
                break;
        }
        out += c;
    }
    return out;
}

bool AllDigits(std::string_view s) {
    if (s.empty()) return false;
    for (char c : s)
        if (c < '0' || c > '9') return false;
    return true;
}

} // namespace

bool SearchQuery::ParseFilter(const std::string& body, const SearchFilters& filters, std::vector<Alternative>& out) {
    // The game: ^[ ]*(?:<aliases>)[ ]*:[ ]*(?:[0-9]+|\[.*\])[ ]*$, case-insensitive. A number becomes
    // the whole line "<label>: <n>"; a [bracketed] value the regular expression "<label>: [..]".
    const size_t colon = body.find(':');
    if (colon == std::string::npos) return false;
    const std::string alias = FoldCase(TrimSpaces(std::string_view(body).substr(0, colon)));
    const std::string value = TrimSpaces(std::string_view(body).substr(colon + 1));
    const bool number = AllDigits(value);
    const bool bracket = value.size() >= 2 && value.front() == '[' && value.back() == ']';
    if (alias.empty() || (!number && !bracket)) return false;
    for (const SearchFilter* filter : {&filters.itemLevel, &filters.waystoneTier}) {
        bool named = false;
        for (const std::string& a : filter->aliases)
            if (!a.empty() && FoldCase(a) == alias) { named = true; break; }
        if (!named) continue;
        for (const std::string& label : filter->labels) {
            if (label.empty()) continue;
            const std::string line = FoldCase(label) + ": ";
            if (number) {
                out.push_back(Alternative{line + value, true, nullptr});
            } else {
                try {
                    out.push_back(Alternative{std::string(), false, std::make_shared<const std::regex>(
                        EscapeRegex(line) + value, std::regex::ECMAScript | std::regex::icase | std::regex::nosubs)});
                } catch (const std::regex_error&) {
                    // Not a valid expression: no alternative from this label.
                }
            }
        }
        return true;   // the first filter whose alias the token names decides, as in the game
    }
    return false;
}

SearchQuery::SearchQuery(const std::string& raw, const SearchFilters* filters) : m_raw(raw) {
    // Split into terms at whitespace outside double quotes; quotes only group.
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;
    bool any = false;
    for (char c : raw) {
        if (c == '"') { inQuotes = !inQuotes; any = true; continue; }
        if (!inQuotes && IsSpace(c)) {
            if (any && !current.empty()) tokens.push_back(current);
            current.clear();
            any = false;
            continue;
        }
        current.push_back(c);
        any = true;
    }
    if (any && !current.empty()) tokens.push_back(current);

    for (std::string& token : tokens) {
        Term term;
        size_t start = 0;
        while (start < token.size() && token[start] == '!') {
            term.negate = !term.negate;
            ++start;
        }
        const std::string body = token.substr(start);
        if (body.empty()) continue;

        if (filters && ParseFilter(body, *filters, term.alternatives)) {
            // A filter that yields no line (no label in any language) matches nothing; keep the term so
            // it still narrows the search instead of disappearing.
            if (term.alternatives.empty()) term.alternatives.push_back(Alternative{std::string("\n"), true, nullptr});
        } else if (HasRegexSyntax(body)) {
            try {
                auto re = std::make_shared<const std::regex>(
                    body, std::regex::ECMAScript | std::regex::icase | std::regex::nosubs);
                term.alternatives.push_back(Alternative{std::string(), false, std::move(re)});
            } catch (const std::regex_error&) {
                // Not a valid expression: match the text literally instead.
                term.alternatives.push_back(Alternative{FoldCase(body), false, nullptr});
            }
        } else {
            size_t pos = 0;
            while (pos <= body.size()) {
                const size_t bar = body.find('|', pos);
                const std::string part = body.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos);
                if (!part.empty()) term.alternatives.push_back(Alternative{FoldCase(part), false, nullptr});
                if (bar == std::string::npos) break;
                pos = bar + 1;
            }
        }
        if (!term.alternatives.empty()) m_terms.push_back(std::move(term));
    }
}

bool SearchQuery::Matches(const std::string& foldedHaystack) const {
    if (m_terms.empty()) return true;
    // One line at a time, like the game: empty lines never match.
    std::vector<std::string_view> lines;
    for (size_t pos = 0; pos <= foldedHaystack.size();) {
        size_t end = foldedHaystack.find('\n', pos);
        if (end == std::string::npos) end = foldedHaystack.size();
        if (end > pos) lines.emplace_back(foldedHaystack.data() + pos, end - pos);
        pos = end + 1;
    }
    for (const Term& term : m_terms) {
        bool hit = false;
        for (const Alternative& alt : term.alternatives) {
            for (const std::string_view line : lines) {
                if (alt.re) {
                    try {
                        hit = std::regex_search(line.begin(), line.end(), *alt.re);
                    } catch (const std::regex_error&) {
                        hit = false;  // e.g. complexity limit on a pathological pattern
                    }
                } else if (alt.wholeLine) {
                    hit = line == alt.literal;
                } else {
                    hit = alt.literal.empty() || line.find(alt.literal) != std::string_view::npos;
                }
                if (hit) break;
            }
            if (hit) break;
        }
        if (hit == term.negate) return false;
    }
    return true;
}

} // namespace QuickStashGame
