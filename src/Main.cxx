// FEXL Training Tools: a frame meter, hitbox view and frame stepping for Fighting EX Layer's
// Training mode. Loads as dinput8.dll beside EXF-Win64-Shipping.exe and forwards
// DirectInput8Create to the system's DLL; everything else starts on a worker thread.

#include "Detour.hxx"
#include "Log.hxx"
#include "Settings.hxx"
#include "Trainer.hxx"
#include "game/Battle.hxx"
#include "game/Build.hxx"
#include "game/Memory.hxx"
#include "input/Pad.hxx"
#include "overlay/Host.hxx"
#include "ui/Theme.hxx"
#include "ui/TrainerUi.hxx"

#include <windows.h>

#include <string>

namespace {

HMODULE s_self;

std::wstring BesideDll(const wchar_t* name) {
  wchar_t path[MAX_PATH];
  const DWORD length = GetModuleFileNameW(s_self, path, MAX_PATH);
  std::wstring folder(path, length);
  return folder.substr(0, folder.find_last_of(L"\\/") + 1) + name;
}

DWORD WINAPI Start(void*) {
  using namespace fexl;
  OpenLog(BesideDll(L"fexl_trainer.log").c_str());
  Log("FEXL Training Tools 0.6");
  Log("display: %s", IsProcessDPIAware() ? "DPI-aware, real pixels" : "scaled by Windows");
  if (!game::IsSupportedBuild()) {
    Log("this game build is not the supported one; the trainer stays off");
    return 0;
  }
  if (!game::ReadsFaultSafely()) {
    Log("memory reads are not fault-safe in this build of the trainer; it stays off");
    return 0;
  }
  LoadSettings(BesideDll(L"fexl_trainer.ini").c_str());
  // The overlay first and the trainer after it, or neither: a trainer with no overlay would draw
  // spheres and hold frames with no menu to change them. The overlay attaches to the game's
  // window only once both are in, so a failed install leaves nothing behind.
  if (!overlay::InstallHost(overlay::OverlayUi{ui::ApplyScale, ui::DrawTrainerUi, game::InTraining})) {
    Log("the overlay is not in; the trainer stays off");
    UnhookAll();
    return 0;
  }
  if (!InstallTrainer()) {  // which takes every hook out again, Present's too
    Log("the trainer's hooks are not in; it stays off");
    return 0;
  }
  overlay::EnableHost();
  // The pad is the menu's too; without it the keyboard and mouse still work.
  if (!input::InstallPad()) Log("pad: not hooked; the menu takes the keyboard and mouse only");
  return 0;
}

}  // namespace

extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(HINSTANCE instance, DWORD version, REFIID iid,
                                                                    LPVOID* out, LPUNKNOWN outer) {
  using Fn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
  static const Fn real = [] {
    wchar_t path[MAX_PATH];
    const UINT length = GetSystemDirectoryW(path, MAX_PATH);
    const std::wstring dll = std::wstring(path, length) + L"\\dinput8.dll";
    HMODULE system = LoadLibraryW(dll.c_str());
    return system ? reinterpret_cast<Fn>(GetProcAddress(system, "DirectInput8Create")) : nullptr;
  }();
  return real ? real(instance, version, iid, out, outer) : E_FAIL;
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    s_self = self;
    DisableThreadLibraryCalls(self);
    // The one change outside Training, made deliberately: the game is not marked DPI-aware, so
    // on a scaled display Windows gives it a smaller desktop (3440x1440 at 150 % shows as
    // 2293x960) and its resolution list stops short. The game's import table loads this DLL
    // before the game's own code runs, so marking the process aware here comes before the game
    // makes its window, and it sees the display's real pixels. Only for the supported build.
    if (fexl::game::IsSupportedBuild()) SetProcessDPIAware();
    if (HANDLE thread = CreateThread(nullptr, 0, Start, nullptr, 0, nullptr)) CloseHandle(thread);
  }
  return TRUE;
}
