#include "scene/HitboxScene.hxx"

#include "HitboxKinds.hxx"
#include "scene/LineBatcher.hxx"

#include <algorithm>
#include <cmath>
#include <vector>

namespace fexl::scene {

namespace {

using game::Sphere;
using game::Vec3;

// The battle is y up with z toward the camera; Unreal's world is z up and left-handed:
// engine (X, Y, Z) = battle (x, z, y), checked against the fighters in Training.
FVector ToEngine(const Vec3& v) { return FVector{v.x, v.z, v.y}; }

Vec3 Add(const Vec3& a, const Vec3& b, float scale) { return Vec3{a.x + b.x * scale, a.y + b.y * scale, a.z + b.z * scale}; }

Vec3 Normalised(const Vec3& v) {
  const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
  return length > 1e-4f ? Vec3{v.x / length, v.y / length, v.z / length} : Vec3{};
}

// How one sphere looks: its colour, fill and outline weights.
struct Look {
  Rgb rgb;
  float fill, outline;
};

// The plane facing the camera through a sphere's centre.
struct Facing {
  Vec3 right, up;
};

constexpr int Segments = 40;
constexpr float RimFrom = 0.8f;  // the rim band runs from here to the edge, in radii

// Each sphere as a bubble facing the camera: a faint disc, a stronger band at its rim, a bright
// outline, and a thin dark edge outside it so it reads on the bright floor. Flat discs pile up
// far less than solid spheres where spheres overlap, so a body's spheres read as one shape.
class SphereDrawer {
 public:
  SphereDrawer(SceneBatch& batch, const Vec3& eye, const HitboxSettings& s, bool fills)
      : batch_(batch), eye_(eye), s_(s), fills_(fills) {}

  void Draw(const Sphere& sphere, const Look& look) {
    const Facing f = FacingCamera(sphere);
    const Rgb& c = look.rgb;
    const float fill = s_.fillOpacity * look.fill;
    if (fills_) {
      Band(sphere, f, 0.0f, RimFrom, Colour{c.r, c.g, c.b, fill * 0.3f});
      Band(sphere, f, RimFrom, 1.0f, Colour{c.r, c.g, c.b, std::min(1.0f, fill * 1.15f)});
    }
    const float width = s_.outlineWidth * look.outline;
    Ring(sphere.centre, f.right, f.up, sphere.radius + width * 0.7f, Colour{0.0f, 0.0f, 0.0f, 0.3f}, width * 0.5f);
    const Colour bright{c.r + (1 - c.r) * 0.3f, c.g + (1 - c.g) * 0.3f, c.b + (1 - c.b) * 0.3f, 1.0f};
    Ring(sphere.centre, f.right, f.up, sphere.radius, bright, width);
    if (s_.style != SphereStyle::Wireframe) return;
    const Colour dim{c.r * 0.6f, c.g * 0.6f, c.b * 0.6f, 1.0f};
    Ring(sphere.centre, Vec3{1, 0, 0}, Vec3{0, 0, 1}, sphere.radius, dim, width * 0.45f);
    Ring(sphere.centre, Vec3{0, 1, 0}, Vec3{0, 0, 1}, sphere.radius, dim, width * 0.45f);
  }

 private:
  Facing FacingCamera(const Sphere& sphere) const {
    const Vec3 forward = Normalised(Vec3{sphere.centre.x - eye_.x, sphere.centre.y - eye_.y, sphere.centre.z - eye_.z});
    const Vec3 right = Normalised(Vec3{-forward.z, 0.0f, forward.x});
    return {right, Vec3{right.y * forward.z - right.z * forward.y, right.z * forward.x - right.x * forward.z,
                        right.x * forward.y - right.y * forward.x}};
  }

  // The ring of the facing disc from `from` to `to` radii (a full disc from 0), both windings
  // so it shows however the debug material culls.
  void Band(const Sphere& sphere, const Facing& f, float from, float to, const Colour& colour) {
    vertices_.clear();
    indices_.clear();
    for (int i = 0; i < Segments; ++i) {
      const float t = 6.2831853f * float(i) / Segments;
      const Vec3 direction = Add(Vec3{std::cos(t) * f.right.x, std::cos(t) * f.right.y, std::cos(t) * f.right.z}, f.up, std::sin(t));
      vertices_.push_back(ToEngine(Add(sphere.centre, direction, sphere.radius * from)));
      vertices_.push_back(ToEngine(Add(sphere.centre, direction, sphere.radius * to)));
    }
    for (int i = 0; i < Segments; ++i) {
      const int a = 2 * i, b = a + 1, c = 2 * ((i + 1) % Segments), d = c + 1;
      indices_.insert(indices_.end(), {a, b, c, c, b, d, a, c, b, c, d, b});
    }
    batch_.Mesh(vertices_, indices_, colour);
  }

  void Ring(const Vec3& centre, const Vec3& a, const Vec3& b, float radius, const Colour& colour, float width) {
    Vec3 previous{};
    for (int i = 0; i <= Segments; ++i) {
      const float t = 6.2831853f * float(i) / Segments;
      const Vec3 p = Add(Add(centre, a, std::cos(t) * radius), b, std::sin(t) * radius);
      if (i > 0) batch_.Line(ToEngine(previous), ToEngine(p), colour, width);
      previous = p;
    }
  }

  SceneBatch& batch_;
  Vec3 eye_;
  const HitboxSettings& s_;
  bool fills_;
  std::vector<FVector> vertices_;
  std::vector<int32_t> indices_;
};

// Each sphere of the frame with its kind, and whether it is a hurtbox that cannot be hit now.
template <class Visit>
void ForEachSphere(const game::BattleFrame& frame, Visit visit) {
  for (int side = 0; side < 2; ++side) {
    const game::Fighter& f = frame.fighters[side];
    for (const Sphere& s : frame.strikeHurt[side]) visit(HitboxKind::Hurt, s, f.strikeProof > 0 || !s.tested);
    for (const Sphere& s : frame.throwHurt[side]) visit(HitboxKind::Throwable, s, f.throwProof > 0 || !s.tested);
    for (const Sphere& s : frame.attack[side]) visit(s.tested ? HitboxKind::Attack : HitboxKind::Upcoming, s, false);
  }
  for (const Sphere& s : frame.shells) visit(HitboxKind::Projectile, s, false);
}

}  // namespace

void DrawHitboxes(const game::BattleFrame& frame, const HitboxSettings& s) {
  Vec3 eye;
  if (!s.shown || !game::ReadCameraEye(eye)) {
    Withdraw();
    return;
  }
  SceneBatch batch;
  SphereDrawer drawer(batch, eye, s, s.style == SphereStyle::Filled && MeshesAvailable());
  ForEachSphere(frame, [&](HitboxKind kind, const Sphere& sphere, bool invincible) {
    const HitboxKindInfo& info = Info(kind);
    if (!Shown(info, s)) return;
    drawer.Draw(sphere, invincible ? Look{InvincibleColour, InvincibleFill, info.outline}
                                   : Look{info.colours[int(s.palette)], info.fill, info.outline});
  });
  Submit(s.overFighters ? Layer::Foreground : Layer::World, batch);
}

void ClearHitboxes() { Withdraw(); }

bool FilledSpheresAvailable() { return MeshesAvailable(); }

}  // namespace fexl::scene
