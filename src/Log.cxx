#include "Log.hxx"

#include <windows.h>

#include <cstdarg>
#include <cstdio>

namespace fexl {

namespace {
SRWLOCK s_lock = SRWLOCK_INIT;
FILE* s_file;
}  // namespace

void OpenLog(const wchar_t* path) {
  AcquireSRWLockExclusive(&s_lock);
  s_file = _wfopen(path, L"w");
  ReleaseSRWLockExclusive(&s_lock);
}

void Log(const char* format, ...) {
  AcquireSRWLockExclusive(&s_lock);
  if (s_file) {
    va_list args;
    va_start(args, format);
    vfprintf(s_file, format, args);
    va_end(args);
    fputc('\n', s_file);
    fflush(s_file);
  }
  ReleaseSRWLockExclusive(&s_lock);
}

}  // namespace fexl
