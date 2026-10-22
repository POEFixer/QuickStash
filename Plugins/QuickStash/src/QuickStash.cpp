// Quick Stash: one-click Ctrl+click transfer between the backpack and the
// open storage panel (Transfer), and back out of the stash by the game's own
// search (TAKE).
//
// Originally created by Omer Faruk ARPA (https://github.com/omrfarukarpa/poefixer-quickstash).
// Maintained by POEFixer since October 2026.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"

#include "Settings.h"
#include "game/PanelDetector.h"
#include "game/Scanner.h"
#include "game/TransferPlanner.h"
#include "game/TransferState.h"
#include "ui/Diagnostics.h"
#include "ui/Overlay.h"
#include "ui/SettingsUi.h"

#include <imgui.h>
#include "../../../POEFixer/plugin_sdk/PluginUi.h"

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Plugin info: the host checks it before CreatePlugin; the publish tool reads it.
POEFIXER_PLUGIN_INFO(
    "QuickStash",
    "Quick Stash",
    "1.4.5",
    "Omer Faruk ARPA, POEFixer",
    "One-click Transfer from your backpack into the open stash, vendor or trade window, and TAKE from the stash by the game's own item search.")

// Fills Dear ImGui's word-wrap character classes; declared in imgui_internal.h,
// which this plugin does not otherwise need.
IMGUI_API void ImTextInitClassifiers();

namespace {

using Clock = std::chrono::steady_clock;

constexpr long long kSnapshotMaxAgeMs   = 1000;  // older readings count as "nothing known"
constexpr long long kTakeStartMaxAgeMs  = 500;   // TAKE starts only from a recent selection
constexpr long long kBackpackFullMs     = 1500;  // TAKE stops after the bag stays full this long
constexpr float     kTransferGapPx      = 16.0f; // gap between the Transfer button and the grid (at scale 1)

long long AgeMs(const QuickStashGame::ScanSnapshot& s) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - s.at).count();
}

// Binds the host's ImGui context. This DLL carries its own copy of Dear ImGui,
// whose word-wrap character classes are filled only where a font atlas is built
// (in the host's copy): without this call text wrapped by this DLL breaks in the
// middle of words. Cheap after the first call.
void BindImGui(void* context) {
    if (!context) return;
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(context));
    ImTextInitClassifiers();
}

bool Usable(const std::shared_ptr<const QuickStashGame::ScanSnapshot>& s, long long maxAgeMs = kSnapshotMaxAgeMs) {
    return s && s->active && s->seq != 0 && AgeMs(*s) <= maxAgeMs;
}

} // namespace

class QuickStashPlugin : public PluginSDK::Plugin {
public:
    ~QuickStashPlugin() override {
        m_scanner.Stop();
        m_transfer.Abort(false);
    }

    const char* GetName() const override { return "Quick Stash"; }

    bool WantsOverlay() const override { return m_active; }

    void OnEnable(bool) override {
        if (!HostCompatible()) {
            ctx()->Log.Error("Quick Stash: this POEFixer version is not compatible; the plugin stays off");
            return;
        }
        BindImGui(ctx()->ImGuiContext);
        EnsureLoaded();
        PushOptions();
        m_scanner.Start(ctx());
        auto& events = const_cast<PluginSDK::EventsService&>(ctx()->Events);
        m_frameToken = events.OnFrame([this] { OnFrameTick(); });
        ResetClickTrackers();
        m_active = true;
    }

    void OnDisable() override {
        m_active = false;
        m_transfer.Abort();
        if (m_frameToken.Valid()) {
            auto& events = const_cast<PluginSDK::EventsService&>(ctx()->Events);
            events.Unsubscribe(m_frameToken);
            m_frameToken = {};
        }
        m_scanner.Stop();
        m_capturePending = false;
        m_captureApplied = false;
        ctx()->Overlay.SetWantsOverlayInput(false);
        m_diagnostics = {};
        SaveSettings();
    }

    void SaveSettings() override {
        if (m_loaded) m_settings.Save(DirectoryPath());
        m_dirty = false;
    }

