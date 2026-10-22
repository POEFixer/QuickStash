// Game client languages for mod text (names accepted by
// InventoryService::FormatStatLocalized).
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#pragma once

namespace QuickStashGame {

enum class GameLanguage : int {
    Auto = 0,
    English,
    French,
    German,
    Portuguese,
    Russian,
    Spanish,
    Thai,
    Japanese,
    Korean,
    TraditionalChinese,
    SimplifiedChinese,
    Count
};

inline constexpr int kGameLanguageCount = static_cast<int>(GameLanguage::Count);

// Display names for the settings combo (index = GameLanguage).
inline const char* GameLanguageLabel(int index) {
    static const char* const kLabels[kGameLanguageCount] = {
        "Automatic", "English", "French", "German", "Portuguese",
        "Russian", "Spanish", "Thai", "Japanese", "Korean", "Traditional Chinese", "Simplified Chinese",
    };
    return (index >= 0 && index < kGameLanguageCount) ? kLabels[index] : kLabels[0];
}

// Name for FormatStatLocalized; nullptr for Auto.
inline const char* GameLanguageHostName(GameLanguage lang) {
    switch (lang) {
        case GameLanguage::English:            return "English";
        case GameLanguage::French:             return "French";
        case GameLanguage::German:             return "German";
        case GameLanguage::Portuguese:         return "Portuguese";
        case GameLanguage::Russian:            return "Russian";
        case GameLanguage::Spanish:            return "Spanish";
        case GameLanguage::Thai:               return "Thai";
        case GameLanguage::Japanese:           return "Japanese";
        case GameLanguage::Korean:             return "Korean";
        case GameLanguage::TraditionalChinese: return "Traditional Chinese";
        case GameLanguage::SimplifiedChinese:  return "Simplified Chinese";
        default:                               return nullptr;
    }
}

// Settings file spelling (stable, language independent).
inline const char* GameLanguageKey(int index) {
    static const char* const kKeys[kGameLanguageCount] = {
        "auto", "en", "fr", "de", "pt", "ru", "es", "th", "ja", "ko", "zh-TW", "zh-CN",
    };
    return (index >= 0 && index < kGameLanguageCount) ? kKeys[index] : kKeys[0];
}

} // namespace QuickStashGame
