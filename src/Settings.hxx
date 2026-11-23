#pragma once

// The player's choices, kept in fexl_trainer.ini beside the DLL. Each part goes only to what
// it controls: the meter panel takes MeterSettings, the scene HitboxSettings, playback
// PlaybackSettings. Every field is listed once, in its part's Visit, which both loading and
// saving walk.

#include <array>

namespace fexl {

// A choice among named values; Names gives them in order.
enum class SphereStyle : int { Filled, Outline, Wireframe };
enum class Palette : int { Ember, HighContrast, ColourSafe };
enum class Speed : int { Normal, Half, Quarter };
enum class Language : int { Auto, English, Japanese };

constexpr std::array<const char*, 3> Names(SphereStyle) { return {"Filled", "Outline", "Wireframe"}; }
constexpr std::array<const char*, 3> Names(Palette) { return {"Ember", "High contrast", "Colour-blind safe"}; }
constexpr std::array<const char*, 3> Names(Speed) { return {"Normal speed", "Half speed", "Quarter speed"}; }
constexpr std::array<const char*, 3> Names(Language) { return {"Auto", "English", "日本語"}; }
// The battle runs one frame in this many.
constexpr int Divisor(Speed speed) { return speed == Speed::Quarter ? 4 : speed == Speed::Half ? 2 : 1; }

// A setting's span and the step the menu moves it by; loading rejects a value outside it.
struct Range {
  float min, max, step;
};
inline constexpr Range FillOpacityRange{0.1f, 0.8f, 0.05f};
inline constexpr Range OutlineWidthRange{0.5f, 4.0f, 0.1f};
inline constexpr Range MeterSizeRange{60.0f, 160.0f, 5.0f};  // percent
inline constexpr Range MeterOpacityRange{20.0f, 100.0f, 5.0f};  // percent

struct MeterSettings {
  bool shown = true;
  bool secondRow = true;
  bool readings = true;
  bool key = true;
  bool atTop = false;
  float size = 100.0f;     // percent of the standard size
  float opacity = 100.0f;  // percent: the whole panel, its cells and text with it

  template <class V>
  void Visit(V& v) {
    v("meter", shown);
    v("meterSecondRow", secondRow);
    v("meterReadings", readings);
    v("meterKey", key);
    v("meterAtTop", atTop);
    v("meterSize", size, MeterSizeRange);
    v("meterOpacity", opacity, MeterOpacityRange);
  }
  bool operator==(const MeterSettings&) const = default;
};

struct HitboxSettings {
  bool shown = true;
  // One switch per kind of sphere (HitboxKinds.hxx).
  bool hurt = true;
  bool attack = true;
  bool upcoming = true;
  bool throwable = false;
  bool projectile = true;
  bool overFighters = true;
  SphereStyle style = SphereStyle::Filled;
  Palette palette = Palette::Ember;
  float fillOpacity = 0.35f;
  float outlineWidth = 1.6f;

  template <class V>
  void Visit(V& v) {
    v("hitboxes", shown);
    v("hurtBoxes", hurt);
    v("attackBoxes", attack);
    v("upcomingAttacks", upcoming);
    v("throwBoxes", throwable);
    v("projectileBoxes", projectile);
    v("overFighters", overFighters);
    v("style", style);
    v("palette", palette);
    v("fillOpacity", fillOpacity, FillOpacityRange);
    v("outlineWidth", outlineWidth, OutlineWidthRange);
  }
  bool operator==(const HitboxSettings&) const = default;
};

struct PlaybackSettings {
  bool pauseWithMenu = true;
  Speed speed = Speed::Normal;

  template <class V>
  void Visit(V& v) {
    v("pauseWithMenu", pauseWithMenu);
    v("speed", speed);
  }
  bool operator==(const PlaybackSettings&) const = default;
};

struct Settings {
  MeterSettings meter;
  HitboxSettings hitboxes;
  PlaybackSettings playback;
  bool keyReminder = true;
  Language language = Language::Auto;

  template <class V>
  void Visit(V& v) {
    meter.Visit(v);
    hitboxes.Visit(v);
    playback.Visit(v);
    v("keyReminder", keyReminder);
    v("language", language);
  }
  bool operator==(const Settings&) const = default;
};

// Loads the file beside the DLL and makes it current; later changes are saved there by a
// background thread a moment after the last one.
void LoadSettings(const wchar_t* path);
Settings CurrentSettings();
void StoreSettings(const Settings& settings);

}  // namespace fexl
