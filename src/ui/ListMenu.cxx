#include "ui/ListMenu.hxx"

#include "ui/Language.hxx"
#include "ui/Theme.hxx"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>

namespace fexl::ui {

namespace {

constexpr ImU32 Ink = IM_COL32(18, 16, 14, 255);          // text on the orange highlight
constexpr ImU32 LabelBox = IM_COL32(36, 36, 42, 235);
constexpr ImU32 ValueBox = IM_COL32(20, 20, 24, 235);
constexpr ImU32 TabIdle = IM_COL32(22, 50, 26, 240);      // the game's dark green tab
constexpr ImU32 Dim = IM_COL32(240, 240, 244, 90);

// Held directions repeat after this long, this often (seconds).
constexpr double RepeatAfter = 0.35, RepeatEvery = 0.07;

ImVec2 Add(ImVec2 a, ImVec2 b) { return ImVec2(a.x + b.x, a.y + b.y); }

ImFont* Body() { return LoadedFonts().body; }

// The game's highlight: orange on the left to gold on the right.
void Highlight(ImDrawList* draw, ImVec2 min, ImVec2 max) {
  draw->AddRectFilledMultiColor(min, max, colour::Orange, colour::Gold, colour::Gold, colour::Orange);
}

// A parallelogram leaning right by `slant`, as the game's tabs do.
void Leaning(ImDrawList* draw, ImVec2 min, ImVec2 max, float slant, ImU32 left, ImU32 right) {
  const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
  const ImVec2 a(min.x + slant, min.y), b(max.x, min.y), c(max.x - slant, max.y), d(min.x, max.y);
  draw->PrimReserve(6, 4);
  draw->PrimWriteVtx(a, uv, left);
  draw->PrimWriteVtx(b, uv, right);
  draw->PrimWriteVtx(c, uv, right);
  draw->PrimWriteVtx(d, uv, left);
  const ImDrawIdx base = ImDrawIdx(draw->_VtxCurrentIdx - 4);
  draw->PrimWriteIdx(base);
  draw->PrimWriteIdx(ImDrawIdx(base + 1));
  draw->PrimWriteIdx(ImDrawIdx(base + 2));
  draw->PrimWriteIdx(base);
  draw->PrimWriteIdx(ImDrawIdx(base + 2));
  draw->PrimWriteIdx(ImDrawIdx(base + 3));
}

void Arrow(ImDrawList* draw, ImVec2 tip, float size, int direction, ImU32 c) {
  const float back = tip.x - direction * size * 0.9f;
  draw->AddTriangleFilled(tip, ImVec2(back, tip.y - size * 0.6f), ImVec2(back, tip.y + size * 0.6f), c);
}

bool Hovered(ImVec2 min, ImVec2 max) { return ImGui::IsMouseHoveringRect(min, max, false); }

}  // namespace

MenuInput ReadMenuInput(const input::PadState& pad) {
  using namespace input::button;
  static uint16_t previous;
  static double heldSince[4], repeatedAt[4];
  const double now = ImGui::GetTime();
  const uint16_t buttons = pad.connected ? pad.buttons : 0;
  const uint16_t pressed = buttons & ~previous;
  previous = buttons;
  const auto direction = [&](int i, uint16_t bit) {
    if (pressed & bit) {
      heldSince[i] = repeatedAt[i] = now;
      return true;
    }
    if ((buttons & bit) && now - heldSince[i] >= RepeatAfter && now - repeatedAt[i] >= RepeatEvery) {
      repeatedAt[i] = now;
      return true;
    }
    return false;
  };
  const auto key = [](ImGuiKey k, bool repeat = true) { return ImGui::IsKeyPressed(k, repeat); };
  MenuInput in;
  const bool padUp = direction(0, Up), padDown = direction(1, Down), padLeft = direction(2, Left), padRight = direction(3, Right);
  const bool keyUp = key(ImGuiKey_UpArrow), keyDown = key(ImGuiKey_DownArrow), keyLeft = key(ImGuiKey_LeftArrow),
             keyRight = key(ImGuiKey_RightArrow);
  const bool keyConfirm = key(ImGuiKey_Enter, false) || key(ImGuiKey_KeypadEnter, false) || key(ImGuiKey_Space, false);
  const bool keyClose = key(ImGuiKey_Escape, false) || key(ImGuiKey_Backspace, false);
  const bool keyPrevious = key(ImGuiKey_Q, false) || key(ImGuiKey_PageUp, false);
  const bool keyNext = key(ImGuiKey_E, false) || key(ImGuiKey_PageDown, false) || key(ImGuiKey_Tab, false);
  in.up = padUp || keyUp;
  in.down = padDown || keyDown;
  in.left = padLeft || keyLeft;
  in.right = padRight || keyRight;
  in.confirm = (pressed & A) || keyConfirm;
  in.close = (pressed & B) || keyClose;
  in.previousTab = (pressed & LB) || keyPrevious;
  in.nextTab = (pressed & RB) || keyNext;
  in.pad = (pressed & (Up | Down | Left | Right | A | B | LB | RB)) != 0;
  in.keys = keyUp || keyDown || keyLeft || keyRight || keyConfirm || keyClose || keyPrevious || keyNext;
  return in;
}

void ListMenu::Begin(const char* title, std::span<const char* const> tabs, int& tab, const MenuInput& input, bool padConnected,
                     float scale) {
  input_ = input;
  if (input.pad) padHints_ = true;
  else if (input.keys) padHints_ = false;
  else if (shownTab_ < 0) padHints_ = padConnected;
  const ImGuiIO& io = ImGui::GetIO();
  const float s = scale_ = scale;
  rowHeight_ = 46 * s;
  gap_ = 5 * s;
  mouseMoved_ = io.MouseDelta.x != 0 || io.MouseDelta.y != 0;

  const float width = std::min(900 * s, io.DisplaySize.x - 40 * s), height = std::min(860 * s, io.DisplaySize.y - 110 * s);
  panelMin_ = ImVec2((io.DisplaySize.x - width) * 0.5f, 64 * s);
  panelMax_ = Add(panelMin_, ImVec2(width, height));
  ImGui::SetNextWindowPos(panelMin_);
  ImGui::SetNextWindowSize(ImVec2(width, height));
  ImGui::Begin("##fexl-training-tools", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse);
  draw_ = ImGui::GetWindowDrawList();
  draw_->AddRectFilled(panelMin_, panelMax_, IM_COL32(8, 8, 10, 238), 4 * s);
  draw_->AddRect(panelMin_, panelMax_, colour::PanelEdge, 4 * s);

  // The title, after an orange slash as the game's headings have.
  const float pad = 24 * s;
  float y = panelMin_.y + 16 * s;
  Leaning(draw_, ImVec2(panelMin_.x + pad, y + 4 * s), ImVec2(panelMin_.x + pad + 16 * s, y + 30 * s), 8 * s, colour::Orange, colour::Gold);
  ImFont* titleFont = LoadedFonts().title;
  draw_->AddText(titleFont, titleFont->FontSize * 1.2f, ImVec2(panelMin_.x + pad + 26 * s, y), colour::Text, T(title));
  y += 50 * s;

  // The tabs between the bumpers' glyphs (Q and E on the keyboard).
  const int count = int(tabs.size());
  if (input.previousTab) tab = (tab + count - 1) % count;
  if (input.nextTab) tab = (tab + 1) % count;
  const float stripHeight = 40 * s, textSize = Body()->FontSize * 0.9f;
  const HintItem previous{{Glyph::LB, Glyph::None}, "Q", ""}, next{{Glyph::RB, Glyph::None}, "E", ""};
  const float bumper = std::max(HintsWidth(stripHeight * 0.6f, {&previous, 1}, padHints_), HintsWidth(stripHeight * 0.6f, {&next, 1}, padHints_));
  const float left = panelMin_.x + pad + bumper + 10 * s, right = panelMax_.x - pad - bumper - 10 * s;
  DrawHints(draw_, ImVec2(panelMin_.x + pad, y + stripHeight * 0.2f), stripHeight * 0.6f, {&previous, 1}, padHints_);
  DrawHints(draw_, ImVec2(right + 10 * s, y + stripHeight * 0.2f), stripHeight * 0.6f, {&next, 1}, padHints_);
  const float each = (right - left) / count, slant = 12 * s;
  for (int i = 0; i < count; ++i) {
    const ImVec2 min(left + i * each + 2 * s, y), max(left + (i + 1) * each - 2 * s, y + stripHeight);
    if (Hovered(min, max) && ImGui::IsMouseClicked(0)) tab = i;
    const bool selected = i == tab;
    Leaning(draw_, min, max, slant, selected ? colour::Orange : TabIdle, selected ? colour::Gold : TabIdle);
    // A name too long for its tab (some in Japanese) is drawn smaller, to fit inside the slant.
    const char* name = T(tabs[i]);
    const float room = max.x - min.x - 2 * slant;
    const float size = std::min(textSize, textSize * room / std::max(1.0f, TextWidth(Body(), textSize, name)));
    const float w = TextWidth(Body(), size, name);
    draw_->AddText(Body(), size, ImVec2((min.x + max.x - w) * 0.5f, y + (stripHeight - size) * 0.5f), selected ? Ink : colour::Text,
                   name);
  }
  y += stripHeight + 18 * s;

  contentMin_ = ImVec2(panelMin_.x + pad, y);
  contentMax_ = ImVec2(panelMax_.x - pad, panelMax_.y - 104 * s);
  if (tab != shownTab_) {
    shownTab_ = tab;
    focus_ = 0;
    scroll_ = 0;
    lastEnabled_.clear();
  }
  // The focus moves over last frame's rows, skipping those switched off, and wraps around.
  const int rows = int(lastEnabled_.size());
  const bool any = std::find(lastEnabled_.begin(), lastEnabled_.end(), true) != lastEnabled_.end();
  if (any) {
    const auto step = [&](int from, int direction) {
      for (int i = 1; i <= rows; ++i) {
        const int at = ((from + direction * i) % rows + rows) % rows;
        if (lastEnabled_[at]) return at;
      }
      return from;
    };
    focus_ = std::clamp(focus_, 0, rows - 1);
    if (!lastEnabled_[focus_]) focus_ = step(focus_, 1);
    if (input.down) focus_ = step(focus_, 1);
    if (input.up) focus_ = step(focus_, -1);
  }
  draw_->PushClipRect(contentMin_, contentMax_, true);
  cursor_ = 0;
  rowIndex_ = 0;
  description_ = nullptr;
  pageDescription_ = nullptr;
  enabled_.clear();
}

ImVec2 ListMenu::Line(float height) {
  const ImVec2 at(contentMin_.x, contentMin_.y + cursor_ - scroll_);
  cursor_ += height + gap_;
  return at;
}

ListMenu::RowLook ListMenu::NextRow(const char* description, bool enabled) {
  const int index = rowIndex_++;
  enabled_.push_back(enabled);
  const float top = cursor_;
  const ImVec2 at = Line(rowHeight_);
  RowLook row{index == focus_, enabled, at, ImVec2(contentMax_.x, at.y + rowHeight_)};
  const bool inView = row.max.y > contentMin_.y && row.min.y < contentMax_.y;
  if (enabled && inView && mouseMoved_ && Hovered(row.min, row.max)) {
    focus_ = index;
    row.focused = true;
  }
  if (row.focused) {
    description_ = T(description);
    focusTop_ = top;
    focusBottom_ = top + rowHeight_;
  }
  return row;
}

bool ListMenu::Clicked(const RowLook& row) const {
  return row.enabled && ImGui::IsMouseClicked(0) && Hovered(row.min, row.max) && Hovered(contentMin_, contentMax_);
}

int ListMenu::ClickedArrow(const RowLook& row) const {
  if (!Clicked(row)) return 0;
  const float split = row.min.x + (row.max.x - row.min.x) * 0.56f, middle = (split + row.max.x) * 0.5f;
  const float x = ImGui::GetIO().MousePos.x;
  return x < split ? 0 : x < middle ? -1 : 1;
}

void ListMenu::DrawRow(const RowLook& row, const char* english, const char* key, const char* value, bool arrows, float fill,
                       const Swatch& swatch) {
  std::string label = T(english);
  if (key) label = label + "   " + key;
  const float s = scale_;
  const float split = row.min.x + (row.max.x - row.min.x) * 0.56f;
  const bool lit = row.focused && row.enabled;
  if (lit) {
    Highlight(draw_, row.min, row.max);
    draw_->AddRectFilled(ImVec2(split, row.min.y + 4 * s), ImVec2(row.max.x - 4 * s, row.max.y - 4 * s), IM_COL32(0, 0, 0, 40));
  } else {
    draw_->AddRectFilled(row.min, ImVec2(split - 3 * s, row.max.y), LabelBox);
    draw_->AddRectFilled(ImVec2(split, row.min.y), row.max, ValueBox);
  }
  const ImU32 text = lit ? Ink : row.enabled ? colour::Text : Dim;
  const float size = Body()->FontSize, centre = (row.min.y + row.max.y) * 0.5f;
  float x = row.min.x + 16 * s;
  if (swatch) {
    const float box = rowHeight_ * 0.5f;
    swatch(draw_, ImVec2(x, centre - box * 0.5f), box);
    x += box + 12 * s;
  }
  draw_->AddText(Body(), size, ImVec2(x, centre - size * 0.5f), text, label.c_str());
  if (fill >= 0) {
    const float from = split + 44 * s, to = row.max.x - 44 * s, y = row.max.y - 9 * s;
    draw_->AddRectFilled(ImVec2(from, y), ImVec2(to, y + 3 * s), lit ? IM_COL32(0, 0, 0, 60) : IM_COL32(255, 255, 255, 40));
    draw_->AddRectFilled(ImVec2(from, y), ImVec2(from + (to - from) * fill, y + 3 * s), lit ? Ink : colour::Orange);
  }
  const float w = TextWidth(Body(), size, value);
  draw_->AddText(Body(), size, ImVec2((split + row.max.x - w) * 0.5f, centre - size * 0.5f), text, value);
  if (arrows) {
    const ImU32 arrow = lit ? Ink : row.enabled ? colour::Muted : Dim;
    Arrow(draw_, ImVec2(split + 14 * s, centre), 9 * s, -1, arrow);
    Arrow(draw_, ImVec2(row.max.x - 14 * s, centre), 9 * s, 1, arrow);
  }
}

void ListMenu::Toggle(const char* label, const char* description, bool& value, bool enabled, const Swatch& swatch,
                      const char* key) {
  const RowLook row = NextRow(description, enabled);
  if (row.enabled && ((row.focused && (input_.confirm || input_.left || input_.right)) || Clicked(row))) value = !value;
  DrawRow(row, label, key, T(value ? "ON" : "OFF"), true, -1, swatch);
}

bool ListMenu::ToggleValue(const char* label, const char* description, bool value, bool enabled, const char* key) {
  bool changed = value;
  Toggle(label, description, changed, enabled, {}, key);
  return changed != value;
}

void ListMenu::Choice(const char* label, const char* description, int& value, std::span<const char* const> names, bool enabled) {
  const RowLook row = NextRow(description, enabled);
  const int count = int(names.size());
  int move = ClickedArrow(row);
  if (row.enabled && row.focused) move += input_.right || input_.confirm ? 1 : input_.left ? -1 : 0;
  if (row.enabled && move == 0 && Clicked(row)) move = 1;
  if (row.enabled && move) value = ((value + move) % count + count) % count;
  DrawRow(row, label, nullptr, T(names[std::clamp(value, 0, count - 1)]), true, -1, {});
}

void ListMenu::Slider(const char* label, const char* description, float& value, Range range, const char* format, bool enabled) {
  const RowLook row = NextRow(description, enabled);
  int move = ClickedArrow(row);
  if (row.enabled && row.focused) move += input_.right ? 1 : input_.left ? -1 : 0;
  if (row.enabled && move) {
    const float stepped = std::round((value + move * range.step - range.min) / range.step) * range.step + range.min;
    value = std::clamp(stepped, range.min, range.max);
  }
  char text[32];
  snprintf(text, sizeof(text), format, value);
  DrawRow(row, label, nullptr, text, true, (value - range.min) / (range.max - range.min), {});
}

bool ListMenu::Action(const char* label, const char* description, const char* value, bool enabled, const char* key) {
  const RowLook row = NextRow(description, enabled);
  const bool pressed = row.enabled && ((row.focused && input_.confirm) || Clicked(row));
  DrawRow(row, label, key, T(value), false, -1, {});
  return pressed;
}

void ListMenu::Section(const char* heading) {
  const float s = scale_, size = LoadedFonts().caption->FontSize;
  if (cursor_ > 0) cursor_ += 8 * s;
  const ImVec2 at = Line(26 * s);
  heading = T(heading);
  draw_->AddText(LoadedFonts().caption, size, ImVec2(at.x, at.y + 4 * s), colour::Gold, heading);
  const float x = at.x + TextWidth(LoadedFonts().caption, size, heading) + 12 * s, y = at.y + 4 * s + size * 0.55f;
  draw_->AddLine(ImVec2(x, y), ImVec2(contentMax_.x, y), IM_COL32(255, 200, 64, 60), 1);
}

void ListMenu::Text(const char* english) {
  const char* text = T(english);
  ImFont* font = LoadedFonts().caption;
  const float size = font->FontSize * 1.1f, wrap = contentMax_.x - contentMin_.x - 8 * scale_;
  const float height = font->CalcTextSizeA(size, FLT_MAX, wrap, text).y;
  const ImVec2 at = Line(height + 2 * scale_);
  draw_->AddText(font, size, ImVec2(at.x + 4 * scale_, at.y), colour::Muted, text, nullptr, wrap);
}

void ListMenu::KeyEntry(const Swatch& swatch, const char* name, const char* meaning) {
  const float s = scale_, height = 32 * s, size = Body()->FontSize * 0.92f, box = 20 * s;
  const ImVec2 at = Line(height);
  const float centre = at.y + height * 0.5f;
  swatch(draw_, ImVec2(at.x + 8 * s, centre - box * 0.5f), box);
  draw_->AddText(Body(), size, ImVec2(at.x + 44 * s, centre - size * 0.5f), colour::Text, T(name));
  ImFont* caption = LoadedFonts().caption;
  const float meaningSize = caption->FontSize * 1.1f;
  draw_->AddText(caption, meaningSize, ImVec2(at.x + 250 * s, centre - meaningSize * 0.5f), colour::Muted, T(meaning));
}

void ListMenu::HintLine(const HintItem& hint, bool pad) {
  const float height = 34 * scale_;
  const ImVec2 at = Line(height);
  DrawHints(draw_, ImVec2(at.x + 8 * scale_, at.y + height * 0.15f), height * 0.7f, {&hint, 1}, pad);
}

bool ListMenu::End() {
  draw_->PopClipRect();
  const float s = scale_, view = contentMax_.y - contentMin_.y;
  const bool any = std::find(enabled_.begin(), enabled_.end(), true) != enabled_.end();
  // The view follows the focus; a page with nothing to focus scrolls with up and down.
  if (any && description_) {
    if (focusTop_ < scroll_) scroll_ = focusTop_;
    if (focusBottom_ > scroll_ + view) scroll_ = focusBottom_ - view;
  } else if (!any) {
    scroll_ += (input_.down ? rowHeight_ : 0) - (input_.up ? rowHeight_ : 0);
  }
  if (Hovered(contentMin_, contentMax_)) scroll_ -= ImGui::GetIO().MouseWheel * rowHeight_;
  scroll_ = std::clamp(scroll_, 0.0f, std::max(0.0f, cursor_ - gap_ - view));
  // More above or below: a fade at that edge.
  const ImU32 clear = IM_COL32(8, 8, 10, 0), solid = IM_COL32(8, 8, 10, 238);
  if (scroll_ > 0)
    draw_->AddRectFilledMultiColor(contentMin_, ImVec2(contentMax_.x, contentMin_.y + 24 * s), solid, solid, clear, clear);
  if (scroll_ < cursor_ - gap_ - view - 1)
    draw_->AddRectFilledMultiColor(ImVec2(contentMin_.x, contentMax_.y - 24 * s), contentMax_, clear, clear, solid, solid);
  lastEnabled_ = enabled_;

  // The description bar: what the focused row does.
  const float pad = 24 * s, barTop = panelMax_.y - 92 * s;
  draw_->AddRectFilled(ImVec2(panelMin_.x + pad, barTop), ImVec2(panelMax_.x - pad, barTop + 40 * s), IM_COL32(0, 0, 0, 200));
  draw_->AddRectFilled(ImVec2(panelMin_.x + pad, barTop), ImVec2(panelMin_.x + pad + 4 * s, barTop + 40 * s), colour::Orange);
  if (const char* text = description_ ? description_ : T(pageDescription_)) {
    ImFont* caption = LoadedFonts().caption;
    const float size = caption->FontSize * 1.15f;
    draw_->AddText(caption, size, ImVec2(panelMin_.x + pad + 16 * s, barTop + (40 * s - size) * 0.5f), colour::Text, text);
  }

  // The controls, right-aligned at the foot.
  static constexpr HintItem footer[] = {
      {{Glyph::LB, Glyph::RB}, "Q/E", "Tab"},
      {{Glyph::DPad, Glyph::None}, "Arrows", "Move"},
      {{Glyph::A, Glyph::None}, "Enter", "Select"},
      {{Glyph::B, Glyph::None}, "Esc", "Close"},
  };
  const float hintSize = 26 * s, width = HintsWidth(hintSize, footer, padHints_);
  DrawHints(draw_, ImVec2(panelMax_.x - pad - width, panelMax_.y - 42 * s), hintSize, footer, padHints_);
  const bool clickedOutside = ImGui::IsMouseClicked(0) && !Hovered(panelMin_, panelMax_);
  ImGui::End();
  return input_.close || clickedOutside;
}

}  // namespace fexl::ui
