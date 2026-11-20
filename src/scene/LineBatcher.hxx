#pragma once

// Drawing inside the game's own 3D scene through one of the world's line batchers, the
// component Unreal's debug drawing uses: lines, and coloured meshes shaded with the engine's
// debug mesh material. Nothing else in the shipping game draws with them. The trainer records
// what it wrote to each and replaces only that; a batcher holding anything else is left alone.
// Engine memory is read through game::Read into local copies and written back through
// game::WriteBytes. What is written lives half a second unless redrawn, so nothing outlives the
// trainer's drawing. Game thread.

#include <cstdint>
#include <vector>

namespace fexl::scene {

// Unreal's world coordinates: z up, centimetres.
struct FVector {
  float x, y, z;
};
struct Colour {
  float r, g, b, a;
};

struct LinePart {
  FVector from, to;
  Colour colour;
  float thickness;
};

// Triangles of one colour; one part per colour, so the engine draws few meshes.
struct MeshPart {
  Colour colour;
  std::vector<FVector> vertices;
  std::vector<int32_t> indices;
};

class SceneBatch {
 public:
  void Line(const FVector& from, const FVector& to, const Colour& colour, float thickness);
  // Indices refer to this call's vertices.
  void Mesh(const std::vector<FVector>& vertices, const std::vector<int32_t>& indices, const Colour& colour);

  const std::vector<LinePart>& Lines() const { return lines_; }
  const std::vector<MeshPart>& Meshes() const { return meshes_; }

 private:
  std::vector<LinePart> lines_;
  std::vector<MeshPart> meshes_;
};

enum class Layer { World, Foreground };  // depth tested, or over everything

// The trainer's drawing in `layer` becomes the batch, and its drawing in the other layer is taken
// out. Nothing happens without a world.
void Submit(Layer layer, const SceneBatch& batch);

// The trainer's drawing taken back out of both layers, from the world it was drawn in if that
// world is still the current one (otherwise it went with its world). Writes nothing else.
void Withdraw();

// Whether meshes can be drawn: the engine's debug mesh material is loaded.
bool MeshesAvailable();

}  // namespace fexl::scene
