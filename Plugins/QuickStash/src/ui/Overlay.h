// In-game overlay: the Transfer and TAKE buttons, the TAKE selection boxes,
// and the progress panel. Sizes follow the game's inventory cell size (so the
// buttons scale with the game resolution) times the player's overlay scale.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#pragma once

#include <Windows.h>
#include <imgui.h>
#include "../../../../POEFixer/plugin_sdk/PluginUi.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <vector>

namespace QuickStashOverlay {

// Backpack cell size at 1920x1080 (the reference the default sizes were made at).
inline constexpr float kReferenceCellPx = 52.0f;

struct Metrics {
    float scale = 1.0f;
    float textPx = 14.0f;
    float padX = 9.0f;
    float padY = 4.0f;
    float rounding = 3.0f;
    float height = 22.0f;
    float gap = 6.0f;
};

inline Metrics MakeMetrics(float cellSize, float userScale) {
    const float game = cellSize > 1.0f ? std::clamp(cellSize / kReferenceCellPx, 0.6f, 3.0f) : 1.0f;
    Metrics m;
    m.scale = game * std::clamp(userScale, 0.5f, 2.0f);
    m.textPx = std::round(std::clamp(14.0f * m.scale, 10.0f, 40.0f));
    m.padX = std::round(9.0f * m.scale);
    m.padY = std::round(4.0f * m.scale);
    m.rounding = std::round(3.0f * m.scale);
    m.height = m.textPx + m.padY * 2.0f;
    m.gap = std::round(6.0f * m.scale);
    return m;
}

inline ImVec2 Snap(ImVec2 p) { return ImVec2(std::floor(p.x), std::floor(p.y)); }

inline ImVec2 TextSize(const Metrics& m, const char* text) {
    ImFont* font = ImGui::GetFont();
    if (!font || !text || !*text) return ImVec2(0.0f, 0.0f);
    return font->CalcTextSizeA(m.textPx, FLT_MAX, 0.0f, text);
}

inline ImU32 WithAlpha(ImU32 c, float mul) {
    const int a = static_cast<int>(static_cast<float>((c >> IM_COL32_A_SHIFT) & 0xFF) * mul);
    return (c & ~IM_COL32_A_MASK) | (static_cast<ImU32>(std::clamp(a, 0, 255)) << IM_COL32_A_SHIFT);
}

// Game-like palette: dark leather panel, brass frame, parchment text.
inline constexpr ImU32 kPanel        = IM_COL32(20, 16, 11, 228);
inline constexpr ImU32 kPanelHover   = IM_COL32(36, 29, 19, 238);
inline constexpr ImU32 kPanelActive  = IM_COL32(12, 10, 7, 245);
inline constexpr ImU32 kFrame        = IM_COL32(122, 99, 52, 255);
inline constexpr ImU32 kFrameHover   = IM_COL32(186, 152, 86, 255);
inline constexpr ImU32 kText         = IM_COL32(214, 190, 120, 255);
inline constexpr ImU32 kTextSecond   = IM_COL32(150, 210, 255, 255);
inline constexpr ImU32 kTextDim      = IM_COL32(170, 160, 140, 255);

struct ButtonLook {
    const char* label = "";
    const char* secondary = nullptr;  // drawn after the label in a second colour
    bool enabled = true;
};

inline ImVec2 ButtonSize(const Metrics& m, const ButtonLook& look) {
    float w = TextSize(m, look.label).x;
    if (look.secondary && *look.secondary) w += m.padX * 0.8f + TextSize(m, look.secondary).x;
    w = std::max(w + m.padX * 2.0f, std::round(64.0f * m.scale));
    return ImVec2(std::round(w), m.height);
}

struct ButtonResult {
    bool clicked = false;        // ImGui click (overlay receives the mouse)
    bool hovered = false;
    bool windowHovered = false;  // our window is the one under the mouse
    ImVec2 p0{};
    ImVec2 p1{};
};

inline bool HitRect(const ImVec2& p, ImVec2 p0, ImVec2 p1) {
    return p.x >= p0.x && p.x < p1.x && p.y >= p0.y && p.y < p1.y;
}

inline ButtonResult DrawButton(const char* windowId, ImVec2 pos, const Metrics& m, const ButtonLook& look) {
    ButtonResult r;
    pos = Snap(pos);
    const ImVec2 size = ButtonSize(m, look);
    r.p0 = pos;
    r.p1 = ImVec2(pos.x + size.x, pos.y + size.y);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1.0f, 1.0f));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav |
                                   ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::Begin(windowId, nullptr, flags)) {
        r.windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        ImGui::BeginDisabled(!look.enabled);
        r.clicked = ImGui::InvisibleButton("##btn", size) && look.enabled;
        r.hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
        const bool held = ImGui::IsItemActive();
        ImGui::EndDisabled();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float alpha = look.enabled ? 1.0f : 0.5f;
        const bool hot = look.enabled && r.hovered;
        const ImU32 bg = held ? kPanelActive : (hot ? kPanelHover : kPanel);
        dl->AddRectFilled(r.p0, r.p1, WithAlpha(bg, alpha), m.rounding);
        // The frame is stroked inside the button (ImGui 1.93 default for closed shapes).
        const float stroke = std::max(1.0f, std::round(m.scale));
        dl->AddRect(r.p0, r.p1, WithAlpha(hot ? kFrameHover : kFrame, alpha), m.rounding, stroke);

        const ImVec2 ts = TextSize(m, look.label);
        float total = ts.x;
        ImVec2 ss(0.0f, 0.0f);
        if (look.secondary && *look.secondary) {
            ss = TextSize(m, look.secondary);
            total += m.padX * 0.8f + ss.x;
        }
        const float tx = r.p0.x + (size.x - total) * 0.5f;
        const float ty = r.p0.y + (size.y - ts.y) * 0.5f;
        PluginUi::TextOutlined(dl, nullptr, m.textPx, ImVec2(tx, ty), WithAlpha(look.enabled ? kText : kTextDim, alpha),
                               look.label);
        if (ss.x > 0.0f)
            PluginUi::TextOutlined(dl, nullptr, m.textPx, ImVec2(tx + ts.x + m.padX * 0.8f, ty),
                                   WithAlpha(kTextSecond, alpha), look.secondary);
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    return r;
}

