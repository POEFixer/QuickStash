// Game client languages for mod text (names accepted by
// InventoryService::FormatStatLocalized).
//
// Copyright (C) Omer Faruk ARPA and POEFixer contributors.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
#pragma once

#include <string_view>

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

// A lang/en.json key written where it is looked up later, at drawing time (what
// PluginI18n.h's I18n::Key does; this header stays free of the SDK so the tests
// compile it alone). tools/plugin_i18n/gen.py --gate checks every Key("...")
// literal against lang/en.json.
constexpr const char* Key(const char* key) { return key; }

// lang/en.json keys of the names shown in the settings combo (index = GameLanguage).
inline const char* GameLanguageLabelKey(int index) {
    static constexpr const char* kKeys[kGameLanguageCount] = {
        Key("language_auto"),       Key("language_english"), Key("language_french"),  Key("language_german"),
        Key("language_portuguese"), Key("language_russian"), Key("language_spanish"), Key("language_thai"),
        Key("language_japanese"),   Key("language_korean"),  Key("language_traditional_chinese"),
        Key("language_simplified_chinese"),
    };
    return (index >= 0 && index < kGameLanguageCount) ? kKeys[index] : kKeys[0];
}

// English names (the text report and the log; the settings tab shows GameLanguageLabelKey's).
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

// The language of a .csd block name, as the host names the game language
// (LanguageService::GameLanguage().csdName); Auto when it is none of these.
inline GameLanguage GameLanguageFromCsdName(std::string_view csdName) {
    for (int i = 1; i < kGameLanguageCount; ++i) {
        const char* name = GameLanguageHostName(static_cast<GameLanguage>(i));
        if (name && csdName == name) return static_cast<GameLanguage>(i);
    }
    return GameLanguage::Auto;
}

// The game's Languages.dat code of a language (LanguageService::GameString); nullptr for Auto.
inline const char* GameLanguageGameCode(GameLanguage lang) {
    switch (lang) {
        case GameLanguage::English:            return "en";
        case GameLanguage::French:             return "fr";
        case GameLanguage::German:             return "de";
        case GameLanguage::Portuguese:         return "pt-BR";
        case GameLanguage::Russian:            return "ru";
        case GameLanguage::Spanish:            return "es";
        case GameLanguage::Thai:               return "th";
        case GameLanguage::Japanese:           return "ja-JP";
        case GameLanguage::Korean:             return "ko-KR";
        case GameLanguage::TraditionalChinese: return "zh-TW";
        case GameLanguage::SimplifiedChinese:  return "zh-CN";
        default:                               return nullptr;
    }
}

// The language mod text is read in: the player's choice; Automatic takes the game language
// the host knows (desktop 371+), else the one the search label's spelling shows (older hosts).
inline GameLanguage EffectiveGameLanguage(int setting, GameLanguage hostLanguage, GameLanguage labelLanguage) {
    if (setting > 0 && setting < kGameLanguageCount) return static_cast<GameLanguage>(setting);
    if (hostLanguage != GameLanguage::Auto) return hostLanguage;
    return labelLanguage;
}

// Settings file spelling (stable, language independent).
inline const char* GameLanguageKey(int index) {
    static const char* const kKeys[kGameLanguageCount] = {
        "auto", "en", "fr", "de", "pt", "ru", "es", "th", "ja", "ko", "zh-TW", "zh-CN",
    };
    return (index >= 0 && index < kGameLanguageCount) ? kKeys[index] : kKeys[0];
}

} // namespace QuickStashGame
