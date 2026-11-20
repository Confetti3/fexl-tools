#pragma once

// The trainer's game-thread side. One session per Training battle, owned by the game thread
// alone: its two hooks (the battle tick, where a battle begins and ends and playback holds
// frames back, and the hit check, where each frame is read) both pass the one Training gate
// first, then feed the session's frame meter, draw its spheres and run its playback. The
// overlay sees a snapshot published in one piece, and sends its commands through a mailbox
// that the battle tick empties.

#include "training/FrameMeter.hxx"

#include <cstdint>
#include <memory>

namespace fexl {

// Both hooks, or neither (false).
bool InstallTrainer();

// What the overlay draws, as of the game thread's last frame. Any thread.
struct TrainerView {
  // A Training battle, numbered from 1 for each new one; 0 when none runs.
  uint32_t battle = 0;
  // Its frames are on screen: it ran or was held a moment ago (the game's pause menu stops it).
  bool onScreen = false;
  bool paused = false;
  bool holding = false;          // paused, or held by the open menu
  bool meshesAvailable = false;  // filled spheres can be drawn
  std::shared_ptr<const training::MeterView> meter;
};
TrainerView CurrentView();

// The overlay's commands for battle `battle` (TrainerView::battle), carried out on that battle's
// next tick; a command for a battle that has since ended does nothing. Any thread.
enum class PlaybackCommand { TogglePause, Step };
void Post(uint32_t battle, PlaybackCommand command);
// The trainer's menu is open over `battle` (0: closed), which holds it when the settings say so.
void SetMenuOpen(uint32_t battle);

}  // namespace fexl