    void DrawSettings() override {
        if (!ctx()->ImGuiContext) return;
        BindImGui(ctx()->ImGuiContext);
        EnsureLoaded();

        std::shared_ptr<const QuickStashGame::ScanSnapshot> live;
        if (m_active) live = m_scanner.Latest();
        if (QuickStashUi::DrawSettingsTab(m_settings, m_ui, live.get(), m_transfer.IsRunning())) {
            m_dirty = true;
            PushOptions();
        }
        // Save once the edit ends (not on every frame of a slider drag).
        if (m_dirty && !ImGui::IsAnyItemActive()) SaveSettings();

        QuickStashUi::DrawDiagnosticsCard(ctx(), m_diagnostics, PluginSDK_GetInfo()->version, DirectoryPath(), live.get(),
                                          m_scanner.FaultCount(), m_settings.modLanguage);
        ImGui::Spacing();
        PluginUi::Hint("Originally created by Omer Faruk ARPA. Maintained by POEFixer. GPL-3.0.");
        ImGui::Spacing();
    }

    void DrawUI() override {
        if (!m_active) return;
        BindImGui(ctx()->ImGuiContext);
        m_drewThisFrame = true;
        const ImGuiIO& io = ImGui::GetIO();
        m_scanner.SetDisplaySize(io.DisplaySize.x, io.DisplaySize.y);

        if (!ctx()->Game.IsInGame() || !ctx()->Game.IsForeground()) {
            ReleaseOverlayInput();
            return;
        }

        if (m_transfer.IsRunning()) {
            ReleaseOverlayInput();
            QuickStashOverlay::DrawProgress(ImGui::GetBackgroundDrawList(), m_progressPos, m_progressMetrics,
                                            m_transfer.IsWithdrawMode() ? "Taking" : "Transferring",
                                            m_transfer.ProgressIndex(), m_transfer.ProgressTotal(),
                                            m_settings.cancelOnRightClick);
            return;
        }

        const auto snap = m_scanner.Latest();
        if (!Usable(snap) || !snap->hasBackpack || !snap->backpack.Grid.Valid || snap->backpack.Grid.CellSize <= 0.0f) {
            ReleaseOverlayInput();
            return;
        }
        const PluginSDK::Inventory& bp = snap->backpack;
        const QuickStashOverlay::Metrics m = QuickStashOverlay::MakeMetrics(bp.Grid.CellSize, m_settings.OverlayScale());
        const bool menuVisible = ctx()->Game.IsMenuVisible();

        // Transfer button above the backpack.
        const int transferCount = QuickStashGame::CountTransferable(bp, m_settings);
        char transferCountText[16];
        std::snprintf(transferCountText, sizeof(transferCountText), "%d", transferCount);
        QuickStashOverlay::ButtonLook tlook;
        tlook.label = "Transfer";
        tlook.secondary = transferCountText;
        tlook.enabled = !menuVisible && transferCount > 0;
        const ImVec2 tpos(bp.Grid.GridScreenX + m_settings.buttonOffsetX,
                          bp.Grid.GridScreenY - m.height - std::round(kTransferGapPx * m.scale) + m_settings.buttonOffsetY);

        // TAKE button above the game's search box.
        const bool showTake = m_settings.showTakeButton && snap->stashOpen && snap->takeScanned && snap->highlightFound;
        const int takeCount = static_cast<int>(snap->takeRects.size());
        char takeLabel[32];
        char takeQty[24] = "";
        std::snprintf(takeLabel, sizeof(takeLabel), snap->stashIsGuild ? "TAKE G(%d)" : "TAKE (%d)", takeCount);
        if (snap->takePending) std::snprintf(takeQty, sizeof(takeQty), "...");
        else if (takeCount > 0 && snap->takeQty != takeCount) std::snprintf(takeQty, sizeof(takeQty), "x%d", snap->takeQty);
        QuickStashOverlay::ButtonLook wlook;
        wlook.label = takeLabel;
        wlook.secondary = takeQty;
        wlook.enabled = !menuVisible && takeCount > 0 && !snap->takePending;
        const ImVec2 wpos(snap->highlightX, snap->highlightY - m.height - m.gap);

        if (showTake && m_settings.showHighlights)
            QuickStashOverlay::DrawSelection(ImGui::GetBackgroundDrawList(), snap->takeRects,
                                             m_settings.highlightColor.data(), m);

        const auto tres = QuickStashOverlay::DrawButton("##qsTransferButton", tpos, m, tlook);
        QuickStashOverlay::ButtonResult wres;
        if (showTake) wres = QuickStashOverlay::DrawButton("##qsTakeButton", wpos, m, wlook);

        // Capture the mouse over our buttons so the click does not reach the game.
        const ImVec2 mouse = io.MousePos;
        const bool overTransfer = QuickStashOverlay::HitRect(mouse, tres.p0, tres.p1);
        const bool overTake = showTake && QuickStashOverlay::HitRect(mouse, wres.p0, wres.p1);
        m_capturePending = overTransfer || overTake;
        ctx()->Overlay.SetWantsOverlayInput(m_capturePending || m_captureApplied);

        bool transferActivated = tres.clicked;
        bool takeActivated = showTake && wres.clicked;

        // Click-through overlay mode: ImGui may not see the press, so watch the
        // raw button too, but only where no other overlay window covers ours.
        const bool overlayMode = ctx()->Game.IsOverlayMode();
        const bool otherWindow = io.WantCaptureMouse;
        const bool hwTransfer = m_transferClick.Update(
            overlayMode && tlook.enabled && (tres.windowHovered || !otherWindow), tres.p0, tres.p1);
        const bool hwTake = m_takeClick.Update(
            overlayMode && showTake && wlook.enabled && (wres.windowHovered || !otherWindow), wres.p0, wres.p1);
        if (!transferActivated && !takeActivated) {
            if (hwTransfer) transferActivated = true;
            else if (hwTake) takeActivated = true;
        }

        if (transferActivated) {
            ReleaseOverlayInput();
            if (m_transfer.Start(ctx(), m_settings, bp)) BeginRun(tpos, m);
        } else if (takeActivated) {
            ReleaseOverlayInput();
            StartTake(*snap, wpos, m);
        }
    }

private:
    void EnsureLoaded() {
        if (m_loaded) return;
        const QuickStashConfig::LoadResult r = m_settings.Load(DirectoryPath());
        m_loaded = true;
        if (r == QuickStashConfig::LoadResult::Unreadable)
            ctx()->Log.Warn("Quick Stash: config/settings.json could not be read; defaults are used and the file is "
                            "left alone until you change a setting (a copy was kept as settings.json.corrupt if possible).");
        else if (r == QuickStashConfig::LoadResult::Unparsable)
            ctx()->Log.Warn("Quick Stash: config/settings.json is damaged; defaults are used and the file is left "
                            "alone until you change a setting (a copy was kept as settings.json.corrupt).");
    }

