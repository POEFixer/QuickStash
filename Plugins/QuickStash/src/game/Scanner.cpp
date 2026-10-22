// Background reader for Quick Stash.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "Scanner.h"

#include "PanelDetector.h"
#include "PoeHighlight.h"
#include "TransferPlanner.h"

#include <Windows.h>

#include <exception>

namespace QuickStashGame {
namespace {

using Clock = std::chrono::steady_clock;

constexpr int kScanRequestMs     = 150;  // a fresh host inventory read at most this often
constexpr int kOpenPanelDelayMs  = 150;  // pass interval while the backpack is open
constexpr int kPendingDelayMs    = 50;   // while TAKE item texts are still being read
constexpr int kClosedDelayMs     = 250;  // backpack closed
constexpr int kIdleDelayMs       = 300;  // not in game / game in the background
constexpr int kHighlightReuseMs  = 300;  // reuse the search box read during fast passes
constexpr uint32_t kFaultBackoff = 10;   // consecutive faults before slowing down

long long MsSince(Clock::time_point t, Clock::time_point now) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - t).count();
}

} // namespace

void Scanner::Start(const PluginSDK::Context* ctx) {
    Stop();
    m_ctx = ctx;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = false;
        m_wake = true;
        m_latest = std::make_shared<const ScanSnapshot>();
    }
    m_seq = 0;
    m_nextDelayMs = 0;
    m_lastAreaCounter = 0;
    m_lastLanguage = -1;
    m_lastPublishedActive = true;
    m_lastScanRequest = {};
    m_textCache.Clear();
    m_query = SearchQuery();
    m_lastHighlight = PoeHighlight();
    m_lastHighlightOk = false;
    m_lastPassPending = false;
    m_faults.store(0, std::memory_order_relaxed);
    m_thread = std::thread([this] { Run(); });
}

void Scanner::Stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_latest = std::make_shared<const ScanSnapshot>();
}

std::shared_ptr<const ScanSnapshot> Scanner::Latest() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_latest;
}

void Scanner::SetOptions(bool takeEnabled, bool readMods, int languageSetting) {
    m_takeEnabled.store(takeEnabled, std::memory_order_relaxed);
    m_readMods.store(readMods, std::memory_order_relaxed);
    m_languageSetting.store(languageSetting, std::memory_order_relaxed);
}

void Scanner::SetDisplaySize(float w, float h) {
    m_displayW.store(w, std::memory_order_relaxed);
    m_displayH.store(h, std::memory_order_relaxed);
}

void Scanner::SetTransferRunning(bool running) {
    m_transferRunning.store(running, std::memory_order_relaxed);
}

void Scanner::Wake() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_wake = true;
    }
    m_cv.notify_all();
}

void Scanner::Publish(std::shared_ptr<ScanSnapshot> snap) {
    snap->seq = ++m_seq;
    snap->at = Clock::now();
    snap->textCacheSize = m_textCache.Size();
    m_lastPublishedActive = snap->active;
    std::shared_ptr<const ScanSnapshot> frozen = std::move(snap);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_latest = std::move(frozen);
}

void Scanner::PublishInactive() {
    // One inactive snapshot is enough; the overlay ignores stale ones anyway,
    // but refresh it now and then so its age stays meaningful.
    Publish(std::make_shared<ScanSnapshot>());
}

void Scanner::Run() {
    uint32_t consecutive = 0;
    bool reported = false;
    for (;;) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(m_nextDelayMs), [this] { return m_stop || m_wake; });
            if (m_stop) break;
            m_wake = false;
        }
        if (PassSeh(this) != 0) {
            m_faults.fetch_add(1, std::memory_order_relaxed);
            ++consecutive;
            m_textCache.Clear();
            if (consecutive >= kFaultBackoff) {
                if (!reported && m_ctx) {
                    m_ctx->Log.Error("Quick Stash: the background reader keeps failing; retrying every 2 s");
                    reported = true;
                }
                m_nextDelayMs = 2000;
            } else {
                m_nextDelayMs = kClosedDelayMs;
            }
            try { PublishInactive(); } catch (...) {}
        } else {
            consecutive = 0;
        }
    }
}

