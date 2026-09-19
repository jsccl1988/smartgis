// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_TILE_XYZ_MATH_H_
#define SDB_TILE_XYZ_MATH_H_

#include <cmath>
#include <string>
#include <vector>

#include "base/core/bas_struct.h"

namespace gis {
namespace tile {

// XYZ / Web Mercator helpers for TileProvider (EPSG:3857 meters).
struct TileCoord {
  int z = 0;
  int x = 0;
  int y = 0;
};

struct Viewport {
  double min_x = 0;
  double min_y = 0;
  double max_x = 0;
  double max_y = 0;
  // If < 0, estimate_zoom() picks z from span.
  int z = -1;
};

inline constexpr double k_web_mercator_half = 20037508.342789244;

// Replace {z}/{x}/{y} (and optional {s} → "a") in an XYZ URL template.
inline std::string format_xyz_url(const std::string& tmpl, int z, int x,
                                  int y) {
  std::string out = tmpl;
  auto replace_all = [&out](const std::string& from, const std::string& to) {
    for (size_t pos = 0; (pos = out.find(from, pos)) != std::string::npos;) {
      out.replace(pos, from.size(), to);
      pos += to.size();
    }
  };
  replace_all("{z}", std::to_string(z));
  replace_all("{x}", std::to_string(x));
  replace_all("{y}", std::to_string(y));
  replace_all("{s}", "a");
  return out;
}

inline base::fRect tile_world_rect(int z, int x, int y) {
  const int n = 1 << z;
  const double tile_size = (2.0 * k_web_mercator_half) / static_cast<double>(n);
  base::fRect rect;
  rect.lb.x = static_cast<float>(x * tile_size - k_web_mercator_half);
  rect.rt.y = static_cast<float>(k_web_mercator_half - y * tile_size);
  rect.rt.x = static_cast<float>(rect.lb.x + tile_size);
  rect.lb.y = static_cast<float>(rect.rt.y - tile_size);
  return rect;
}

inline int estimate_zoom(const Viewport& vp, int /*tile_px*/ = 256) {
  const double span_x = std::abs(vp.max_x - vp.min_x);
  if (span_x <= 0.0) {
    return 0;
  }
  const double world = 2.0 * k_web_mercator_half;
  const double z = std::log2(world / span_x);
  int zi = static_cast<int>(std::floor(z + 1e-9));
  if (zi < 0) {
    zi = 0;
  }
  if (zi > 22) {
    zi = 22;
  }
  return zi;
}

inline std::vector<TileCoord> tiles_for_viewport(const Viewport& vp) {
  std::vector<TileCoord> out;
  int z = vp.z;
  if (z < 0) {
    z = estimate_zoom(vp);
  }
  if (z < 0) {
    z = 0;
  }
  if (z > 22) {
    z = 22;
  }
  const int n = 1 << z;
  const double tile_size = (2.0 * k_web_mercator_half) / static_cast<double>(n);
  auto clamp_idx = [n](int v) {
    if (v < 0) {
      return 0;
    }
    if (v >= n) {
      return n - 1;
    }
    return v;
  };
  const int x0 = clamp_idx(static_cast<int>(
      std::floor((vp.min_x + k_web_mercator_half) / tile_size)));
  const int x1 = clamp_idx(static_cast<int>(
      std::floor((vp.max_x + k_web_mercator_half) / tile_size)));
  const int y0 = clamp_idx(static_cast<int>(
      std::floor((k_web_mercator_half - vp.max_y) / tile_size)));
  const int y1 = clamp_idx(static_cast<int>(
      std::floor((k_web_mercator_half - vp.min_y) / tile_size)));
  const int xmin = x0 < x1 ? x0 : x1;
  const int xmax = x0 < x1 ? x1 : x0;
  const int ymin = y0 < y1 ? y0 : y1;
  const int ymax = y0 < y1 ? y1 : y0;
  // Cap fan-out so a wide viewport cannot flood the network before LRU helps.
  constexpr int k_max_tiles = 64;
  out.reserve(static_cast<size_t>((xmax - xmin + 1) * (ymax - ymin + 1)));
  for (int y = ymin; y <= ymax; ++y) {
    for (int x = xmin; x <= xmax; ++x) {
      out.push_back(TileCoord{z, x, y});
      if (static_cast<int>(out.size()) >= k_max_tiles) {
        return out;
      }
    }
  }
  return out;
}

}  // namespace tile
}  // namespace gis

#endif  // SDB_TILE_XYZ_MATH_H_
