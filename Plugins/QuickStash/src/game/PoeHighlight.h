// Reads the game's own "Highlight Items" search box from the UI tree: where
// it is (the TAKE button sits above it) and what the player typed.
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#pragma once

#include "Languages.h"

#include <string>
#include <vector>

namespace PluginSDK { struct Context; }

namespace QuickStashGame {

std::string TrimAscii(const std::string& s);

// The language of a localized "Highlight Items" label, or -1 when `raw` is
// not such a label.
int MatchHighlightAnchor(const std::string& raw);

// True for the grey hint the box shows while it is empty ("Type keywords...").
bool IsPlaceholderText(const std::string& raw);

struct UiText {
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
};

struct PoeHighlight {
    std::string  filter;                 // trimmed search text ("" = take the whole tab)
    bool         found = false;          // box located and its text read unambiguously
    float        x = 0.0f;               // label's top-left, client pixels
    float        y = 0.0f;
    GameLanguage language = GameLanguage::Auto;  // from the label's spelling
    int          nodesVisited = 0;
};

// Chooses the label and the search text among the visible UI texts. The text
// nearest to the right of the label on its row is the search; several
// different texts there, or text plus the empty-box hint, leave the result
// not found (TAKE then stays hidden rather than guessing).
PoeHighlight SelectHighlight(const std::vector<UiText>& texts);

// Walks the visible game UI (at most 6000 elements) and applies SelectHighlight.
PoeHighlight ReadPoeHighlight(const PluginSDK::Context* ctx);

// The texts ReadPoeHighlight looks at (diagnostics), at most `maxTexts`.
std::vector<UiText> CollectUiTexts(const PluginSDK::Context* ctx, size_t maxTexts, int* visited = nullptr);

} // namespace QuickStashGame
