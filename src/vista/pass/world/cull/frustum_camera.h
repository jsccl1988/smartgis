// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CameraMatrices → FrustumPlanes before prep fan-out (no Device).

#ifndef VISTA_PASS_WORLD_CULL_FRUSTUM_CAMERA_H_
#define VISTA_PASS_WORLD_CULL_FRUSTUM_CAMERA_H_

#include "render/rhi/rhi.h"
#include "vista/component/world/space/cull/frustum_aabb.h"
#include "vista/vista_export.h"

namespace vista {

VISTA_EXPORT FrustumPlanes extract_frustum_planes(
    const render::rhi::CameraMatrices& camera);

}  // namespace vista

#endif  // VISTA_PASS_WORLD_CULL_FRUSTUM_CAMERA_H_
