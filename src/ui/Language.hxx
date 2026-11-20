#pragma once

// The overlay's languages: English, as the sources write it, and Japanese. Text is drawn
// through T(), which gives the current language's text for an English string; a string with no
// translation is drawn in English (and named once in the log while Japanese is shown). Render
// thread.

#include "Settings.hxx"

#include <imgui.h>

namespace fexl::ui {

// The language shown this frame: the setting, with Auto following Windows' display language,
// and English whenever no Japanese font could be loaded.
void UseLanguage(Language language, bool japaneseFont);

const char* T(const char* english);

// Every character the Japanese texts use, for the font atlas.
void AddJapaneseGlyphs(ImFontGlyphRangesBuilder& builder);

}  // namespace fexl::ui
