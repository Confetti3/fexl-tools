#include "game/Build.hxx"

#include "game/Memory.hxx"

#include <windows.h>

namespace fexl::game {

namespace {
constexpr uint32_t SupportedTimeDateStamp = 0x6939020E;
}

uintptr_t Exe() {
  static const uintptr_t exe = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  return exe;
}

bool IsSupportedBuild() {
  IMAGE_DOS_HEADER dos{};
  IMAGE_NT_HEADERS64 nt{};
  return Read(Exe(), dos) && dos.e_magic == IMAGE_DOS_SIGNATURE && Read(Exe() + uintptr_t(dos.e_lfanew), nt) &&
         nt.Signature == IMAGE_NT_SIGNATURE && nt.FileHeader.TimeDateStamp == SupportedTimeDateStamp;
}

}  // namespace fexl::game
