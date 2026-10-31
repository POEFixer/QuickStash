# Changelog

## 1.4.8

- Built with the current plugin SDK: a controller that is not XInput reports no port (-1) even when ready.

## 1.4.7

- Follows the program language (ten languages).
- "Automatic" mod text language takes the game language POEFixer knows (POEFixer 371 and later); an older POEFixer still reads it from the search box label.
- TAKE also finds the search box by the game's own label in the game language, so clients whose label the plugin did not know work too.
- In a non-English game the search also finds the game's own word for Corrupted and the waystone reward lines (Item Rarity, Monster Pack Size and the others).

## 1.4.6

- Works with the separate menu window (POEFixer 366): the host hands the plugin the right drawing context for its settings tab and for its overlay.
- The diagnostics capture records the game's screen size, not the size of a separate menu window.

## 1.4.5

- Rebuilt with the updated plugin SDK headers (POEFixer 365 adds optional calls for UI control names, stash objects and tabs, wider entity scans and the host automation flag). No change in what the plugin does.

## 1.4.4

- The "?" next to a section title now sits in its own gap instead of on the section's line.
- Diagnostics now looks the same as in the other POEFixer plugins.
- The transfer section has a shorter title, so its "?" stays visible in a narrow window at a large font size; the status lines under "How it works" wrap instead of running off the card.

## 1.4.3

- First POEFixer release: runs smoothly even with full stash tabs; item reading moved off the game frame.
- TAKE search now works like the game's: several words, "quoted phrases", !exclusions, a|b and regular expressions.
- Searching in Russian, Korean, Chinese and other languages ignores capital letters correctly; mods can be searched in your client's language.
- TAKE waits until all items are read, stops when you open another stash tab or your backpack is full, and no longer matches hidden mod ids.
- A run stops when the POEFixer menu opens; alt-tab no longer moves your cursor in another program.
- Redesigned settings (exclusion map you can drag across, live status, Diagnostics with a copyable report) and crisper in-game buttons that scale with the game.
- Settings are saved safely and only when changed; a damaged or locked settings file is kept (copied to settings.json.corrupt) instead of being replaced by the defaults.

## 1.4.2-beta.2 and earlier

See the original project: https://github.com/omrfarukarpa/poefixer-quickstash
