// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_UTIL_EXE_SIDECAR_PATH_H_
#define APP_VIEWS_SHELL_UTIL_EXE_SIDECAR_PATH_H_

#include <windows.h>

#include <cstring>
#include <cwchar>

namespace app {
namespace detail {

// Writes the directory of the running module plus a trailing slash into
// |path|. Rejects truncated GetModuleFileNameW results so callers never
// wcslen an unterminated buffer (RTC #2 / stack smash on mark_path).
inline bool exe_dir_with_slash(wchar_t* path, size_t path_cch) {
  if (!path || path_cch == 0) {
    return false;
  }
  path[0] = L'\0';
  const DWORD n =
      GetModuleFileNameW(nullptr, path, static_cast<DWORD>(path_cch));
  // n == path_cch means truncation (and on older Windows may lack NUL).
  if (n == 0 || n >= path_cch) {
    path[0] = L'\0';
    return false;
  }
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    if (path[i] == L'\\' || path[i] == L'/') {
      path[i + 1] = L'\0';
      return true;
    }
  }
  path[0] = L'\0';
  return false;
}

// Builds "<exe_dir>\\<leaf>" into |path|. Returns false if the module path
// is truncated or |leaf| would overflow the buffer.
inline bool exe_sidecar_path(wchar_t* path,
                             size_t path_cch,
                             const wchar_t* leaf) {
  if (!leaf || !exe_dir_with_slash(path, path_cch)) {
    return false;
  }
  return wcscat_s(path, path_cch, leaf) == 0;
}

// ANSI sibling for leftover MFC self-test marks (char path[MAX_PATH]).
inline bool exe_dir_with_slash_a(char* path, size_t path_cch) {
  if (!path || path_cch == 0) {
    return false;
  }
  path[0] = '\0';
  const DWORD n =
      GetModuleFileNameA(nullptr, path, static_cast<DWORD>(path_cch));
  if (n == 0 || n >= path_cch) {
    path[0] = '\0';
    return false;
  }
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    if (path[i] == '\\' || path[i] == '/') {
      path[i + 1] = '\0';
      return true;
    }
  }
  path[0] = '\0';
  return false;
}

inline bool exe_sidecar_path_a(char* path, size_t path_cch, const char* leaf) {
  if (!leaf || !exe_dir_with_slash_a(path, path_cch)) {
    return false;
  }
  return strcat_s(path, path_cch, leaf) == 0;
}

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_UTIL_EXE_SIDECAR_PATH_H_
