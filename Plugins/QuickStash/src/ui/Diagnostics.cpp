// Diagnostics card for Quick Stash.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "Diagnostics.h"

#include "../game/Languages.h"
#include "../game/PanelDetector.h"
#include "../game/TextMatch.h"
#include "../game/WithdrawPlanner.h"
#include "../../../../POEFixer/plugin_sdk/PluginSDK.h"

#include <imgui.h>
#include "../../../../POEFixer/plugin_sdk/PluginUi.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <system_error>

namespace QuickStashUi {

using namespace PluginUi;

namespace {

namespace I18n = PluginSDK::I18n;

std::string Num(long long v) { return std::to_string(v); }

// A number with a fixed number of decimals for a TrF argument (the format is code).
std::string Fixed(double v, int decimals) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

constexpr size_t kMaxItems = 40;
constexpr size_t kMaxUiTexts = 400;

bool IsValidUtf8(const std::string& s) {
    size_t i = 0;
    const size_t n = s.size();
    while (i < n) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) { ++i; continue; }
        size_t len = 0;
        unsigned int cp = 0;
        if (c >= 0xC2 && c <= 0xDF)      { len = 2; cp = c & 0x1Fu; }
        else if (c >= 0xE0 && c <= 0xEF) { len = 3; cp = c & 0x0Fu; }
        else if (c >= 0xF0 && c <= 0xF4) { len = 4; cp = c & 0x07u; }
        else return false;
        if (len > n - i) return false;
        for (size_t k = 1; k < len; ++k) {
            const unsigned char cc = static_cast<unsigned char>(s[i + k]);
            if ((cc & 0xC0u) != 0x80u) return false;
            cp = (cp << 6) | (cc & 0x3Fu);
        }
        if (len == 3 && (cp < 0x800u || (cp >= 0xD800u && cp <= 0xDFFFu))) return false;
        if (len == 4 && (cp < 0x10000u || cp > 0x10FFFFu)) return false;
        i += len;
    }
    return true;
}

const char* EncodingClass(const std::string& s) {
    if (s.empty()) return "empty";
    bool high = false, question = false;
    for (const char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c >= 0x80) high = true;
        else if (c == '?') question = true;
    }
    if (!high) return question ? "ascii+?" : "ascii";
    if (!IsValidUtf8(s)) return "not-utf8";
    return question ? "utf8+?" : "utf8";
}

std::string HexBytes(const std::string& s, size_t maxBytes) {
    static const char kHex[] = "0123456789ABCDEF";
    const size_t n = std::min(s.size(), maxBytes);
    std::string out;
    out.reserve(n * 3 + 4);
    for (size_t i = 0; i < n; ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (i) out += ' ';
        out += kHex[c >> 4];
        out += kHex[c & 0x0F];
    }
    if (s.size() > n) out += " ...";
    return out;
}

std::string OneLine(const std::string& s, size_t maxBytes) {
    if (!IsValidUtf8(s)) return "(not utf8, see hex)";
    std::string out;
    out.reserve(std::min(s.size(), maxBytes) + 4);
    bool first = true;
    for (char ch : s) {
        if (ch == '\n' || ch == '\r') {
            if (!first && !out.empty() && out.back() != '|') out += '|';
            continue;
        }
        out += ch;
        first = false;
    }
    if (!out.empty() && out.back() == '|') out.pop_back();
    if (out.size() > maxBytes) {
        size_t cut = maxBytes;
        while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0u) == 0x80u) --cut;
        out.resize(cut);
        out += " ...";
    }
    return out;
}

void AppendField(std::string& r, const char* label, const std::string& value, size_t maxText) {
    r += "  ";
    r += label;
    r += " [";
    r += EncodingClass(value);
    r += "] \"";
    r += OneLine(value, maxText);
    r += '"';
    if (!value.empty()) {
        r += " hex: ";
        r += HexBytes(value, 48);
    }
    r += '\n';
}

