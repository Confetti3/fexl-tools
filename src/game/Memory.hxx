#pragma once

// Reading the game's memory without trusting it: a bad pointer reads as a failure, never a
// crash. The trainer's few writes to the game (scene/LineBatcher) go through WriteBytes, the one
// place they can be audited. Any thread.

#include <cstddef>
#include <cstdint>

namespace fexl::game {

// Copies size bytes from address; false when any of them cannot be read.
bool ReadBytes(uintptr_t address, void* out, std::size_t size);

// Copies size bytes to address; false when any of them cannot be written.
bool WriteBytes(uintptr_t address, const void* in, std::size_t size);

template <class T>
bool Read(uintptr_t address, T& out) {
  return ReadBytes(address, &out, sizeof(T));
}

// A pointer field that holds a real object (not null or a small id).
inline bool Pointer(uintptr_t address, uintptr_t& out) {
  return Read(address, out) && out > 0x10000;
}

// Checks once that a fault is caught (a read of an unmapped address fails cleanly).
bool ReadsFaultSafely();

}  // namespace fexl::game
