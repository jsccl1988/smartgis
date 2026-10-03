// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/common/common.h"

#include <string>

#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/common/io/paths.h"

namespace app {
namespace detail {

void plugin_showcase_mark(const char* step) {
  write_mark(kPluginShowcaseMarkLeaf, step, /*truncate=*/false);
}

bool resolve_rel_under_exe(const wchar_t* const* rels, size_t count,
                           char* out_utf8, size_t out_cap) {
  return resolve_first_existing_under_exe(rels, count, out_utf8, out_cap);
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
