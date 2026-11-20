#include "ui/Theme.hxx"

#include "ui/Language.hxx"

#include <windows.h>

#include <cfloat>
#include <cstddef>

namespace fexl::ui {

namespace {

Fonts s_fonts;
bool s_japaneseFont;

constexpr const char* RegularFont[] = {"C:\\Windows\\Fonts\\segoeui.ttf", "C:\\Windows\\Fonts\\arial.ttf"};
constexpr const char* HeadingFont[] = {"C:\\Windows\\Fonts\\segoeuiz.ttf", "C:\\Windows\\Fonts\\arialbi.ttf",
                                       "C:\\Windows\\Fonts\\arialbd.ttf", "C:\\Windows\\Fonts\\arial.ttf"};

template <std::size_t N>
ImFont* LoadFirst(const char* const (&paths)[N], float size) {
  for (const char* path : paths)
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
      if (ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(path, size)) return font;
  ImFontConfig config;
  config.SizePixels = size;
  return ImGui::GetIO().Fonts->AddFontDefault(&config);
}

constexpr const char* JapaneseFont[] = {"C:\\Windows\\Fonts\\YuGothM.ttc", "C:\\Windows\\Fonts\\meiryo.ttc",
                                        "C:\\Windows\\Fonts\\msgothic.ttc"};

// Only the characters the Japanese texts use, so the atlas stays small. ImGui keeps a pointer
// to the ranges, so they live as long as the trainer.
const ImWchar* JapaneseGlyphs() {
  static ImVector<ImWchar> ranges = [] {
    ImFontGlyphRangesBuilder builder;
    AddJapaneseGlyphs(builder);
    ImVector<ImWchar> built;
    builder.BuildRanges(&built);
    return built;
  }();
  return ranges.Data;
}

// The Japanese font's glyphs merged into the font just added; false when there is none.
bool MergeJapanese(float size) {
  for (const char* path : JapaneseFont) {
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) continue;
    ImFontConfig config;
    config.MergeMode = true;
    if (ImGui::GetIO().Fonts->AddFontFromFileTTF(path, size, &config, JapaneseGlyphs())) return true;
  }
  return false;
}

void LoadFonts(float scale) {
  ImGui::GetIO().Fonts->Clear();
  s_fonts.body = LoadFirst(RegularFont, 22.0f * scale);
  s_japaneseFont = MergeJapanese(22.0f * scale);
  s_fonts.caption = LoadFirst(RegularFont, 16.0f * scale);
  MergeJapanese(16.0f * scale);
  s_fonts.title = LoadFirst(HeadingFont, 24.0f * scale);
  MergeJapanese(24.0f * scale);
  ImGui::GetIO().FontDefault = s_fonts.body;
}

// The interface draws its own shapes (ui/ListMenu, ui/MeterPanel); ImGui's own sizes only need
// the scale, for its cursor and hit areas.
void ApplyStyle(float scale) {
  ImGuiStyle& style = ImGui::GetStyle();
  style = ImGuiStyle();
  style.ScaleAllSizes(scale);
}

}  // namespace

const Fonts& LoadedFonts() { return s_fonts; }

bool JapaneseFontLoaded() { return s_japaneseFont; }

void ApplyScale(float scale) {
  LoadFonts(scale);
  ApplyStyle(scale);
}

ImVec2 TextSize(ImFont* font, const char* text) { return font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text); }

float TextWidth(ImFont* font, float size, const char* text) { return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x; }

void DrawLabel(ImDrawList* draw, ImFont* font, ImVec2 at, ImU32 c, const char* text) {
  draw->AddText(font, font->FontSize, ImVec2(at.x + 1, at.y + 1), IM_COL32(0, 0, 0, 200), text);
  draw->AddText(font, font->FontSize, at, c, text);
}

}  // namespace fexl::ui
