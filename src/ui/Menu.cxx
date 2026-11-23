#include "ui/Menu.hxx"

#include "HitboxKinds.hxx"
#include "ui/Keys.hxx"
#include "ui/Language.hxx"
#include "ui/MeterPanel.hxx"
#include "ui/Theme.hxx"

#include <string>

namespace fexl::ui {

namespace {

enum class Tab : int { Hitboxes, Meter, Playback, Key, About };
constexpr const char* TabNames[] = {"HITBOXES", "FRAME METER", "PLAYBACK", "COLOUR KEY", "ABOUT"};

ListMenu s_menu;
int s_tab;

ImU32 ToImU32(const Rgb& c, float alpha) {
  return IM_COL32(int(c.r * 255), int(c.g * 255), int(c.b * 255), int(std::min(alpha, 1.0f) * 255));
}

// A sphere's swatch: its fill and outline as the scene draws them, at the menu's opacity.
Swatch SphereSwatch(const Rgb& c, float fill) {
  return [c, fill](ImDrawList* draw, ImVec2 at, float size) {
    const ImVec2 centre(at.x + size * 0.5f, at.y + size * 0.5f);
    draw->AddCircleFilled(centre, size * 0.45f, ToImU32(c, fill));
    draw->AddCircle(centre, size * 0.45f, ToImU32(c, 1.0f), 0, std::max(1.5f, size * 0.08f));
  };
}

Swatch KindSwatch(training::MeterKind kind) {
  return [kind](ImDrawList* draw, ImVec2 at, float size) { DrawKindSwatch(draw, ImVec2(at.x, at.y + size * 0.1f), size * 0.8f, kind); };
}

Swatch MarkSwatch(training::Invincibility mark) {
  return [mark](ImDrawList* draw, ImVec2 at, float size) { DrawInvincibilitySwatch(draw, at, size, mark); };
}

void HitboxesTab(ListMenu& m, HitboxSettings& s, bool meshesAvailable) {
  m.Toggle("Show hitboxes", "Spheres in the fight: where each fighter can be hit, and where attacks hit.", s.shown, true, {},
           KeyName(Command::Hitboxes));
  m.Section("WHAT TO SHOW");
  for (const HitboxKindInfo& info : HitboxKinds) {
    m.Toggle(info.name, info.meaning, s.*info.shown, Enabled(info, s), SphereSwatch(info.colours[int(s.palette)], s.fillOpacity * info.fill));
  }
  m.Section("LOOK");
  const bool filledMissing = s.style == SphereStyle::Filled && !meshesAvailable;
  m.Choice("Style", filledMissing ? "Filled spheres need the engine's debug material, which is not loaded: outlines are drawn."
                                  : "Filled spheres with an outline, outlines alone, or a wireframe.",
           s.style, s.shown);
  m.Choice("Colours", "Ember's colours, a high-contrast set, or a set for colour-blind players.", s.palette, s.shown);
  m.Slider("Fill opacity", "How solid the filled spheres are.", s.fillOpacity, FillOpacityRange, "%.2f",
           s.shown && s.style == SphereStyle::Filled);
  m.Slider("Outline width", "How thick each sphere's outline is.", s.outlineWidth, OutlineWidthRange, "%.1f", s.shown);
  m.Toggle("Draw over the fighters", "Off: the fighters' bodies hide the spheres inside them.", s.overFighters, s.shown);
}

void MeterTab(ListMenu& m, MeterSettings& s, bool& controlsLine) {
  m.Toggle("Show the frame meter", "One cell per frame for each fighter, with the exchange's frame advantage.", s.shown, true, {},
           KeyName(Command::Meter));
  m.Toggle("Player 2's bar", "A bar for each fighter, or player 1's alone.", s.secondRow, s.shown);
  m.Toggle("Frame data line", "Startup, active, recovery and total of the move, above the bars.", s.readings, s.shown);
  m.Toggle("Colour key under the bars", "Each colour's name under the meter. The Colour key tab explains them all.", s.key, s.shown);
  m.Toggle("Controls line", "The trainer's keys, at the top right of the meter.", controlsLine);
  m.Slider("Meter size", "How large the frame meter is drawn; 100% is the standard size.", s.size, MeterSizeRange, "%.0f%%",
           s.shown);
  m.Slider("Meter opacity", "How solid the frame meter is drawn; lower lets the fight show through it.", s.opacity,
           MeterOpacityRange, "%.0f%%", s.shown);
  m.Toggle("At the top of the screen", "Under the life bars instead of above the Gougi icons.", s.atTop, s.shown);
  m.Section("READING THE METER");
  m.Text("Each cell is one frame (1/60 s). A bar starts when either fighter commits and ends when both are free again. "
         "Hit-freeze is left out of the bars, as frame data leaves it out; a pink line marks where it was.");
  m.Text("On hit +1: once the exchange is over, the attacker can act one frame before the other fighter. Green is plus, red "
         "is minus.");
}

void PlaybackTab(ListMenu& m, PlaybackSettings& s, const TrainerView& view, const CommandRunner& run) {
  if (m.ToggleValue("Pause the fight", "Stops the fight; the camera and menus keep running.", view.paused, true,
                    KeyName(Command::Pause)))
    run(Command::Pause);
  if (m.Action("Step one frame", "Runs one frame of a held fight. To press a button on that frame, close this menu and hold it through F8.",
               "STEP", view.holding, KeyName(Command::Step)))
    run(Command::Step);
  m.Toggle("Pause while this menu is open", "The fight waits while you change settings.", s.pauseWithMenu);
  m.Choice("Speed", "Slow motion for the fight only; menus and the camera keep running.", s.speed);
}

void KeyTab(ListMenu& m, const HitboxSettings& hitboxes) {
  m.PageDescription("What every colour on the frame meter and the hitboxes means. Up and down scroll the page.");
  m.Section("FRAME METER");
  for (const MeterKeyEntry& entry : MeterKey()) m.KeyEntry(KindSwatch(entry.kind), entry.name, entry.meaning);
  m.Section("MARKS ABOVE A CELL");
  for (const InvincibilityKeyEntry& mark : InvincibilityKey()) m.KeyEntry(MarkSwatch(mark.invincibility), mark.name, mark.meaning);
  m.Section("HITBOXES");
  for (const HitboxKindInfo& info : HitboxKinds)
    m.KeyEntry(SphereSwatch(info.colours[int(hitboxes.palette)], hitboxes.fillOpacity * info.fill), info.name, info.meaning);
  m.KeyEntry(SphereSwatch(InvincibleColour, hitboxes.fillOpacity * InvincibleFill), "Invincible", InvincibleMeaning);
  m.Text("An attack hits when its hitbox touches the other fighter's hurtbox.");
}

void AboutTab(ListMenu& m, Language& language) {
  m.PageDescription("The trainer's controls. Up and down scroll the page.");
  m.Section("LANGUAGE");
  m.Choice("Language", JapaneseFontLoaded() ? "The trainer's language. Auto follows Windows' display language."
                                            : "Japanese needs a Japanese font (Yu Gothic, Meiryo or MS Gothic), which was not found.",
           language);
  m.Section("FEXL TRAINING TOOLS 0.7");
  m.Text("For FIGHTING EX LAYER on Steam, version 1.5.7. Works in Training mode only: nothing shows, and nothing changes, "
         "in online or other matches. In Training it draws over the fight, and holds or slows it only when you ask.");
  m.Section("PAD, WHILE THIS MENU IS OPEN");
  m.HintLine({{Glyph::LB, Glyph::RB}, nullptr, "Change tab"}, true);
  m.HintLine({{Glyph::DPad, Glyph::None}, nullptr, "Move, and change a value with left and right"}, true);
  m.HintLine({{Glyph::A, Glyph::None}, nullptr, "Change the setting"}, true);
  m.HintLine({{Glyph::B, Glyph::None}, nullptr, "Close"}, true);
  m.Section("KEYBOARD");
  for (const KeyBinding& binding : KeyBindings) m.HintLine({{Glyph::None, Glyph::None}, binding.name, binding.action}, false);
}

}  // namespace

void DrawMenu(Settings& s, bool& open, const TrainerView& view, const MenuInput& input, bool padConnected, float scale,
              const CommandRunner& run) {
  s_menu.Begin("TRAINING TOOLS", TabNames, s_tab, input, padConnected, scale);
  switch (Tab(s_tab)) {
    case Tab::Hitboxes: HitboxesTab(s_menu, s.hitboxes, view.meshesAvailable); break;
    case Tab::Meter: MeterTab(s_menu, s.meter, s.keyReminder); break;
    case Tab::Playback: PlaybackTab(s_menu, s.playback, view, run); break;
    case Tab::Key: KeyTab(s_menu, s.hitboxes); break;
    case Tab::About: AboutTab(s_menu, s.language); break;
  }
  if (s_menu.End()) open = false;
}

}  // namespace fexl::ui
