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
  Startup,    // an attack before it can hit
  Active,     // an attack that can hit on this frame
  Recovery,   // after the hit window, before the fighter is free
  Stop,       // hit-freeze: the attacker held still on contact
  Proximity,  // the guard pose a defender takes while an attack comes; no commitment
  Guard,      // blockstun
  Hit,        // hitstun
  Down,       // knocked down or getting up
  Unknown,    // a state not identified yet (throws among them)
};

// What cannot touch the fighter on this frame.
enum class Invincibility : uint8_t { None, Throw, Strike, Full };

struct MeterCell {
  MeterKind kind = MeterKind::Neutral;
  Invincibility invincibility = Invincibility::None;
  bool moveStart = false;  // the first frame of a new attack
};

// A fighter's commitment: the frames a meter exchange is made of.
bool Committed(MeterKind kind);

// The first move a fighter made in the exchange: its startup (frame data's count), its active
// frames (several windows for a multi-hit move, `span` from the first to the last active frame,
// gaps included) and its recovery once it has ended.
struct MoveReading {
  bool seen = false;    // the move reached an active frame
  bool ended = false;   // and is over, so recovery and total are known
  int startup = 0;
  int active = 0;
  int hits = 0;         // active windows
  int span = 0;
  int recovery = 0;
  int Total() const { return startup - 1 + span + recovery; }
};

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
  bool scrolled = false;                        // its start has scrolled off the left
  std::array<MoveReading, 2> moves;
  Advantage advantage;      // this exchange's, once both fighters are free
  Advantage lastAdvantage;  // the last measured, kept after the exchange
  // The move to read out: the attacker's once there was contact, else the first mover's;
  // -1 while neither fighter's move has been active.
  int readingSide = -1;
};

// The view as one line of text, for the log: each fighter's cells as letters (. free, m moving,
// s startup, A active, r recovery, x hit-freeze, p guard pose, G blockstun, H hitstun, D down,
// ? other), the move read out and the result.
std::string Describe(const MeterView& view);

class FrameMeter {
 public:
  static constexpr int History = 90;          // cells shown
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
