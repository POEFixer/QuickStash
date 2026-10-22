// Quick Stash settings tab, drawn with the plugin UI kit (PluginUi.h).
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "SettingsUi.h"

#include "../game/Languages.h"
#include "../game/Scanner.h"

#include <imgui.h>
#include "../../../../POEFixer/imgui/IconsFontAwesome6.h"
#include "../../../../POEFixer/plugin_sdk/PluginUi.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace QuickStashUi {
namespace {

using QuickStashConfig::Settings;
using namespace PluginUi;

// Quiet buttons laid out left to right, wrapping to a new line when the row is
// full (the kit has no flow layout).
class WrappingButtonRow {
public:
    WrappingButtonRow() : m_avail(ImGui::GetContentRegionAvail().x) {}
    bool Next(const char* label) {
        const float w = ButtonWidth(label);
        if (m_used > 0.0f) {
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            if (m_used + spacing + w <= m_avail) {
                ImGui::SameLine();
                m_used += spacing + w;
            } else {
                m_used = w;
            }
        } else {
            m_used = w;
        }
        return SubtleButton(label);
    }

private:
    float m_avail = 0.0f;
    float m_used = 0.0f;
};

// A small status dot in front of a line of live status text (the kit has none).
void StatusDot(bool ok) {
    const float r = std::floor(ImGui::GetFontSize() * 0.22f);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float cy = std::floor(p.y + ImGui::GetTextLineHeight() * 0.5f);
    const Palette pal = Colors();
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(std::floor(p.x + r), cy), r,
                                                ImGui::ColorConvertFloat4ToU32(ok ? pal.Success : pal.TextSecondary), 12);
    ImGui::Dummy(ImVec2(r * 2.0f, ImGui::GetTextLineHeight()));
    ImGui::SameLine();
}

void DrawSectionHowTo(const QuickStashGame::ScanSnapshot* live, bool transferRunning) {
    BeginCard("##qsHowTo");
    Heading(ICON_FA_CIRCLE_INFO, "How it works");
    HelpMarker("Quick Stash Ctrl+clicks items for you.\n"
               "Transfer empties your backpack into whatever panel is open on the other side "
               "(stash, guild stash, vendor, trade window).\n"
               "TAKE pulls the items matching the game's own \"Highlight Items\" search out of the open "
               "stash tab into your backpack.\n"
               "Turn off POEFixer's own auto-stash while you use this plugin: both Ctrl+click.");
    Hint("1. Open your inventory and a storage panel.\n"
         "2. Transfer: click the Transfer button above your backpack.\n"
         "3. TAKE: type in the game's search box under the stash; a TAKE button appears above the box and "
         "the matching items are outlined. Click TAKE.\n"
         "Right-click, alt-tab or opening the POEFixer menu stops a running transfer.");
    ImGui::Spacing();

    const bool active = live && live->active;
    const bool bagOpen = active && live->hasBackpack && live->backpack.Grid.Valid;
    StatusDot(bagOpen);
    ImGui::TextUnformatted(!active ? "Waiting for the game (in game and in front)" :
                           bagOpen ? "Inventory open" : "Inventory closed");
    if (bagOpen) {
        StatusDot(live->stashOpen);
        if (live->stashOpen) {
            ImGui::Text("%s tab open, %d item%s", live->stashIsGuild ? "Guild stash" : "Storage",
                        live->stashItemCount, live->stashItemCount == 1 ? "" : "s");
        } else {
            ImGui::TextUnformatted("No storage tab with items on screen");
        }
        if (live->stashOpen && live->takeScanned) {
            StatusDot(live->highlightFound);
            if (live->highlightFound) {
                ImGui::PushTextWrapPos(0.0f);
                ImGui::Text("Search \"%s\": TAKE would grab %d item%s%s", live->filter.c_str(),
                            static_cast<int>(live->takeRects.size()), live->takeRects.size() == 1 ? "" : "s",
                            live->takePending ? " (still reading)" : "");
                ImGui::PopTextWrapPos();
            } else {
                ImGui::TextUnformatted("Search box not readable (TAKE hidden)");
            }
        }
    }
    if (transferRunning) {
        StatusDot(true);
        ImGui::TextUnformatted("A transfer is running");
    }
    EndCard();
}

