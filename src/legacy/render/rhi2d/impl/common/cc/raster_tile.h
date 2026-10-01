// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_IMPL_COMMON_CC_RASTER_TILE_H_
#define LEGACY_RENDER_RHI2D_IMPL_COMMON_CC_RASTER_TILE_H_

#include <cstdint>
#include <vector>

#include <windows.h>

namespace render {
namespace detail {

// Parallel map-paint mode for leftover rhi2d (perf A/B).
// SMT_RHI2D_PARALLEL=serial|tile|layer (default tile).
// Legacy: SMT_RHI2D_TILE_RASTER=0 maps to serial when PARALLEL unset.
enum class Rhi2dParallelMode : uint8_t {
  kSerial = 0,
  kTile = 1,
  kLayer = 2,
};

Rhi2dParallelMode rhi2d_parallel_mode();

// One viewport-grid tile for leftover map2d multi-thread raster.
struct Rhi2dRasterTile {
  int tx = 0;
  int ty = 0;
  // Inclusive-exclusive device rect in the full viewport (center, no skirt).
  RECT center{0, 0, 0, 0};
  // Center expanded by outset, clipped to viewport.
  RECT paint{0, 0, 0, 0};
  uint64_t gen = 0;
};

inline int raster_tile_width(const Rhi2dRasterTile& t) {
  return t.paint.right - t.paint.left;
}

inline int raster_tile_height(const Rhi2dRasterTile& t) {
  return t.paint.bottom - t.paint.top;
}

// Default 256; override via SMT_RHI2D_TILE_SIZE (128..1024).
int rhi2d_tile_pixel_size();

// Picks a larger tile when the viewport would otherwise exceed ~4 tiles
// (full-IR replay cost). Honors SMT_RHI2D_TILE_SIZE when set.
int rhi2d_adaptive_tile_pixel_size(int viewport_w, int viewport_h);

// Default 16px skirt.
int rhi2d_tile_outset_px();

// True when parallel mode is kTile (legacy helper).
bool rhi2d_tile_raster_enabled();

// True when parallel mode is kLayer.
bool rhi2d_layer_raster_enabled();

// Clamp worker count to [1, 8] (or 1 when serial / single job).
int rhi2d_tile_raster_worker_count(size_t tile_count);
int rhi2d_parallel_worker_count(size_t job_count);

// Enumerate viewport tiles that intersect |damage| (or all if full_damage).
std::vector<Rhi2dRasterTile> enumerate_viewport_tiles(int viewport_w,
                                                      int viewport_h,
                                                      const RECT& damage,
                                                      bool full_damage,
                                                      uint64_t gen);

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_RHI2D_IMPL_COMMON_CC_RASTER_TILE_H_