    void PushOptions() {
        m_scanner.SetOptions(m_settings.showTakeButton, m_settings.readMods, m_settings.modLanguage);
    }

    void ResetClickTrackers() {
        m_transferClick.Reset();
        m_takeClick.Reset();
    }

    void ReleaseOverlayInput() {
        ResetClickTrackers();
        m_capturePending = false;
        ctx()->Overlay.SetWantsOverlayInput(false);
    }

    void StartTake(const QuickStashGame::ScanSnapshot& snap, ImVec2 pos, const QuickStashOverlay::Metrics& m) {
        if (snap.takeRects.empty() || snap.takePending) return;
        if (AgeMs(snap) > kTakeStartMaxAgeMs) {
            ctx()->Log.Info("Quick Stash: TAKE ignored (the selection is being refreshed, click again)");
            return;
        }
        std::vector<QuickStashGame::ScreenPoint> points;
        points.reserve(snap.takeRects.size());
        for (const auto& r : snap.takeRects) points.push_back(QuickStashGame::RectCenter(r));
        if (m_transfer.StartWithdraw(ctx(), m_settings, std::move(points), snap.stashId)) BeginRun(pos, m);
    }

    void BeginRun(ImVec2 pos, const QuickStashOverlay::Metrics& m) {
        m_progressPos = pos;
        m_progressMetrics = m;
        m_stashMissStreak = 0;
        m_lastCheckedSeq = 0;
        m_fullSince = {};
        m_fullStashCount = -1;
        m_capturePending = false;
        m_captureApplied = false;
        ctx()->Overlay.SetWantsOverlayInput(false);
        m_scanner.SetTransferRunning(true);
    }

