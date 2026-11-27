#pragma once

// What the trainer reads of a Training battle on one tick: each fighter's state and the hit
// spheres the game tests. Read on the game thread inside the hit check, before the game
// tests this tick's hits (ReadBattleFrame). Read only: nothing here writes to the game.

#include <array>
#include <cstdint>
#include <optional>

namespace fexl::game {

// The battle's coordinates: y up, x across the stage, z toward the camera; centimetres.
struct Vec3 {
  float x = 0, y = 0, z = 0;
};

struct Sphere {
  Vec3 centre;
  float radius = 0;
  // The game tests it on this frame: a hurtbox not switched off (some invincibility switches
  // them off), an attack sphere inside its hit window.
  bool tested = true;
};

template <int Capacity>
struct SphereList {
  std::array<Sphere, Capacity> items{};
  int count = 0;
  const Sphere* begin() const { return items.data(); }
  const Sphere* end() const { return items.data() + count; }
};

// Player actions (player +0x2A8), named from Training. There are no names in the game.
// A jump: take-off (2 frames), airborne, landing, for each of the three directions; an air attack
// (ActNormal in the air) lands in ActLandForward whichever way the jump went. The landing's
// motion goes on in ActRecovery.
// A throw: 23 the attempt (its active frames are the catch tests), on a catch 24 the hold (the
// window to break it) and 25 the throw itself, while the one caught is in 26.
// Gougi abilities: EX Arrow (39) is the air of its jump, after an ordinary take-off and before
// ActLand; Hunting Dash (42) is a whole low jump in one act, its take-off, air and landing told
// apart by the pose; Teleport (43) is all of the warp, followed by ActRecovery.
enum Act : int {
  ActStand = 0, ActCrouch = 1, ActCrouching = 2, ActRising = 3, ActWalkForward = 4, ActWalkBack = 5,
  ActTakeOffForward = 6, ActAirForward = 7, ActLandForward = 8, ActTakeOffBack = 9, ActAirBack = 10, ActLandBack = 11,
  ActTakeOff = 12, ActAir = 13, ActLand = 14, ActTurn = 16, ActRecovery = 17, ActNormal = 18, ActSpecial = 19,
  ActGuardPose = 20, ActBlockStun = 21, ActThrow = 23, ActThrowHold = 24, ActThrowing = 25, ActThrown = 26,
  ActKnockedDown = 27, ActDown = 28, ActHitStun = 29, ActExArrow = 39, ActHuntingDash = 42, ActTeleport = 43,
};

// The hit an action is playing: its window in motion frames, whether the game tests it on this
// frame (inside the window, and the attack not switched off) and whether the window is still to
// come. Neither once the window has passed or the game switched the hit off after contact.
struct Attack {
  uint16_t first = 0, last = 0;
  bool live = false;
  bool upcoming = false;
};

struct Fighter {
  int act = ActStand;
  uint32_t motionFrame = 0;     // the action's frame; it holds still in hit-freeze
  bool airborne = false;        // off the ground (not on a jump's take-off or landing frames)
  bool hitFreeze = false;       // held by contact (it can still move a frame now and then)
  std::optional<Attack> attack;  // absent when the action has no hit left
  bool catchTested = false;     // the game tested this fighter's throw on this frame (ReadThrowTests)
  // Frames left during which strikes, or throws, cannot touch this fighter.
  int16_t strikeProof = 0, throwProof = 0;
};

struct BattleFrame {
  uint64_t number = 0;  // the battle's frame count
  std::array<Fighter, 2> fighters{};
  std::array<SphereList<24>, 2> strikeHurt{};  // hit object layer 1: what strikes can hit
  std::array<SphereList<24>, 2> throwHurt{};   // layer 2: what throws can catch
  std::array<SphereList<8>, 2> attack{};       // the current attack's spheres, tested while it is live
  SphereList<16> shells{};                     // projectiles
};

// The battle running is Training's. The hooked battle functions serve other modes too, online
// ones among them; nothing of the trainer may act there.
bool InTraining();

// False when the fighters are not there. The caller has checked InTraining.
bool ReadBattleFrame(BattleFrame& frame);

// The game is testing `player`'s throw (the catch test's hook, in Training). Any thread.
void NoteCatchTest(const void* player);
// Once `frame` has run: whose throw the game tested on it. The game tests throws after the hit
// check that reads the frame, so this comes at the end of the battle tick.
void ReadThrowTests(BattleFrame& frame);
// A new battle: tests noted in the last one count for nothing (its frame count starts again).
void ForgetThrowTests();

// Which battle runs: its object, its fighters' objects and its frame count. A different object
// or fighter, or a count that went back, is a new battle (a reload, a character change).
struct BattleIdentity {
  const void* battle = nullptr;
  uintptr_t players[2] = {};
  uint64_t frames = 0;
  bool Continues(const BattleIdentity& before) const {
    return battle == before.battle && players[0] == before.players[0] && players[1] == before.players[1] &&
           frames >= before.frames;
  }
};
BattleIdentity ReadBattleIdentity(const void* battle);

// The battle camera's eye, for drawing spheres facing it.
bool ReadCameraEye(Vec3& eye);

}  // namespace fexl::game
