#pragma once

// Control hints, as a line of "button action" pairs: the pad's buttons drawn as Xbox glyphs
// (A green, B red, bumpers as small caps) or the keyboard's key in a
// keycap. The meter's controls line and the menu's footer both draw them. Render thread.

#include <imgui.h>

#include <array>
#include <span>

namespace fexl::ui {

enum class Glyph : unsigned char { None, A, B, LB, RB, DPad };

struct HintItem {
  std::array<Glyph, 2> pad;  // one button, or two (the bumpers)
  const char* key;           // the keyboard's equivalent; null for none
  const char* action;
};

// Draws the hints from `at`, text `size` tall; returns the width drawn.
float DrawHints(ImDrawList* draw, ImVec2 at, float size, std::span<const HintItem> hints, bool pad);
// The width DrawHints would take.
float HintsWidth(float size, std::span<const HintItem> hints, bool pad);

}  // namespace fexl::ui
