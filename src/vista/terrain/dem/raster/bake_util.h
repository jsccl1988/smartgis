// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_RASTER_BAKE_UTIL_H_
#define VISTA_TERRAIN_DEM_RASTER_BAKE_UTIL_H_

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "vista/terrain/process/bake_parallel.h"
#include "vista/terrain/process/nv/bake_pixel.h"

namespace vista {
namespace detail {

inline float dem_clampf(float v, float lo, float hi) {
  return bake_clampf(v, lo, hi);
}

// Parallel row walk when the bake grid is large enough.
template <typename Fn>
void for_each_bake_row(int w, int h, Fn fn) {
  if (bake_rows_should_parallel(w, h)) {
    base::execution::GlobalNThreadPoolExecutor executor;
    base::execution::parallel_for(executor, 0, h, fn);
  } else {
    for (int row = 0; row < h; ++row) {
      fn(row);
    }
  }
}

}  // namespace detail
}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_RASTER_BAKE_UTIL_H_
