#include "game/MeterSample.hxx"

namespace fexl::game {

namespace {

using training::Invincibility;
using training::MeterKind;

bool Attacking(int act) { return act == ActNormal || act == ActSpecial; }

MeterKind Kind(const Fighter& now, const Fighter& previous) {
  switch (now.act) {
    case ActStand: case ActCrouch: case ActCrouching: case ActRising:
    // A jump shows as nothing, as in Ember: its frames are the same every time.
    case ActJumpStart: case ActAirborne: case ActLanding:
      return MeterKind::Neutral;
    case ActWalkForward: case ActWalkBack:
      return MeterKind::Movement;
    case ActNormal: case ActSpecial:
      // On contact the attacker alone holds one motion frame for a few ticks.
      if (previous.act == now.act && previous.motionFrame == now.motionFrame) return MeterKind::Stop;
      if (now.attack && now.attack->live) return MeterKind::Active;
      return now.attack && now.motionFrame < now.attack->first ? MeterKind::Startup : MeterKind::Recovery;
    case ActAirAttackLanding:
      return MeterKind::Recovery;
    case ActRecovery:
      // The settling tail after a move or after stun. After a move its first frame is still the
      // move's last; from then on the fighter can act, as after stun at once. Measured against
      // the community frame data: Kairi's standing LP, MP, HP, LK and MK all total and advantage
      // as it lists them (LK totals 17, not 31; HP is 0 on hit, not +8).
      return Attacking(previous.act) ? MeterKind::Recovery : MeterKind::Neutral;
    case ActGuardPose:
      return MeterKind::Proximity;
    case ActBlockStun:
      return MeterKind::Guard;
    case ActHitStun:
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

training::MeterCell MeterSample(const Fighter& now, const Fighter& previous) {
  training::MeterCell cell;
  cell.kind = Kind(now, previous);
  cell.invincibility = InvincibilityOf(now);
  cell.moveStart = Attacking(now.act) && (previous.act != now.act || now.motionFrame < previous.motionFrame);
  return cell;
}

}  // namespace fexl::game
