// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_BAKE_BAKE_PARALLEL_H_
#define VISTA_TERRAIN_DEM_BAKE_BAKE_PARALLEL_H_

#include "vista/terrain/dem/bake/bake_backend.h"

#include "base/process/switches.h"

#include <cstdlib>

namespace vista {

inline constexpr int kParallelBakeMinRows = 8;
inline constexpr int kParallelBakeMinPixels = 4096;

// Dispatch overrides for equal-profile benches (thread-local).
// -1 = honor env / auto; 0 = force off; 1 = force on.
inline constexpr int kBakeDispatchAuto = -1;
inline constexpr int kBakeDispatchOff = 0;
inline constexpr int kBakeDispatchOn = 1;

inline int& bake_parallel_override_slot() {
  thread_local int v = kBakeDispatchAuto;
  return v;
}

inline int& bake_simd_override_slot() {
  thread_local int v = kBakeDispatchAuto;
  return v;
}

inline void set_bake_parallel_override(int mode) {
  bake_parallel_override_slot() = mode;
}

inline void set_bake_simd_override(int mode) {
  bake_simd_override_slot() = mode;
}

inline int bake_parallel_override() {
  return bake_parallel_override_slot();
}

inline int bake_simd_override() {
  return bake_simd_override_slot();
}

inline bool bake_parallel_env_enabled() {
  const char* v = std::getenv("BAKE_PARALLEL");
  if (v && v[0] == '0' && v[1] == '\0') {
    return false;
  }
  return true;
}

inline bool bake_simd_env_enabled() {
  const char* v = std::getenv("BAKE_SIMD");
  if (v && v[0] == '0' && v[1] == '\0') {
    return false;
  }
  return true;
}

inline bool bake_parallel_wanted() {
  const int o = bake_parallel_override();
  if (o == kBakeDispatchOff) {
    return false;
  }
  if (o == kBakeDispatchOn) {
    return true;
  }
  return bake_parallel_env_enabled();
}

inline bool bake_simd_wanted() {
  const int o = bake_simd_override();
  if (o == kBakeDispatchOff) {
    return false;
  }
  if (o == kBakeDispatchOn) {
    return true;
  }
  return bake_simd_env_enabled();
}

inline bool bake_rows_should_parallel(int w, int h) {
  if (!bake_parallel_wanted()) {
    return false;
  }
  return h >= kParallelBakeMinRows && w * h >= kParallelBakeMinPixels;
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

#endif  // VISTA_TERRAIN_DEM_BAKE_BAKE_PARALLEL_H_
