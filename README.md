# Quick Stash

Current version: 1.4.3

Requires POE2Fixer 364 or newer.

Quick Stash moves items between your backpack and the open storage panel with
one click, using Ctrl+click for each item.

- **Transfer** empties your backpack into whatever panel is open on the other
  side: your stash, the guild stash, a vendor or a trade window. Cells you mark
  in the settings stay in your bag.
- **TAKE** pulls items out of the open stash tab (personal or guild) into your
  backpack. It uses the game's own "Highlight Items" search box: type there,
  and the TAKE button above the box takes every matching item.

Install it from the **Plugin Manager** (the first page of the Plugins tab), which
also keeps it up to date.

## How to use

### Transfer (backpack to the open panel)

1. Open your inventory and the panel you want to fill (stash, vendor, trade...).
2. Click **Transfer** above your backpack. The number on the button is how many
   items will move.
3. Items move row by row from the top-left. Items touching a red cell of the
   exclusion map stay in your bag.

With a vendor open, Ctrl+click sells; with a trade window open, it offers the item.

### TAKE (stash to backpack)

1. Open your inventory and a stash tab.
2. Type in the game's **Highlight Items** box under the stash. An empty box
   means "the whole tab".
3. The matching items are outlined and **TAKE (N)** appears above the box. For
   stacked items the total amount is shown too (for example `TAKE (3) x30`). On
   the guild stash the button reads **TAKE G(N)**.
4. Click **TAKE**.

The search follows the game's rules: words separated by spaces must all match
(in any order), `"quoted text"` is one phrase, `!word` excludes, `a|b` matches
either, and regular expressions work. Item names, mods (when "Search item mods
too" is on), `corrupted` and waystone reward stats (item rarity, pack size,
monster rarity, monster effectiveness, waystone drop chance) can be matched.
While the plugin is still reading item mods the button shows `...` and waits.

TAKE never touches your backpack, equipment, flasks or charms, and ignores the
Ritual "Favours" window. With a vendor window open, Ctrl+click buys: close
vendors before using TAKE.

### Stopping a run

A running Transfer or TAKE shows a progress panel in place of its button. It
stops when you:

- right-click (if "Right-click cancels" is on),
- alt-tab or the game loses focus,
- open the POEFixer menu,
- close your inventory (and, for TAKE, close the stash or open another tab) when
  "Stop when a panel closes" is on.

TAKE also stops by itself when your backpack stays full.

Turn off POEFixer's own auto-stash while you use this plugin: both use Ctrl+click.

## Settings

**How it works** shows short instructions and what the plugin sees right now
(inventory open, storage tab open, what TAKE would grab).

**Transfer**
- *Exclusion map*: click or drag across the 12x5 map of your backpack. Red cells
  keep their items in the bag. Presets: *Keep left column* (the default),
  *Transfer everything*, *Invert*.

**TAKE**
- *Show the TAKE button*: turn off if you only use Transfer (the plugin then
  stops reading the search box).
- *Search item mods too*: also match words in the items' mods.
- *Mod text language*: mod lines are always searchable in English; pick your
  game client's language to search its wording too. *Automatic* takes the
  language from the search box label.
- *Outline the items TAKE will grab* and *Outline colour*.

**Buttons on screen**
- *Button size*: size of the buttons and the progress panel (they also follow
  the game's inventory size, so they keep their place at any resolution).
- *Move left / right*, *Move up / down*: shift the Transfer button if it covers
  something. *Reset size and position* restores the defaults.

**Click timing**: raise these if items are sometimes skipped or picked up onto
the cursor (slow PC, high latency); lower them for faster runs.
- *Between items*, *After each click*, *Cursor settle*, *Hold Ctrl at the end*.
  *Reset timing* restores the defaults.

**Safety**: *Right-click cancels*, *Stop when a panel closes*.

**Diagnostics** (collapsed): for bug reports. *Capture* reads the inventories,
the open tab's items with their searchable text and the visible game UI texts
once; *Copy text report* copies a report and saves it as
`config/text-report.txt` in the plugin folder.

Settings are saved in `config/settings.json` in the plugin folder. Settings from
earlier Quick Stash versions are read and kept.

## Known limits

- TAKE can only find the search box when its text is readable; with several
  different texts on the box's row it stays hidden rather than guess.
- Localized mod wording depends on the game text the host has for that
  language; English mod text is always included.
- Chat being open is not detected: close chat before a run (Ctrl+click in chat
  links the item instead of moving it).

## Install

Install it from POE2Fixer → Plugins → Plugin Manager (one click, updates included), or download `QuickStash.zip` from [Releases](https://github.com/POEFixer/QuickStash/releases/latest) and extract it into the POE2Fixer folder (it creates `Plugins\QuickStash\`). Then enable the plugin in the Plugins tab.

## Build from source

This repository is the plugin folder from the POE2Fixer source tree (`Plugins/QuickStash/`) together with the host files it compiles against, kept at the same relative paths:

- `POEFixer/plugin_sdk/` - the POE2Fixer Plugin SDK v6 headers (`PluginAbi.h`, `PluginSDK.h`)
- `POEFixer/imgui/` - the Dear ImGui sources the DLL compiles against

1. Install Visual Studio 2022 with the "Desktop development with C++" workload (MSVC v143, Windows 10/11 SDK).
2. Open `QuickStash.sln`, select **Release | x64** and build the solution.
3. The plugin is written to `x64\Release\Plugins\QuickStash\QuickStash.dll` (Release builds carry no debug information, see `Directory.Build.targets`).
4. Copy `QuickStash.dll` into `Plugins\QuickStash\` next to the POE2Fixer executable.

## Credits

Originally created by Ömer Faruk ARPA
([omrfarukarpa/poefixer-quickstash](https://github.com/omrfarukarpa/poefixer-quickstash)).
Maintained by POEFixer since October 2026.

Licensed under GPL-3.0, see [LICENSE](LICENSE). Bundled: nlohmann/json (MIT).
