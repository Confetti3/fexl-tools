#pragma once

// The overlay's look, in Fighting EX Layer's own style: near-black panels, white italic-weight
// text, and the game's orange-to-gold highlight. Render thread.

#include <imgui.h>

namespace fexl::ui {

namespace colour {
constexpr ImU32 Text = IM_COL32(240, 240, 244, 255);
constexpr ImU32 Muted = IM_COL32(168, 172, 184, 255);
constexpr ImU32 Panel = IM_COL32(8, 8, 11, 214);
constexpr ImU32 PanelEdge = IM_COL32(255, 168, 60, 70);
constexpr ImU32 Orange = IM_COL32(244, 128, 28, 255);
constexpr ImU32 Gold = IM_COL32(255, 200, 64, 255);
constexpr ImU32 Ahead = IM_COL32(118, 224, 160, 255);   // a fighter who acts first
constexpr ImU32 Behind = IM_COL32(255, 121, 129, 255);  // a fighter who acts later
}  // namespace colour

struct Fonts {
  ImFont* body = nullptr;
  ImFont* caption = nullptr;
  ImFont* title = nullptr;  // bold italic, as the game's headings
};
const Fonts& LoadedFonts();
// A Japanese font was found and merged into the fonts (ui/Language can show Japanese).
bool JapaneseFontLoaded();

// The fonts (rasterised at this size, so text stays sharp at any resolution) and the style, for
// `scale`, the picture's height over 1080. The overlay host calls it when the size changes.
void ApplyScale(float scale);

ImVec2 TextSize(ImFont* font, const char* text);
float TextWidth(ImFont* font, float size, const char* text);
void DrawLabel(ImDrawList* draw, ImFont* font, ImVec2 at, ImU32 colour, const char* text);

}  // namespace fexl::ui
