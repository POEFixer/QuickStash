// Reads the game's "Highlight Items" search box from the UI tree.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "PoeHighlight.h"

#include "TextMatch.h"
#include "../../../../POEFixer/plugin_sdk/PluginSDK.h"

#include <utility>

namespace QuickStashGame {
namespace {

struct Anchor {
    const char*  text;
    GameLanguage language;
};

// The label left of the box in every client language (UTF-8).
const Anchor kAnchors[] = {
    {"Highlight Items", GameLanguage::English},
    {"Highlight Item", GameLanguage::English},
    {"\xec\x95\x84\xec\x9d\xb4\xed\x85\x9c \xea\xb0\x95\xec\xa1\xb0\xed\x95\x98\xea\xb8\xb0", GameLanguage::Korean},
    {"\xec\x95\x84\xec\x9d\xb4\xed\x85\x9c \xea\xb0\x95\xec\xa1\xb0", GameLanguage::Korean},
    {"\xe7\xaa\x81\xe9\xa1\xaf\xe9\xa0\x85\xe7\x9b\xae", GameLanguage::TraditionalChinese},
    {"\xe9\x86\x92\xe7\x9b\xae\xe6\x8f\x90\xe7\xa4\xba\xe9\x81\x93\xe5\x85\xb7", GameLanguage::TraditionalChinese},
    {"\xe7\xaa\x81\xe9\xa1\xaf\xe9\x81\x93\xe5\x85\xb7", GameLanguage::TraditionalChinese},
    {"\xe9\x86\x92\xe7\x9b\xae\xe6\x8f\x90\xe7\xa4\xba\xe9\xa0\x85\xe7\x9b\xae", GameLanguage::TraditionalChinese},
    {"\xe9\xab\x98\xe4\xba\xae\xe7\x89\xa9\xe5\x93\x81", GameLanguage::SimplifiedChinese},
    {"\xe7\xaa\x81\xe5\x87\xba\xe6\x98\xbe\xe7\xa4\xba\xe7\x89\xa9\xe5\x93\x81", GameLanguage::SimplifiedChinese},
    {"\xe9\xab\x98\xe4\xba\xae\xe6\x98\xbe\xe7\xa4\xba\xe7\x89\xa9\xe5\x93\x81", GameLanguage::SimplifiedChinese},
    {"\xd0\x9f\xd0\xbe\xd0\xb4\xd1\x81\xd0\xb2\xd0\xb5\xd1\x82\xd0\xb8\xd1\x82\xd1\x8c \xd0\xb2\xd0\xb5\xd1\x89\xd0\xb8", GameLanguage::Russian},
    {"\xd0\x9f\xd0\xbe\xd0\xb4\xd1\x81\xd0\xb2\xd0\xb5\xd1\x82\xd0\xba\xd0\xb0 \xd0\xbf\xd1\x80\xd0\xb5\xd0\xb4\xd0\xbc\xd0\xb5\xd1\x82\xd0\xbe\xd0\xb2", GameLanguage::Russian},
    {"\xd0\x9f\xd0\xbe\xd0\xb4\xd1\x81\xd0\xb2\xd0\xb5\xd1\x82\xd0\xb8\xd1\x82\xd1\x8c \xd0\xbf\xd1\x80\xd0\xb5\xd0\xb4\xd0\xbc\xd0\xb5\xd1\x82\xd1\x8b", GameLanguage::Russian},
    {"Gegenst\xc3\xa4nde hervorheben", GameLanguage::German},
    {"Gegenstand hervorheben", GameLanguage::German},
    {"Mettre les objets en surbrillance", GameLanguage::French},
    {"Mettre en surbrillance", GameLanguage::French},
    {"Objets en surbrillance", GameLanguage::French},
    {"Resaltar objetos", GameLanguage::Spanish},
    {"Resaltar objeto", GameLanguage::Spanish},
    {"Destacar Itens", GameLanguage::Portuguese},
    {"Destacar Item", GameLanguage::Portuguese},
    {"\xe3\x82\xa2\xe3\x82\xa4\xe3\x83\x86\xe3\x83\xa0\xe3\x82\x92\xe3\x83\x8f\xe3\x82\xa4\xe3\x83\xa9\xe3\x82\xa4\xe3\x83\x88", GameLanguage::Japanese},
    {"\xe3\x82\xa2\xe3\x82\xa4\xe3\x83\x86\xe3\x83\xa0\xe3\x81\xae\xe3\x83\x8f\xe3\x82\xa4\xe3\x83\xa9\xe3\x82\xa4\xe3\x83\x88", GameLanguage::Japanese},
    {"\xe0\xb9\x80\xe0\xb8\x99\xe0\xb9\x89\xe0\xb8\x99\xe0\xb9\x84\xe0\xb8\xad\xe0\xb9\x80\xe0\xb8\x97\xe0\xb8\xa1", GameLanguage::Thai},
};

// Starts of the empty-box hint per language.
const char* const kPlaceholderPrefixes[] = {
    "Type keywords",
    "\xed\x82\xa4\xec\x9b\x8c\xeb\x93\x9c",
    "\xea\xb2\x80\xec\x83\x89\xec\x96\xb4",
    "\xec\x95\x84\xec\x9d\xb4\xed\x85\x9c\xec\x9d\x84 \xea\xb0\x95\xec\xa1\xb0",
    "\xec\x95\x84\xec\x9d\xb4\xed\x85\x9c \xea\xb0\x95\xec\xa1\xb0",
    "\xe9\x97\x9c\xe9\x8d\xb5\xe5\xad\x97",
    "\xe5\x85\xb3\xe9\x94\xae\xe5\xad\x97",
    "\xd0\x92\xd0\xb2\xd0\xb5\xd0\xb4\xd0\xb8\xd1\x82\xd0\xb5",
    "Schlagw\xc3\xb6rt",
    "Schlagworte",
    "Saisir",
    "Saisissez",
    "Escribe",
    "Digite",
    "\xe3\x82\xad\xe3\x83\xbc\xe3\x83\xaf\xe3\x83\xbc\xe3\x83\x89",
    "\xe0\xb8\x84\xe0\xb8\xb3\xe0\xb8\x84\xe0\xb9\x89\xe0\xb8\x99\xe0\xb8\xab\xe0\xb8\xb2",
    "\xe0\xb8\x9e\xe0\xb8\xb4\xe0\xb8\xa1\xe0\xb8\x9e\xe0\xb9\x8c",
};

// Words the empty-box hint contains anywhere.
const char* const kPlaceholderWords[] = {
    "mots-cl\xc3\xa9s",
    "mots-cles",
    "palabras clave",
    "palavras-chave",
    "\xd0\xba\xd0\xbb\xd1\x8e\xd1\x87\xd0\xb5\xd0\xb2\xd1\x8b\xd0\xb5 \xd1\x81\xd0\xbb\xd0\xbe\xd0\xb2\xd0\xb0",
    "\xe9\x97\x9c\xe9\x8d\xb5\xe5\xad\x97",
    "\xe5\x85\xb3\xe9\x94\xae\xe5\xad\x97",
    "\xed\x82\xa4\xec\x9b\x8c\xeb\x93\x9c",
};

// Internal identifiers a label may carry instead of its translated text.
const char* const kAnchorIds[] = {"highlightitems", "item_highlight", "stash_search"};

struct FoldedTables {
    std::vector<std::pair<std::string, GameLanguage>> anchors;
    std::vector<std::string> prefixes;
    std::vector<std::string> words;
};

const FoldedTables& Tables() {
    static const FoldedTables tables = [] {
        FoldedTables t;
        for (const Anchor& a : kAnchors) t.anchors.emplace_back(FoldCase(a.text), a.language);
        for (const char* p : kPlaceholderPrefixes) t.prefixes.push_back(FoldCase(p));
        for (const char* w : kPlaceholderWords) t.words.push_back(FoldCase(w));
        return t;
    }();
    return tables;
}

constexpr int   kMaxVisited   = 6000;
constexpr int   kMaxDepth     = 32;
constexpr size_t kMaxTextBytes = 128;
constexpr float kRowMaxDx     = 450.0f;  // search text sits within this distance right of the label
constexpr float kRowMaxDy     = 20.0f;

} // namespace

std::string TrimAscii(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && static_cast<unsigned char>(s[start]) <= ' ') ++start;
    size_t end = s.size();
    while (end > start && static_cast<unsigned char>(s[end - 1]) <= ' ') --end;
    return s.substr(start, end - start);
}

