// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_ORBIT_GEO_FRAME_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_ORBIT_GEO_FRAME_H_

#include <algorithm>
#include <cmath>
#include <vector>

#include "content/public/map_layer_types.h"
#include "vista/terrain/dem/dem_frame.h"

namespace content {

// Shared geographic → orbit-normalized transform for China 3D terrain and
// atmosphere passes. Matches OrbitFrame::project_lon_lat:
//   raw mesh: X = -lon, Y = elev (DEM units), Z = lat
//   orbit:    (raw - center) * scale, with Y *= kElevBoost
struct OrbitGeoFrame {
  static constexpr float kTargetSpan = 3.2f;
  // 1.0: DEM fit_vertical_exaggeration already sets relief; avoid double boost.
  static constexpr float kElevBoost = 1.0f;
  // Extra ocean skirt beyond the active lon/lat extent (orbit units).
  static constexpr float kOceanPad = 0.15f;

  content::Extent2 extent{};
  float cx = 0.f;
  float cy = 0.f;
  float cz = 0.f;
  float scale = 1.f;
  bool valid = false;

  static OrbitGeoFrame from_extent(const content::Extent2& e) {
    OrbitGeoFrame f;
    f.extent = e;
    if (!(e.xmax > e.xmin) || !(e.ymax > e.ymin)) {
      return f;
    }
    const float minx = vista::dem_lon_to_x(e.xmax);
    const float maxx = vista::dem_lon_to_x(e.xmin);
    const float minz = static_cast<float>(e.ymin);
    const float maxz = static_cast<float>(e.ymax);
    f.cx = 0.5f * (minx + maxx);
    f.cz = 0.5f * (minz + maxz);
    f.cy = 0.f;
    // Floor must stay far below sub-degree lab pads (hex/mine/coast). A 1.f
    // floor collapsed ~0.03° hex extent to ~3% of kTargetSpan so the amber
    // volume sat as a speck in a black frame.
    const float span =
        (std::max)(maxx - minx, (std::max)(maxz - minz, 1.0e-6f));
    f.scale = kTargetSpan / span;
    f.valid = true;
    return f;
  }

  bool matches_extent(const content::Extent2& e) const {
    constexpr double kEps = 1.0e-6;
    return valid && std::fabs(extent.xmin - e.xmin) < kEps &&
           std::fabs(extent.ymin - e.ymin) < kEps &&
           std::fabs(extent.xmax - e.xmax) < kEps &&
           std::fabs(extent.ymax - e.ymax) < kEps;
  }

  // Mid elevation of a raw DEM xyz buffer (before normalize). Sea level is 0.
  void capture_elev_center(const std::vector<float>& xyz) {
    if (xyz.size() < 3) {
      cy = 0.f;
      return;
    }
    float min_y = xyz[1];
    float max_y = xyz[1];
    for (size_t i = 1; i + 1 < xyz.size(); i += 3) {
      min_y = (std::min)(min_y, xyz[i]);
      max_y = (std::max)(max_y, xyz[i]);
    }
    cy = 0.5f * (min_y + max_y);
  }

  void normalize_xyz(std::vector<float>* xyz) const {
    if (!xyz || !valid) {
      return;
    }
    for (size_t i = 0; i + 2 < xyz->size(); i += 3) {
      (*xyz)[i] = ((*xyz)[i] - cx) * scale;
      (*xyz)[i + 1] = ((*xyz)[i + 1] - cy) * scale * kElevBoost;
      (*xyz)[i + 2] = ((*xyz)[i + 2] - cz) * scale;
    }
  }

  void lon_lat_to_orbit(double lon, double lat, float elev_raw, float* x,
                        float* y, float* z) const {
    if (x) {
      *x = (vista::dem_lon_to_x(lon) - cx) * scale;
    }
    if (y) {
      *y = (elev_raw - cy) * scale * kElevBoost;
    }
    if (z) {
      *z = (static_cast<float>(lat) - cz) * scale;
    }
  }

  // Orbit Y of geographic sea level (elev_raw = 0).
  float sea_level_y() const { return (0.f - cy) * scale * kElevBoost; }

  void extent_orbit_xz(float* min_x, float* max_x, float* min_z,
                       float* max_z) const {
    float x0 = 0.f;
    float x1 = 0.f;
    float z0 = 0.f;
    float z1 = 0.f;
    lon_lat_to_orbit(extent.xmin, extent.ymin, 0.f, &x0, nullptr, &z0);
    lon_lat_to_orbit(extent.xmax, extent.ymax, 0.f, &x1, nullptr, &z1);
    if (min_x) {
      *min_x = (std::min)(x0, x1);
    }
    if (max_x) {
      *max_x = (std::max)(x0, x1);
    }
    if (min_z) {
      *min_z = (std::min)(z0, z1);
    }
    if (max_z) {
      *max_z = (std::max)(z0, z1);
    }
  }

  // Map GIS meters (cloud base/top, wave Hs) into orbit Y deltas.
  float meters_to_orbit_y(float meters) const {
    // DEM verts use elev * scale * kElevBoost. Atmosphere Hs / cloud base are
    // GIS meters on the same axis — keep a sizable fraction of that scale so
    // China-frame waves read as living water (0.05 left Hs ≈ flat navy).
    return meters * scale * kElevBoost * 0.40f;
  }
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_ORBIT_GEO_FRAME_H_
