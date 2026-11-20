#pragma once

// The hit spheres drawn inside the game's 3D scene (scene/LineBatcher), each kind as the hitbox
// kind table (HitboxKinds.hxx) and the settings say: an outline facing the camera, filled or as
// a wireframe. Game thread.

#include "Settings.hxx"
#include "game/Battle.hxx"

namespace fexl::scene {

void DrawHitboxes(const game::BattleFrame& frame, const HitboxSettings& settings);
// Takes the spheres out of the scene.
void ClearHitboxes();

// Filled spheres can be drawn: the engine's debug mesh material is loaded.
bool FilledSpheresAvailable();

}  // namespace fexl::scene
