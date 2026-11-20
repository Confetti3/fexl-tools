#pragma once

// The trainer's menu (F1): what is shown and how, playback, and what every colour means. Drawn as a ListMenu, so the pad, the keyboard and the
// mouse all work it. Render thread, inside ImGui's frame.

#include "Settings.hxx"
#include "Trainer.hxx"
#include "ui/Keys.hxx"
#include "ui/ListMenu.hxx"

#include <functional>

#include <cstdint>

namespace fexl::ui {

// Carries out one of the trainer's commands, as its key does (ui/TrainerUi).
using CommandRunner = std::function<void(Command)>;

// Edits `settings` in place; `open` turns false when the player closes it. view: the Training
// battle it is over. scale: the picture's height over 1080. Its playback rows run their
// commands through `run`, as the keys do.
void DrawMenu(Settings& settings, bool& open, const TrainerView& view, const MenuInput& input, bool padConnected, float scale,
              const CommandRunner& run);

}  // namespace fexl::ui
