#include "Detour.hxx"

#include "Log.hxx"

#include <MinHook.h>

namespace fexl {

bool HookFunction(const char* name, void* target, void* detour, void** original) {
  static const MH_STATUS initialised = MH_Initialize();
  MH_STATUS status = initialised;
  if (status == MH_OK) status = MH_CreateHook(target, detour, original);
  if (status == MH_OK) status = MH_EnableHook(target);
  Log("%s hook: %s", name, MH_StatusToString(status));
  return status == MH_OK;
}

void UnhookAll() {
  MH_DisableHook(MH_ALL_HOOKS);
  Log("all hooks taken out");
}

}  // namespace fexl
