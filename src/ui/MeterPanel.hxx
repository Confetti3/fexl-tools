#pragma once

// The frame meter as the player sees it: a bar per fighter with a cell per frame in Street
// Fighter 6's colours, the move's frame data and the exchange's result above them with the
// keys on the right, and the colour key below. Draws a training::MeterView and nothing else.
// Render thread.

#include "Settings.hxx"
#include "training/FrameMeter.hxx"
#include "ui/Hints.hxx"

#include <imgui.h>

#include <span>

namespace fexl::ui {

// scale: the picture's height over 1080. hints: the keys shown at the panel's top right, empty for
// none.
void DrawMeterPanel(const training::MeterView& meter, const MeterSettings& settings, float scale, std::span<const HintItem> hints);

struct MeterKeyEntry {
  training::MeterKind kind;
  const char* name;
  const char* meaning;
};
// The kinds the colour key lists, in order.
std::span<const MeterKeyEntry> MeterKey();

struct InvincibilityKeyEntry {
  training::Invincibility invincibility;
  const char* name;
  const char* meaning;
};
std::span<const InvincibilityKeyEntry> InvincibilityKey();

// A key swatch drawn as the bars draw the kind.
void DrawKindSwatch(ImDrawList* draw, ImVec2 at, float height, training::MeterKind kind);
// The mark above a cell while the fighter cannot be touched.
void DrawInvincibilitySwatch(ImDrawList* draw, ImVec2 at, float height, training::Invincibility invincibility);

}  // namespace fexl::ui
