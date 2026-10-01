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
// Prefer |exe_capture_path| for harness marks / screenshots / reports so the
// gen-root stays browsable (binaries + ninja only at the top level).
inline bool exe_sidecar_path(wchar_t* path,
                             size_t path_cch,
                             const wchar_t* leaf) {
  if (!leaf || !exe_dir_with_slash(path, path_cch)) {
    return false;
  }
  return wcscat_s(path, path_cch, leaf) == 0;
}

// Creates each directory segment of |dir| (drive-aware). Existing dirs OK.
inline void ensure_directory_tree_w(const wchar_t* dir) {
  if (!dir || !dir[0]) {
    return;
  }
  wchar_t buf[MAX_PATH] = {};
  if (wcsncpy_s(buf, dir, _TRUNCATE) != 0) {
    return;
  }
  size_t len = wcslen(buf);
  size_t i = 0;
  if (len >= 3 && buf[1] == L':' && (buf[2] == L'\\' || buf[2] == L'/')) {
    i = 3;
  } else if (len >= 2 && (buf[0] == L'\\' || buf[0] == L'/') &&
             (buf[1] == L'\\' || buf[1] == L'/')) {
    // UNC \\server\share\... — skip to after share name.
    i = 2;
    int seps = 0;
    for (; i < len; ++i) {
      if (buf[i] == L'\\' || buf[i] == L'/') {
        if (++seps == 2) {
          ++i;
          break;
        }
      }
    }
  }
  for (; i < len; ++i) {
    if (buf[i] == L'\\' || buf[i] == L'/') {
      buf[i] = L'\0';
      CreateDirectoryW(buf, nullptr);
      buf[i] = L'\\';
    }
  }
  CreateDirectoryW(buf, nullptr);
}

// Ensures parent directories of |file_path| exist (file leaf may be absent).
inline void ensure_parent_dirs_w(const wchar_t* file_path) {
  if (!file_path || !file_path[0]) {
    return;
  }
  wchar_t parent[MAX_PATH] = {};
  if (wcsncpy_s(parent, file_path, _TRUNCATE) != 0) {
    return;
  }
  wchar_t* slash = wcsrchr(parent, L'\\');
  if (!slash) {
    slash = wcsrchr(parent, L'/');
  }
  if (!slash || slash == parent) {
    return;
  }
  *slash = L'\0';
  ensure_directory_tree_w(parent);
}

// Scenario subdir under captures/ for flat leaves (aligned with
// testing/tools/harness/<family>/). Nested leaves (record/, analysis/, …)
// and already-prefixed paths are left unchanged.
inline const wchar_t* capture_scenario_prefix_w(const wchar_t* leaf) {
  if (!leaf || !leaf[0]) {
    return L"";
  }
  for (const wchar_t* p = leaf; *p; ++p) {
    if (*p == L'\\' || *p == L'/') {
      return L"";
    }
  }
  auto starts = [&](const wchar_t* pre) {
    return wcsncmp(leaf, pre, wcslen(pre)) == 0;
  };
  if (starts(L"atmosphere-") || starts(L"atmosphere_")) {
    return L"atmosphere\\";
  }
  if (starts(L"map2d-") || starts(L"map2d_")) {
    return L"map2d\\";
  }
  if (starts(L"plugin-") || starts(L"plugin_")) {
    return L"plugin\\";
  }
  if (starts(L"ui-") || starts(L"ui_")) {
    return L"ui\\";
  }
  if (starts(L"legacy-") || starts(L"legacy_")) {
    return L"legacy\\";
  }
  if (starts(L"input-") || starts(L"self-test-") || starts(L"views-plain-") ||
      starts(L"views_plain_") || starts(L"browse_") || starts(L"browse-") ||
      starts(L"console_") || starts(L"console-") || starts(L"input_")) {
    return L"shell\\";
  }
  if (leaf[0] == L'_') {
    return L"_scratch\\";
  }
  return L"";
}

