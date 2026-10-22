// Quick Stash settings file: tolerant parsing and atomic writes.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "Settings.h"

#include "game/Languages.h"

#include <Windows.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <system_error>

namespace QuickStashConfig {
namespace {

using nlohmann::json;

bool ReadBool(const json& j, const char* key, bool fallback) {
    const auto it = j.find(key);
    return (it != j.end() && it->is_boolean()) ? it->get<bool>() : fallback;
}

int ReadInt(const json& j, const char* key, int fallback, int lo, int hi) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_number()) return fallback;
    const double v = it->get<double>();
    if (!std::isfinite(v)) return fallback;
    return static_cast<int>(std::clamp(v, static_cast<double>(lo), static_cast<double>(hi)));
}

float ReadFloat(const json& j, const char* key, float fallback, float lo, float hi) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_number()) return fallback;
    const double v = it->get<double>();
    if (!std::isfinite(v)) return fallback;
    return static_cast<float>(std::clamp(v, static_cast<double>(lo), static_cast<double>(hi)));
}

float Clamp01(float v) {
    return std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 1.0f;
}

} // namespace

void Settings::Sanitize() {
    clickDelayMs     = std::clamp(clickDelayMs, kClickDelayMinMs, kClickDelayMaxMs);
    postClickDelayMs = std::clamp(postClickDelayMs, kPostClickDelayMinMs, kPostClickDelayMaxMs);
    cursorSettleMs   = std::clamp(cursorSettleMs, kCursorSettleMinMs, kCursorSettleMaxMs);
    completionHoldMs = std::clamp(completionHoldMs, kCompletionHoldMinMs, kCompletionHoldMaxMs);
    if (!std::isfinite(buttonOffsetX)) buttonOffsetX = 0.0f;
    if (!std::isfinite(buttonOffsetY)) buttonOffsetY = 0.0f;
    buttonOffsetX = std::clamp(buttonOffsetX, kButtonOffsetMin, kButtonOffsetMax);
    buttonOffsetY = std::clamp(buttonOffsetY, kButtonOffsetMin, kButtonOffsetMax);
    overlayScalePct = std::clamp(overlayScalePct, kOverlayScaleMinPct, kOverlayScaleMaxPct);
    if (modLanguage < 0 || modLanguage >= QuickStashGame::kGameLanguageCount) modLanguage = 0;
    for (float& c : highlightColor) c = Clamp01(c);
}

std::string Settings::ToJson() const {
    json j = json::object();
    j["click_delay_ms"]        = clickDelayMs;
    j["post_click_delay_ms"]   = postClickDelayMs;
    j["cursor_settle_ms"]      = cursorSettleMs;
    j["completion_hold_ms"]    = completionHoldMs;
    j["cancel_on_right_click"] = cancelOnRightClick;
    j["verify_panels_open"]    = verifyPanelsOpen;
    j["show_take_button"]      = showTakeButton;
    j["read_mods"]             = readMods;
    j["show_highlights"]       = showHighlights;
    j["mod_language"]          = QuickStashGame::GameLanguageKey(modLanguage);
    j["highlight_color"]       = {highlightColor[0], highlightColor[1], highlightColor[2], highlightColor[3]};
    j["button_offset_x"]       = buttonOffsetX;
    j["button_offset_y"]       = buttonOffsetY;
    j["overlay_scale_pct"]     = overlayScalePct;

    json rows = json::array();
    for (int y = 0; y < kGridRows; ++y) {
        json row = json::array();
        for (int x = 0; x < kGridCols; ++x)
            row.push_back(ignoredCells[static_cast<size_t>(y)][static_cast<size_t>(x)]);
        rows.push_back(std::move(row));
    }
    j["ignored_cells"] = std::move(rows);
    return j.dump(2, ' ', false, json::error_handler_t::replace);
}

