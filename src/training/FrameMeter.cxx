#include "training/FrameMeter.hxx"

#include <cstdio>

namespace fexl::training {

namespace {

bool Struck(MeterKind kind) { return kind == MeterKind::Hit || kind == MeterKind::Guard; }

// The first move in a row of cells: startup up to its first active frame, its active windows,
// and recovery after the last of them. Hit-freeze is not a frame of the move. A throw that
// catches ends there: what follows is the throw, not recovery.
MoveReading ReadMove(const std::vector<MeterCell>& cells) {
  MoveReading move;
  std::size_t i = 0;
  while (i < cells.size() && cells[i].kind != MeterKind::Startup && cells[i].kind != MeterKind::Active) ++i;
  for (; i < cells.size() && cells[i].kind != MeterKind::Active; ++i)
    move.startup += cells[i].kind == MeterKind::Startup;
  ++move.startup;  // the first active frame counts as startup in frame data
  int frames = 0, lastActive = -1;
  bool inWindow = false;
  for (; i < cells.size(); ++i) {
    const MeterKind kind = cells[i].kind;
    if (kind == MeterKind::Stop) continue;
    if (kind == MeterKind::Active) {
      move.hits += !inWindow;
      inWindow = true;
      ++move.active;
      lastActive = frames;
    } else if (kind == MeterKind::Recovery || kind == MeterKind::Startup) {
      inWindow = false;
    } else {
      move.ended = true;
      move.caught = kind == MeterKind::Throw;
      break;
    }
    ++frames;
  }
  move.seen = move.active > 0;
  move.span = lastActive + 1;
  move.recovery = frames - move.span;
  return move;
}

// The exchange's first jump, if it stays a jump: its frames on the ground before take-off, in the
// air, and its landing frame. A jump that attacks is read as its attack (ReadMove), and then no
// later jump in the exchange is read. A jump cut short (hit out of the air) has no landing.
JumpReading ReadJump(const std::vector<MeterCell>& cells) {
  JumpReading jump;
  std::size_t i = 0;
  while (i < cells.size() && cells[i].kind != MeterKind::Jump) ++i;
  for (; i < cells.size() && cells[i].kind == MeterKind::Jump; ++i) {
    if (cells[i].airborne) ++jump.air;
    else if (jump.air == 0) ++jump.takeOff;
    else ++jump.landing;
  }
  if (i < cells.size()) {
    const MeterKind next = cells[i].kind;
    if (cells[i].airborne || next == MeterKind::Startup || next == MeterKind::Active || next == MeterKind::Stop)
      return JumpReading{};  // an attack in the air
    jump.ended = jump.landing > 0;
  }
  jump.seen = jump.takeOff + jump.air > 0;
  return jump;
}

}  // namespace

std::string Describe(const MeterView& view) {
  static constexpr char Letters[] = ".mJsArxpGHDT?";
  static_assert(sizeof(Letters) - 1 == std::size_t(MeterKind::Unknown) + 1, "one letter per MeterKind, in order");
  std::string line;
  for (int side = 0; side < 2; ++side) {
    line += side ? " | P2 " : "P1 ";
    for (const MeterCell& cell : view.cells[side]) line += Letters[int(cell.kind)];
  }
  char text[96];
  if (view.readingSide >= 0) {
    const MoveReading& m = view.moves[view.readingSide];
    snprintf(text, sizeof(text), " || P%d startup %d active %d hits %d recovery %d total %d%s", view.readingSide + 1, m.startup,
             m.active, m.hits, m.ended ? m.recovery : -1, m.ended ? m.Total() : -1, m.caught ? " caught" : "");
    line += text;
  } else if (const int side = view.jumps[0].seen ? 0 : view.jumps[1].seen ? 1 : -1; side >= 0) {
    const JumpReading& j = view.jumps[side];
    snprintf(text, sizeof(text), " || P%d jump take-off %d air %d landing %d total %d", side + 1, j.takeOff, j.air, j.landing,
             j.ended ? j.Total() : -1);
    line += text;
  }
  if (view.advantage.valid) {
    snprintf(text, sizeof(text), " | %s %+d%s", view.advantage.blocked ? "block" : "hit", view.advantage.frames,
             view.advantage.knockdown ? " knockdown" : "");
    line += text;
  }
  return line;
}

bool Committed(MeterKind kind) {
  return kind != MeterKind::Neutral && kind != MeterKind::Movement && kind != MeterKind::Proximity;
}

void FrameMeter::Restart() {
  const Advantage last = view_.lastAdvantage;
  *this = FrameMeter{};
  view_.lastAdvantage = last;
}

bool FrameMeter::Push(const std::array<MeterCell, 2>& frame) {
  const bool busy = Committed(frame[0].kind) || Committed(frame[1].kind);
  if (busy && !open_) {
    Restart();
    open_ = true;
  }
  if (!open_) return false;
  idle_ = busy ? 0 : idle_ + 1;
  if (idle_ >= IdleFramesToClose) {
    open_ = false;
    for (auto& row : view_.cells) row.resize(row.size() > std::size_t(idle_ - 1) ? row.size() - (idle_ - 1) : 0);
    return true;
  }
  for (int side = 0; side < 2; ++side) {
    auto& row = view_.cells[side];
    if (row.size() == Kept) {
      row.erase(row.begin());
      view_.scrolled = true;
    }
    row.push_back(frame[side]);
  }
  Measure(frame);
  for (int side = 0; side < 2; ++side) {
    view_.moves[side] = view_.scrolled ? MoveReading{} : ReadMove(view_.cells[side]);
    view_.jumps[side] = view_.scrolled ? JumpReading{} : ReadJump(view_.cells[side]);
  }
  view_.readingSide = view_.advantage.valid ? view_.advantage.attacker : view_.moves[0].seen ? 0 : view_.moves[1].seen ? 1 : -1;
  ++tick_;
  return true;
}

// A fighter is released, free to act again, on the first frame after its commitment that is
// either free or the start of a new attack or jump (a reversal out of stun, a move buffered
// after recovery). Advantage is the difference between the two fighters' release frames.
void FrameMeter::Measure(const std::array<MeterCell, 2>& frame) {
  for (int side = 0; side < 2; ++side) {
    const MeterCell& cell = frame[side];
    struck_[side] = struck_[side] || Struck(cell.kind);
    hit_[side] = hit_[side] || cell.kind == MeterKind::Hit;
    down_[side] = down_[side] || cell.kind == MeterKind::Down;
    if (!committed_[side]) {
      committed_[side] = Committed(cell.kind);
    } else if (released_[side] < 0 && (!Committed(cell.kind) || cell.moveStart)) {
      released_[side] = tick_;
    }
  }
  if (view_.advantage.valid || released_[0] < 0 || released_[1] < 0) return;
  const int defender = struck_[1] ? 1 : struck_[0] ? 0 : -1;
  if (defender < 0) return;  // a whiff: nothing to measure
  Advantage& a = view_.advantage;
  a.valid = true;
  a.attacker = 1 - defender;
  a.frames = released_[defender] - released_[a.attacker];
  a.blocked = !hit_[defender];
  a.knockdown = down_[defender];
  view_.lastAdvantage = a;
}

}  // namespace fexl::training