inline const char* capture_scenario_prefix_a(const char* leaf) {
  if (!leaf || !leaf[0]) {
    return "";
  }
  for (const char* p = leaf; *p; ++p) {
    if (*p == '\\' || *p == '/') {
      return "";
    }
  }
  auto starts = [&](const char* pre) {
    return std::strncmp(leaf, pre, std::strlen(pre)) == 0;
  };
  if (starts("atmosphere-") || starts("atmosphere_")) {
    return "atmosphere\\";
  }
  if (starts("map2d-") || starts("map2d_")) {
    return "map2d\\";
  }
  if (starts("plugin-") || starts("plugin_")) {
    return "plugin\\";
  }
  if (starts("ui-") || starts("ui_")) {
    return "ui\\";
  }
  if (starts("legacy-") || starts("legacy_")) {
    return "legacy\\";
  }
  if (starts("input-") || starts("self-test-") || starts("views-plain-") ||
      starts("views_plain_") || starts("browse_") || starts("browse-") ||
      starts("console_") || starts("console-") || starts("input_")) {
    return "shell\\";
  }
  if (leaf[0] == '_') {
    return "_scratch\\";
  }
  return "";
}

// Harness / loop artifacts live under
// "<exe_dir>\\captures\\[<scenario>\\]<leaf>".
// |leaf| may be nested ("record\\…", "analysis\\flood\\frame_0000.bmp").
// Flat showcase/mark leaves are routed into scenario subdirs (atmosphere/,
// map2d/, plugin/, ui/, legacy/, shell/, _scratch/). Creates captures/ and
// any intermediate directories when missing.
inline bool exe_capture_path(wchar_t* path,
                             size_t path_cch,
                             const wchar_t* leaf) {
  if (!leaf || !exe_dir_with_slash(path, path_cch)) {
    return false;
  }
  if (wcscat_s(path, path_cch, L"captures") != 0) {
    return false;
  }
  CreateDirectoryW(path, nullptr);
  if (wcscat_s(path, path_cch, L"\\") != 0) {
    return false;
  }
  const wchar_t* scenario = capture_scenario_prefix_w(leaf);
  if (scenario[0] && wcscat_s(path, path_cch, scenario) != 0) {
    return false;
  }
  // Normalize '/' → '\\' so nested leaves work from Interact DSL paths.
  wchar_t normalized[MAX_PATH] = {};
  size_t n = 0;
  for (; leaf[n] && n + 1 < MAX_PATH; ++n) {
    normalized[n] = (leaf[n] == L'/') ? L'\\' : leaf[n];
  }
  normalized[n] = L'\0';
  if (wcscat_s(path, path_cch, normalized) != 0) {
    return false;
  }
  ensure_parent_dirs_w(path);
  return true;
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

inline bool exe_capture_path_a(char* path, size_t path_cch, const char* leaf) {
  if (!leaf || !exe_dir_with_slash_a(path, path_cch)) {
    return false;
  }
  if (strcat_s(path, path_cch, "captures") != 0) {
    return false;
  }
  CreateDirectoryA(path, nullptr);
  if (strcat_s(path, path_cch, "\\") != 0) {
    return false;
  }
  const char* scenario = capture_scenario_prefix_a(leaf);
  if (scenario[0] && strcat_s(path, path_cch, scenario) != 0) {
    return false;
  }
  char normalized[MAX_PATH] = {};
  size_t n = 0;
  for (; leaf[n] && n + 1 < MAX_PATH; ++n) {
    normalized[n] = (leaf[n] == '/') ? '\\' : leaf[n];
  }
  normalized[n] = '\0';
  if (strcat_s(path, path_cch, normalized) != 0) {
    return false;
  }
  wchar_t wide[MAX_PATH] = {};
  if (MultiByteToWideChar(CP_ACP, 0, path, -1, wide, MAX_PATH) > 0) {
    ensure_parent_dirs_w(wide);
  }
  return true;
}

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_UTIL_EXE_SIDECAR_PATH_H_
