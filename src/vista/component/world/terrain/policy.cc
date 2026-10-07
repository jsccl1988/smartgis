// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/terrain/policy.h"

#include <algorithm>
#include <cmath>

#include "vista/terrain/dem/dem_raster.h"

namespace vista {
int terrain_lod_max_edge(float camera_distance) {
  return DemRaster::lod_max_edge(camera_distance);
}

int terrain_lod_cache_key(float camera_distance) {
  return dem_seed_cache_key(camera_distance);
}

int terrain_lod_tin_stride(float camera_distance) {
  // Far → keep 1/8; mid → 1/4; near → 1/2; very near → full.
  if (camera_distance < 1.2f) {
    return 1;
  }
  if (camera_distance < 2.4f) {
    return 2;
  }
  if (camera_distance < 4.0f) {
    return 4;
  }
  return 8;
}

int terrain_lod_tin_stride_at(float camera_distance, float centroid_distance,
                              float mesh_span) {
  if (!(mesh_span > 1.0e-6f) || !(centroid_distance >= 0.f)) {
    return terrain_lod_tin_stride(camera_distance);
  }
  const float radial = centroid_distance / mesh_span;
  return terrain_lod_tin_stride(camera_distance + radial * 4.0f);
}

int terrain_lod_surface_edge(float camera_distance) {
  return terrain_lod_max_edge(camera_distance);
}

int terrain_lod_tin_cache_key(float camera_distance) {
  return terrain_lod_tin_stride(camera_distance) * 1000 +
         terrain_lod_surface_edge(camera_distance);
}

int terrain_lod_tin_cache_key(float camera_distance, float camera_x,
                              float camera_y, float camera_z) {
  const int qx = static_cast<int>(std::lround(static_cast<double>(camera_x) * 4.0));
  const int qy = static_cast<int>(std::lround(static_cast<double>(camera_y) * 4.0));
  const int qz = static_cast<int>(std::lround(static_cast<double>(camera_z) * 4.0));
  int h = qx * 73856093 ^ qy * 19349663 ^ qz * 83492791;
  if (h < 0) {
    h = -h;
  }
  return 1000000 + (h % 997) * 1000 + terrain_lod_tin_stride(camera_distance);
}

int terrain_lod_nested_rings(float camera_distance) {
  if (camera_distance < 1.2f) {
    return 4;
  }
  if (camera_distance < 2.4f) {
    return 3;
  }
  if (camera_distance < 4.0f) {
    return 2;
  }
  return 1;
}

int terrain_lod_nested_edge(float camera_distance, int ring) {
  int edge = terrain_lod_max_edge(camera_distance);
  const int r = ring < 0 ? 0 : ring;
  edge >>= r;
  if (edge < 8) {
    edge = 8;
  }
  return edge;
}

int terrain_lod_nested_cache_key(float camera_distance, int ring) {
  const int r = ring < 0 ? 0 : ring;
  return terrain_lod_nested_tile_key(
      r, terrain_lod_nested_edge(camera_distance, r));
}

int terrain_lod_nested_tile_key(int ring, int max_edge) {
  const int r = ring < 0 ? 0 : ring;
  const int e = max_edge < 0 ? 0 : max_edge;
  return 2000000 + r * 10000 + e;
}

float terrain_lod_patch_distance(float camera_distance, double view_minx,
                                 double view_miny, double view_maxx,
                                 double view_maxy, double tile_minx,
                                 double tile_miny, double tile_maxx,
                                 double tile_maxy) {
  const double hx = 0.5 * (view_maxx - view_minx);
  const double hy = 0.5 * (view_maxy - view_miny);
  if (!(hx > 0.0) || !(hy > 0.0)) {
    return camera_distance;
  }
  const double cx = 0.5 * (view_minx + view_maxx);
  const double cy = 0.5 * (view_miny + view_maxy);
  const double tx = 0.5 * (tile_minx + tile_maxx);
  const double ty = 0.5 * (tile_miny + tile_maxy);
  const double radial = std::hypot((tx - cx) / hx, (ty - cy) / hy);
  // +4.0 maps the view-box edge into the coarser lod_max_edge buckets.
  return camera_distance + static_cast<float>(radial) * 4.0f;
}

float terrain_lod_morph_weight(float patch_distance, int ring) {
  const float t = (patch_distance - 0.8f) / 5.2f;
  float w = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
  const int r = ring < 0 ? 0 : ring;
  w = (std::min)(1.f, w + 0.12f * static_cast<float>(r));
  return w;
}

}  // namespace vista