    void EndRun() {
        m_scanner.SetTransferRunning(false);
        m_scanner.Wake();
    }

    void AbortRun(const char* why, bool restoreCursor) {
        if (why) ctx()->Log.Info(why);
        m_transfer.Abort(restoreCursor);
        EndRun();
    }

    // TAKE-only checks on each new reading: stash closed or another tab, bag full.
    bool CheckTakeRun(const QuickStashGame::ScanSnapshot& snap) {
        if (snap.seq == m_lastCheckedSeq) return true;
        m_lastCheckedSeq = snap.seq;
        if (m_settings.verifyPanelsOpen) {
            const bool same = snap.active && snap.stashOpen && snap.stashId == m_transfer.WithdrawStashId();
            m_stashMissStreak = same ? 0 : m_stashMissStreak + 1;
            if (m_stashMissStreak >= 2) {
                AbortRun("Quick Stash: TAKE stopped (stash closed or another tab opened)", true);
                return false;
            }
        }
        if (snap.active && snap.hasBackpack) {
            const auto now = Clock::now();
            if (snap.backpackFreeCells > 0) {
                m_fullSince = {};
                m_fullStashCount = -1;
            } else if (m_fullSince.time_since_epoch().count() == 0 ||
                       (snap.stashOpen && snap.stashItemCount < m_fullStashCount)) {
                // Full, or still full but items keep leaving the tab (stacking): (re)start the clock.
                m_fullSince = now;
                m_fullStashCount = snap.stashOpen ? snap.stashItemCount : -1;
            } else if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_fullSince).count() >= kBackpackFullMs) {
                AbortRun("Quick Stash: TAKE stopped (backpack full)", true);
                return false;
            }
        }
        return true;
    }

    void OnFrameTick() {
        if (!m_active) return;
        if (m_transfer.IsRunning()) {
            m_capturePending = false;
            m_captureApplied = false;
            ctx()->Overlay.SetWantsOverlayInput(false);
            if (!ctx()->Game.IsForeground()) {
                AbortRun("Quick Stash: cancelled (game no longer in front)", false);
                return;
            }
            if (ctx()->Game.IsMenuVisible()) {
                AbortRun("Quick Stash: cancelled (POEFixer menu opened)", true);
                return;
            }
            const auto snap = m_scanner.Latest();
            const bool usable = Usable(snap) && snap->hasBackpack;
            if (usable && m_transfer.IsWithdrawMode() && m_transfer.IsClicking() && !CheckTakeRun(*snap)) return;
            m_transfer.Tick(ctx(), usable ? &snap->backpack : nullptr);
            if (!m_transfer.IsRunning()) EndRun();
            return;
        }

        if (!ctx()->Game.IsInGame() || !ctx()->Game.IsForeground()) {
            m_capturePending = false;
            m_captureApplied = false;
            ctx()->Overlay.SetWantsOverlayInput(false);
            return;
        }
        // Two-phase hand-over: DrawUI marks "mouse over a button" this frame;
        // the overlay input request follows it here, after the frame.
        // A frame without DrawUI (overlay hidden) drops the request.
        m_captureApplied = m_capturePending && m_drewThisFrame;
        m_drewThisFrame = false;
        ctx()->Overlay.SetWantsOverlayInput(m_captureApplied);
    }

    QuickStashConfig::Settings m_settings;
    bool m_loaded = false;
    bool m_dirty = false;
    bool m_active = false;
    QuickStashUi::SettingsUiState m_ui;
    QuickStashUi::DiagnosticsState m_diagnostics;
    QuickStashGame::Scanner m_scanner;
    QuickStashGame::TransferState m_transfer;
    QuickStashOverlay::HardwareClick m_transferClick;
    QuickStashOverlay::HardwareClick m_takeClick;
    PluginSDK::EventsService::Token m_frameToken{};
    bool m_capturePending = false;
    bool m_captureApplied = false;
    bool m_drewThisFrame = false;
    ImVec2 m_progressPos{};
    QuickStashOverlay::Metrics m_progressMetrics{};
    int m_stashMissStreak = 0;
    uint64_t m_lastCheckedSeq = 0;
    Clock::time_point m_fullSince{};
    int m_fullStashCount = -1;
};

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() { return new QuickStashPlugin(); }

extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* p) { delete p; }
