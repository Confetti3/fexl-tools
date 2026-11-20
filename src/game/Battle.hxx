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
enum Act : int {
  ActStand = 0, ActCrouch = 1, ActCrouching = 2, ActRising = 3, ActWalkForward = 4, ActWalkBack = 5,
  ActAirAttackLanding = 8, ActJumpStart = 12, ActAirborne = 13, ActLanding = 14, ActRecovery = 17,
  ActNormal = 18, ActSpecial = 19, ActGuardPose = 20, ActBlockStun = 21, ActKnockedDown = 27, ActDown = 28,
  ActHitStun = 29,
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
  std::optional<Attack> attack;  // absent when the action has no hit left
  // Frames left during which strikes, or throws, cannot touch this fighter.
  int16_t strikeProof = 0, throwProof = 0;
};

struct BattleFrame {
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