std::string BuildReport(const DiagCapture& c, const char* version) {
    std::string r;
    r.reserve(16384);
    r += "Quick Stash ";
    r += version ? version : "";
    r += " text report\n";
    r += "Display: " + std::to_string(static_cast<int>(c.displayW)) + "x" + std::to_string(static_cast<int>(c.displayH)) + "\n";
    r += "Highlight box found: ";
    r += c.poe.found ? "yes" : "no";
    r += ", language: ";
    r += QuickStashGame::GameLanguageLabel(static_cast<int>(c.poe.language));
    r += '\n';
    AppendField(r, "filter", c.poe.filter, 160);
    r += "Open tab: " + (c.tabSummary.empty() ? std::string("none") : c.tabSummary) + "\n";
    const QuickStashGame::SearchQuery query(c.poe.filter);
    int nameHits = 0, modHits = 0;
    std::string body;
    int row = 0;
    for (const auto& it : c.items) {
        ++row;
        const std::string names = QuickStashGame::FoldCase(it.baseName + "\n" + it.uniqueName + "\n" + it.directBase);
        const bool nameHit = !query.Empty() && query.Matches(names);
        const bool modHit = !query.Empty() && query.Matches(QuickStashGame::FoldCase(it.modText));
        nameHits += nameHit ? 1 : 0;
        modHits += modHit ? 1 : 0;
        body += "#" + std::to_string(row) + " slot " + std::to_string(it.slotX) + "," + std::to_string(it.slotY) +
                " match names/mods " + (nameHit ? "1" : "0") + "/" + (modHit ? "1" : "0") + "\n";
        AppendField(body, "path", it.path, 160);
        AppendField(body, "list base", it.baseName, 160);
        AppendField(body, "direct base", it.directBase, 160);
        if (!it.uniqueName.empty()) AppendField(body, "list unique", it.uniqueName, 160);
        AppendField(body, "mods", it.modText, 600);
    }
    r += "Reported items: " + std::to_string(c.items.size()) + ", matches names/mods: " + std::to_string(nameHits) +
         "/" + std::to_string(modHits) + "\n\n";
    r += body;
    return r;
}

} // namespace

DiagCapture CaptureDiagnostics(const PluginSDK::Context* ctx, const char* version, float displayW, float displayH,
                               int languageSetting) {
    DiagCapture c;
    if (!ctx) return c;
    c.valid = true;
    c.displayW = displayW;
    c.displayH = displayH;

    const std::vector<PluginSDK::Inventory> all = ctx->Inventory.GetAll();
    c.inventoryCount = static_cast<int>(all.size());
    const PluginSDK::Inventory* bp = QuickStashGame::FindMainInventory(ctx, all);
    const int bpId = bp ? bp->InventoryId : -1;
    if (bp) {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "id %d, %d items, panel %s, grid (%.0f, %.0f) cell %.1f", bp->InventoryId,
                      static_cast<int>(bp->Items.size()), bp->Grid.Valid ? "open" : "closed", bp->Grid.GridScreenX,
                      bp->Grid.GridScreenY, bp->Grid.CellSize);
        c.backpack = buf;
    }
    for (const auto& inv : all) {
        if (inv.InventoryId == bpId) continue;
        const char* name = ctx->Inventory.GetName(inv.InventoryId);
        if (!QuickStashGame::IsGuildStashInventory(inv) && QuickStashGame::IsPlayerSlotName(name)) continue;
        if (inv.Items.empty()) continue;
        DiagInventoryRow row;
        row.id = inv.InventoryId;
        row.name = name ? name : "";
        row.total = static_cast<int>(inv.Items.size());
        for (const auto& it : inv.Items) {
            if (it.ScreenValid) ++row.screenValid;
            if (QuickStashGame::ItemOnScreen(it, displayW, displayH)) ++row.onScreen;
        }
        row.gridValid = inv.Grid.Valid;
        row.gridOnScreen = QuickStashGame::GridOnScreen(inv, displayW, displayH);
        row.gridX = inv.Grid.GridScreenX;
        row.gridY = inv.Grid.GridScreenY;
        row.cellSize = inv.Grid.CellSize;
        row.boxesX = inv.TotalBoxesX;
        row.boxesY = inv.TotalBoxesY;
        c.inventories.push_back(std::move(row));
    }

    c.uiTexts = QuickStashGame::CollectUiTexts(ctx, kMaxUiTexts, &c.uiVisited);
    c.poe = QuickStashGame::ReadPoeHighlight(ctx);

    const QuickStashGame::ModLanguage language =
        QuickStashGame::ResolveModLanguage(ctx, languageSetting, c.poe.language);

    if (const PluginSDK::Inventory* tab = QuickStashGame::FindOpenStashAny(ctx, all, bpId, displayW, displayH)) {
        const char* name = ctx->Inventory.GetName(tab->InventoryId);
        char buf[160];
        std::snprintf(buf, sizeof(buf), "id %d (%s)%s, %d items, %dx%d", tab->InventoryId, name ? name : "",
                      QuickStashGame::IsGuildStashInventory(*tab) ? " guild" : "",
                      static_cast<int>(tab->Items.size()), tab->TotalBoxesX, tab->TotalBoxesY);
        c.tabSummary = buf;
        for (const auto& it : tab->Items) {
            if (c.items.size() >= kMaxItems) break;
            DiagItemRow row;
            row.slotX = it.SlotX;
            row.slotY = it.SlotY;
            row.baseName = it.BaseTypeName;
            row.uniqueName = it.UniqueName;
            row.path = it.Path;
            if (it.Address) {
                row.directBase = ctx->Inventory.ReadItemBaseTypeName(it.Address);
                if (!row.directBase.empty()) {
                    row.modText = QuickStashGame::BuildModText(ctx, it, language).text;
                    row.corrupted = ctx->Inventory.ReadItemMods(it.Address).IsCorrupted;
                    for (const auto& kv : ctx->Inventory.ReadItemAggregatedStats(it.Address)) {
                        row.aggregated += std::to_string(kv.first) + ":" + std::to_string(kv.second) + " ";
                    }
                }
            }
            c.items.push_back(std::move(row));
        }
    }
    c.report = BuildReport(c, version);
    return c;
}

