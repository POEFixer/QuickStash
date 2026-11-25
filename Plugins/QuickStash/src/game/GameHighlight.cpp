// The game's own Highlight Items result for the open stash tab.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "GameHighlight.h"

#include "../../../../POEFixer/plugin_sdk/PluginSDK.h"

#include <Windows.h>

#include <algorithm>

namespace QuickStashGame {
namespace {

constexpr int kMaxVisited       = 8000;  // UI elements per walk
constexpr int kMaxDepth         = 32;
constexpr size_t kMaxSearchText = 4096;  // characters (diagnostics)

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), n, nullptr, nullptr);
    return out;
}

} // namespace

int AgreedState(const std::vector<int>& states) {
    if (states.empty()) return -1;
    int judged = 0;
    for (const int s : states) {
        if (s < 0 || s > 2) return -1;
        if (s == 0) continue;
        if (judged != 0 && judged != s) return -1;
        judged = s;
    }
    return judged;
}

GameVerdict JudgeGameStates(const std::vector<GameItemStates>& items, std::string* why) {
    if (items.empty()) {
        if (why) *why = "no item on screen";
        return GameVerdict::Unavailable;
    }
    bool pending = false;
    for (const GameItemStates& it : items) {
        const int s = AgreedState(it.states);
        if (s < 0) {
            if (why) *why = it.states.empty() ? "an item has no element" : "an item's elements disagree";
            return GameVerdict::Unavailable;
        }
        if (s == 0) pending = true;
    }
    if (pending && why) *why = "the game has not judged every item yet";
    return pending ? GameVerdict::Pending : GameVerdict::Ready;
}

bool GameHighlightSettle::Settled(const std::string& filter, const std::vector<std::pair<uintptr_t, int>>& signature,
                                  Clock::time_point now) {
    bool sameItems = m_set && m_last.size() == signature.size();
    for (size_t i = 0; sameItems && i < signature.size(); ++i) sameItems = m_last[i].first == signature[i].first;
    if (!m_set || filter != m_filter || !sameItems) {
        m_set = true;
        m_filter = filter;
        m_since = now;
        m_stableSince = now;
        m_last = signature;
        return false;
    }
    if (signature != m_last) {
        m_last = signature;
        m_stableSince = now;
        return false;
    }
    const auto ms = [&](Clock::time_point t) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - t).count();
    };
    return ms(m_since) >= kQuerySettleMs && ms(m_stableSince) >= kStableMs;
}

void GameHighlightReader::Reset() {
    m_elements.clear();
    m_discoveredFor.clear();
    m_stale = true;
    m_walkFresh = false;
    m_lastDiscovery = {};
    m_settle.Reset();
    m_episodeFilter.clear();
    m_episodeItems.clear();
    m_episodeStart = {};
    m_failedWalks = 0;
    m_gaveUp = false;
    m_gaveUpWhy.clear();
}

// The state byte of `element` for `item`; -2 when the element is hidden or names another item
// (stale: walk again), -1 when unreadable.
int GameHighlightReader::ReadState(const PluginSDK::Context* ctx, uintptr_t element, uintptr_t item) const {
    if (!ctx->Ui.IsVisible(element)) return -2;
    uintptr_t named = 0;
    if (!ctx->Memory.Read(element + m_layout.itemEntity, &named, sizeof(named)) || named != item) return -2;
    uint8_t state = 0xFF;
    if (!ctx->Memory.Read(element + m_layout.matchState, &state, sizeof(state))) return -1;
    return state;
}

void GameHighlightReader::Discover(const PluginSDK::Context* ctx, const std::vector<uintptr_t>& sortedItems) {
    m_elements.clear();
    m_discoveredFor = sortedItems;
    m_stale = false;
    if (!ctx || sortedItems.empty()) return;
    const uintptr_t root = ctx->Ui.GetGameUiRoot();
    if (!root) return;
    std::vector<std::pair<uintptr_t, int>> stack;
    stack.reserve(256);
    stack.emplace_back(root, 0);
    int visited = 0;
    while (!stack.empty() && visited < kMaxVisited) {
        const auto [addr, depth] = stack.back();
        stack.pop_back();
        ++visited;
        if (!addr || depth > kMaxDepth) continue;
        if (depth > 0 && !ctx->Ui.IsVisible(addr)) continue;
        uintptr_t named = 0;
        if (ctx->Memory.Read(addr + m_layout.itemEntity, &named, sizeof(named)) && named &&
            std::binary_search(sortedItems.begin(), sortedItems.end(), named))
            m_elements[named].push_back(addr);
        for (const uintptr_t c : ctx->Ui.GetChildren(addr))
            if (c) stack.emplace_back(c, depth + 1);
    }
}

void GameHighlightReader::DiscoverNow(const PluginSDK::Context* ctx, const std::vector<uintptr_t>& items) {
    std::vector<uintptr_t> sorted = items;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    Discover(ctx, sorted);
}

