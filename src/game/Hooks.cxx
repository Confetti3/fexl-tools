#include "game/Hooks.hxx"

#include "Detour.hxx"
#include "Log.hxx"
#include "game/Build.hxx"
#include "game/Memory.hxx"

#include <cstring>

namespace fexl::game {

bool HookGameFunction(const char* name, uintptr_t rva, const uint8_t (&prologue)[5], void* detour, void** original) {
  void* target = reinterpret_cast<void*>(Exe() + rva);
  uint8_t code[sizeof(prologue)] = {};
  if (!ReadBytes(Exe() + rva, code, sizeof(code)) || memcmp(code, prologue, sizeof(code)) != 0) {
    Log("%s: the game's code is not as expected; not hooked", name);
    return false;
  }
  return HookFunction(name, target, detour, original);
}

}  // namespace fexl::game
