#pragma once

// Fighting EX Layer's fighters as frame meter cells: the one place that knows what the game's
// actions mean to the meter (after SFV Ember's SfvMeterSample).

#include "game/Battle.hxx"
#include "training/FrameMeter.hxx"

namespace fexl::game {

// The fighter's cell this frame; `previous` is the same fighter's last frame and `previousCell`
// the cell it gave.
training::MeterCell MeterSample(const Fighter& now, const Fighter& previous, const training::MeterCell& previousCell);

}  // namespace fexl::game
