// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_ORBIT_GEO_FRAME_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_ORBIT_GEO_FRAME_H_

#include <vector>

#include "content/public/types.h"
#include "vista/terrain/dem/orbit_geo_frame.h"

namespace content {

// Thin present glue: content::Extent2 ↔ vista::OrbitGeoFrame. Normalization
// math lives in vista; callers outside frame/ keep this type surface.
struct OrbitGeoFrame {
  static constexpr float kTargetSpan = vista::OrbitGeoFrame::kTargetSpan;
  static constexpr float kElevBoost = vista::OrbitGeoFrame::kElevBoost;
  static constexpr float kOceanPad = vista::OrbitGeoFrame::kOceanPad;

  content::Extent2 extent{};
  float cx = 0.f;
  float cy = 0.f;
  float cz = 0.f;
  float scale = 1.f;
  bool valid = false;

  vista::OrbitGeoFrame as_vista() const {
    vista::OrbitGeoFrame v;
    v.xmin = extent.xmin;
    v.ymin = extent.ymin;
    v.xmax = extent.xmax;
    v.ymax = extent.ymax;
    v.cx = cx;
    v.cy = cy;
    v.cz = cz;
    v.scale = scale;
    v.valid = valid;
    return v;
  }

  void sync_from(const vista::OrbitGeoFrame& v) {
    extent.xmin = v.xmin;
    extent.ymin = v.ymin;
    extent.xmax = v.xmax;
    extent.ymax = v.ymax;
    cx = v.cx;
    cy = v.cy;
    cz = v.cz;
    scale = v.scale;
    valid = v.valid;
  }

  static OrbitGeoFrame from_extent(const content::Extent2& e) {
    OrbitGeoFrame f;
    f.sync_from(
        vista::OrbitGeoFrame::from_lonlat(e.xmin, e.ymin, e.xmax, e.ymax));
    return f;
  }

  bool matches_extent(const content::Extent2& e) const {
    return as_vista().matches_lonlat(e.xmin, e.ymin, e.xmax, e.ymax);
  }

  void capture_elev_center(const std::vector<float>& xyz) {
    vista::OrbitGeoFrame v = as_vista();
    v.capture_elev_center(xyz);
    cy = v.cy;
  }

  void normalize_xyz(std::vector<float>* xyz) const {
    as_vista().normalize_xyz(xyz);
  }

  void lon_lat_to_orbit(double lon, double lat, float elev_raw, float* x,
                        float* y, float* z) const {
    as_vista().lon_lat_to_orbit(lon, lat, elev_raw, x, y, z);
  }

  float sea_level_y() const { return as_vista().sea_level_y(); }

  void extent_orbit_xz(float* min_x, float* max_x, float* min_z,
                       float* max_z) const {
    as_vista().extent_orbit_xz(min_x, max_x, min_z, max_z);
  }

  float meters_to_orbit_y(float meters) const {
    return as_vista().meters_to_orbit_y(meters);
  }
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_ORBIT_GEO_FRAME_H_
