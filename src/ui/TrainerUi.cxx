#include "ui/TrainerUi.hxx"

#include "Settings.hxx"
#include "Trainer.hxx"
#include "game/Battle.hxx"
#include "input/Pad.hxx"
#include "ui/Hints.hxx"
#include "ui/Keys.hxx"
#include "ui/Language.hxx"
#include "ui/ListMenu.hxx"
#include "ui/Menu.hxx"
#include "ui/MeterPanel.hxx"
#include "ui/Theme.hxx"

#include <windows.h>

#include <imgui.h>

#include <string>

namespace fexl::ui {

namespace {

// Render thread: the menu belongs to one battle; a new battle starts with it closed.
uint32_t s_battle;
bool s_menuOpen;
uint64_t s_battleSeenMs;  // when this battle first showed, for the menu reminder

// The keys, at the meter's top right.
constexpr HintItem MeterHints[] = {
    {{Glyph::None, Glyph::None}, "F1", "Training tools"},
    {{Glyph::None, Glyph::None}, "F5", "Meter"},
    {{Glyph::None, Glyph::None}, "F6", "Hitbox view"},
    {{Glyph::None, Glyph::None}, "F7", "Pause"},
};

// Paused: how to step and resume.
constexpr HintItem PausedHints[] = {
    {{Glyph::None, Glyph::None}, "F8", "Step"},
    {{Glyph::None, Glyph::None}, "F7", "Resume"},
};

void Banner(float scale, const char* english, std::span<const HintItem> hints) {
  const char* text = T(english);
  const Fonts& fonts = LoadedFonts();
  ImDrawList* draw = ImGui::GetBackgroundDrawList();
  const float hintSize = 24 * scale;
  const ImVec2 size = TextSize(fonts.title, text);
  const float hintsWidth = HintsWidth(hintSize, hints, false);
  const float width = size.x + (hintsWidth > 0 ? 28 * scale + hintsWidth : 0);
  const ImVec2 at((ImGui::GetIO().DisplaySize.x - width) * 0.5f, 118.0f * scale);
  const ImVec2 from(at.x - 18 * scale, at.y - 8 * scale), to(at.x + width + 18 * scale, at.y + size.y + 8 * scale);
  draw->AddRectFilled(from, to, colour::Panel, 4.0f * scale);
  draw->AddLine(ImVec2(from.x, to.y), to, colour::Orange, 2.0f * scale);
  DrawLabel(draw, fonts.title, at, colour::Gold, text);
  DrawHints(draw, ImVec2(at.x + size.x + 28 * scale, at.y + (size.y - hintSize) * 0.5f), hintSize, hints, false);
}

// With the meter hidden, the menu's key alone, in the corner.
void Reminder(float scale) {
  const float size = 22 * scale;
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  const std::span<const HintItem> hint(MeterHints, 1);
  const float width = HintsWidth(size, hint, false);
  const bool fresh = GetTickCount64() - s_battleSeenMs < 8000;
  ImDrawList* draw = ImGui::GetBackgroundDrawList();
  const ImVec2 at(screen.x - width - 18 * scale, screen.y - size - 14 * scale);
  draw->AddRectFilled(ImVec2(at.x - 8 * scale, at.y - 5 * scale), ImVec2(at.x + width + 8 * scale, at.y + size + 5 * scale),
                      IM_COL32(20, 19, 18, fresh ? 158 : 90), 3 * scale);
  DrawHints(draw, at, size, hint, false);
}

void Run(Command command, Settings& s, uint32_t battle) {
  switch (command) {
    case Command::Menu: s_menuOpen = !s_menuOpen; break;
    case Command::Meter: s.meter.shown = !s.meter.shown; break;
    case Command::Hitboxes: s.hitboxes.shown = !s.hitboxes.shown; break;
    case Command::Pause: Post(battle, PlaybackCommand::TogglePause); break;
    case Command::Step: Post(battle, PlaybackCommand::Step); break;
  }
}

void HandleKeys(Settings& s, uint32_t battle) {
  for (const KeyBinding& binding : KeyBindings)
    if (ImGui::IsKeyPressed(binding.key, binding.repeats)) Run(binding.command, s, battle);
}

}  // namespace

bool DrawTrainerUi(float scale, bool focused) {
  const TrainerView view = CurrentView();
  if (view.battle != s_battle) {
    s_battle = view.battle;
    s_menuOpen = false;
    s_battleSeenMs = GetTickCount64();
  }
  Settings s = CurrentSettings();
  UseLanguage(s.language, JapaneseFontLoaded());
  const input::PadState pad = input::CurrentPad();
  const MenuInput menuInput = ReadMenuInput(pad);  // every frame, so a press is seen as it happens
  // Only over a Training battle on screen: behind the game's pause menu, in other modes and in
  // the game's menus the trainer neither draws nor takes input.
  const bool active = view.onScreen && view.battle != 0 && game::InTraining();
  if (active) {
    if (focused) HandleKeys(s, view.battle);
    if (s.meter.shown)
      DrawMeterPanel(*view.meter, s.meter, scale, s.keyReminder ? std::span<const HintItem>(MeterHints) : std::span<const HintItem>());
    else if (s.keyReminder && !s_menuOpen) Reminder(scale);
    if (view.paused) Banner(scale, "PAUSED", PausedHints);
    else if (s.playback.speed != Speed::Normal) Banner(scale, Names(s.playback.speed)[int(s.playback.speed)], {});
    if (s_menuOpen)
      DrawMenu(s, s_menuOpen, view, menuInput, pad.connected, scale, [&](Command command) { Run(command, s, view.battle); });
  }
  // The menu holds its own battle while it is open, shown or not (the game's pause menu hides it).
  // While it is shown it holds the pad too, in Training only (input/Pad checks the game's state).
  SetMenuOpen(s_menuOpen ? view.battle : 0);
  input::HoldPad(active && s_menuOpen);
  StoreSettings(s);
  return active && s_menuOpen;
}

}  // namespace fexl::ui
