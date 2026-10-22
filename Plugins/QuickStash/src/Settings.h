// Quick Stash settings: defaults, ranges, and the JSON file in the plugin folder.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#pragma once

#include <array>
#include <filesystem>
#include <string>

namespace QuickStashConfig {

inline constexpr int kGridCols = 12;
inline constexpr int kGridRows = 5;

inline constexpr int kClickDelayMinMs      = 10;
inline constexpr int kClickDelayMaxMs      = 500;
inline constexpr int kPostClickDelayMinMs  = 0;
inline constexpr int kPostClickDelayMaxMs  = 500;
inline constexpr int kCursorSettleMinMs    = 0;
inline constexpr int kCursorSettleMaxMs    = 200;
inline constexpr int kCompletionHoldMinMs  = 0;
inline constexpr int kCompletionHoldMaxMs  = 1000;

inline constexpr float kButtonOffsetMin    = -200.0f;
inline constexpr float kButtonOffsetMax    = 200.0f;
inline constexpr int   kOverlayScaleMinPct = 70;
inline constexpr int   kOverlayScaleMaxPct = 160;

inline constexpr int kDefaultClickDelayMs     = 75;
inline constexpr int kDefaultPostClickDelayMs = 50;
inline constexpr int kDefaultCursorSettleMs   = 20;
inline constexpr int kDefaultCompletionHoldMs = 125;

// What Settings::Load found.
enum class LoadResult {
    Loaded,      // the file was read and applied
    Missing,     // no file yet (first start): defaults
    Unreadable,  // the file exists but could not be opened or read (locked, no access): defaults
    Unparsable,  // the file is not a JSON object (hand edit, truncated): defaults
};

struct Settings {
    // Backpack cells whose items Transfer leaves in the bag ([row][column]).
    std::array<std::array<bool, kGridCols>, kGridRows> ignoredCells{};

    // Click timing.
    int  clickDelayMs     = kDefaultClickDelayMs;
    int  postClickDelayMs = kDefaultPostClickDelayMs;
    int  cursorSettleMs   = kDefaultCursorSettleMs;
    int  completionHoldMs = kDefaultCompletionHoldMs;

    // Safety.
    bool cancelOnRightClick = true;
    bool verifyPanelsOpen   = true;

    // Take from stash.
    bool showTakeButton  = true;
    bool readMods        = true;
    bool showHighlights  = true;
    int  modLanguage     = 0;   // QuickStashGame::GameLanguage (0 = automatic)
    std::array<float, 4> highlightColor{0.35f, 0.78f, 1.0f, 0.92f};

    // Overlay buttons.
    float buttonOffsetX   = 0.0f;
    float buttonOffsetY   = 0.0f;
    int   overlayScalePct = 100;

    Settings() { SetLeftColumnExcluded(); }

    void SetLeftColumnExcluded() {
        ClearIgnoredCells();
        for (int y = 0; y < kGridRows; ++y)
            ignoredCells[static_cast<size_t>(y)][0] = true;
    }

    void ClearIgnoredCells() {
        for (auto& row : ignoredCells)
            row.fill(false);
    }

    void InvertIgnoredCells() {
        for (auto& row : ignoredCells)
            for (auto& cell : row)
                cell = !cell;
    }

    int IgnoredCellCount() const {
        int n = 0;
        for (const auto& row : ignoredCells)
            for (bool cell : row)
                n += cell ? 1 : 0;
        return n;
    }

    bool IsCellIgnored(int x, int y) const {
        if (x < 0 || x >= kGridCols || y < 0 || y >= kGridRows) return true;
        return ignoredCells[static_cast<size_t>(y)][static_cast<size_t>(x)];
    }

    void ResetTimings() {
        clickDelayMs     = kDefaultClickDelayMs;
        postClickDelayMs = kDefaultPostClickDelayMs;
        cursorSettleMs   = kDefaultCursorSettleMs;
        completionHoldMs = kDefaultCompletionHoldMs;
    }

    void ResetOverlay() {
        buttonOffsetX = 0.0f;
        buttonOffsetY = 0.0f;
        overlayScalePct = 100;
    }

    float OverlayScale() const { return static_cast<float>(overlayScalePct) / 100.0f; }

    // Clamps every value into its range (a hand-edited file cannot push a
    // value outside what the UI offers).
    void Sanitize();

    // The JSON text Save() writes (stable key order, 2-space indent).
    std::string ToJson() const;

    // Applies a JSON document; unknown keys are ignored, a missing or wrongly
    // typed key keeps its current value. Returns false when the text is not a
    // JSON object (nothing is changed then).
    bool FromJson(const std::string& text);

    static std::filesystem::path SettingsPath(const std::filesystem::path& pluginDir) {
        return pluginDir / "config" / "settings.json";
    }

    // Reads config/settings.json; a missing or broken file keeps the defaults.
    // A file that exists but cannot be read or parsed is copied to
    // settings.json.corrupt (when it can be copied) and is not overwritten by
    // Save() until a setting differs from the defaults it fell back to.
    LoadResult Load(const std::filesystem::path& pluginDir);

    // Writes config/settings.json atomically (temporary file + replace) when
    // its content differs from the last load or save. Never throws.
    bool Save(const std::filesystem::path& pluginDir) const;

private:
    mutable std::string m_lastWritten;
};

} // namespace QuickStashConfig
