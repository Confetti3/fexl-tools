#pragma once

// The pad, read where the game reads it: the game's own call of XInputGetState (one thunk that
// every reader in the game goes through) is detoured. Each read records the pad for the
// trainer's menu and, while the menu holds the pad in Training, gives the game a pad with
// nothing pressed. Buttons, sticks and triggers in use when the menu lets go stay hidden from the
// game until each is back at rest, so none of them acts in the fight or opens the game's pause
// menu. Outside Training the game always gets the pad as it is.

#include <cstdint>

namespace fexl::input {

// XInput's button bits, with the left stick folded into the d-pad's.
namespace button {
constexpr uint16_t Up = 0x0001, Down = 0x0002, Left = 0x0004, Right = 0x0008;
constexpr uint16_t LB = 0x0100, RB = 0x0200;
constexpr uint16_t A = 0x1000, B = 0x2000, X = 0x4000, Y = 0x8000;
}  // namespace button

// Hooks the game's XInput call; false when it could not (the menu then has the keyboard and
// mouse only). Any thread, once.
bool InstallPad();

struct PadState {
  bool connected = false;
  uint16_t buttons = 0;  // every connected pad's, together
};
// The pads as the game last read them, real presses even while the game is given nothing.
// Any thread.
PadState CurrentPad();

// Render thread, each frame: whether the menu holds the pad.
void HoldPad(bool held);

}  // namespace fexl::input
