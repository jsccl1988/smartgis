// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/plugin/path_resolve.h"

#include "app/views/util/exe_sidecar_path.h"

#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {
namespace detail {

bool path_exists_a(const char* path) {
  return path && path[0] &&
         GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

std::string resolve_under_exe(const char* const* rels, size_t count) {
  if (!rels || count == 0) {
    return {};
  }
  char base[MAX_PATH] = {};
  if (!exe_dir_with_slash_a(base, MAX_PATH)) {
    return {};
  }
  for (size_t i = 0; i < count; ++i) {
    const char* rel = rels[i];
    if (!rel || !*rel) {
      continue;
    }
    char full[MAX_PATH] = {};
    if (strcpy_s(full, base) != 0 || strcat_s(full, rel) != 0) {
      continue;
    }
    if (!path_exists_a(full)) {
      continue;
    }
    return std::string(full);
  }
  return {};
}

}  // namespace detail
}  // namespace app