// Raw-mouse click tracking for click-through overlay mode, where ImGui does
// not receive the button press. A click counts when both press and release
// happen over the rectangle and `allowed` holds at press time.
struct HardwareClick {
    bool wasDown = false;
    bool pressedOnRect = false;

    void Reset() {
        wasDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        pressedOnRect = false;
    }

    bool Update(bool allowed, ImVec2 p0, ImVec2 p1) {
        const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const bool overRect = allowed && ImGui::IsMousePosValid() && HitRect(mouse, p0, p1);
        if (down && !wasDown) pressedOnRect = overRect;
        const bool released = !down && wasDown;
        const bool clicked = released && pressedOnRect && overRect;
        if (released) pressedOnRect = false;
        wasDown = down;
        return clicked;
    }
};

// Outlines around the items TAKE will Ctrl+click.
template <typename Rect>
inline void DrawSelection(ImDrawList* dl, const std::vector<Rect>& rects, const float rgba[4], const Metrics& m) {
    if (!dl || rects.empty()) return;
    const ImU32 stroke = ImGui::ColorConvertFloat4ToU32(ImVec4(rgba[0], rgba[1], rgba[2], rgba[3]));
    const ImU32 fill = ImGui::ColorConvertFloat4ToU32(ImVec4(rgba[0], rgba[1], rgba[2], rgba[3] * 0.2f));
    const float t = std::max(1.0f, std::round(2.0f * m.scale));
    const float inset = std::max(1.0f, std::round(m.scale));
    for (const auto& r : rects) {
        if (r.w <= 2.0f * inset || r.h <= 2.0f * inset) continue;
        const ImVec2 p0(std::floor(r.x) + inset, std::floor(r.y) + inset);
        const ImVec2 p1(std::floor(r.x + r.w) - inset, std::floor(r.y + r.h) - inset);
        dl->AddRectFilled(p0, p1, fill, m.rounding);
        dl->AddRect(p0, p1, stroke, m.rounding, t);  // stroked inside the box
    }
}

// Progress panel shown in place of the buttons while a run is clicking: `line`
// ("Transferring 3 / 12") and the optional cancel `hint` are the caller's texts in
// the program language; index / total fill the bar.
inline void DrawProgress(ImDrawList* dl, ImVec2 pos, const Metrics& m, const char* line, int index, int total,
                         const char* hint) {
    if (!dl || total <= 0 || !line) return;
    if (hint && !*hint) hint = nullptr;
    const ImVec2 ts = TextSize(m, line);
    const ImVec2 hs = hint ? TextSize(m, hint) : ImVec2(0.0f, 0.0f);
    const float barH = std::max(3.0f, std::round(4.0f * m.scale));
    const float w = std::round(std::max(ts.x + (hint ? m.padX * 2.0f + hs.x : 0.0f), 140.0f * m.scale) + m.padX * 2.0f);
    const float h = std::round(m.padY * 3.0f + ts.y + barH);
    pos = Snap(pos);
    const ImVec2 p1(pos.x + w, pos.y + h);
    dl->AddRectFilled(pos, p1, kPanel, m.rounding);
    const float stroke = std::max(1.0f, std::round(m.scale));
    dl->AddRect(pos, p1, kFrame, m.rounding, stroke);  // stroked inside the panel
    PluginUi::TextOutlined(dl, nullptr, m.textPx, ImVec2(pos.x + m.padX, pos.y + m.padY), kText, line);
    if (hint) PluginUi::TextOutlined(dl, nullptr, m.textPx, ImVec2(p1.x - m.padX - hs.x, pos.y + m.padY), kTextDim, hint);
    const float bx0 = pos.x + m.padX, bx1 = p1.x - m.padX;
    const float by0 = std::floor(pos.y + m.padY * 2.0f + ts.y);
    dl->AddRectFilled(ImVec2(bx0, by0), ImVec2(bx1, by0 + barH), IM_COL32(0, 0, 0, 160), barH * 0.5f);
    const float frac = std::clamp(static_cast<float>(index) / static_cast<float>(total), 0.0f, 1.0f);
    if (frac > 0.0f)
        dl->AddRectFilled(ImVec2(bx0, by0), ImVec2(std::floor(bx0 + (bx1 - bx0) * frac), by0 + barH), kFrameHover,
                          barH * 0.5f);
}

} // namespace QuickStashOverlay
