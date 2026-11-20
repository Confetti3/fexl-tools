#include "ui/MeterPanel.hxx"

#include "ui/Language.hxx"
#include "ui/Theme.hxx"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>

namespace fexl::ui {

namespace {

using training::Invincibility;
using training::MeterCell;
using training::MeterKind;
using training::MeterView;

constexpr ImU32 EmptyCell = IM_COL32(40, 41, 46, 170);
constexpr ImU32 PanelFill = IM_COL32(12, 12, 14, 175);

const MeterKeyEntry kKey[] = {
    {MeterKind::Startup, "Startup", "The move is coming out; it cannot hit yet."},
    {MeterKind::Active, "Active", "The move can hit on these frames."},
    {MeterKind::Recovery, "Recovery", "After the hit window; the fighter cannot act yet."},
    {MeterKind::Stop, "Hit-freeze", "Both fighters freeze on contact; not counted, shown as a pink line."},
    {MeterKind::Hit, "Hitstun", "Being hit; cannot act."},
    {MeterKind::Guard, "Blockstun", "Blocking; cannot act."},
    {MeterKind::Proximity, "Guard pose", "Ready to block as an attack comes; free to act."},
    {MeterKind::Movement, "Movement", "Walking or dashing."},
    {MeterKind::Down, "Knockdown", "Knocked down or getting up."},
    {MeterKind::Unknown, "Other", "Throws and states not identified yet."},
};

const InvincibilityKeyEntry kMarks[] = {
    {Invincibility::Full, "Invincible", "Neither strikes nor throws can touch the fighter."},
    {Invincibility::Strike, "Strike-proof", "Strikes cannot hit the fighter."},
    {Invincibility::Throw, "Throw-proof", "Throws cannot catch the fighter."},
};

// Street Fighter 6's frame meter colours.
ImU32 KindColour(MeterKind kind) {
  switch (kind) {
    case MeterKind::Neutral: return IM_COL32(120, 126, 134, 150);
    case MeterKind::Movement: return IM_COL32(86, 196, 226, 235);
    case MeterKind::Startup: return IM_COL32(52, 200, 96, 245);
    case MeterKind::Active: return IM_COL32(230, 52, 74, 245);
    case MeterKind::Recovery: return IM_COL32(52, 112, 230, 245);
    case MeterKind::Stop: return IM_COL32(244, 150, 176, 245);
    case MeterKind::Proximity: return IM_COL32(240, 204, 70, 110);
    case MeterKind::Guard: return IM_COL32(242, 140, 40, 245);
    case MeterKind::Hit: return IM_COL32(242, 204, 60, 245);
    case MeterKind::Down: return IM_COL32(160, 104, 220, 245);
    default: return IM_COL32(210, 200, 168, 245);
  }
}

ImU32 InvincibilityColour(Invincibility i) {
  switch (i) {
    case Invincibility::Full: return IM_COL32(255, 255, 255, 240);
    case Invincibility::Strike: return IM_COL32(130, 210, 255, 240);
    case Invincibility::Throw: return IM_COL32(255, 222, 96, 240);
    default: return 0;
  }
}

ImU32 AdvantageColour(int frames) { return frames > 0 ? colour::Ahead : frames < 0 ? colour::Behind : colour::Text; }

std::string Signed(int frames) {
  char text[16];
  snprintf(text, sizeof(text), "%+d", frames);
  return text;
}

// The panel's measures at this frame's size.
struct Layout {
  float scale, text, small, pad, label, cell, gap, barHeight, rowGap, width, left, top, height;
};

Layout Arrange(const MeterSettings& s, int rows, float picture) {
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  Layout l{};
  // Places on screen follow the picture alone; sizes the picture and the size setting.
  l.scale = picture * s.size / 100.0f;
  l.text = 17.0f * l.scale;
  l.small = 13.0f * l.scale;
  l.pad = 9.0f * l.scale;
  l.label = 27.0f * l.scale;
  l.gap = 1.75f * l.scale;
  l.barHeight = 14.0f * l.scale;
  l.rowGap = 5.5f * l.scale;
  l.width = std::min(1120.0f * l.scale, screen.x - 60.0f * picture);
  l.cell = (l.width - 2 * l.pad - l.label) / training::FrameMeter::History - l.gap;
  const float readings = l.text + 8 * l.scale, key = s.key ? l.small + 10 * l.scale : 0;
  l.height = 2 * l.pad + readings + rows * l.barHeight + (rows - 1) * l.rowGap + 5 * l.scale + key;
  l.left = (screen.x - l.width) * 0.5f;
  // Above the game's Gougi icons, or under its data display.
  l.top = s.atTop ? 320.0f * picture : screen.y - 118.0f * picture - l.height;
  return l;
}

// "Startup 7  Active 5  Recovery 14  Total 25  On hit +1": the move the meter reads out (the
// attacker's once there was contact) and the exchange's result.
void Readings(ImDrawList* draw, const Layout& l, ImVec2 at, const MeterView& meter) {
  ImFont* font = LoadedFonts().body;
  float x = at.x;
  // Labels are given already in the current language; numbers as they are.
  const auto put = [&](const std::string& text, ImU32 c, float after) {
    draw->AddText(font, l.text, ImVec2(x, at.y), c, text.c_str());
    x += TextWidth(LoadedFonts().body, l.text, text.c_str()) + after;
  };
  const int side = meter.readingSide;
  if (side < 0) {
    put(T("Frame data shows once a move is active"), colour::Muted, 0);
    return;
  }
  const training::MoveReading& move = meter.moves[side];
  const float space = 6 * l.scale, apart = 16 * l.scale;
  if (side == 1) put(T("P2"), colour::Gold, apart);
  put(T("Startup"), colour::Muted, space);
  put(std::to_string(move.startup), colour::Text, apart);
  put(T("Active"), colour::Muted, space);
  put(move.hits > 1 ? std::to_string(move.active) + " (" + std::to_string(move.hits) + " " + T("hits") + ")" : std::to_string(move.active),
      colour::Text, apart);
  put(T("Recovery"), colour::Muted, space);
  put(move.ended ? std::to_string(move.recovery) : "--", colour::Text, apart);
  if (move.ended) {
    put(T("Total"), colour::Muted, space);
    put(std::to_string(move.Total()), colour::Text, apart);
  }
  const training::Advantage& a = meter.advantage.valid ? meter.advantage : meter.lastAdvantage;
  if (!a.valid) {
    if (move.ended) put(T("No contact"), colour::Muted, 0);
    return;
  }
  x += apart;
  put(T(a.blocked ? "On block" : "On hit"), colour::Muted, space);
  const ImU32 result = meter.advantage.valid ? AdvantageColour(a.frames) : (AdvantageColour(a.frames) & 0x80FFFFFF);
  put(Signed(a.frames) + (a.knockdown ? std::string("  ") + T("knockdown") : std::string()), result, 0);
}

// The frames the bars show, oldest first: every frame but hit-freeze, which is left out (as
// Ember leaves it) so the cells add up to the frame data. The frame after one is marked.
struct Shown {
  std::size_t index;
  bool afterFreeze;
};

const std::vector<Shown>& ShownFrames(const MeterView& meter) {
  static std::vector<Shown> shown;  // kept, so drawing does not allocate
  shown.clear();
  bool frozen = false;
  const std::size_t frames = std::min(meter.cells[0].size(), meter.cells[1].size());
  for (std::size_t i = 0; i < frames; ++i) {
    if (meter.cells[0][i].kind == MeterKind::Stop || meter.cells[1][i].kind == MeterKind::Stop) {
      frozen = true;
      continue;
    }
    shown.push_back({i, frozen});
    frozen = false;
  }
  return shown;
}

// One fighter's bar: a cell per frame, invincibility above, hit-freeze's mark between cells,
// a tick under every fifth.
void Bar(ImDrawList* draw, const Layout& l, ImVec2 origin, const std::vector<MeterCell>& cells, const std::vector<Shown>& shown) {
  const float step = l.cell + l.gap;
  for (int i = 0; i < training::FrameMeter::History; ++i) {
    const float x = origin.x + i * step;
    const bool filled = i < int(shown.size());
    const MeterCell* cell = filled ? &cells[shown[i].index] : nullptr;
    draw->AddRectFilled(ImVec2(x, origin.y), ImVec2(x + l.cell, origin.y + l.barHeight), cell ? KindColour(cell->kind) : EmptyCell,
                        1.5f * l.scale);
    if (cell && shown[i].afterFreeze)
      draw->AddRectFilled(ImVec2(x - l.gap - 1.0f * l.scale, origin.y - 2 * l.scale), ImVec2(x + 1.0f * l.scale, origin.y + l.barHeight + 2 * l.scale),
                          KindColour(MeterKind::Stop));
    if (cell && cell->invincibility != Invincibility::None)
      draw->AddRectFilled(ImVec2(x, origin.y - 4 * l.scale), ImVec2(x + l.cell, origin.y - 1.5f * l.scale),
                          InvincibilityColour(cell->invincibility));
    if ((i + 1) % 5 == 0) {
      const bool tenth = (i + 1) % 10 == 0;
      const float mx = x + l.cell + l.gap * 0.5f;
      draw->AddLine(ImVec2(mx, origin.y + l.barHeight + 1 * l.scale), ImVec2(mx, origin.y + l.barHeight + (tenth ? 4.0f : 2.5f) * l.scale),
                    IM_COL32(240, 240, 244, tenth ? 150 : 80), 1.0f * l.scale);
    }
  }
}

void KeyLine(ImDrawList* draw, const Layout& l, ImVec2 at) {
  ImFont* font = LoadedFonts().body;
  const float swatch = 9 * l.scale;
  float x = at.x;
  for (const MeterKeyEntry& entry : kKey) {
    DrawKindSwatch(draw, ImVec2(x, at.y + (l.small - swatch) * 0.5f), swatch, entry.kind);
    x += swatch + 5 * l.scale;
    const char* name = T(entry.name);
    draw->AddText(font, l.small, ImVec2(x, at.y), colour::Muted, name);
    x += TextWidth(LoadedFonts().body, l.small, name) + 12 * l.scale;
  }
}

}  // namespace

std::span<const MeterKeyEntry> MeterKey() { return kKey; }

std::span<const InvincibilityKeyEntry> InvincibilityKey() { return kMarks; }

void DrawKindSwatch(ImDrawList* draw, ImVec2 at, float size, MeterKind kind) {
  if (kind == MeterKind::Stop) {  // drawn as the bars mark it: a line between cells
    draw->AddRectFilled(ImVec2(at.x + size * 0.4f, at.y - size * 0.1f), ImVec2(at.x + size * 0.6f, at.y + size * 1.1f), KindColour(kind));
    return;
  }
  draw->AddRectFilled(at, ImVec2(at.x + size, at.y + size), KindColour(kind), size * 0.15f);
}

void DrawInvincibilitySwatch(ImDrawList* draw, ImVec2 at, float size, Invincibility invincibility) {
  const float line = std::max(2.0f, size * 0.18f);
  draw->AddRectFilled(ImVec2(at.x, at.y + line + 2), ImVec2(at.x + size, at.y + size), KindColour(MeterKind::Startup), size * 0.15f);
  draw->AddRectFilled(at, ImVec2(at.x + size, at.y + line), InvincibilityColour(invincibility));
}

void DrawMeterPanel(const MeterView& meter, const MeterSettings& s, float scale, std::span<const HintItem> hints) {
  const int rows = s.secondRow ? 2 : 1;
  const Layout l = Arrange(s, rows, scale);
  ImDrawList* draw = ImGui::GetBackgroundDrawList();  // under the menu
  draw->AddRectFilled(ImVec2(l.left, l.top), ImVec2(l.left + l.width, l.top + l.height), PanelFill, 4 * l.scale);
  float y = l.top + l.pad;
  const float left = l.left + l.pad;
  if (s.readings) Readings(draw, l, ImVec2(left, y), meter);
  if (!hints.empty()) {
    const float size = l.small * 1.15f, width = HintsWidth(size, hints, false);
    DrawHints(draw, ImVec2(l.left + l.width - l.pad - width, y + (l.text - size) * 0.5f), size, hints, false);
  }
  y += l.text + 8 * l.scale;
  const std::vector<Shown>& shown = ShownFrames(meter);
  for (int side = 0; side < rows; ++side) {
    draw->AddText(LoadedFonts().body, l.small, ImVec2(left, y + (l.barHeight - l.small) * 0.5f), colour::Muted, side == 0 ? "P1" : "P2");
    Bar(draw, l, ImVec2(left + l.label, y), meter.cells[side], shown);
    y += l.barHeight + l.rowGap;
  }
  if (s.key) KeyLine(draw, l, ImVec2(left + l.label, y + 5 * l.scale - l.rowGap + 4 * l.scale));
}

}  // namespace fexl::ui
