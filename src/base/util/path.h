// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_UTIL_PATH_H_
#define BASE_UTIL_PATH_H_

#include <filesystem>
#include <string>

#include "base/core/build_config.h"
#include "base/core/log.h"

#if defined(OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace base {

namespace fs = std::filesystem;
using path = fs::path;

inline path operator+(const path& lhs, const path& rhs) {
  path::string_type result = lhs.native();
  result += rhs.native();
  return path(result);
}

// Directory containing the current process image (exe).
inline std::string self_path() {
#if defined(OS_WIN)
  char buf[MAX_PATH] = {};
  DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    LOGGING(LOG_DEBUG, "GetModuleFileNameA failed");
    return {};
  }
  fs::path p(buf);
  return p.parent_path().string();
#else
  // Non-Windows: leave empty; callers should not rely on /proc on this tree.
  return {};
#endif
}

}  // namespace base

#endif  // BASE_UTIL_PATH_H_
