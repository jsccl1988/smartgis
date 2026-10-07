// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_BROWSER_PLUGIN_PATH_RESOLVE_H_
#define APP_VIEWS_BROWSER_PLUGIN_PATH_RESOLVE_H_

#include <cstddef>
#include <string>
#include <string_view>

namespace app {
namespace detail {

// True when |path| exists as a filesystem object (file or directory).
bool path_exists_a(const char* path);

// First existing absolute path under the exe directory for the given relative
// candidates. Empty string when none match.
std::string resolve_under_exe(const char* const* rels, size_t count);

template <size_t N>
inline std::string resolve_under_exe(const char* (&rels)[N]) {
  return resolve_under_exe(rels, N);
}

// Assigns |json| into |out| when |out| is non-null (bridge result helpers).
inline void write_json_out(std::string* out, std::string_view json) {
  if (out) {
    out->assign(json.data(), json.size());
  }
}

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_BROWSER_PLUGIN_PATH_RESOLVE_H_
