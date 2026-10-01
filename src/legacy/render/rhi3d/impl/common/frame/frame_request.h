// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI3D_IMPL_COMMON_FRAME_FRAME_REQUEST_H_
#define LEGACY_RENDER_RHI3D_IMPL_COMMON_FRAME_FRAME_REQUEST_H_

namespace render {
namespace detail {

// Staged camera / viewport inputs for one leftover 3D FrameJob.
struct Rhi3dFrameRequest {
  float yaw = 0.f;
  float pitch = 0.f;
  float distance = 0.f;
  int width_px = 0;
  int height_px = 0;
};

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_RHI3D_IMPL_COMMON_FRAME_FRAME_REQUEST_H_
