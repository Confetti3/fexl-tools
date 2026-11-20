#pragma once

// The trainer's log, fexl_trainer.log beside the DLL: what was installed, and with
// FEXL_DEBUG=1 each fighter's change of action. Any thread.

namespace fexl {

void OpenLog(const wchar_t* path);
void Log(const char* format, ...);

}  // namespace fexl
