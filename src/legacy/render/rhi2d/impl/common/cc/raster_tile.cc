// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/common/cc/raster_tile.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace render {
namespace detail {
namespace {

bool env_is_off(const char* v) {
  if (v == nullptr || v[0] == '\0') {
    return false;
  }
  return _stricmp(v, "0") == 0 || _stricmp(v, "false") == 0 ||
         _stricmp(v, "off") == 0 || _stricmp(v, "no") == 0;
}

RECT clip_rect(RECT r, int vw, int vh) {
  r.left = (std::max)(0L, r.left);
  r.top = (std::max)(0L, r.top);
  r.right = (std::min)(static_cast<LONG>(vw), r.right);
  r.bottom = (std::min)(static_cast<LONG>(vh), r.bottom);
  return r;
}

bool rects_intersect(const RECT& a, const RECT& b) {
  return a.left < b.right && b.left < a.right && a.top < b.bottom &&
         b.top < a.bottom;
}

}  // namespace

int rhi2d_tile_pixel_size() {
  const char* v = std::getenv("SMT_RHI2D_TILE_SIZE");
  if (v == nullptr || v[0] == '\0') {
    return 256;
  }
  const int n = std::atoi(v);
  if (n < 128) {
    return 128;
  }
  if (n > 1024) {
    return 1024;
  }
  return n;
}

int rhi2d_tile_outset_px() {
  const char* v = std::getenv("SMT_RHI2D_TILE_OUTSET");
  if (v == nullptr || v[0] == '\0') {
    return 16;
  }
  const int n = std::atoi(v);
  if (n < 0) {
    return 0;
  }
  if (n > 64) {
    return 64;
  }
  return n;
}

Rhi2dParallelMode rhi2d_parallel_mode() {
  const char* p = std::getenv("SMT_RHI2D_PARALLEL");
  if (p != nullptr && p[0] != '\0') {
    if (_stricmp(p, "serial") == 0 || _stricmp(p, "off") == 0 ||
        _stricmp(p, "0") == 0 || _stricmp(p, "false") == 0 ||
        _stricmp(p, "no") == 0) {
      return Rhi2dParallelMode::kSerial;
    }
    if (_stricmp(p, "layer") == 0) {
      return Rhi2dParallelMode::kLayer;
    }
    if (_stricmp(p, "tile") == 0 || _stricmp(p, "1") == 0 ||
        _stricmp(p, "true") == 0 || _stricmp(p, "on") == 0) {
      return Rhi2dParallelMode::kTile;
    }
  }
  // Legacy fallback when PARALLEL is unset.
  if (env_is_off(std::getenv("SMT_RHI2D_TILE_RASTER"))) {
    return Rhi2dParallelMode::kSerial;
  }
  // Debug Edit/china: default tile execute has been AV'ing after prep on
  // first ZoomToRect (legacy.browse.2d). Keep Release on tile; opt in Debug
  // with SMT_RHI2D_PARALLEL=tile / SMT_RHI2D_TILE_FORCE=1.
#if defined(_DEBUG)
  return Rhi2dParallelMode::kSerial;
#else
  return Rhi2dParallelMode::kTile;
#endif
}

bool rhi2d_tile_raster_enabled() {
  return rhi2d_parallel_mode() == Rhi2dParallelMode::kTile;
}

bool rhi2d_layer_raster_enabled() {
  return rhi2d_parallel_mode() == Rhi2dParallelMode::kLayer;
}

int rhi2d_parallel_worker_count(size_t job_count) {
  if (rhi2d_parallel_mode() == Rhi2dParallelMode::kSerial || job_count <= 1) {
    return 1;
  }
  unsigned hw = std::thread::hardware_concurrency();
  if (hw == 0) {
    hw = 2;
  }
  // Prefer more of the machine for full-IR tile/layer replay; still cap to
  // avoid CreateCompatibleDC storms on huge grids.
  int n = static_cast<int>(hw);
  if (n < 2) {
    n = 2;
  }
  if (n > 8) {
    n = 8;
  }
  if (static_cast<size_t>(n) > job_count) {
    n = static_cast<int>(job_count);
  }
  return n;
}

int rhi2d_adaptive_tile_pixel_size(int viewport_w, int viewport_h) {
  int tile = rhi2d_tile_pixel_size();
  // Explicit SMT_RHI2D_TILE_SIZE keeps the env value for A/B harnesses.
  if (const char* forced = std::getenv("SMT_RHI2D_TILE_SIZE");
      forced != nullptr && forced[0] != '\0') {
    return tile;
  }
  if (viewport_w <= 0 || viewport_h <= 0) {
    return tile;
  }
  // Prefer ~2x2 tiles so AABB-culled execute_tile can beat serial replay.
  const int tile_w = (viewport_w + 1) / 2;
  const int tile_h = (viewport_h + 1) / 2;
  tile = (std::max)(tile_w, tile_h);
  if (tile < 128) {
    tile = 128;
  }
  if (tile > 1024) {
    tile = 1024;
  }
  return tile;
}

int rhi2d_tile_raster_worker_count(size_t tile_count) {
  return rhi2d_parallel_worker_count(tile_count);
}

std::vector<Rhi2dRasterTile> enumerate_viewport_tiles(int viewport_w,
                                                      int viewport_h,
                                                      const RECT& damage,
                                                      bool full_damage,
                                                      uint64_t gen) {
  std::vector<Rhi2dRasterTile> out;
  if (viewport_w <= 0 || viewport_h <= 0) {
    return out;
  }

  const int tile = rhi2d_adaptive_tile_pixel_size(viewport_w, viewport_h);
  const int outset = rhi2d_tile_outset_px();
  const RECT vp{0, 0, viewport_w, viewport_h};
  RECT dirty = full_damage ? vp : clip_rect(damage, viewport_w, viewport_h);
  if (!full_damage && (dirty.right <= dirty.left || dirty.bottom <= dirty.top)) {
    dirty = vp;
  }

  const int cols = (viewport_w + tile - 1) / tile;
  const int rows = (viewport_h + tile - 1) / tile;
  out.reserve(static_cast<size_t>(cols * rows));

  for (int ty = 0; ty < rows; ++ty) {
    for (int tx = 0; tx < cols; ++tx) {
      RECT center;
      center.left = static_cast<LONG>(tx * tile);
      center.top = static_cast<LONG>(ty * tile);
      center.right =
          static_cast<LONG>((std::min)(viewport_w, tx * tile + tile));
      center.bottom =
          static_cast<LONG>((std::min)(viewport_h, ty * tile + tile));
      if (center.right <= center.left || center.bottom <= center.top) {
        continue;
      }
      if (!full_damage && !rects_intersect(center, dirty)) {
        continue;
      }

      RECT paint = center;
      paint.left -= outset;
      paint.top -= outset;
      paint.right += outset;
      paint.bottom += outset;
      paint = clip_rect(paint, viewport_w, viewport_h);

      Rhi2dRasterTile t;
      t.tx = tx;
      t.ty = ty;
      t.center = center;
      t.paint = paint;
      t.gen = gen;
      out.push_back(t);
    }
  }
  return out;
}

}  // namespace detail
}  // namespace render
