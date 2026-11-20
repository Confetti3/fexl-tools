#pragma once

// Hooking one of the supported build's functions, after checking its first bytes are that
// build's (fexl::HookFunction does the detour).

#include <cstdint>

namespace fexl::game {

bool HookGameFunction(const char* name, uintptr_t rva, const uint8_t (&prologue)[5], void* detour, void** original);

}  // namespace fexl::game