HostAnchor MakeHostAnchor(const std::string& label, GameLanguage language) {
    HostAnchor a;
    a.folded = FoldCase(TrimAscii(label));
    a.language = language;
    return a;
}

int MatchHighlightAnchor(const std::string& raw, const HostAnchor& host) {
    const std::string s = FoldCase(TrimAscii(raw));
    if (s.empty()) return -1;
    if (!host.folded.empty() && s == host.folded) return static_cast<int>(host.language);
    for (const auto& [text, lang] : Tables().anchors)
        if (s == text) return static_cast<int>(lang);
    return -1;
}

bool IsPlaceholderText(const std::string& raw) {
    const std::string s = FoldCase(TrimAscii(raw));
    if (s.empty()) return false;
    for (const auto& p : Tables().prefixes)
        if (s.compare(0, p.size(), p) == 0) return true;
    for (const auto& w : Tables().words)
        if (s.find(w) != std::string::npos) return true;
    return false;
}

PoeHighlight SelectHighlight(const std::vector<UiText>& texts, const HostAnchor& host) {
    PoeHighlight r;
    const UiText* anchor = nullptr;
    for (const auto& e : texts) {
        const int lang = MatchHighlightAnchor(e.text, host);
        if (lang >= 0) {
            anchor = &e;
            r.language = static_cast<GameLanguage>(lang);
            break;
        }
    }
    if (!anchor) {
        for (const auto& e : texts) {
            const std::string folded = FoldCase(e.text);
            for (const char* id : kAnchorIds) {
                if (folded.find(id) != std::string::npos) { anchor = &e; break; }
            }
            if (anchor) break;
        }
    }
    if (!anchor) return r;

    r.found = true;
    r.x = anchor->x;
    r.y = anchor->y;

    const UiText* value = nullptr;
    bool placeholderFound = false;
    bool ambiguous = false;
    for (const auto& e : texts) {
        if (&e == anchor) continue;
        if (TrimAscii(e.text).empty()) continue;
        if (MatchHighlightAnchor(e.text, host) >= 0) continue;
        const float dx = e.x - anchor->x;
        float dy = e.y - anchor->y;
        if (dy < 0.0f) dy = -dy;
        if (dx <= 0.0f || dx > kRowMaxDx || dy > kRowMaxDy) continue;
        if (IsPlaceholderText(e.text)) {
            placeholderFound = true;
            continue;
        }
        if (value && value->text != e.text) ambiguous = true;
        if (!value || dx < value->x - anchor->x) value = &e;
    }
    if (ambiguous || (placeholderFound && value)) r.found = false;
    else if (value) r.filter = TrimAscii(value->text);
    else if (!placeholderFound) r.found = false;
    return r;
}

