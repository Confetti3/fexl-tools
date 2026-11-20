#pragma once

// The trainer's keys, each once: what TrainerUi acts on, what the menu and banners name.

#include <imgui.h>

#include <array>

namespace fexl::ui {

enum class Command { Menu, Meter, Hitboxes, Pause, Step };

struct KeyBinding {
  Command command;
  ImGuiKey key;
  const char* name;
  const char* action;
  bool repeats;  // held down, it acts again
};

inline constexpr std::array<KeyBinding, 5> KeyBindings{{
    {Command::Menu, ImGuiKey_F1, "F1", "Training tools menu", false},
    {Command::Meter, ImGuiKey_F5, "F5", "Frame meter on or off", false},
    {Command::Hitboxes, ImGuiKey_F6, "F6", "Hitboxes on or off", false},
    {Command::Pause, ImGuiKey_F7, "F7", "Pause or resume the fight", false},
    {Command::Step, ImGuiKey_F8, "F8", "Step one frame while paused", true},
}};

// The table is indexed by command: each row in its command's place.
constexpr bool InCommandOrder() {
  for (std::size_t i = 0; i < KeyBindings.size(); ++i)
    if (int(KeyBindings[i].command) != int(i)) return false;
  return true;
}
static_assert(InCommandOrder(), "KeyBindings rows must follow Command's order");

inline const char* KeyName(Command command) { return KeyBindings[int(command)].name; }

}  // namespace fexl::ui