int Scanner::PassSeh(Scanner* self) {
    __try {
        self->PassCpp();
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}

__declspec(noinline) void Scanner::PassCpp() {
    try {
        const PluginSDK::Context* ctx = m_ctx;
        if (!ctx) { m_nextDelayMs = kIdleDelayMs; return; }
        const auto t0 = Clock::now();

        if (!ctx->Game.IsInGame()) {
            m_textCache.Clear();
            PublishInactive();
            m_nextDelayMs = kIdleDelayMs;
            return;
        }
        if (!ctx->Game.IsForeground()) {
            PublishInactive();
            m_nextDelayMs = kIdleDelayMs;
            return;
        }

        const uint64_t area = ctx->Game.GetAreaChangeCounter();
        if (area != m_lastAreaCounter) {
            m_lastAreaCounter = area;
            m_textCache.Clear();
        }

        if (m_lastScanRequest.time_since_epoch().count() == 0 || MsSince(m_lastScanRequest, t0) >= kScanRequestMs) {
            ctx->Inventory.Scan(-1);  // asynchronous: the host reads on its own thread
            m_lastScanRequest = t0;
        }
        const std::vector<PluginSDK::Inventory> all = ctx->Inventory.GetAll();

        float w = m_displayW.load(std::memory_order_relaxed);
        float h = m_displayH.load(std::memory_order_relaxed);
        if (w <= 0.0f || h <= 0.0f) {
            const PluginSDK::ScreenSize s = ctx->Game.GetScreenSize();
            w = static_cast<float>(s.Width);
            h = static_cast<float>(s.Height);
        }

        auto snap = std::make_shared<ScanSnapshot>();
        snap->active = true;
        const PluginSDK::Inventory* bp = FindMainInventory(ctx, all);
        if (bp) {
            snap->hasBackpack = true;
            snap->backpack = *bp;
            snap->backpackFreeCells = FreeCells(*bp);
        }
        if (!bp || !bp->Grid.Valid) {
            snap->passMs = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
            Publish(std::move(snap));
            m_nextDelayMs = kClosedDelayMs;
            return;
        }

        const PluginSDK::Inventory* stash = FindOpenStashAny(ctx, all, bp->InventoryId, w, h);
        if (stash) {
            snap->stashOpen = true;
            snap->stashIsGuild = IsGuildStashInventory(*stash);
            snap->stashId = stash->InventoryId;
            snap->stashItemCount = static_cast<int>(stash->Items.size());
        }

        m_nextDelayMs = kOpenPanelDelayMs;
        const bool running = m_transferRunning.load(std::memory_order_relaxed);
        if (stash && !running && m_takeEnabled.load(std::memory_order_relaxed)) {
            const bool reuse = m_lastHighlightOk && MsSince(m_lastHighlightAt, t0) < kHighlightReuseMs &&
                               m_lastPassPending;
            if (!reuse) {
                m_lastHighlight = ReadPoeHighlight(ctx);
                m_lastHighlightAt = t0;
                m_lastHighlightOk = true;
            }
            const PoeHighlight& poe = m_lastHighlight;
            snap->takeScanned = true;
            snap->highlightFound = poe.found;
            snap->highlightX = poe.x;
            snap->highlightY = poe.y;
            snap->filter = poe.filter;
            snap->language = poe.language;
            snap->uiNodesVisited = poe.nodesVisited;
            if (poe.found) {
                const int setting = m_languageSetting.load(std::memory_order_relaxed);
                const int effective = setting == static_cast<int>(GameLanguage::Auto)
                                          ? static_cast<int>(poe.language) : setting;
                if (effective != m_lastLanguage) {
                    m_lastLanguage = effective;
                    m_textCache.Clear();
                }
                if (poe.filter != m_query.Raw())
                    m_query = SearchQuery(poe.filter);
                const char* language = GameLanguageHostName(static_cast<GameLanguage>(effective));
                WithdrawSelection sel = SelectWithdrawTargets(ctx, *stash, w, h, m_textCache, m_query,
                                                              m_readMods.load(std::memory_order_relaxed), language);
                snap->takeRects = std::move(sel.rects);
                snap->takeQty = sel.totalQty;
                snap->takePending = sel.pending;
                if (sel.pending) m_nextDelayMs = kPendingDelayMs;
            }
        }
        m_lastPassPending = snap->takePending;

        snap->passMs = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
        Publish(std::move(snap));
    } catch (const std::exception&) {
        m_lastHighlightOk = false;
        m_nextDelayMs = kClosedDelayMs;
    } catch (...) {
        m_lastHighlightOk = false;
        m_nextDelayMs = kClosedDelayMs;
    }
}

} // namespace QuickStashGame