std::vector<UiText> CollectUiTexts(const PluginSDK::Context* ctx, size_t maxTexts, int* visitedOut) {
    std::vector<UiText> texts;
    int visited = 0;
    if (ctx) {
        const uintptr_t root = ctx->Ui.GetGameUiRoot();
        if (root) {
            std::vector<std::pair<uintptr_t, int>> stack;
            stack.reserve(256);
            stack.emplace_back(root, 0);
            while (!stack.empty() && visited < kMaxVisited && texts.size() < maxTexts) {
                const auto [addr, depth] = stack.back();
                stack.pop_back();
                ++visited;
                if (!addr || depth > kMaxDepth) continue;
                if (!ctx->Ui.IsVisible(addr)) continue;
                // GetText and GetStringId return the same localized string on
                // this host, so one read per element is enough.
                std::string t = ctx->Ui.GetText(addr);
                if (!t.empty() && t.size() < kMaxTextBytes) {
                    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
                    if (ctx->Ui.ComputeScreenRect(addr, x, y, w, h))
                        texts.push_back(UiText{std::move(t), x, y});
                }
                for (const uintptr_t c : ctx->Ui.GetChildren(addr))
                    if (c) stack.emplace_back(c, depth + 1);
            }
        }
    }
    if (visitedOut) *visitedOut = visited;
    return texts;
}

PoeHighlight ReadPoeHighlight(const PluginSDK::Context* ctx) {
    int visited = 0;
    const std::vector<UiText> texts = CollectUiTexts(ctx, static_cast<size_t>(kMaxVisited), &visited);
    // The game's own label in the game language the host knows ("" on an older host).
    HostAnchor host;
    if (ctx) {
        const PluginSDK::GameLanguageInfo info = ctx->Lang.GameLanguage();
        host = MakeHostAnchor(ctx->Lang.GameString("StashPanelSearchLabel"),
                              info.known ? GameLanguageFromCsdName(info.csdName) : GameLanguage::Auto);
    }
    PoeHighlight r = SelectHighlight(texts, host);
    r.nodesVisited = visited;
    return r;
}

} // namespace QuickStashGame
