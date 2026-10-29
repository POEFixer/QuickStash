// Quick Stash settings tab, drawn with the plugin UI kit (PluginUi.h).
// Every text is a key of lang/en.json, looked up here in the program language
// (PluginI18n) every frame.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "SettingsUi.h"
#include "TextUtil.h"

#include "../game/Languages.h"
#include "../game/Scanner.h"

#include <imgui.h>
#include "../../../../POEFixer/imgui/IconsFontAwesome6.h"
#include "../../../../POEFixer/plugin_sdk/PluginUi.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>

namespace QuickStashUi {
namespace {

using QuickStashConfig::Settings;
using namespace PluginUi;
namespace I18n = PluginSDK::I18n;

std::string Num(int v) { return std::to_string(v); }

// Marks where a slider's own number goes in a translated text (a TrF argument); NumberFormat
// turns the first mark into the conversion (TextUtil.h).
constexpr const char* kValue = kNumberMark;

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

// A section's reset button under its slider rows: in the field column when its
// translated label fits there, else from the row's left edge (a long label in the
// field column would run past the card). Call at the start of a row.
bool ResetButton(const LabelColumn& col, const char* label) {
    if (ButtonWidth(label) <= ImGui::GetContentRegionAvail().x - col.Width()) col.RowNoWidth("");
    return SubtleButton(label);
}

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
    Heading(ICON_FA_CIRCLE_INFO, I18n::Tr("howto_title"), I18n::Tr("howto_help"));
    Hint(I18n::Tr("howto_steps"));
    ImGui::Spacing();

