// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Parallel tessellation thresholds. Workers never touch Device.

#ifndef VISTA_COMPONENT_MAP_LAYOUT_TESS_GRAIN_H_
#define VISTA_COMPONENT_MAP_LAYOUT_TESS_GRAIN_H_

#include <cstddef>
#include <cstdlib>

namespace vista {
namespace detail {

// Below this, stay serial (pool overhead).
inline constexpr size_t kParallelTessMinGeoms = 2;
// Prefer auto grain (span / (workers*4)); fixed grain=1 oversubscribed Debug.
inline constexpr size_t kParallelTessGrain = 0;

// VISTA_LAYOUT_PARALLEL drives Layout emit. =0 keeps serial tess
// (wall clock must not regress). The env string stays.
inline bool vista_layout_parallel_enabled() {
  const char* v = std::getenv("VISTA_LAYOUT_PARALLEL");
  if (v == nullptr || v[0] == '\0') {
    return true;
  }
  return !(v[0] == '0' && v[1] == '\0');
}

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_LAYOUT_TESS_GRAIN_H_