bool DrawSectionTransfer(Settings& s, SettingsUiState& state) {
    bool changed = false;
    BeginCard("##qsTransfer");
    Heading(ICON_FA_TABLE_CELLS, "Transfer: backpack to the open panel");
    HelpMarker("Each cell is a backpack slot. Items touching a red cell stay in your bag; "
               "everything else is Ctrl+clicked into the open panel, row by row.\n"
               "With a vendor open, Ctrl+click sells; with a trade window open, it offers the item.");
    Hint("Click or drag across cells to choose what stays in your bag.");
    ImGui::Spacing();
    changed |= DrawExclusionGrid(s, state.grid);
    ImGui::Spacing();

    LegendSwatch("Transferred", kCellMoved);
    const float legendGap = ImGui::GetFontSize() * 1.2f;
    const float keptW = ImGui::GetFontSize() + ImGui::CalcTextSize("Stays in the bag").x;
    if (ImGui::GetContentRegionAvail().x > keptW + legendGap + ImGui::CalcTextSize("Transferred").x + ImGui::GetFontSize() * 2.0f)
        ImGui::SameLine(0.0f, legendGap);
    LegendSwatch("Stays in the bag", kCellKept);

    ImGui::Spacing();
    WrappingButtonRow row;
    if (row.Next("Keep left column###qsPresetLeft")) { s.SetLeftColumnExcluded(); changed = true; }
    Tip("Keeps the items in the leftmost column (the default).");
    if (row.Next("Transfer everything###qsPresetClear")) { s.ClearIgnoredCells(); changed = true; }
    Tip("Clears every red cell.");
    if (row.Next("Invert###qsPresetInvert")) { s.InvertIgnoredCells(); changed = true; }
    Tip("Swaps red and green cells.");
    ImGui::PushStyleColor(ImGuiCol_Text, Colors().TextSecondary);
    ImGui::Text("%d of %d cells kept", s.IgnoredCellCount(), QuickStashConfig::kGridCols * QuickStashConfig::kGridRows);
    ImGui::PopStyleColor();
    EndCard();
    return changed;
}

bool DrawSectionTake(Settings& s) {
    bool changed = false;
    BeginCard("##qsTake");
    Heading(ICON_FA_MAGNIFYING_GLASS, "TAKE: stash to backpack");
    HelpMarker("TAKE reads the game's own \"Highlight Items\" box under the stash. The search works like the "
               "game's: words separated by spaces must all match, \"quoted text\" is one phrase, !word excludes, "
               "a|b matches either, and regular expressions work.\n"
               "An empty box takes the whole tab. TAKE never touches your backpack or equipment.\n"
               "Careful: with a vendor window open, Ctrl+click buys.");

    changed |= Check("Show the TAKE button", "qsShowTake", &s.showTakeButton,
                     "Shows TAKE above the game's search box while a stash tab is open.\n"
                     "Turn it off if you only use Transfer; the plugin then stops reading the search box.");
    ImGui::BeginDisabled(!s.showTakeButton);
    changed |= Check("Search item mods too", "qsReadMods", &s.readMods,
                     "Also matches words in the items' mods (\"rarity\", \"pack size\", \"corrupted\"),\n"
                     "like the game's highlight. Mods are read only while something is typed.");
    {
        const LabelColumn col({"Mod text language", "Outline colour"});
        ImGui::BeginDisabled(!s.readMods);
        col.Row("Mod text language");
        const char* languages[QuickStashGame::kGameLanguageCount];
        for (int i = 0; i < QuickStashGame::kGameLanguageCount; ++i) languages[i] = QuickStashGame::GameLanguageLabel(i);
        changed |= Combo("###qsModLanguage", s.modLanguage, languages, QuickStashGame::kGameLanguageCount);
        Tip("Mod lines are always searchable in English. Pick your game client's language to\n"
            "search its mod wording too. Automatic uses the language of the search box label.");
        ImGui::EndDisabled();

        changed |= Check("Outline the items TAKE will grab", "qsHighlights", &s.showHighlights,
                         "Draws a box around every matching item, so you see the selection before clicking.");
        ImGui::BeginDisabled(!s.showHighlights);
        col.RowNoWidth("Outline colour");
        changed |= Swatch("##qsHighlightColor", s.highlightColor.data());
        Tip("Colour and opacity of the boxes.\nThe fill uses a fifth of this opacity.");
        ImGui::EndDisabled();
    }
    ImGui::EndDisabled();
    EndCard();
    return changed;
}

