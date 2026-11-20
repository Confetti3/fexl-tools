#include "ui/Hints.hxx"

#include "ui/Language.hxx"
#include "ui/Theme.hxx"

#include <algorithm>
#include <cfloat>

namespace fexl::ui {

namespace {

ImFont* Font() { return LoadedFonts().body; }

struct GlyphLook {
  const char* label;
  ImU32 fill;
  bool round;
};

GlyphLook Look(Glyph g) {
  switch (g) {
    case Glyph::A: return {"A", IM_COL32(92, 178, 70, 255), true};
    case Glyph::B: return {"B", IM_COL32(214, 64, 58, 255), true};
    case Glyph::LB: return {"LB", IM_COL32(58, 58, 64, 255), false};
    case Glyph::RB: return {"RB", IM_COL32(58, 58, 64, 255), false};
    default: return {"", 0, false};
  }
}

// A glyph's width: a circle for the face buttons and the d-pad, a cap sized to its label for
// the bumpers.
float GlyphWidth(Glyph g, float size) {
  const GlyphLook look = Look(g);
  if (g == Glyph::DPad || look.round) return size;
  return TextWidth(Font(), size * 0.62f, look.label) + size * 0.6f;
}

// The font has no arrows, so the d-pad is drawn as a cross.
float DrawDPad(ImDrawList* draw, ImVec2 at, float size) {
  const float cx = at.x + size * 0.5f, cy = at.y + size * 0.5f, arm = size * 0.42f, half = size * 0.13f;
  draw->AddRectFilled(ImVec2(cx - half, cy - arm), ImVec2(cx + half, cy + arm), colour::Text, half * 0.5f);
  draw->AddRectFilled(ImVec2(cx - arm, cy - half), ImVec2(cx + arm, cy + half), colour::Text, half * 0.5f);
  return size;
}

float DrawGlyph(ImDrawList* draw, ImVec2 at, float size, Glyph g) {
  if (g == Glyph::DPad) return DrawDPad(draw, at, size);
  const GlyphLook look = Look(g);
  const float w = GlyphWidth(g, size);
  if (look.round) {
    const ImVec2 centre(at.x + size * 0.5f, at.y + size * 0.5f);
    draw->AddCircleFilled(centre, size * 0.48f, look.fill);
    const float t = size * 0.66f;
    draw->AddText(Font(), t, ImVec2(centre.x - TextWidth(Font(), t, look.label) * 0.5f, centre.y - t * 0.5f), IM_COL32(255, 255, 255, 255),
                  look.label);
    return w;
  }
  const float t = size * 0.62f;
  draw->AddRectFilled(ImVec2(at.x, at.y + size * 0.1f), ImVec2(at.x + w, at.y + size * 0.9f), look.fill, size * 0.22f);
  draw->AddRect(ImVec2(at.x, at.y + size * 0.1f), ImVec2(at.x + w, at.y + size * 0.9f), IM_COL32(255, 255, 255, 60), size * 0.22f);
  draw->AddText(Font(), t, ImVec2(at.x + (w - TextWidth(Font(), t, look.label)) * 0.5f, at.y + (size - t) * 0.5f), colour::Text, look.label);
  return w;
}

float KeyWidth(float size, const char* key) { return TextWidth(Font(), size * 0.7f, key) + size * 0.6f; }

float DrawKey(ImDrawList* draw, ImVec2 at, float size, const char* key) {
  const float w = KeyWidth(size, key), t = size * 0.7f;
  draw->AddRectFilled(ImVec2(at.x, at.y + size * 0.06f), ImVec2(at.x + w, at.y + size * 0.94f), IM_COL32(236, 236, 240, 235), size * 0.15f);
  draw->AddText(Font(), t, ImVec2(at.x + (w - TextWidth(Font(), t, key)) * 0.5f, at.y + (size - t) * 0.5f), IM_COL32(16, 16, 20, 255), key);
  return w;
}

// One hint: its glyphs or key, then its action. Returns the width, drawing when `draw` is set.
float Hint(ImDrawList* draw, ImVec2 at, float size, const HintItem& hint, bool pad) {
  float x = at.x;
  const float gap = size * 0.25f;
  if (pad) {
    for (std::size_t i = 0; i < hint.pad.size() && hint.pad[i] != Glyph::None; ++i) {
      if (i > 0) {
        if (draw) draw->AddText(Font(), size, ImVec2(x, at.y), colour::Muted, "+");
        x += TextWidth(Font(), size, "+") + gap * 0.5f;
      }
      x += (draw ? DrawGlyph(draw, ImVec2(x, at.y), size, hint.pad[i]) : GlyphWidth(hint.pad[i], size)) + gap * 0.5f;
    }
  } else if (hint.key) {
    x += (draw ? DrawKey(draw, ImVec2(x, at.y), size, T(hint.key)) : KeyWidth(size, T(hint.key))) + gap;
  }
  const char* action = T(hint.action);
  if (draw) draw->AddText(Font(), size, ImVec2(x + gap * 0.5f, at.y), colour::Text, action);
  return x + gap * 0.5f + TextWidth(Font(), size, action) - at.x;
}

bool Shows(const HintItem& hint, bool pad) { return pad ? hint.pad[0] != Glyph::None : hint.key != nullptr; }

float Walk(ImDrawList* draw, ImVec2 at, float size, std::span<const HintItem> hints, bool pad) {
  float x = at.x;
  for (const HintItem& hint : hints) {
    if (!Shows(hint, pad)) continue;
    if (x > at.x) x += size * 1.2f;
    x += Hint(draw, ImVec2(x, at.y), size, hint, pad);
  }
  return x - at.x;
}

}  // namespace

float DrawHints(ImDrawList* draw, ImVec2 at, float size, std::span<const HintItem> hints, bool pad) {
  return Walk(draw, at, size, hints, pad);
}

float HintsWidth(float size, std::span<const HintItem> hints, bool pad) { return Walk(nullptr, ImVec2(0, 0), size, hints, pad); }

}  // namespace fexl::ui
