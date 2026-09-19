// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_WORLD_DEM_FRAME_H_
#define GIS_WORLD_DEM_FRAME_H_

namespace gis {

// South-of-target orbit so geographic north is toward screen top (matches
// leftover_frame_pose / 上北下南). yaw=0 places the eye on +Z and looks south.
inline constexpr float kDemDefaultOrbitYaw = 3.14159265f - 0.55f;

}  // namespace gis

#endif  // GIS_WORLD_DEM_FRAME_H_