bool DrawSectionOverlay(Settings& s) {
    bool changed = false;
    BeginCard("##qsOverlay");
    Heading(ICON_FA_PALETTE, "Buttons on screen");
    HelpMarker("The buttons follow the game's inventory size, so they keep their place at any resolution.");
    Hint("Move or resize the Transfer and TAKE buttons if they cover something.");
    const LabelColumn col({"Button size", "Move left / right", "Move up / down"});
    changed |= SliderIntRow(col, "Button size", "##qsScale", &s.overlayScalePct, QuickStashConfig::kOverlayScaleMinPct,
                            QuickStashConfig::kOverlayScaleMaxPct, "%d%%",
                            "Size of the Transfer and TAKE buttons and the progress panel.");
    changed |= SliderFloatRow(col, "Move left / right", "##qsOffsetX", &s.buttonOffsetX,
                              QuickStashConfig::kButtonOffsetMin, QuickStashConfig::kButtonOffsetMax, "%.0f px",
                              "Moves the Transfer button sideways from its place above the backpack.\n"
                              "Ctrl+click to type a value.");
    changed |= SliderFloatRow(col, "Move up / down", "##qsOffsetY", &s.buttonOffsetY,
                              QuickStashConfig::kButtonOffsetMin, QuickStashConfig::kButtonOffsetMax, "%.0f px",
                              "Moves the Transfer button up (negative) or down (positive).\n"
                              "Ctrl+click to type a value.");
    col.RowNoWidth("");
    if (SubtleButton("Reset size and position###qsResetOverlay")) {
        s.ResetOverlay();
        changed = true;
    }
    Tip("Back to 100% and no offset.");
    EndCard();
    return changed;
}

bool DrawSectionTiming(Settings& s) {
    bool changed = false;
    BeginCard("##qsTiming");
    Heading(ICON_FA_STOPWATCH, "Click timing");
    HelpMarker("Raise these if items are sometimes skipped or picked up onto the cursor instead of moving "
               "(slow PC, high latency). Lower them for faster runs.");
    const LabelColumn col({"Between items", "After each click", "Cursor settle", "Hold Ctrl at the end"});
    changed |= SliderIntRow(col, "Between items", "##qsClickDelay", &s.clickDelayMs, QuickStashConfig::kClickDelayMinMs,
                            QuickStashConfig::kClickDelayMaxMs, "%d ms", "Pause before moving to the next item.");
    changed |= SliderIntRow(col, "After each click", "##qsPostClick", &s.postClickDelayMs,
                            QuickStashConfig::kPostClickDelayMinMs, QuickStashConfig::kPostClickDelayMaxMs, "%d ms",
                            "Wait after a click before the next step,\nso the game registers it.");
    changed |= SliderIntRow(col, "Cursor settle", "##qsSettle", &s.cursorSettleMs, QuickStashConfig::kCursorSettleMinMs,
                            QuickStashConfig::kCursorSettleMaxMs, "%d ms",
                            "Wait after moving the cursor onto an item\nbefore clicking it.");
    changed |= SliderIntRow(col, "Hold Ctrl at the end", "##qsHold", &s.completionHoldMs,
                            QuickStashConfig::kCompletionHoldMinMs, QuickStashConfig::kCompletionHoldMaxMs, "%d ms",
                            "How long Ctrl stays pressed after the last click, so the last item still moves.\n"
                            "Never shorter than 60 ms or the \"after each click\" wait.");
    col.RowNoWidth("");
    if (SubtleButton("Reset timing###qsResetTiming")) {
        s.ResetTimings();
        changed = true;
    }
    Tip("Back to the default timing.");
    EndCard();
    return changed;
}

bool DrawSectionSafety(Settings& s) {
    bool changed = false;
    BeginCard("##qsSafety");
    Heading(ICON_FA_SHIELD_HALVED, "Safety");
    changed |= Check("Right-click cancels", "qsCancelRight", &s.cancelOnRightClick,
                     "Pressing the right mouse button stops a running transfer or TAKE.");
    changed |= Check("Stop when a panel closes", "qsVerifyPanels", &s.verifyPanelsOpen,
                     "Stops when your inventory closes, and stops TAKE when the stash closes\n"
                     "or you switch to another stash tab.");
    Hint("Always on: alt-tab, opening the POEFixer menu or losing the game window stops a run, "
         "and TAKE stops when your backpack is full.");
    EndCard();
    return changed;
}

} // namespace

bool DrawSettingsTab(Settings& s, SettingsUiState& state, const QuickStashGame::ScanSnapshot* live,
                     bool transferRunning) {
    bool changed = false;
    ImGui::Spacing();
    DrawSectionHowTo(live, transferRunning);
    ImGui::Spacing();
    changed |= DrawSectionTransfer(s, state);
    ImGui::Spacing();
    changed |= DrawSectionTake(s);
    ImGui::Spacing();
    changed |= DrawSectionOverlay(s);
    ImGui::Spacing();

    if (WideEnoughForTwoColumns() &&
        ImGui::BeginTable("##qsTimingSafety", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::TableNextColumn();
        changed |= DrawSectionTiming(s);
        ImGui::TableNextColumn();
        changed |= DrawSectionSafety(s);
        ImGui::EndTable();
    } else {
        changed |= DrawSectionTiming(s);
        ImGui::Spacing();
        changed |= DrawSectionSafety(s);
    }
    ImGui::Spacing();
    return changed;
}

} // namespace QuickStashUi
