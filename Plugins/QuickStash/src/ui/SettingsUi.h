// Quick Stash settings tab (drawn below the host's Enabled / version header).
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#pragma once

#include "../Settings.h"
#include "ExclusionGrid.h"

namespace QuickStashGame { struct ScanSnapshot; }

namespace QuickStashUi {

struct SettingsUiState {
    ExclusionGridState grid;
};

// Draws every card except Diagnostics. Returns true when a setting changed.
bool DrawSettingsTab(QuickStashConfig::Settings& s, SettingsUiState& state, const QuickStashGame::ScanSnapshot* live,
                     bool transferRunning);

} // namespace QuickStashUi