bool Settings::FromJson(const std::string& text) {
    const json j = json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return false;

    // "enabled" and "debug_mode" (upstream 1.4.2 and older) are no longer
    // read: the host's Enabled checkbox and the Diagnostics card replace them.
    clickDelayMs     = ReadInt(j, "click_delay_ms", clickDelayMs, kClickDelayMinMs, kClickDelayMaxMs);
    postClickDelayMs = ReadInt(j, "post_click_delay_ms", postClickDelayMs, kPostClickDelayMinMs, kPostClickDelayMaxMs);
    cursorSettleMs   = ReadInt(j, "cursor_settle_ms", cursorSettleMs, kCursorSettleMinMs, kCursorSettleMaxMs);
    completionHoldMs = ReadInt(j, "completion_hold_ms", completionHoldMs, kCompletionHoldMinMs, kCompletionHoldMaxMs);
    cancelOnRightClick = ReadBool(j, "cancel_on_right_click", cancelOnRightClick);
    verifyPanelsOpen   = ReadBool(j, "verify_panels_open", verifyPanelsOpen);
    showTakeButton     = ReadBool(j, "show_take_button", showTakeButton);
    readMods           = ReadBool(j, "read_mods", readMods);
    showHighlights     = ReadBool(j, "show_highlights", showHighlights);
    if (const auto it = j.find("mod_language"); it != j.end() && it->is_string()) {
        const std::string key = it->get<std::string>();
        for (int i = 0; i < QuickStashGame::kGameLanguageCount; ++i)
            if (key == QuickStashGame::GameLanguageKey(i)) modLanguage = i;
    }
    buttonOffsetX = ReadFloat(j, "button_offset_x", buttonOffsetX, kButtonOffsetMin, kButtonOffsetMax);
    buttonOffsetY = ReadFloat(j, "button_offset_y", buttonOffsetY, kButtonOffsetMin, kButtonOffsetMax);
    overlayScalePct = ReadInt(j, "overlay_scale_pct", overlayScalePct, kOverlayScaleMinPct, kOverlayScaleMaxPct);

    if (const auto it = j.find("highlight_color"); it != j.end() && it->is_array() && it->size() == 4) {
        std::array<float, 4> c = highlightColor;
        bool ok = true;
        for (size_t i = 0; i < 4; ++i) {
            if (!(*it)[i].is_number()) { ok = false; break; }
            c[i] = Clamp01(static_cast<float>((*it)[i].get<double>()));
        }
        if (ok) highlightColor = c;
    }

    if (const auto it = j.find("ignored_cells"); it != j.end() && it->is_array()) {
        const auto& rows = *it;
        for (size_t y = 0; y < static_cast<size_t>(kGridRows) && y < rows.size(); ++y) {
            if (!rows[y].is_array()) continue;
            for (size_t x = 0; x < static_cast<size_t>(kGridCols) && x < rows[y].size(); ++x) {
                if (rows[y][x].is_boolean())
                    ignoredCells[y][x] = rows[y][x].get<bool>();
            }
        }
    }
    Sanitize();
    return true;
}

LoadResult Settings::Load(const std::filesystem::path& pluginDir) {
    LoadResult result = LoadResult::Missing;
    try {
        if (pluginDir.empty()) return result;
        const std::filesystem::path path = SettingsPath(pluginDir);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            // Missing is a first start. An error asking (no access to the folder)
            // is treated as unreadable: the file may well be there.
            if (!ec) return result;
            result = LoadResult::Unreadable;
        } else {
            std::ifstream in(path, std::ios::binary);
            if (!in.is_open()) {
                result = LoadResult::Unreadable;   // locked by another program, no access
            } else {
                const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                const bool readOk = !in.bad();
                in.close();
                if (readOk && FromJson(text)) {
                    m_lastWritten = ToJson();
                    return LoadResult::Loaded;
                }
                result = readOk ? LoadResult::Unparsable : LoadResult::Unreadable;
            }
        }
        // The file is there but gave nothing usable: keep a copy for the player and
        // treat the defaults as already written, so Save() leaves the file alone
        // until a setting is really changed.
        std::filesystem::path corrupt = path;
        corrupt += L".corrupt";
        std::filesystem::copy_file(path, corrupt, std::filesystem::copy_options::overwrite_existing, ec);
        m_lastWritten = ToJson();
    } catch (...) {
        // Keep whatever was applied; a broken file never takes the host down.
        try { m_lastWritten = ToJson(); } catch (...) {}
        if (result == LoadResult::Missing) result = LoadResult::Unreadable;
    }
    return result;
}

bool Settings::Save(const std::filesystem::path& pluginDir) const {
    try {
        if (pluginDir.empty()) return false;
        const std::string text = ToJson();
        if (text == m_lastWritten) return true;

        std::error_code ec;
        std::filesystem::create_directories(pluginDir / "config", ec);
        const std::filesystem::path target = SettingsPath(pluginDir);
        std::filesystem::path tmp = target;
        tmp += L".tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out.is_open()) return false;
            out.write(text.data(), static_cast<std::streamsize>(text.size()));
            out.flush();
            if (!out) {
                out.close();
                std::filesystem::remove(tmp, ec);
                return false;
            }
        }
        if (!::MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            std::filesystem::remove(tmp, ec);
            return false;
        }
        m_lastWritten = text;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace QuickStashConfig
