// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy_render/render3d/camera.h"

namespace render {

SmtPerspCamera* make_view3d_camera(View3dCameraKind kind,
                                   LP3DRENDERDEVICE device,
                                   Viewport3D& viewport) {
  if (!device) {
    return nullptr;
  }
  switch (kind) {
    case View3dCameraKind::kArbv:
      return new SmtArbvCamera(device, viewport);
    case View3dCameraKind::kFps:
      return new SmtFPSCamera(device, viewport);
    case View3dCameraKind::kPersp:
      return new SmtPerspCamera(device, viewport);
  }
  return new SmtPerspCamera(device, viewport);
}

}  // namespace render
