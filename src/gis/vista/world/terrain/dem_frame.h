// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_WORLD_DEM_FRAME_H_
#define GIS_WORLD_DEM_FRAME_H_

namespace gis {

// South-of-target orbit so geographic north is toward screen top (matches
// leftover_frame_pose). yaw=0 places the eye on +Z and looks south.
inline constexpr float kDemDefaultOrbitYaw = 3.14159265f - 0.55f;

// Leftover / host geographic mesh uses Y-up with +Z=north. RH lookAt /
// rhi::look_at builds camera-right as cross(forward, up); looking +Z with
// +Y up yields right=-X. Mapping X=-lon puts east on screen-right without
// mirroring the view matrix (which would break non-geo 3D).
inline float dem_lon_to_x(double lon) {
  return static_cast<float>(-lon);
}

inline double dem_x_to_lon(float x) {
  return static_cast<double>(-x);
}

}  // namespace gis

#endif  // GIS_WORLD_DEM_FRAME_H_