void DrawDiagnosticsCard(const PluginSDK::Context* ctx, DiagnosticsState& state, const char* version,
                         const std::filesystem::path& pluginDir, const QuickStashGame::ScanSnapshot* live,
                         uint32_t readerFaults, int languageSetting) {
    if (!BeginDiagnostics(I18n::Tr("diag_tip"), "qsDiagnostics")) return;
    Hint(I18n::Tr("diag_hint"));
    ImGui::Spacing();

    if (live) {
        const double ageMs = live->seq == 0 ? 0.0 :
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - live->at).count();
        Hint(I18n::TrF("diag_reader", { { "pass", Fixed(live->passMs, 1) },
                                        { "elements", Num(live->uiNodesVisited) },
                                        { "texts", Num(static_cast<long long>(live->textCacheSize)) },
                                        { "age", Fixed(ageMs, 0) },
                                        { "faults", Num(static_cast<long long>(readerFaults)) } })
                 .c_str());
    }

    if (ImGui::Button(I18n::TrId("diag_capture"))) {
        // The game's size: this tab may be drawn in a separate menu window, whose
        // display size is that window's own.
        const PluginSDK::ScreenSize game = ctx->Game.GetScreenSize();
        const ImVec2 disp = (game.Width > 0.0f && game.Height > 0.0f) ? ImVec2(game.Width, game.Height)
                                                                     : ImGui::GetIO().DisplaySize;
        state.capture = CaptureDiagnostics(ctx, version, disp.x, disp.y, languageSetting);
        state.copy = DiagnosticsState::CopyStatus::None;
    }
    Tip(I18n::Tr("diag_capture_tip"));
    DiagCapture& c = state.capture;
    if (c.valid) {
        ImGui::SameLine();
        if (SubtleButton(I18n::TrId("diag_copy"))) {
            ImGui::SetClipboardText(c.report.c_str());
            bool saved = false;
            if (!pluginDir.empty()) {
                std::error_code ec;
                std::filesystem::create_directories(pluginDir / "config", ec);
                std::ofstream out(pluginDir / "config" / "text-report.txt", std::ios::binary | std::ios::trunc);
                if (out) {
                    out.write(c.report.data(), static_cast<std::streamsize>(c.report.size()));
                    saved = static_cast<bool>(out);
                }
            }
            state.copiedBytes = c.report.size();
            state.copy = saved ? DiagnosticsState::CopyStatus::Saved : DiagnosticsState::CopyStatus::NotSaved;
        }
        Tip(I18n::Tr("diag_copy_tip"));
    }
    if (state.copy != DiagnosticsState::CopyStatus::None) {
        const std::string bytes = Num(static_cast<long long>(state.copiedBytes));
        Hint((state.copy == DiagnosticsState::CopyStatus::Saved
                  ? I18n::TrF("diag_copied_saved", { { "bytes", bytes } })
                  : I18n::TrF("diag_copied_not_saved", { { "bytes", bytes } }))
                 .c_str());
    }
    if (!c.valid) {
        EndDiagnostics();
        return;
    }

    ImGui::Spacing();
    ImGui::TextUnformatted(I18n::TrF("diag_display", { { "width", Fixed(c.displayW, 0) },
                                                       { "height", Fixed(c.displayH, 0) },
                                                       { "count", Num(c.inventoryCount) } })
                               .c_str());
    ImGui::TextUnformatted(
        I18n::TrF("diag_backpack", { { "value", c.backpack.empty() ? std::string(I18n::Tr("diag_not_found")) : c.backpack } })
            .c_str());
    ImGui::TextUnformatted(
        I18n::TrF("diag_open_tab", { { "value", c.tabSummary.empty() ? std::string(I18n::Tr("diag_none")) : c.tabSummary } })
            .c_str());
    ImGui::TextUnformatted(
        I18n::TrF("diag_search_box",
                  { { "state", c.poe.found ? I18n::Tr("diag_found") : I18n::Tr("diag_not_found") },
                    { "text", c.poe.filter },
                    { "encoding", EncodingClass(c.poe.filter) },
                    { "language", I18n::Tr(QuickStashGame::GameLanguageLabelKey(static_cast<int>(c.poe.language))) } })
            .c_str());

    const float tableH = ImGui::GetTextLineHeightWithSpacing() * 12.0f;
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTabBar("##qsDiagTabs")) {
        if (ImGui::BeginTabItem(I18n::TrId("diag_tab_inventories"))) {
            if (ImGui::BeginTable("##qsDiagInvTable", 8, flags, ImVec2(0.0f, tableH))) {
                // Width 0 in a table without Resizable: each column fits its
                // translated header and its cells every frame.
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn(I18n::TrId("diag_col_id"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
                ImGui::TableSetupColumn(I18n::TrId("diag_col_name"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
                ImGui::TableSetupColumn(I18n::TrId("diag_col_items"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
                ImGui::TableSetupColumn(I18n::TrId("diag_col_on_screen"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
                ImGui::TableSetupColumn(I18n::TrId("diag_col_grid"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
                ImGui::TableSetupColumn(I18n::TrId("diag_col_grid_xy"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
                ImGui::TableSetupColumn(I18n::TrId("diag_col_cell"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
                ImGui::TableSetupColumn(I18n::TrId("diag_col_boxes"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
                ImGui::TableHeadersRow();
                for (const auto& r : c.inventories) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%d", r.id);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(r.name.c_str());
                    ImGui::TableNextColumn(); ImGui::Text("%d", r.total);
                    ImGui::TableNextColumn(); ImGui::Text("%d / %d", r.onScreen, r.screenValid);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(r.gridValid ? (r.gridOnScreen ? I18n::Tr("diag_grid_on_visible") : I18n::Tr("diag_grid_on"))
                                                       : "-");
                    ImGui::TableNextColumn(); ImGui::Text("%.0f, %.0f", r.gridX, r.gridY);
                    ImGui::TableNextColumn(); ImGui::Text("%.1f", r.cellSize);
                    ImGui::TableNextColumn(); ImGui::Text("%dx%d", r.boxesX, r.boxesY);
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(I18n::TrId("diag_tab_items"))) {
            Hint(I18n::Tr("diag_items_hint"));
            if (ImGui::BeginTable("##qsDiagItemTable", 5, flags, ImVec2(0.0f, tableH))) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn(I18n::TrId("diag_col_slot"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
                ImGui::TableSetupColumn(I18n::TrId("diag_col_item_name"), ImGuiTableColumnFlags_WidthFixed,
                                        ImGui::GetFontSize() * 12.0f);
                ImGui::TableSetupColumn(I18n::TrId("diag_col_corrupted"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
                ImGui::TableSetupColumn(I18n::TrId("diag_col_mods"), ImGuiTableColumnFlags_WidthFixed,
                                        ImGui::GetFontSize() * 28.0f);
                ImGui::TableSetupColumn(I18n::TrId("diag_col_aggregated"), ImGuiTableColumnFlags_WidthFixed,
                                        ImGui::GetFontSize() * 14.0f);
                ImGui::TableHeadersRow();
                for (const auto& r : c.items) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%d,%d", r.slotX, r.slotY);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(r.uniqueName.empty() ? r.baseName.c_str() : r.uniqueName.c_str());
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(r.corrupted ? I18n::Tr("diag_yes") : "");
                    ImGui::TableNextColumn();
                    if (r.modText.empty()) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                        ImGui::TextUnformatted(I18n::Tr("diag_no_mods"));
                        ImGui::PopStyleColor();
                    }
                    else ImGui::TextUnformatted(OneLine(r.modText, 400).c_str());
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(r.aggregated.c_str());
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(I18n::TrId("diag_tab_ui"))) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextUnformatted(I18n::TrF("diag_ui_counts", { { "visited", Num(c.uiVisited) },
                                                                 { "texts", Num(static_cast<long long>(c.uiTexts.size())) } })
                                       .c_str());
            ImGui::PopStyleColor();
            if (ImGui::BeginTable("##qsDiagUiTable", 2, flags, ImVec2(0.0f, tableH))) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn(I18n::TrId("diag_col_xy"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
                ImGui::TableSetupColumn(I18n::TrId("diag_col_text"), ImGuiTableColumnFlags_WidthFixed,
                                        ImGui::GetFontSize() * 24.0f);
                ImGui::TableHeadersRow();
                for (const auto& t : c.uiTexts) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%.0f, %.0f", t.x, t.y);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(t.text.c_str());
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    EndDiagnostics();
}

} // namespace QuickStashUi
