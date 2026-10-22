// The collapsed Diagnostics card: one-click capture of what the plugin sees
// (inventories, the open tab's searchable text, the game's search box, a
// shareable text report). Nothing here runs unless the player presses Capture.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#pragma once

#include "../game/PoeHighlight.h"
#include "../game/Scanner.h"

#include <filesystem>
#include <string>
#include <vector>

namespace PluginSDK { struct Context; }

namespace QuickStashUi {

struct DiagInventoryRow {
    int id = 0;
    std::string name;
    int total = 0;
    int screenValid = 0;
    int onScreen = 0;
    bool gridValid = false;
    bool gridOnScreen = false;
    float gridX = 0.0f;
    float gridY = 0.0f;
    float cellSize = 0.0f;
    int boxesX = 0;
    int boxesY = 0;
};

struct DiagItemRow {
    int slotX = 0;
    int slotY = 0;
    std::string baseName;
    std::string uniqueName;
    std::string path;
    std::string directBase;
    std::string modText;    // one line, '|' between mod lines
    std::string aggregated; // "key:value " pairs
    bool corrupted = false;
};

struct DiagCapture {
    bool valid = false;
    float displayW = 0.0f;
    float displayH = 0.0f;
    int inventoryCount = 0;
    std::string backpack;
    std::string tabSummary;
    std::vector<DiagInventoryRow> inventories;
    std::vector<DiagItemRow> items;
    QuickStashGame::PoeHighlight poe;
    std::vector<QuickStashGame::UiText> uiTexts;
    int uiVisited = 0;
    std::string report;
};

struct DiagnosticsState {
    DiagCapture capture;
    std::string status;
};

// Reads everything once (render thread, on request only).
DiagCapture CaptureDiagnostics(const PluginSDK::Context* ctx, const char* version, float displayW, float displayH,
                               int languageSetting);

void DrawDiagnosticsCard(const PluginSDK::Context* ctx, DiagnosticsState& state, const char* version,
                         const std::filesystem::path& pluginDir, const QuickStashGame::ScanSnapshot* live,
                         uint32_t readerFaults, int languageSetting);

} // namespace QuickStashUi
