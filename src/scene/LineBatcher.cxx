#include "scene/LineBatcher.hxx"

#include "Log.hxx"
#include "game/Build.hxx"
#include "game/Memory.hxx"

#include <algorithm>
#include <atomic>

namespace fexl::scene {

namespace {

// ULineBatchComponent's arrays (UE 4.19): BatchedLines at +0x660, BatchedMeshes at +0x688.
// UWorld holds the batchers at +0x40 (LineBatcher) and +0x50 (ForegroundLineBatcher).
// UEngine::DebugMeshMaterial is at +0x368.
constexpr uintptr_t LinesOffset = 0x660, MeshesOffset = 0x688;
constexpr uintptr_t WorldBatcher = 0x40, ForegroundBatcher = 0x50;
constexpr uintptr_t EngineDebugMeshMaterial = 0x368;

// Each frame the trainer draws, its lines and meshes are written afresh with this lifetime; the
// batcher removes what the trainer stops redrawing on its own, so nothing the trainer drew can
// outlive the trainer's drawing by more than this.
constexpr float LifeTimeSeconds = 0.5f;
constexpr int32_t SaneCount = 1 << 20;

struct TArrayRaw {
  void* data;
  int32_t num;
  int32_t max;
};

struct BatchedLine {  // FBatchedLine
  FVector start, end;
  Colour colour;
  float thickness;
  float remainingLifeTime;
  uint8_t depthPriority;  // 0 world, 1 foreground
  uint8_t pad[3];
};
static_assert(sizeof(BatchedLine) == 52, "FBatchedLine");

struct BatchedMesh {  // FBatchedMesh
  TArrayRaw vertices;  // FVector
  TArrayRaw indices;   // int32
  uint32_t colour;     // FColor, BGRA
  uint8_t depthPriority;
  uint8_t pad[3];
  float remainingLifeTime;
  uint32_t pad2;
};
static_assert(sizeof(BatchedMesh) == 48, "FBatchedMesh");

using ReallocFn = void*(__fastcall*)(void*, size_t, uint32_t);
using MarkDirtyFn = void(__fastcall*)(void*);

void* EngineRealloc(void* p, size_t size) {
  return reinterpret_cast<ReallocFn>(game::Exe() + game::rva::Realloc)(p, size, 0);
}

bool Sane(const TArrayRaw& a) { return a.num >= 0 && a.max >= a.num && a.max <= SaneCount && (a.max == 0 || a.data); }

// An engine array's header, read through game/Memory and checked.
bool ReadArray(uintptr_t address, TArrayRaw& a) { return game::Read(address, a) && Sane(a); }

// A local copy of an array's header given room for `need` elements, through the engine's own
// allocator (whose block the engine frees with the array).
bool Reserve(TArrayRaw& a, int32_t need, size_t element) {
  if (need <= a.max) return true;
  const int32_t grow = std::max({need, a.max * 2, 64});
  void* data = EngineRealloc(a.data, size_t(grow) * element);
  if (!data) return false;
  a.data = data;
  a.max = grow;
  return true;
}

void Release(TArrayRaw& a) {
  if (a.data) EngineRealloc(a.data, 0);
  a = TArrayRaw{};
}

// The values written into the array's storage, grown first; `a` is a local copy of its header.
template <class T>
bool Assign(TArrayRaw& a, const std::vector<T>& values) {
  if (!Reserve(a, int32_t(values.size()), sizeof(T))) return false;
  if (!values.empty() && !game::WriteBytes(uintptr_t(a.data), values.data(), values.size() * sizeof(T))) return false;
  a.num = int32_t(values.size());
  return true;
}

uint32_t PackedColour(const Colour& c) {
  const auto byte = [](float v) { return uint32_t(v <= 0 ? 0 : v >= 1 ? 255 : v * 255.0f); };
  return byte(c.b) | byte(c.g) << 8 | byte(c.r) << 16 | byte(c.a) << 24;
}

uintptr_t Batcher(Layer layer) {
  uintptr_t world = 0, batcher = 0, cls = 0, expected = 0;
  const uintptr_t slot = layer == Layer::Foreground ? ForegroundBatcher : WorldBatcher;
  if (!game::Pointer(game::Exe() + game::rva::GWorld, world) || !game::Pointer(world + slot, batcher)) return 0;
  if (!game::Read(batcher + 0x10, cls) || !game::Read(game::Exe() + game::rva::LineBatchClass, expected) || !cls ||
      cls != expected)
    return 0;
  return batcher;
}

// The trainer's mesh slots, copied out of the engine, become the batch's: each slot's arrays are
// refilled (or freed when the batch has fewer parts). False when a slot's arrays do not read as
// arrays; then nothing has been changed.
bool BuildMeshes(std::vector<BatchedMesh>& slots, const std::vector<MeshPart>& parts, uint8_t depth) {
  for (const BatchedMesh& slot : slots)
    if (!Sane(slot.vertices) || !Sane(slot.indices)) return false;
  slots.resize(std::max(slots.size(), parts.size()), BatchedMesh{});
  std::size_t used = 0;
  for (const MeshPart& part : parts) {
    BatchedMesh& slot = slots[used];
    if (!Assign(slot.vertices, part.vertices) || !Assign(slot.indices, part.indices)) break;
    slot.colour = PackedColour(part.colour);
    slot.depthPriority = depth;
    slot.remainingLifeTime = LifeTimeSeconds;
    ++used;
  }
  for (std::size_t i = used; i < slots.size(); ++i) {
    Release(slots[i].vertices);
    Release(slots[i].indices);
  }
  slots.resize(used);
  return true;
}

// What the trainer wrote into a batcher: its first `lines` lines and `meshes` meshes.
struct Owned {
  uintptr_t batcher = 0;
  int32_t lines = 0, meshes = 0;
};
Owned s_owned[2];  // by Layer

// The batcher's contents become the batch. Engine memory is read through game::Read into local
// copies and written back through game::WriteBytes; each array's header is written as soon as
// its storage may have moved. False, with the batcher left as it was, when its arrays do not
// read as arrays or hold more than the trainer wrote (something else draws with it). The
// record of what the trainer owns is kept through a frame without a world or a failed read.
bool Write(Owned& owned, uintptr_t batcher, const SceneBatch& batch, uint8_t depth) {
  if (!batcher) return false;
  if (owned.batcher != batcher) owned = Owned{batcher};  // another world: the old one took its own
  TArrayRaw lines, meshes;
  if (!ReadArray(batcher + LinesOffset, lines) || !ReadArray(batcher + MeshesOffset, meshes)) return false;
  if (lines.num > owned.lines || meshes.num > owned.meshes) {
    static bool logged;
    if (!logged) Log("scene: a line batcher holds something else; the trainer leaves it alone");
    logged = true;
    return false;
  }
  if (lines.num == 0 && meshes.num == 0 && batch.Lines().empty() && batch.Meshes().empty()) return true;

  std::vector<BatchedLine> records;
  records.reserve(batch.Lines().size());
  for (const LinePart& p : batch.Lines()) records.push_back(BatchedLine{p.from, p.to, p.colour, p.thickness, LifeTimeSeconds, depth, {}});
  std::vector<BatchedMesh> slots(std::size_t(meshes.num));
  if (!slots.empty() && !game::ReadBytes(uintptr_t(meshes.data), slots.data(), slots.size() * sizeof(BatchedMesh))) return false;
  if (!BuildMeshes(slots, batch.Meshes(), depth)) return false;

  const bool linesWritten = Assign(lines, records) && game::WriteBytes(batcher + LinesOffset, &lines, sizeof(lines));
  const bool meshesWritten = Assign(meshes, slots) && game::WriteBytes(batcher + MeshesOffset, &meshes, sizeof(meshes));
  if (linesWritten) owned.lines = lines.num;
  if (meshesWritten) owned.meshes = meshes.num;
  reinterpret_cast<MarkDirtyFn>(game::Exe() + game::rva::MarkRenderStateDirty)(reinterpret_cast<void*>(batcher));
  return linesWritten && meshesWritten;
}

// Takes the trainer's drawing out of a layer, if its batcher is still the current world's; a
// batcher that is not went with its world (and its lifetime ends what is left anyway).
void Withdraw(Layer layer) {
  Owned& owned = s_owned[int(layer)];
  if (owned.batcher && owned.batcher == Batcher(layer)) Write(owned, owned.batcher, SceneBatch{}, 0);
  owned = Owned{};
}

}  // namespace

void SceneBatch::Line(const FVector& from, const FVector& to, const Colour& colour, float thickness) {
  lines_.push_back(LinePart{from, to, colour, thickness});
}

void SceneBatch::Mesh(const std::vector<FVector>& vertices, const std::vector<int32_t>& indices, const Colour& colour) {
  MeshPart* part = nullptr;
  for (MeshPart& p : meshes_)
    if (PackedColour(p.colour) == PackedColour(colour)) part = &p;
  if (!part) part = &meshes_.emplace_back(MeshPart{colour, {}, {}});
  const int32_t base = int32_t(part->vertices.size());
  part->vertices.insert(part->vertices.end(), vertices.begin(), vertices.end());
  for (int32_t index : indices) part->indices.push_back(base + index);
}

void Submit(Layer layer, const SceneBatch& batch) {
  const Layer other = layer == Layer::Foreground ? Layer::World : Layer::Foreground;
  if (s_owned[int(other)].batcher) Withdraw(other);
  Write(s_owned[int(layer)], Batcher(layer), batch, layer == Layer::Foreground ? 1 : 0);
}

void Withdraw() {
  Withdraw(Layer::World);
  Withdraw(Layer::Foreground);
}

bool MeshesAvailable() {
  static std::atomic<bool> loaded{false};
  if (loaded.load()) return true;
  uintptr_t engine = 0, material = 0;
  if (game::Pointer(game::Exe() + game::rva::GEngine, engine) && game::Pointer(engine + EngineDebugMeshMaterial, material))
    loaded.store(true);
  return loaded.load();
}

}  // namespace fexl::scene
