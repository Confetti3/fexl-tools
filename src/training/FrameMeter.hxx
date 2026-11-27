#pragma once

// The frame meter's model, game-neutral, after SF4 Ember's (training/FrameMeter.hxx): one cell
// per battle frame and fighter, grouped into exchanges, with each fighter's move readings and
// the exchange's frame advantage. The game's side (game/MeterSample) turns its fighters into
// cells; the overlay (ui/MeterPanel) draws the view. No game or ImGui code here.
//
// Cells are frames as they play, Street Fighter 6's way: frame data counts the frame a move
// first hits on as startup, so a jab of 3 startup, 2 active, 7 recovery has 2 startup cells.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace fexl::training {

enum class MeterKind : uint8_t {
  Neutral,    // free to act
  Movement,   // walking or dashing
  Jump,       // a jump with no attack: take-off, the air and the landing frame
  Teleport,   // a teleport: gone from one place to another, unable to act
  Startup,    // an attack before it can hit
  Active,     // an attack that can hit on this frame
  Recovery,   // after the hit window, before the fighter is free
  Stop,       // hit-freeze: the attacker held still on contact
  Proximity,  // the guard pose a defender takes while an attack comes; no commitment
  Guard,      // blockstun
  Hit,        // hitstun, or being thrown
  Down,       // knocked down or getting up
  Throw,      // holding or throwing the opponent once a throw has caught
  Unknown,    // a state not identified yet
};

// What cannot touch the fighter on this frame.
enum class Invincibility : uint8_t { None, Throw, Strike, Full };

struct MeterCell {
  MeterKind kind = MeterKind::Neutral;
  Invincibility invincibility = Invincibility::None;
  bool airborne = false;  // off the ground
  // The first frame of an attack or jump begun from outside one: from neutral, after a move's
  // end, or out of stun. A chain, a cancel, a move's next stage or an attack in the air
  // continues the same commitment.
  bool moveStart = false;
};

// A fighter's commitment: the frames a meter exchange is made of.
bool Committed(MeterKind kind);

// The first move a fighter made in the exchange: its startup (frame data's count), its active
// frames (several windows for a multi-hit move, `span` from the first to the last active frame,
// gaps included) and its recovery once it has ended.
struct MoveReading {
  bool seen = false;    // the move reached an active frame
  bool ended = false;   // and is over, so recovery and total are known
  bool caught = false;  // it ended in a throw that caught: no recovery or total to read
  int startup = 0;
  int active = 0;
  int hits = 0;         // active windows
  int span = 0;
  int recovery = 0;
  int Total() const { return startup - 1 + span + recovery; }
};

// The first jump a fighter made in the exchange, if it attacked in none: the frames before it
// leaves the ground, in the air, and its landing frame.
struct JumpReading {
  bool seen = false;
  bool ended = false;  // landed, so the landing and total are known
  int takeOff = 0;
  int air = 0;
  int landing = 0;
  int Total() const { return takeOff + air + landing; }
};

// The first teleport a fighter made in the exchange: its frames up to the first on which the
// fighter can act again.
struct TeleportReading {
  bool seen = false;
  bool ended = false;  // the fighter can act again, so the total is known
  int total = 0;
};

// The side whose reading was seen, P1's first; -1 for neither.
template <class Reading>
int SeenSide(const std::array<Reading, 2>& readings) {
  return readings[0].seen ? 0 : readings[1].seen ? 1 : -1;
}

// Frame advantage after contact: how many frames sooner each fighter could act than the other.
struct Advantage {
  bool valid = false;
  int attacker = 0;      // the side whose attack made contact
  int frames = 0;        // the attacker's advantage; the defender's is -frames
  bool blocked = false;
  bool knockdown = false;
};

struct MeterView {
  std::array<std::vector<MeterCell>, 2> cells;  // the exchange, oldest first
  bool scrolled = false;                        // its start is no longer kept (FrameMeter::Kept)
  std::array<MoveReading, 2> moves;
  std::array<JumpReading, 2> jumps;
  std::array<TeleportReading, 2> teleports;
  Advantage advantage;      // this exchange's, once both fighters are free
  Advantage lastAdvantage;  // the last measured, kept after the exchange
  // The move to read out: the attacker's once there was contact, else the first mover's;
  // -1 while neither fighter's move has been active.
  int readingSide = -1;
};

// The view as one line of text, for the log: each fighter's cells as letters (. free, m moving,
// J jump, W teleport, s startup, A active, r recovery, x hit-freeze, p guard pose, G blockstun,
// H hitstun, D down, T throw, ? other), the move read out and the result.
std::string Describe(const MeterView& view);

class FrameMeter {
 public:
  static constexpr int History = 90;  // cells shown: the exchange's latest frames, hit-freeze left out
  // Frames kept per exchange, hit-freeze and the idle wait before it closes included, so a long
  // move keeps its reading; an exchange longer than this loses its start.
  static constexpr int Kept = 600;
  static constexpr int IdleFramesToClose = 24;  // both free this long ends an exchange

  // A frame of both fighters; true when the view changed. A new battle gets a new FrameMeter.
  bool Push(const std::array<MeterCell, 2>& frame);
  const MeterView& View() const { return view_; }
  // An exchange is being recorded (closed again once both fighters have been free a while).
  bool Open() const { return open_; }

 private:
  // A new exchange: everything but the last advantage measured starts afresh.
  void Restart();
  void Measure(const std::array<MeterCell, 2>& frame);

  MeterView view_;
  bool open_ = false;
  int idle_ = 0;
  int tick_ = 0;  // frames into the exchange
  std::array<bool, 2> committed_{};
  std::array<int, 2> released_{{-1, -1}};
  std::array<bool, 2> struck_{};  // hit or guard seen
  std::array<bool, 2> hit_{};
  std::array<bool, 2> down_{};
};

}  // namespace fexl::training
