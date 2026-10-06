// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_COMMON_IO_PATHS_H_
#define APP_VIEWS_HARNESS_COMMON_IO_PATHS_H_

#include <cstddef>

namespace app {
namespace detail {

// First existing relative path under the running exe directory, UTF-8 out.
// Does not open GIS samples — china open policy stays in common/io/sample.
bool resolve_first_existing_under_exe(const wchar_t* const* rels, size_t count,
                                      char* out_utf8, size_t out_cap);

inline bool resolve_path_candidates(const wchar_t* const* rels, size_t count,
                                    char* out_utf8, size_t out_cap) {
  return resolve_first_existing_under_exe(rels, count, out_utf8, out_cap);
}

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_COMMON_IO_PATHS_H_
