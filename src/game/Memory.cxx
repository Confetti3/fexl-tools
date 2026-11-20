#include "game/Memory.hxx"

#include <windows.h>

namespace fexl::game {

namespace {

// The copy happens in a call the guarded region makes: without asynchronous exception
// support the compiler only promises to catch a fault raised inside a call, not one from
// loads it inlined or moved.
__declspec(noinline) void CopyBytes(volatile const uint8_t* from, uint8_t* to, std::size_t size) {
  for (std::size_t i = 0; i < size; ++i) to[i] = from[i];
}

__declspec(noinline) void StoreBytes(const uint8_t* from, volatile uint8_t* to, std::size_t size) {
  for (std::size_t i = 0; i < size; ++i) to[i] = from[i];
}

}  // namespace

bool ReadBytes(uintptr_t address, void* out, std::size_t size) {
  __try {
    CopyBytes(reinterpret_cast<volatile const uint8_t*>(address), static_cast<uint8_t*>(out), size);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool WriteBytes(uintptr_t address, const void* in, std::size_t size) {
  __try {
    StoreBytes(static_cast<const uint8_t*>(in), reinterpret_cast<volatile uint8_t*>(address), size);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool ReadsFaultSafely() {
  uint64_t value = 0;
  return !Read(uintptr_t(0x10), value);
}

}  // namespace fexl::game
