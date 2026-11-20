#include "Playback.hxx"

namespace fexl {

bool Playback::RunsThisFrame(const PlaybackSettings& settings) {
  if (Holding(settings)) {
    if (steps_ == 0) return false;
    --steps_;
    return true;
  }
  steps_ = 0;  // a step asked for while running is not kept for a later hold
  return ++frames_ % unsigned(Divisor(settings.speed)) == 0;
}

}  // namespace fexl
