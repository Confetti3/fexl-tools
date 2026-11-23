#include "game/Battle.hxx"

#include "game/Build.hxx"
#include "game/Memory.hxx"

#include <atomic>
#include <cmath>

namespace fexl::game {

namespace {

// Offsets in a player (CPlayerBase).
namespace player {
constexpr uintptr_t Flags = 0x278;      // uint64
constexpr uintptr_t Act = 0x2A8;
constexpr uintptr_t Pose = 0x2B0;       // int: 3 in the air
constexpr uintptr_t Model = 0x308;      // -> +0x18 skeleton -> +0x68 bone matrices (3x4 floats each)
constexpr uintptr_t Motion = 0x310;     // -> +0xB0 motion frame
constexpr uintptr_t HitIndex = 0x828;   // the attack's current hit entry (< 16)
constexpr uintptr_t HitSet = 0x840;     // -> +0x10 hit entry pointers
constexpr uintptr_t HitTimers = 0x868;  // -> +0x10 strike-proof, +0x20 throw-proof, +0x70 attack off (int16)
}  // namespace player

// A hit entry: +0x20/+0x22 the first and last active motion frame, +0x28 up to eight sphere
// records. A sphere record: local offset (3 floats), radius at +0x10, bone id at +0x14.
// A hit object (hurt list node): +0x10 next, +0x28 sphere record, +0x68 the bone's world 3x4
// matrix, +0xC8 the owner's player bit (low half) and the object's layer (high half), +0xCC
// flags (0x10000 on, 0x10 off). The hit test (sub_140222140) leaves out objects whose layer is
// in the attack's exclusion mask: strikes exclude layers 2 and 4, throw-type attacks 1 and 4.
// So layer 1 is the strike hurtbox and layer 2 the throw hurtbox.
constexpr uint32_t StrikeLayer = 1, ThrowLayer = 2;

// A player's flag while contact holds it in hit-freeze (measured: set from the frame after
// contact until the freeze ends, on hit and on block).
constexpr uint64_t HitFreezeFlag = 0x100;
constexpr int32_t AirPose = 3;

// The battle frame (count + 1) on which each side's throw was last tested; 0 for none.
std::atomic<uint64_t> s_catchTested[2];

uint64_t BattleFrames() {
  uint64_t frames = 0;
  Read(Exe() + rva::BattleFrameCount, frames);
  return frames;
}

Vec3 Transform(const float* m, const float* local) {
  return Vec3{local[0] * m[0] + local[1] * m[1] + local[2] * m[2] + m[3],
              local[0] * m[4] + local[1] * m[5] + local[2] * m[6] + m[7],
              local[0] * m[8] + local[1] * m[9] + local[2] * m[10] + m[11]};
}

// A sphere from the game's matrices, not garbage from a matrix being rewritten.
bool Finite(const Sphere& s) {
  return std::isfinite(s.centre.x) && std::isfinite(s.centre.y) && std::isfinite(s.centre.z) &&
         std::fabs(s.centre.x) < 1e5f && std::fabs(s.centre.y) < 1e5f && std::fabs(s.centre.z) < 1e5f;
}

bool ReadSphereRecord(uintptr_t record, float* local, float& radius) {
  return ReadBytes(record, local, 3 * sizeof(float)) && Read(record + 0x10, radius) && radius > 0.5f && radius < 300.0f;
}

bool ReadFighter(uintptr_t p, Fighter& f, uintptr_t& hit) {
  uintptr_t motion = 0, timers = 0, hitSet = 0;
  uint64_t flags = 0;
  if (!Read(p + player::Act, f.act) || !Pointer(p + player::Motion, motion) || !Read(motion + 0xB0, f.motionFrame) ||
      !Read(p + player::Flags, flags))
    return false;
  f.hitFreeze = (flags & HitFreezeFlag) != 0;
  int32_t pose = 0;
  f.airborne = Read(p + player::Pose, pose) && pose == AirPose;
  int16_t attackOff = 0;
  if (Pointer(p + player::HitTimers, timers)) {
    Read(timers + 0x10, f.strikeProof);
    Read(timers + 0x20, f.throwProof);
    Read(timers + 0x70, attackOff);
  }
  int32_t index = -1;
  hit = 0;
  Attack attack;
  if (Read(p + player::HitIndex, index) && index >= 0 && index < 16 && Pointer(p + player::HitSet, hitSet) &&
      Pointer(hitSet + 0x10 + 8 * uintptr_t(index), hit) && Read(hit + 0x20, attack.first) && Read(hit + 0x22, attack.last)) {
    attack.live = attackOff <= 0 && f.motionFrame >= attack.first && f.motionFrame <= attack.last;
    attack.upcoming = f.motionFrame < attack.first;
    f.attack = attack;
  } else {
    hit = 0;
  }
  return true;
}

// The attack's spheres in world space, as sub_140237330 places them on the attacker's bones.
template <int N>
void ReadAttackSpheres(uintptr_t p, uintptr_t hit, bool tested, SphereList<N>& out) {
  uintptr_t model = 0, skeleton = 0, matrices = 0;
  if (!hit || !Pointer(p + player::Model, model) || !Pointer(model + 0x18, skeleton) || !Pointer(skeleton + 0x68, matrices))
    return;
  for (int i = 0; i < N; ++i) {
    uintptr_t record = 0;
    float local[3] = {}, radius = 0, m[12] = {};
    uint32_t bone = 0;
    int32_t a = -1, b = -1;
    if (!Pointer(hit + 0x28 + 8 * uintptr_t(i), record) || !ReadSphereRecord(record, local, radius) ||
        !Read(record + 0x14, bone) || bone > 255)
      return;
    if (!Read(Exe() + rva::BoneMapA + 4 * uintptr_t(bone), a) || a < 0 || a > 255 ||
        !Read(Exe() + rva::BoneMapB + 4 * uintptr_t(a), b) || b < 0 || b > 1023 ||
        !ReadBytes(matrices + 48 * uintptr_t(b), m, sizeof(m)))
      return;
    const Sphere sphere{Transform(m, local), radius, tested};
    if (!Finite(sphere)) return;
    out.items[out.count++] = sphere;
  }
}

// Walks a hit object list once; `place` gets each sphere with its layer and keeps it or not.
template <class Place>
void ReadHitObjects(uintptr_t head, Place place) {
  uintptr_t node = 0;
  Read(head, node);
  for (int guard = 0; guard < 96 && node > 0x10000; ++guard) {
    uint32_t owner = 0, flags = 0;
    uintptr_t record = 0;
    float local[3] = {}, radius = 0, m[12] = {};
    Read(node + 0xC8, owner);
    Read(node + 0xCC, flags);
    if (Pointer(node + 0x28, record) && ReadSphereRecord(record, local, radius) && ReadBytes(node + 0x68, m, sizeof(m)))
      if (const Sphere sphere{Transform(m, local), radius, (flags & 0x10010) == 0x10000}; Finite(sphere))
        place(owner >> 16, sphere);
    if (!Read(node + 0x10, node)) return;
  }
}

template <int N>
void Keep(SphereList<N>& list, const Sphere& sphere) {
  if (list.count < N) list.items[list.count++] = sphere;
}

}  // namespace

bool InTraining() {
  int32_t kind = -1;
  return Read(Exe() + rva::BattleKind, kind) && kind == TrainingBattle;
}

bool ReadBattleFrame(BattleFrame& frame) {
  frame = BattleFrame{};
  uintptr_t players[2] = {}, hits[2] = {};
  frame.number = BattleFrames();
  for (int side = 0; side < 2; ++side) {
    if (!Pointer(Exe() + rva::PlayerTable + 8 * uintptr_t(side), players[side]) ||
        !ReadFighter(players[side], frame.fighters[side], hits[side]))
      return false;
  }
  for (int side = 0; side < 2; ++side) {
    ReadHitObjects(Exe() + rva::HurtLists + 0x40 * uintptr_t(side), [&](uint32_t layer, const Sphere& sphere) {
      if (layer & StrikeLayer) Keep(frame.strikeHurt[side], sphere);
      if (layer & ThrowLayer) Keep(frame.throwHurt[side], sphere);
    });
    // Spheres only for a hit that is live or still to come: a spent one is drawn as nothing.
    const auto& attack = frame.fighters[side].attack;
    if (attack && (attack->live || attack->upcoming)) ReadAttackSpheres(players[side], hits[side], attack->live, frame.attack[side]);
  }
  ReadHitObjects(Exe() + rva::ShellList, [&](uint32_t, const Sphere& sphere) { Keep(frame.shells, sphere); });
  return true;
}

void NoteCatchTest(const void* player) {
  uintptr_t players[2] = {};
  if (!Read(Exe() + rva::PlayerTable, players)) return;
  for (int side = 0; side < 2; ++side)
    if (players[side] == reinterpret_cast<uintptr_t>(player)) s_catchTested[side].store(BattleFrames() + 1);
}

void ReadThrowTests(BattleFrame& frame) {
  for (int side = 0; side < 2; ++side) frame.fighters[side].catchTested = s_catchTested[side].load() == frame.number + 1;
}

void ForgetThrowTests() {
  for (auto& tested : s_catchTested) tested.store(0);
}

BattleIdentity ReadBattleIdentity(const void* battle) {
  BattleIdentity id;
  id.battle = battle;
  Read(Exe() + rva::PlayerTable, id.players);
  id.frames = BattleFrames();
  return id;
}

bool ReadCameraEye(Vec3& eye) {
  float xyz[3] = {};
  if (!ReadBytes(Exe() + rva::CameraEye, xyz, sizeof(xyz))) return false;
  eye = Vec3{xyz[0], xyz[1], xyz[2]};
  return true;
}

}  // namespace fexl::game
