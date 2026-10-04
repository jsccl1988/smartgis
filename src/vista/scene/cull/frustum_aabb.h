// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_SCENE_FRUSTUM_AABB_H_
#define VISTA_SCENE_FRUSTUM_AABB_H_

#include "vista/vista_export.h"

namespace vista {

// CPU frustum vs AABB for GpuScene scale (P0). GPU BVH / occupancy compute
// remains Deferred — see rhi-3d-capability-p0 design Out of scope.

// Six clip planes as (nx, ny, nz, d). Built for column-major clip = VP * vec
// with *inward* normals: a point is inside when nx*x + ny*y + nz*z + d >= 0.
struct FrustumPlanes {
  float planes[6][4];
};

// Extract planes from column-major view and projection (clip = proj * view).
VISTA_EXPORT FrustumPlanes extract_frustum_planes(const float view[16],
                                                  const float proj[16]);

// True if the AABB is inside or intersects the frustum (not fully culled).
VISTA_EXPORT bool aabb_intersects_frustum(float min_x, float min_y, float min_z,
                                          float max_x, float max_y, float max_z,
                                          const FrustumPlanes& frustum);

// Axis-aligned world bounds of the view frustum (NDC cube ±1, RH clip).
// False when VP is not invertible.
VISTA_EXPORT bool frustum_world_aabb(const float view[16], const float proj[16],
                                     float* min_x, float* min_y, float* min_z,
                                     float* max_x, float* max_y, float* max_z);

}  // namespace vista

#endif  // VISTA_SCENE_FRUSTUM_AABB_H_
