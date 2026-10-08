// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Viewport AABB tiles (Scenic RasterTile semantics, no HDC) plus style
// slice keys. cache_key = layer × world tile so un-crossed tiles splice.

#ifndef VISTA_COMPONENT_MAP_LAYOUT_SLICE_KEY_H_
#define VISTA_COMPONENT_MAP_LAYOUT_SLICE_KEY_H_

#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

#include "vista/component/map/view.h"

namespace vista {
namespace detail {

inline constexpr int kLayoutAabbTilePx = 256;
// Scenic rhi2d-tile-outset default. World skirt so strokes at tile edges
// still tessellate into the slice (GPU compose, not HDC blit).
inline constexpr int kLayoutTileOutsetPx = 16;

// One world-grid cell covering the view, with skirt AABB for geom tests.
struct LayoutTile {
  int tx = 0;
  int ty = 0;
  double min_x = 0;
  double min_y = 0;
  double max_x = 0;
  double max_y = 0;
};

inline double layout_tile_world_size(const View& view) {
  if (view.width_px == 0 || view.max_x <= view.min_x) {
    return 0;
  }
  const double wupp =
      (view.max_x - view.min_x) / static_cast<double>(view.width_px);
  if (!(wupp > 0.0)) {
    return 0;
  }
  return static_cast<double>(kLayoutAabbTilePx) * wupp;
}

inline double layout_tile_outset_world(const View& view) {
  if (view.width_px == 0 || view.max_x <= view.min_x) {
    return 0;
  }
  const double wupp =
      (view.max_x - view.min_x) / static_cast<double>(view.width_px);
  if (!(wupp > 0.0)) {
    return 0;
  }
  return static_cast<double>(kLayoutTileOutsetPx) * wupp;
}

inline uint32_t layout_tile_hash(int tx, int ty) {
  return (static_cast<uint32_t>(tx) * 73856093u) ^
         (static_cast<uint32_t>(ty) * 19349663u);
}

inline uint32_t layout_aabb_tile_id(const View& view) {
  const double tile_w = layout_tile_world_size(view);
  if (!(tile_w > 0.0) || view.height_px == 0 || view.max_y <= view.min_y) {
    return 0;
  }
  const int tx = static_cast<int>(std::floor(view.min_x / tile_w));
  const int ty = static_cast<int>(std::floor(view.min_y / tile_w));
  return layout_tile_hash(tx, ty);
}

inline uint64_t layout_slice_key(std::string_view layer_id, double zoom, int tx,
                                 int ty) {
  uint64_t h = static_cast<uint64_t>(layout_tile_hash(tx, ty)) << 32;
  h ^= static_cast<uint64_t>(static_cast<uint32_t>(zoom * 8.0 + 0.5));
  for (unsigned char c : layer_id) {
    h = (h * 16777619ull) ^ c;
  }
  return h == 0 ? 1 : h;
}

inline uint64_t layout_slice_key(const View& view, std::string_view layer_id,
                                 double zoom) {
  const double tile_w = layout_tile_world_size(view);
  if (!(tile_w > 0.0)) {
    return 0;
  }
  const int tx = static_cast<int>(std::floor(view.min_x / tile_w));
  const int ty = static_cast<int>(std::floor(view.min_y / tile_w));
  return layout_slice_key(layer_id, zoom, tx, ty);
}

inline bool aabb_intersects(double min_x, double min_y, double max_x,
                            double max_y, const LayoutTile& tile) {
  return min_x <= tile.max_x && max_x >= tile.min_x && min_y <= tile.max_y &&
         max_y >= tile.min_y;
}

inline bool aabb_contained(double min_x, double min_y, double max_x,
                           double max_y, const LayoutTile& tile) {
  return min_x >= tile.min_x && min_y >= tile.min_y && max_x <= tile.max_x &&
         max_y <= tile.max_y;
}

// World tiles whose un-skirted cell meets the view. Skirt is only on the
// AABB used for geom hit tests / clip (strokes at the tile edge).
inline std::vector<LayoutTile> enumerate_layout_tiles(const View& view) {
  std::vector<LayoutTile> out;
  const double tile_w = layout_tile_world_size(view);
  const double outset = layout_tile_outset_world(view);
  if (!(tile_w > 0.0) || view.height_px == 0 || view.max_y <= view.min_y) {
    return out;
  }
  const double x0 = view.min_x;
  const double y0 = view.min_y;
  const double x1 = view.max_x;
  const double y1 = view.max_y;
  const int tx0 = static_cast<int>(std::floor(x0 / tile_w));
  const int ty0 = static_cast<int>(std::floor(y0 / tile_w));
  const int tx1 = static_cast<int>(std::floor((x1 - 1e-12) / tile_w));
  const int ty1 = static_cast<int>(std::floor((y1 - 1e-12) / tile_w));
  if (tx1 < tx0 || ty1 < ty0) {
    return out;
  }
  out.reserve(static_cast<size_t>(tx1 - tx0 + 1) *
              static_cast<size_t>(ty1 - ty0 + 1));
  for (int ty = ty0; ty <= ty1; ++ty) {
    for (int tx = tx0; tx <= tx1; ++tx) {
      LayoutTile tile;
      tile.tx = tx;
      tile.ty = ty;
      tile.min_x = static_cast<double>(tx) * tile_w - outset;
      tile.min_y = static_cast<double>(ty) * tile_w - outset;
      tile.max_x = static_cast<double>(tx + 1) * tile_w + outset;
      tile.max_y = static_cast<double>(ty + 1) * tile_w + outset;
      out.push_back(tile);
    }
  }
  return out;
}

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_LAYOUT_SLICE_KEY_H_