GameHighlightResult GameHighlightReader::Read(const PluginSDK::Context* ctx, const std::vector<uintptr_t>& onScreenItems,
                                              const std::string& filter, Clock::time_point now) {
    GameHighlightResult r;
    r.itemsOnScreen = static_cast<int>(onScreenItems.size());
    if (!ctx || onScreenItems.empty() || filter.empty()) {
        r.why = onScreenItems.empty() ? "no item on screen" : "no search text";
        Reset();
        return r;
    }
    std::vector<uintptr_t> sorted = onScreenItems;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    const auto ms = [&](Clock::time_point t) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - t).count();
    };

    // A new episode: another search text or other items on screen.
    if (filter != m_episodeFilter || sorted != m_episodeItems) {
        m_episodeFilter = filter;
        m_episodeItems = sorted;
        m_episodeStart = now;
        m_failedWalks = 0;
        m_gaveUp = false;
        m_gaveUpWhy.clear();
        m_settle.Reset();
    }
    if (m_gaveUp) {
        r.why = m_gaveUpWhy;
        return r;
    }
    const auto giveUp = [&](const std::string& why) {
        m_gaveUp = true;
        m_gaveUpWhy = why;
        m_settle.Reset();
        r.verdict = GameVerdict::Unavailable;
        r.matched.clear();
        r.why = why;
        return r;
    };

    // The elements: walk when the items changed since the last walk or an element went stale.
    if (m_stale || sorted != m_discoveredFor) {
        const bool due = m_lastDiscovery.time_since_epoch().count() == 0 || ms(m_lastDiscovery) >= kRediscoverMs;
        if (!due) {
            r.verdict = GameVerdict::Pending;
            r.why = "looking for the items in the game's stash panel";
            return r;
        }
        Discover(ctx, sorted);
        m_lastDiscovery = now;
        m_walkFresh = true;
    }

    std::vector<GameItemStates> items;
    items.reserve(sorted.size());
    std::vector<std::pair<uintptr_t, int>> signature;
    signature.reserve(sorted.size());
    bool stale = false;
    for (const uintptr_t item : sorted) {
        GameItemStates st;
        st.item = item;
        const auto found = m_elements.find(item);
        if (found != m_elements.end()) {
            for (const uintptr_t element : found->second) {
                const int s = ReadState(ctx, element, item);
                if (s == -2) { stale = true; continue; }   // hidden, or no longer this item's element
                st.states.push_back(s);
            }
        }
        if (!st.states.empty()) ++r.itemsWithElement;
        signature.emplace_back(item, AgreedState(st.states));
        items.push_back(std::move(st));
    }
    if (stale) {
        m_stale = true;
        m_settle.Reset();
        r.verdict = GameVerdict::Pending;
        r.why = "an item's element changed; looking again";
        return r;
    }

    std::string why;
    const GameVerdict verdict = JudgeGameStates(items, &why);
    if (verdict == GameVerdict::Unavailable) {
        const bool missing = r.itemsWithElement < static_cast<int>(sorted.size());
        bool conflict = false;
        for (const GameItemStates& it : items)
            if (!it.states.empty() && AgreedState(it.states) < 0) conflict = true;
        if (missing && !conflict) {
            // The game may not have built an item's element yet: walk again before giving up.
            if (m_walkFresh) ++m_failedWalks;
            m_walkFresh = false;
            if (m_failedWalks >= kFailedWalks) return giveUp("an item has no element in the game's stash panel");
            m_stale = true;
            r.verdict = GameVerdict::Pending;
            r.why = "looking for the items in the game's stash panel";
            return r;
        }
        return giveUp(why);
    }
    m_walkFresh = false;
    for (const auto& [item, state] : signature)
        if (state == 2) r.matched.insert(item);
    if (verdict == GameVerdict::Pending) {
        if (ms(m_episodeStart) >= kPendingTimeoutMs) return giveUp("the game did not judge every item in time");
        m_settle.Settled(filter, signature, now);   // keep the stability clock running
        r.verdict = GameVerdict::Pending;
        r.why = why;
        return r;
    }
    if (!m_settle.Settled(filter, signature, now)) {
        r.verdict = GameVerdict::Pending;
        r.why = "waiting for the game's highlight to settle";
        return r;
    }
    r.verdict = GameVerdict::Ready;
    return r;
}

uintptr_t GameHighlightReader::ElementOf(uintptr_t item) const {
    const auto found = m_elements.find(item);
    return (found == m_elements.end() || found->second.empty()) ? 0 : found->second.front();
}

int GameHighlightReader::StateOf(const PluginSDK::Context* ctx, uintptr_t item) const {
    const auto found = m_elements.find(item);
    if (!ctx || found == m_elements.end()) return -1;
    std::vector<int> states;
    for (const uintptr_t element : found->second) {
        const int s = ReadState(ctx, element, item);
        if (s != -2) states.push_back(s);
    }
    return AgreedState(states);
}

std::string GameHighlightReader::SearchTextOf(const PluginSDK::Context* ctx, uintptr_t item) const {
    const uintptr_t element = ElementOf(item);
    if (!ctx || !element) return {};
    std::wstring text = ctx->Memory.ReadStdWString(element + m_layout.searchText);
    if (text.size() > kMaxSearchText) text.resize(kMaxSearchText);
    return WideToUtf8(text);
}

} // namespace QuickStashGame
