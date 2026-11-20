#pragma once

// Pause, frame step and slow motion for one Training battle, as a policy: the trainer's session
// owns one per battle and asks it on each game frame whether the battle runs. Game thread only;
// the overlay's commands reach it through the trainer's mailbox (Trainer.hxx).

#include "Settings.hxx"

namespace fexl {

class Playback {
 public:
  void TogglePause() { paused_ = !paused_; }
  void Step() { ++steps_; }  // while held: let one battle frame run
  void SetMenuOpen(bool open) { menuOpen_ = open; }

  bool Paused() const { return paused_; }
  // The battle is held: paused, or held by the open menu.
  bool Holding(const PlaybackSettings& settings) const { return paused_ || (settings.pauseWithMenu && menuOpen_); }

  // Once per game frame of the battle: whether it runs, after holds, steps and slow motion.
  bool RunsThisFrame(const PlaybackSettings& settings);

 private:
  bool paused_ = false;
  bool menuOpen_ = false;
  int steps_ = 0;
  unsigned frames_ = 0;  // counted for slow motion
};

}  // namespace fexl
