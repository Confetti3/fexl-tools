#include "input/Pad.hxx"

#include "game/Battle.hxx"
#include "game/Build.hxx"
#include "game/Hooks.hxx"

#include <windows.h>
#include <Xinput.h>

#include <atomic>

namespace fexl::input {

namespace {

using GetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
GetStateFn o_getState;

constexpr int Slots = XUSER_MAX_COUNT;
constexpr SHORT StickLean = 16000;  // of 32767: how far the stick leans before it counts
constexpr uint32_t Connected = 0x10000;

// Any thread.
std::atomic<uint32_t> s_seen[Slots];  // Connected | the slot's buttons as last read
std::atomic<bool> s_held{false};

// What the game does not see after the menu lets go, each until it is back at rest: the
// buttons then down, each stick, each trigger. The game reads the pad from one thread.
struct Hidden {
  uint16_t buttons = 0;
  bool leftStick = false, rightStick = false, leftTrigger = false, rightTrigger = false;
};
Hidden s_hidden[Slots];

bool AtRest(SHORT x, SHORT y, SHORT deadzone) { return x > -deadzone && x < deadzone && y > -deadzone && y < deadzone; }

// What is still held from the menu stays hidden; anything back at rest is the game's again.
void HideUntilRest(Hidden& h, XINPUT_GAMEPAD& pad) {
  h.buttons &= pad.wButtons;
  pad.wButtons &= ~h.buttons;
  h.leftStick = h.leftStick && !AtRest(pad.sThumbLX, pad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
  h.rightStick = h.rightStick && !AtRest(pad.sThumbRX, pad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
  h.leftTrigger = h.leftTrigger && pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
  h.rightTrigger = h.rightTrigger && pad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
  if (h.leftStick) pad.sThumbLX = pad.sThumbLY = 0;
  if (h.rightStick) pad.sThumbRX = pad.sThumbRY = 0;
  if (h.leftTrigger) pad.bLeftTrigger = 0;
  if (h.rightTrigger) pad.bRightTrigger = 0;
}

uint16_t Folded(const XINPUT_GAMEPAD& pad) {
  return pad.wButtons | (pad.sThumbLY > StickLean ? button::Up : 0) | (pad.sThumbLY < -StickLean ? button::Down : 0) |
         (pad.sThumbLX < -StickLean ? button::Left : 0) | (pad.sThumbLX > StickLean ? button::Right : 0);
}

// The menu holds the pad only in Training, checked on every read against the game's own state:
// in any other mode the game gets the pad as it is.
void Filter(DWORD user, XINPUT_GAMEPAD& pad) {
  s_seen[user].store(Connected | Folded(pad));
  if (!game::InTraining()) {
    s_hidden[user] = Hidden{};
    return;
  }
  if (s_held.load()) {
    s_hidden[user] = Hidden{pad.wButtons, true, true, true, true};  // nothing held now acts on release
    pad = XINPUT_GAMEPAD{};
    return;
  }
  HideUntilRest(s_hidden[user], pad);
}

// The game's call of XInputGetState. Steam's overlay rewrites what XInput answers (Steam Input),
// so the pad is filtered here, after everything below has answered.
DWORD WINAPI GameGetState(DWORD user, XINPUT_STATE* state) {
  const DWORD result = o_getState(user, state);
  if (user < DWORD(Slots)) {
    if (result == ERROR_SUCCESS && state) Filter(user, state->Gamepad);
    else s_seen[user].store(0);
  }
  return result;
}

}  // namespace

bool InstallPad() {
  return game::HookGameFunction("XInputGetState call", game::rva::XInputGetState, game::XInputGetStatePrologue,
                                reinterpret_cast<void*>(&GameGetState), reinterpret_cast<void**>(&o_getState));
}

PadState CurrentPad() {
  PadState pad;
  for (const auto& slot : s_seen) {
    const uint32_t seen = slot.load();
    pad.connected = pad.connected || (seen & Connected);
    pad.buttons |= uint16_t(seen & 0xFFFF);
  }
  return pad;
}

void HoldPad(bool held) { s_held.store(held); }

}  // namespace fexl::input