    const bool active = live && live->active;
    const bool bagOpen = active && live->hasBackpack && live->backpack.Grid.Valid;
    ImGui::PushTextWrapPos(0.0f);   // the status lines wrap in a narrow window
    StatusDot(bagOpen);
    ImGui::TextUnformatted(!active ? I18n::Tr("howto_waiting") :
                           bagOpen ? I18n::Tr("howto_inventory_open") : I18n::Tr("howto_inventory_closed"));
    if (bagOpen) {
        StatusDot(live->stashOpen);
        if (live->stashOpen) {
            const std::string count = Num(live->stashItemCount);
            ImGui::TextUnformatted((live->stashIsGuild ? I18n::TrF("howto_guild_tab_open", { { "count", count } })
                                                       : I18n::TrF("howto_storage_tab_open", { { "count", count } }))
                                       .c_str());
        } else {
            ImGui::TextUnformatted(I18n::Tr("howto_no_storage"));
        }
        if (live->stashOpen && live->takeScanned) {
            StatusDot(live->highlightFound);
            if (live->highlightFound) {
                std::string line = I18n::TrF("howto_take_would_grab",
                                             { { "filter", live->filter },
                                               { "count", Num(static_cast<int>(live->takeRects.size())) } });
                if (live->takePending) line += std::string(" ") + I18n::Tr("howto_still_reading");
                ImGui::TextUnformatted(line.c_str());
            } else {
                ImGui::TextUnformatted(I18n::Tr("howto_search_unreadable"));
            }
        }
    }
    if (transferRunning) {
        StatusDot(true);
        ImGui::TextUnformatted(I18n::Tr("howto_running"));
    }
    ImGui::PopTextWrapPos();
    EndCard();
}

bool DrawSectionTransfer(Settings& s, SettingsUiState& state) {
    bool changed = false;
    BeginCard("##qsTransfer");
    Heading(ICON_FA_TABLE_CELLS, I18n::Tr("transfer_title"), I18n::Tr("transfer_help"));
    Hint(I18n::Tr("transfer_hint"));
    ImGui::Spacing();
    changed |= DrawExclusionGrid(s, state.grid);
    ImGui::Spacing();

    const char* movedLabel = I18n::Tr("transfer_legend_moved");
    const char* keptLabel = I18n::Tr("transfer_legend_kept");
    LegendSwatch(movedLabel, kCellMoved);
    const float legendGap = ImGui::GetFontSize() * 1.2f;
    const float keptW = ImGui::GetFontSize() + ImGui::CalcTextSize(keptLabel).x;
    if (ImGui::GetContentRegionAvail().x > keptW + legendGap + ImGui::CalcTextSize(movedLabel).x + ImGui::GetFontSize() * 2.0f)
        ImGui::SameLine(0.0f, legendGap);
    LegendSwatch(keptLabel, kCellKept);

    ImGui::Spacing();
    WrappingButtonRow row;
    if (row.Next(I18n::TrId("transfer_preset_left"))) { s.SetLeftColumnExcluded(); changed = true; }
    Tip(I18n::Tr("transfer_preset_left_tip"));
    if (row.Next(I18n::TrId("transfer_preset_clear"))) { s.ClearIgnoredCells(); changed = true; }
    Tip(I18n::Tr("transfer_preset_clear_tip"));
    if (row.Next(I18n::TrId("transfer_preset_invert"))) { s.InvertIgnoredCells(); changed = true; }
    Tip(I18n::Tr("transfer_preset_invert_tip"));
    ImGui::PushStyleColor(ImGuiCol_Text, Colors().TextSecondary);
    ImGui::TextUnformatted(I18n::TrF("transfer_cells_kept",
                                     { { "kept", Num(s.IgnoredCellCount()) },
                                       { "total", Num(QuickStashConfig::kGridCols * QuickStashConfig::kGridRows) } })
                               .c_str());
    ImGui::PopStyleColor();
    EndCard();
    return changed;
}

bool DrawSectionTake(Settings& s) {
    bool changed = false;
    BeginCard("##qsTake");
    Heading(ICON_FA_MAGNIFYING_GLASS, I18n::Tr("take_title"), I18n::Tr("take_help"));

    changed |= Check(I18n::Tr("take_show_button"), "qsShowTake", &s.showTakeButton, I18n::Tr("take_show_button_tip"));
    ImGui::BeginDisabled(!s.showTakeButton);
    changed |= Check(I18n::Tr("take_read_mods"), "qsReadMods", &s.readMods, I18n::Tr("take_read_mods_tip"));
    {
        const char* languageLabel = I18n::Tr("take_mod_language");
        const char* colourLabel = I18n::Tr("take_outline_colour");
        const LabelColumn col({languageLabel, colourLabel});
        ImGui::BeginDisabled(!s.readMods);
        col.Row(languageLabel);
        const char* languages[QuickStashGame::kGameLanguageCount];
        for (int i = 0; i < QuickStashGame::kGameLanguageCount; ++i)
            languages[i] = I18n::Tr(QuickStashGame::GameLanguageLabelKey(i));
        changed |= Combo("###qsModLanguage", s.modLanguage, languages, QuickStashGame::kGameLanguageCount);
        Tip(I18n::Tr("take_mod_language_tip"));
        ImGui::EndDisabled();

        changed |= Check(I18n::Tr("take_outline"), "qsHighlights", &s.showHighlights, I18n::Tr("take_outline_tip"));
        ImGui::BeginDisabled(!s.showHighlights);
        col.RowNoWidth(colourLabel);
        changed |= Swatch("##qsHighlightColor", s.highlightColor.data());
        Tip(I18n::Tr("take_outline_colour_tip"));
        ImGui::EndDisabled();
    }
    ImGui::EndDisabled();
    EndCard();
    return changed;
}

bool DrawSectionOverlay(Settings& s) {
    bool changed = false;
    BeginCard("##qsOverlay");
    Heading(ICON_FA_PALETTE, I18n::Tr("buttons_title"), I18n::Tr("buttons_help"));
    Hint(I18n::Tr("buttons_hint"));
    const char* sizeLabel = I18n::Tr("buttons_size");
    const char* moveXLabel = I18n::Tr("buttons_move_x");
    const char* moveYLabel = I18n::Tr("buttons_move_y");
    const LabelColumn col({sizeLabel, moveXLabel, moveYLabel});
    const std::string pctFormat = NumberFormat(I18n::TrF("unit_percent", { { "value", kValue } }));
    const std::string pxFormat = NumberFormat(I18n::TrF("unit_px", { { "value", kValue } }), "%.0f");
    changed |= SliderIntRow(col, sizeLabel, "##qsScale", &s.overlayScalePct, QuickStashConfig::kOverlayScaleMinPct,
                            QuickStashConfig::kOverlayScaleMaxPct, pctFormat.c_str(), I18n::Tr("buttons_size_tip"));
    changed |= SliderFloatRow(col, moveXLabel, "##qsOffsetX", &s.buttonOffsetX, QuickStashConfig::kButtonOffsetMin,
                              QuickStashConfig::kButtonOffsetMax, pxFormat.c_str(), I18n::Tr("buttons_move_x_tip"));
    changed |= SliderFloatRow(col, moveYLabel, "##qsOffsetY", &s.buttonOffsetY, QuickStashConfig::kButtonOffsetMin,
                              QuickStashConfig::kButtonOffsetMax, pxFormat.c_str(), I18n::Tr("buttons_move_y_tip"));
    if (ResetButton(col, I18n::TrId("buttons_reset"))) {
        s.ResetOverlay();
        changed = true;
    }
    Tip(I18n::Tr("buttons_reset_tip"));
    EndCard();
    return changed;
}

bool DrawSectionTiming(Settings& s) {
    bool changed = false;
    BeginCard("##qsTiming");
    Heading(ICON_FA_STOPWATCH, I18n::Tr("timing_title"), I18n::Tr("timing_help"));
    const char* betweenLabel = I18n::Tr("timing_between");
    const char* afterLabel = I18n::Tr("timing_after_click");
    const char* settleLabel = I18n::Tr("timing_settle");
    const char* holdLabel = I18n::Tr("timing_hold");
    const LabelColumn col({betweenLabel, afterLabel, settleLabel, holdLabel});
    const std::string msFormat = NumberFormat(I18n::TrF("unit_ms", { { "value", kValue } }));
    changed |= SliderIntRow(col, betweenLabel, "##qsClickDelay", &s.clickDelayMs, QuickStashConfig::kClickDelayMinMs,
                            QuickStashConfig::kClickDelayMaxMs, msFormat.c_str(), I18n::Tr("timing_between_tip"));
    changed |= SliderIntRow(col, afterLabel, "##qsPostClick", &s.postClickDelayMs, QuickStashConfig::kPostClickDelayMinMs,
                            QuickStashConfig::kPostClickDelayMaxMs, msFormat.c_str(), I18n::Tr("timing_after_click_tip"));
    changed |= SliderIntRow(col, settleLabel, "##qsSettle", &s.cursorSettleMs, QuickStashConfig::kCursorSettleMinMs,
                            QuickStashConfig::kCursorSettleMaxMs, msFormat.c_str(), I18n::Tr("timing_settle_tip"));
    changed |= SliderIntRow(col, holdLabel, "##qsHold", &s.completionHoldMs, QuickStashConfig::kCompletionHoldMinMs,
                            QuickStashConfig::kCompletionHoldMaxMs, msFormat.c_str(), I18n::Tr("timing_hold_tip"));
    if (ResetButton(col, I18n::TrId("timing_reset"))) {
        s.ResetTimings();
        changed = true;
    }
    Tip(I18n::Tr("timing_reset_tip"));
    EndCard();
    return changed;
}

bool DrawSectionSafety(Settings& s) {
    bool changed = false;
    BeginCard("##qsSafety");
    Heading(ICON_FA_SHIELD_HALVED, I18n::Tr("safety_title"));
    changed |= Check(I18n::Tr("safety_right_click"), "qsCancelRight", &s.cancelOnRightClick,
                     I18n::Tr("safety_right_click_tip"));
    changed |= Check(I18n::Tr("safety_verify_panels"), "qsVerifyPanels", &s.verifyPanelsOpen,
                     I18n::Tr("safety_verify_panels_tip"));
    Hint(I18n::Tr("safety_hint"));
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
