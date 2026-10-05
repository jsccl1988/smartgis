// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_PROCESS_BAKE_PARALLEL_H_
#define VISTA_TERRAIN_PROCESS_BAKE_PARALLEL_H_

#include "vista/terrain/process/bake_backend.h"

#include "base/process/switches.h"

namespace vista {

inline constexpr int kParallelBakeMinRows = 8;
inline constexpr int kParallelBakeMinPixels = 4096;

inline bool bake_rows_should_parallel(int w, int h) {
  return h >= kParallelBakeMinRows &&
         w * h >= kParallelBakeMinPixels;
}

inline BakeBackend resolved_bake_backend() {
  if (const char* s = base::switch_cstr("bake-backend"); s && s[0]) {
    return parse_bake_backend_token(s);
  }
  return bake_backend_from_env();
}

// Explicit cpu/cuda cells must not reuse mem/disk RGBA (kernel compare).
inline bool bake_skip_result_cache() {
  const BakeBackend b = resolved_bake_backend();
  return b == BakeBackend::kCpu || b == BakeBackend::kCuda;
}

}  // namespace vista

#endif  // VISTA_TERRAIN_PROCESS_BAKE_PARALLEL_H_
