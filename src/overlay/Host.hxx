#pragma once

// The overlay's host, after SFV Ember's (overlay/Overlay): Dear ImGui drawn into the game's own
// frames from a hook on IDXGISwapChain::Present, with the game window's keyboard and mouse
// routed to it. It knows nothing of what is drawn: the interface plugs in as an OverlayUi.
//
// Threads: the game's window procedure runs on the game thread and Present on the render
// thread, so window messages are queued and replayed into ImGui at the start of each frame.

#include <functional>

namespace fexl::overlay {

struct OverlayUi {
  // Render thread, before a frame whose picture size changed (and before the first): the
  // interface's fonts and style for `scale`, the picture's height over 1080. The host uploads
  // the font atlas again afterwards.
  std::function<void(float scale)> applyScale;
  // Render thread, inside each ImGui frame. focused: the game's window has the keyboard.
  // Returns whether the interface takes the keyboard and mouse from the game this frame.
  std::function<bool(float scale, bool focused)> draw;
  // Game thread, for each keyboard and mouse message while the interface takes the input:
  // whether it still may (Training runs). The game gets every message when not.
  std::function<bool()> mayTakeInput;
};

// Hooks Present. Its address comes from a throwaway Direct3D 11 swap chain, which is tried again
// a few times if making one fails. False when Present is not hooked. Call from a worker thread.
// The overlay attaches to the game's window and draws only once enabled.
bool InstallHost(OverlayUi ui);
void EnableHost();

}  // namespace fexl::overlay
