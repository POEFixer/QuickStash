# Changelog

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
