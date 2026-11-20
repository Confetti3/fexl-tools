#include "Settings.hxx"

#include "Log.hxx"

#include <windows.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>

namespace fexl {

namespace {

SRWLOCK s_lock = SRWLOCK_INIT;
Settings s_current;
std::wstring s_path;
uint64_t s_saveAtMs;  // under s_lock: when the next save is due; 0 when saved
constexpr DWORD SaveAfterMs = 400;

// Reads one "key=value" line into the field with that key.
struct Reader {
  const char* key;
  float value;
  void operator()(const char* name, bool& field) {
    if (strcmp(name, key) == 0) field = value != 0;
  }
  void operator()(const char* name, float& field, Range range) {
    if (strcmp(name, key) == 0 && value >= range.min && value <= range.max) field = value;
  }
  template <class E, class = std::enable_if_t<std::is_enum_v<E>>>
  void operator()(const char* name, E& field) {
    if (strcmp(name, key) == 0 && value >= 0 && value < float(Names(E{}).size())) field = E(int(value));
  }
};

struct Writer {
  FILE* file;
  void operator()(const char* name, bool field) { fprintf(file, "%s=%d\n", name, int(field)); }
  void operator()(const char* name, float field, Range) { fprintf(file, "%s=%.3f\n", name, field); }
  template <class E, class = std::enable_if_t<std::is_enum_v<E>>>
  void operator()(const char* name, E field) {
    fprintf(file, "%s=%d\n", name, int(field));
  }
};

constexpr DWORD RetryAfterMs = 5000;

// Written beside the file and moved over it, so a crash mid-write never leaves half a file.
bool Save(const Settings& settings) {
  const std::wstring temporary = s_path + L".new";
  FILE* file = _wfopen(temporary.c_str(), L"w");
  if (!file) {
    Log("settings: cannot write %ls (error %d)", temporary.c_str(), errno);
    return false;
  }
  fputs("# FEXL Training Tools\n", file);
  Writer writer{file};
  Settings copy = settings;
  copy.Visit(writer);
  const bool written = fflush(file) == 0;
  fclose(file);
  if (!written) {
    Log("settings: writing %ls failed", temporary.c_str());
    return false;
  }
  if (!MoveFileExW(temporary.c_str(), s_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    Log("settings: cannot replace %ls (error %lu)", s_path.c_str(), GetLastError());
    return false;
  }
  return true;
}

// Saves a moment after the last change, off the game's threads. A failed save is tried again
// a few seconds later, unless a newer change has already asked for one.
DWORD WINAPI Saver(void*) {
  for (;;) {
    Sleep(250);
    AcquireSRWLockExclusive(&s_lock);
    const bool due = s_saveAtMs && GetTickCount64() >= s_saveAtMs;
    const Settings settings = s_current;
    if (due) s_saveAtMs = 0;
    ReleaseSRWLockExclusive(&s_lock);
    if (due && !Save(settings)) {
      AcquireSRWLockExclusive(&s_lock);
      if (!s_saveAtMs) s_saveAtMs = GetTickCount64() + RetryAfterMs;
      ReleaseSRWLockExclusive(&s_lock);
    }
  }
}

}  // namespace

void LoadSettings(const wchar_t* path) {
  s_path = path;
  Settings settings;
  if (FILE* file = _wfopen(path, L"r")) {
    char line[128];
    while (fgets(line, sizeof(line), file)) {
      char key[64] = {};
      Reader reader{key, 0};
      if (sscanf(line, "%63[^=]=%f", key, &reader.value) == 2) settings.Visit(reader);
    }
    fclose(file);
  }
  AcquireSRWLockExclusive(&s_lock);
  s_current = settings;
  ReleaseSRWLockExclusive(&s_lock);
  if (HANDLE thread = CreateThread(nullptr, 0, Saver, nullptr, 0, nullptr)) CloseHandle(thread);
}

Settings CurrentSettings() {
  AcquireSRWLockShared(&s_lock);
  const Settings settings = s_current;
  ReleaseSRWLockShared(&s_lock);
  return settings;
}

void StoreSettings(const Settings& settings) {
  AcquireSRWLockExclusive(&s_lock);
  if (!(settings == s_current)) {
    s_current = settings;
    s_saveAtMs = GetTickCount64() + SaveAfterMs;
  }
  ReleaseSRWLockExclusive(&s_lock);
}

}  // namespace fexl
