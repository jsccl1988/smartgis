// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/common.h"

#include <windows.h>

#include <string>

#include "app/views/shell/harness/common/mark.h"
#include "app/views/shell/util/exe_sidecar_path.h"

namespace app {
namespace detail {

void plugin_showcase_mark(const char* step) {
  write_mark(kPluginShowcaseMarkLeaf, step, /*truncate=*/false);
}

bool resolve_rel_under_exe(const wchar_t* const* rels, size_t count,
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

std::string json_escape_path(const char* path) {
  std::string out;
  if (!path) {
    return out;
  }
  for (const char* p = path; *p; ++p) {
    if (*p == '\\' || *p == '"') {
      out.push_back('\\');
    }
    out.push_back(*p);
  }
  return out;
}

}  // namespace detail
}  // namespace app
