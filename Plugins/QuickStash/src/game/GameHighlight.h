// The game's own result of the "Highlight Items" search, read from its stash item elements, so
// TAKE grabs exactly the items the game outlines: every line of the item's text (properties,
// requirements, item level), the filters (ilvl:, tier:, loc:) and the game's language included.
//
// Each stash item element holds its item entity and a match state byte the game sets while a
// search is typed (2 = matched and outlined, 1 = not matched, 0 = not judged yet). The layout is
// the game build's (GameHighlightLayout); after a game patch it may move. An element is accepted
// only when it names an item of the open tab and every on-screen item has one with a valid state;
// anything else leaves the result Unavailable and TAKE uses the plugin's own search.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PluginSDK { struct Context; }

namespace QuickStashGame {

// PoE2 0.5.5 (client of 18.11.2026): offsets inside a stash item element.
struct GameHighlightLayout {
    uint32_t itemEntity = 0x4E0;  // the item's entity (pointer)
    uint32_t matchState = 0x624;  // uint8: 2 matched, 1 not matched, 0 not judged
    uint32_t searchText = 0x710;  // std::wstring: the item's search text (built when searched)
};

enum class GameVerdict {
    Unavailable,  // not readable for this tab: use the plugin's own search
    Pending,      // readable, but the game has not judged every item for this text yet
    Ready,        // every on-screen item judged, the same twice
};

// One on-screen item: the match states of the elements that name it.
struct GameItemStates {
    uintptr_t item = 0;
    std::vector<int> states;   // one per element; a value outside 0..2 is garbage
};

// The game's state of one item from its elements: 2 / 1 when every judged element agrees (an
// element still at 0 beside a judged one is ignored), 0 when none is judged yet, -1 for no element,
// a conflict (1 and 2) or a value outside 0..2.
int AgreedState(const std::vector<int>& states);

// The verdict for one pass over the on-screen items: Unavailable when there is none, or one has no
// agreed state; Pending when one is not judged yet (0); Ready otherwise (the caller still settles it).
GameVerdict JudgeGameStates(const std::vector<GameItemStates>& items, std::string* why = nullptr);

// Trust in a Ready verdict only after the search text and the tab's items have stood still for
// kQuerySettleMs and the states have not changed for kStableMs: the game judges a few new items per
// frame, and states of the previous text may linger until it has.
class GameHighlightSettle {
public:
    static constexpr int kQuerySettleMs = 400;
    static constexpr int kStableMs      = 300;
    using Clock = std::chrono::steady_clock;

    // `signature`: the agreed states of the on-screen items, sorted by item address.
    bool Settled(const std::string& filter, const std::vector<std::pair<uintptr_t, int>>& signature,
                 Clock::time_point now);
    void Reset() { m_set = false; m_last.clear(); }

private:
    bool m_set = false;
    std::string m_filter;
    Clock::time_point m_since{};        // the text and the items as they are now since then
    Clock::time_point m_stableSince{};  // the states as they are now since then
    std::vector<std::pair<uintptr_t, int>> m_last;
};

struct GameHighlightResult {
    GameVerdict verdict = GameVerdict::Unavailable;
    std::unordered_set<uintptr_t> matched;   // items the game outlines (state 2)
    int itemsOnScreen = 0;
    int itemsWithElement = 0;
    std::string why;                         // why it is not Ready (diagnostics, English)
};

// Reads the states each pass; finds the item elements by walking the visible game UI when the
// tab's items change or an element stops naming its item (Pending meanwhile). One instance per
// thread. Within one episode (the same search text and on-screen items) it gives up on the game's
// result for good (Unavailable until the text or the items change, no further walks) when two walks
// leave an item without an element, two elements of one item disagree, a state is out of range, or
// the game has not judged every item after kPendingTimeoutMs.
class GameHighlightReader {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr int kRediscoverMs     = 500;   // at most one UI walk this often
    static constexpr int kPendingTimeoutMs = 5000;
    static constexpr int kFailedWalks      = 2;

    GameHighlightResult Read(const PluginSDK::Context* ctx, const std::vector<uintptr_t>& onScreenItems,
                             const std::string& filter, Clock::time_point now);
    void Reset();

    // Diagnostics: the element found for an item (0 = none) and the game's search text of it.
    uintptr_t ElementOf(uintptr_t item) const;
    int StateOf(const PluginSDK::Context* ctx, uintptr_t item) const;
    std::string SearchTextOf(const PluginSDK::Context* ctx, uintptr_t item) const;
    // Walks the UI now (diagnostics: no rate limit).
    void DiscoverNow(const PluginSDK::Context* ctx, const std::vector<uintptr_t>& items);

    const GameHighlightLayout& Layout() const { return m_layout; }

private:
    void Discover(const PluginSDK::Context* ctx, const std::vector<uintptr_t>& sortedItems);
    int ReadState(const PluginSDK::Context* ctx, uintptr_t element, uintptr_t item) const;

    GameHighlightLayout m_layout;
    std::unordered_map<uintptr_t, std::vector<uintptr_t>> m_elements;   // item -> its elements
    std::vector<uintptr_t> m_discoveredFor;                              // sorted items of that walk
    bool m_stale = true;            // an element stopped naming its item, or a walk is owed
    bool m_walkFresh = false;       // no pass has judged the last walk yet
    Clock::time_point m_lastDiscovery{};
    GameHighlightSettle m_settle;

    // The episode: the search text and the sorted on-screen items.
    std::string m_episodeFilter;
    std::vector<uintptr_t> m_episodeItems;
    Clock::time_point m_episodeStart{};
    int  m_failedWalks = 0;
    bool m_gaveUp = false;
    std::string m_gaveUpWhy;
};

} // namespace QuickStashGame
