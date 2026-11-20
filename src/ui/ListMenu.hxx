#pragma once

// A list menu laid out as Fighting EX Layer's own pause menu, made for the pad first: a tab strip
// changed with the bumpers, rows of a label box and a value box with arrows, the focused row in
// the game's orange-to-gold, a description bar for the focused row and the controls at the
// foot. Up and down move between rows, left and right change a value, A acts, B closes; the
// keyboard (arrows, Enter, Esc, Q and E) and the mouse do the same. Immediate mode: each frame
// the menu's rows are declared between Begin and End. Render thread, inside ImGui's frame.

#include "Settings.hxx"
#include "input/Pad.hxx"
#include "ui/Hints.hxx"

#include <imgui.h>

#include <functional>
#include <span>
#include <vector>

namespace fexl::ui {

// One frame's menu commands, from the pad and the keyboard.
struct MenuInput {
  bool up = false, down = false, left = false, right = false;
  bool confirm = false, close = false, previousTab = false, nextTab = false;
  bool pad = false, keys = false;  // which device gave any of them
};
// Call every frame, menu open or not, so a press is seen once, as it happens. Held directions
// repeat.
MenuInput ReadMenuInput(const input::PadState& pad);

// Draws a row's swatch in the square from `at`, `size` across.
using Swatch = std::function<void(ImDrawList* draw, ImVec2 at, float size)>;

class ListMenu {
 public:
  // The frame's menu: its panel, title and tabs. tab: the tab shown, changed by the bumpers.
  // scale: the picture's height over 1080.
  void Begin(const char* title, std::span<const char* const> tabs, int& tab, const MenuInput& input, bool padConnected, float scale);
  // Rows, each given its label and description in English (drawn in the current language). A
  // disabled row is drawn dim and skipped by the focus. key: the keyboard key that does the same,
  // shown after the label.
  void Toggle(const char* label, const char* description, bool& value, bool enabled = true, const Swatch& swatch = {},
              const char* key = nullptr);
  // Returns true when the value changed.
  bool ToggleValue(const char* label, const char* description, bool value, bool enabled = true, const char* key = nullptr);
  void Choice(const char* label, const char* description, int& value, std::span<const char* const> names, bool enabled = true);
  template <class E>
  void Choice(const char* label, const char* description, E& value, bool enabled = true) {
    static constexpr auto names = Names(E{});
    int index = int(value);
    Choice(label, description, index, names, enabled);
    value = E(index);
  }
  void Slider(const char* label, const char* description, float& value, Range range, const char* format, bool enabled = true);
  // Returns true on the frame it is pressed.
  bool Action(const char* label, const char* description, const char* value, bool enabled = true, const char* key = nullptr);
  // Lines that take no focus: a section heading, wrapped text, a key entry, a controls line.
  void Section(const char* heading);
  void Text(const char* text);
  void KeyEntry(const Swatch& swatch, const char* name, const char* meaning);
  void HintLine(const HintItem& hint, bool pad);
  // What the description bar says while no row has the focus (a page with nothing to change).
  void PageDescription(const char* text) { pageDescription_ = text; }
  // The description bar and the controls; true when the player asked to close the menu.
  bool End();


 private:
  struct RowLook {
    bool focused, enabled;
    ImVec2 min, max;
  };
  // A focusable row's place: its index and rectangle, and whether it has the focus.
  RowLook NextRow(const char* description, bool enabled);
  // label in English; value as it is drawn.
  void DrawRow(const RowLook& row, const char* label, const char* key, const char* value, bool arrows, float fill,
               const Swatch& swatch);
  bool Clicked(const RowLook& row) const;
  int ClickedArrow(const RowLook& row) const;  // -1 left half of the value box, +1 right, 0 none
  ImVec2 Line(float height);                   // a line of content: where it starts

  // This frame.
  MenuInput input_;
  ImDrawList* draw_ = nullptr;
  float scale_ = 1, rowHeight_ = 0, gap_ = 0;
  ImVec2 panelMin_, panelMax_, contentMin_, contentMax_;
  float cursor_ = 0;  // content height so far, before scrolling
  int rowIndex_ = 0;
  const char* description_ = nullptr;
  const char* pageDescription_ = nullptr;
  float focusTop_ = 0, focusBottom_ = 0;
  bool mouseMoved_ = false;
  std::vector<bool> enabled_;  // this frame's rows, focusable or not

  // Kept from frame to frame.
  int shownTab_ = -1;
  int focus_ = 0;
  float scroll_ = 0;
  std::vector<bool> lastEnabled_;  // last frame's rows, which the focus moves over
  bool padHints_ = false;
};

}  // namespace fexl::ui
