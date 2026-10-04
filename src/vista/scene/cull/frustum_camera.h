// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CameraMatrices → FrustumPlanes before prep fan-out (no Device).

#ifndef VISTA_SCENE_CULL_FRUSTUM_CAMERA_H_
#define VISTA_SCENE_CULL_FRUSTUM_CAMERA_H_

#include "render/rhi/rhi.h"
#include "vista/scene/cull/frustum_aabb.h"
#include "vista/vista_export.h"

namespace vista {

VISTA_EXPORT FrustumPlanes extract_frustum_planes(
    const render::rhi::CameraMatrices& camera);

}  // namespace vista

#endif  // VISTA_SCENE_CULL_FRUSTUM_CAMERA_H_
