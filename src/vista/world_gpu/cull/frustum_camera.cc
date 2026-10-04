// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/world_gpu/cull/frustum_camera.h"

namespace vista {

FrustumPlanes extract_frustum_planes(const render::rhi::CameraMatrices& camera) {
  return extract_frustum_planes(camera.view, camera.proj);
}

}  // namespace vista
