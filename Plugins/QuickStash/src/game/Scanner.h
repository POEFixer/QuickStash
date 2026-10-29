// Background reader: inventories, the open tab, the game's search box and
// the TAKE selection are read on a plugin-owned thread and published as an
// immutable snapshot, so the overlay frame only draws.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#pragma once

#include "Languages.h"
#include "PoeHighlight.h"
#include "TextMatch.h"
#include "WithdrawPlanner.h"
#include "../../../../POEFixer/plugin_sdk/PluginSDK.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace QuickStashGame {

struct ScanSnapshot {
    uint64_t seq = 0;
    std::chrono::steady_clock::time_point at{};
    bool active = false;               // in game, game in front, listing read

    bool hasBackpack = false;          // backpack found (its Grid.Valid = panel open)
    PluginSDK::Inventory backpack;
    int  backpackFreeCells = 0;

    bool stashOpen = false;
    bool stashIsGuild = false;
    int  stashId = -1;
    int  stashItemCount = 0;

    bool takeScanned = false;          // search box and selection read this pass
    bool highlightFound = false;
    float highlightX = 0.0f;
    float highlightY = 0.0f;
    std::string filter;
    GameLanguage language = GameLanguage::Auto;
    std::vector<ScreenRect> takeRects;
    int  takeQty = 0;
    bool takePending = false;

    // Diagnostics.
    int    uiNodesVisited = 0;
    double passMs = 0.0;
    size_t textCacheSize = 0;
};

class Scanner {
public:
    Scanner() = default;
    Scanner(const Scanner&) = delete;
    Scanner& operator=(const Scanner&) = delete;
    ~Scanner() { Stop(); }

    void Start(const PluginSDK::Context* ctx);
    void Stop();  // joins the thread; safe to call twice

    std::shared_ptr<const ScanSnapshot> Latest() const;

    // Inputs from the render thread.
    void SetOptions(bool takeEnabled, bool readMods, int languageSetting);
    void SetDisplaySize(float w, float h);
    void SetTransferRunning(bool running);
    void Wake();

    uint32_t FaultCount() const { return m_faults.load(std::memory_order_relaxed); }

private:
    void Run();
    void PassCpp();
    static int PassSeh(Scanner* self);
    void Publish(std::shared_ptr<ScanSnapshot> snap);
    void PublishInactive();

    const PluginSDK::Context* m_ctx = nullptr;
    std::thread m_thread;
    mutable std::mutex m_mutex;              // guards m_latest, m_stop, m_wake
    std::condition_variable m_cv;
    bool m_stop = false;
    bool m_wake = false;
    std::shared_ptr<const ScanSnapshot> m_latest;

    std::atomic<bool>  m_takeEnabled{true};
    std::atomic<bool>  m_readMods{true};
    std::atomic<int>   m_languageSetting{0};
    std::atomic<float> m_displayW{0.0f};
    std::atomic<float> m_displayH{0.0f};
    std::atomic<bool>  m_transferRunning{false};
    std::atomic<uint32_t> m_faults{0};

    // Worker-thread state.
    uint64_t m_seq = 0;
    int      m_nextDelayMs = 150;
    uint64_t m_lastAreaCounter = 0;
    ModLanguage m_modLanguage;       // the language the cached item texts were built in
    bool        m_modLanguageSet = false;
    bool     m_lastPublishedActive = true;
    std::chrono::steady_clock::time_point m_lastScanRequest{};
    ItemTextCache m_textCache;
    SearchQuery   m_query;
    PoeHighlight  m_lastHighlight;
    std::chrono::steady_clock::time_point m_lastHighlightAt{};
    bool          m_lastHighlightOk = false;
    bool          m_lastPassPending = false;
};

} // namespace QuickStashGame
