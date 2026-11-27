#include "game/MeterSample.hxx"

namespace fexl::game {

namespace {

using training::Invincibility;
using training::MeterCell;
using training::MeterKind;

// The actions of an attack (a throw attempt among them) before it ends or catches.
bool Attacking(int act) { return act == ActNormal || act == ActSpecial || act == ActThrow; }
bool TakeOff(int act) { return act == ActTakeOff || act == ActTakeOffForward || act == ActTakeOffBack; }
// A jump before it lands: its take-off and its time in the air, EX Arrow's among them.
bool Jumping(int act) { return TakeOff(act) || act == ActAir || act == ActAirForward || act == ActAirBack || act == ActExArrow; }
// The first action of a commitment taken from neutral: an attack, a jump, Hunting Dash or Teleport.
bool Begins(int act) { return Attacking(act) || TakeOff(act) || act == ActHuntingDash || act == ActTeleport; }

MeterKind Kind(const Fighter& now, const Fighter& previous, const MeterCell& previousCell) {
  switch (now.act) {
    case ActStand: case ActCrouch: case ActCrouching: case ActRising:
      return MeterKind::Neutral;
    case ActTurn:
      // Turning round as the other fighter passes over: free, as walking (the dummy cancelled
      // it into its guard). Seen on the ground only; in the air it would still be the jump.
      return now.airborne ? MeterKind::Jump : MeterKind::Neutral;
    case ActWalkForward: case ActWalkBack:
      return MeterKind::Movement;
    case ActTakeOff: case ActTakeOffForward: case ActTakeOffBack: case ActAir: case ActAirForward: case ActAirBack:
    case ActExArrow: case ActHuntingDash:
      return MeterKind::Jump;
    case ActTeleport:
      return MeterKind::Teleport;
    case ActLand: case ActLandForward: case ActLandBack:
      // After an air attack the landing holds the fighter until its motion settles (ActRecovery).
      // An empty jump's landing frame is the jump's last; from the next the fighter can act
      // (measured: a normal comes out on it). Neither the game's flags nor its scripts give the
      // hold after an air attack: it measured 1 to 3 frames, shorter than the landing motion,
      // so a jump-in's advantage can read up to 3 frames low.
      if (Attacking(previous.act) || (previous.act == now.act && previousCell.kind == MeterKind::Recovery))
        return MeterKind::Recovery;
      return Jumping(previous.act) ? MeterKind::Jump : MeterKind::Neutral;
    case ActNormal: case ActSpecial:
      // On contact the attacker alone is held for a few frames. Its motion frame holds still
      // (and now and then moves on one); a motion that holds still with no contact is the
      // move's own frame, such as the first frame of a move's next stage.
      if (now.hitFreeze && previous.act == now.act && previous.motionFrame == now.motionFrame) return MeterKind::Stop;
      if (now.attack && now.attack->live) return MeterKind::Active;
      return now.attack && now.motionFrame < now.attack->first ? MeterKind::Startup : MeterKind::Recovery;
    case ActThrow:
      // The game tests a throw only on its active frame and keeps no window for it: before the
      // test the throw is starting up, after it recovering. Only in the attempt itself: the CPU
      // tests throws too, whatever its fighter is doing.
      if (now.catchTested) return MeterKind::Active;
      return previous.act == ActThrow && (previousCell.kind == MeterKind::Active || previousCell.kind == MeterKind::Recovery)
                 ? MeterKind::Recovery
                 : MeterKind::Startup;
    case ActThrowHold: case ActThrowing:
      return MeterKind::Throw;
    case ActRecovery:
      // The settling tail after a move, after stun or after a teleport. After a move its first
      // frame is still the move's last; from then on the fighter can act, as after stun or a
      // teleport at once. Measured against the community frame data: Kairi's standing LP, MP, HP,
      // LK and MK all total and advantage as it lists them (LK totals 17, not 31; HP is 0 on hit,
      // not +8). After Teleport, a normal pressed on this action's first frame comes out on the next.
      return Attacking(previous.act) ? MeterKind::Recovery : MeterKind::Neutral;
    case ActGuardPose:
      return MeterKind::Proximity;
    case ActBlockStun:
      return MeterKind::Guard;
    case ActHitStun: case ActThrown:
      return MeterKind::Hit;
    case ActKnockedDown: case ActDown:
      return MeterKind::Down;
    default:
      return MeterKind::Unknown;
  }
}

Invincibility InvincibilityOf(const Fighter& f) {
  if (f.strikeProof > 0 && f.throwProof > 0) return Invincibility::Full;
  if (f.strikeProof > 0) return Invincibility::Strike;
  if (f.throwProof > 0) return Invincibility::Throw;
  return Invincibility::None;
}

}  // namespace

MeterCell MeterSample(const Fighter& now, const Fighter& previous, const MeterCell& previousCell) {
  MeterCell cell;
  cell.kind = Kind(now, previous, previousCell);
  cell.invincibility = InvincibilityOf(now);
  cell.airborne = now.airborne;
  // A new commitment only from outside one: an attack that follows another (a chain, a cancel,
  // a special's next stage, which starts its motion again) is the same commitment, and so is an
  // attack in the air: the fighter is not free until it lands.
  cell.moveStart = Begins(now.act) && !Begins(previous.act) && !Jumping(previous.act) && !previous.airborne;
  return cell;
}

}  // namespace fexl::game
