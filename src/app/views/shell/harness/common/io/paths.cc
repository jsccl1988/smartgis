// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/common/io/paths.h"

#include "app/views/shell/util/exe_sidecar_path.h"

#include <windows.h>

namespace app {
namespace detail {

bool resolve_first_existing_under_exe(const wchar_t* const* rels, size_t count,
                                      char* out_utf8, size_t out_cap) {
  if (!out_utf8 || out_cap < 2 || !rels || count == 0) {
    return false;
  }
  wchar_t base[MAX_PATH] = {};
  if (!exe_dir_with_slash(base, MAX_PATH)) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    wchar_t full[MAX_PATH] = {};
    if (wcscpy_s(full, base) != 0 || wcscat_s(full, rels[i]) != 0) {
      continue;
    }
    if (GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) {
      continue;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, full, -1, out_utf8,
                            static_cast<int>(out_cap), nullptr, nullptr) <= 0) {
      continue;
    }
    return true;
  }
  return false;
}

}  // namespace detail
}  // namespace app
