#pragma once

// The trainer's interface over Training, each frame the game presents: the frame meter, the
// playback banner, the key reminder and the menu, with their keys. Render thread.

namespace fexl::ui {

// The overlay host's draw callback (overlay::OverlayUi::draw). focused: the game's window has the
// keyboard. Returns whether the menu takes the keyboard and mouse this frame.
bool DrawTrainerUi(float scale, bool focused);

}  // namespace fexl::ui
