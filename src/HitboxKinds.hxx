#pragma once

// The kinds of hit sphere the trainer shows, each once: its switch in the settings, its colour
// in every palette, how strongly it is filled and outlined, and what the menu's key says about
// it. The scene draws from this table and the menu's key lists it, so the two cannot differ.
// The Ember palette is SF4 Ember's (ui/HitboxOverlay): green hurt, yellow throwable, red attack.

#include "Settings.hxx"

#include <array>

namespace fexl {

struct Rgb {
  float r, g, b;
};

enum class HitboxKind : int { Hurt, Attack, Upcoming, Throwable, Projectile };

struct HitboxKindInfo {
  HitboxKind kind;
  const char* name;
  bool HitboxSettings::*shown;
  bool HitboxSettings::*within;  // the kind shows only while this kind's switch is on too, or null
  std::array<Rgb, 3> colours;    // by Palette
  float fill;                  // times the fill opacity setting
  float outline;               // times the outline width setting
  const char* meaning;
};

inline constexpr std::array<HitboxKindInfo, 5> HitboxKinds{{
    {HitboxKind::Hurt, "Hurtboxes", &HitboxSettings::hurt, nullptr,
     {{{0.24f, 0.86f, 0.35f}, {0.0f, 0.9f, 0.8f}, {0.0f, 0.45f, 0.7f}}}, 0.55f, 0.75f,
     "Hurtbox: where strikes can hit the fighter."},
    {HitboxKind::Attack, "Hitboxes", &HitboxSettings::attack, nullptr,
     {{{1.0f, 0.24f, 0.24f}, {1.0f, 0.0f, 0.62f}, {0.84f, 0.37f, 0.0f}}}, 1.3f, 1.25f,
     "Hitbox: an attack that can hit right now."},
    {HitboxKind::Upcoming, "Upcoming hitboxes", &HitboxSettings::upcoming, &HitboxSettings::attack,
     {{{1.0f, 0.59f, 0.16f}, {1.0f, 0.86f, 0.0f}, {0.8f, 0.6f, 0.7f}}}, 0.45f, 0.75f,
     "Upcoming hitbox: where the attack will be once active."},
    {HitboxKind::Throwable, "Throw hurtboxes", &HitboxSettings::throwable, nullptr,
     {{{0.94f, 0.86f, 0.24f}, {0.55f, 1.0f, 0.2f}, {0.94f, 0.89f, 0.26f}}}, 0.5f, 0.75f,
     "Throw hurtbox: where a throw can catch the fighter."},
    {HitboxKind::Projectile, "Projectiles", &HitboxSettings::projectile, nullptr,
     {{{0.75f, 0.45f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.34f, 0.71f, 0.91f}}}, 1.0f, 1.0f,
     "Projectile: fireballs and other thrown attacks."},
}};

// The table is indexed by kind: each row in its kind's place.
constexpr bool InKindOrder() {
  for (std::size_t i = 0; i < HitboxKinds.size(); ++i)
    if (int(HitboxKinds[i].kind) != int(i)) return false;
  return true;
}
static_assert(InKindOrder(), "HitboxKinds rows must follow HitboxKind's order");

inline const HitboxKindInfo& Info(HitboxKind kind) { return HitboxKinds[int(kind)]; }

// The kind's own switch can be used: hitboxes are shown, and so is the kind it belongs to.
inline bool Enabled(const HitboxKindInfo& info, const HitboxSettings& s) { return s.shown && (!info.within || s.*info.within); }
// The kind is drawn.
inline bool Shown(const HitboxKindInfo& info, const HitboxSettings& s) { return Enabled(info, s) && s.*info.shown; }

// A hurtbox that cannot be hit or thrown right now, in every palette.
inline constexpr Rgb InvincibleColour{0.78f, 0.8f, 0.86f};
inline constexpr float InvincibleFill = 0.3f;
inline constexpr const char* InvincibleMeaning = "Grey: invincible; that hurtbox cannot be hit right now.";

}  // namespace fexl
