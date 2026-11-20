#pragma once

// Detouring a function (MinHook, initialised on first use). Any thread.

namespace fexl {

// True once `detour` runs in place of `target`; `original` then calls the target's own code.
bool HookFunction(const char* name, void* target, void* detour, void** original);
// Takes every detour out again (when a set of hooks could not all go in).
void UnhookAll();

}  // namespace fexl
