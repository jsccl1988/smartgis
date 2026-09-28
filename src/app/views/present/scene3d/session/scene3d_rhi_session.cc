// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/present/scene3d/session/scene3d_rhi_session.h"

#include <cstdlib>

namespace app {

bool force_content_mapview_3d() {
  if (const char* env = std::getenv("SMT_FORCE_CONTENT_MAPVIEW_3D")) {
    if (env[0] == '1' && env[1] == '\0') {
      return true;
    }
  }
  // Explicit opt-out of FlyCube keeps ContentMapView + shell GDI SoT.
  if (const char* prefer = std::getenv("SMT_PREFER_FLYCUBE_3D")) {
    if (prefer[0] == '0' && prefer[1] == '\0') {
      return true;
    }
  }
  return false;
}

bool prefer_scene3d_flycube() {
  // Default: FlyCube RHI (present_gpu). Opt out with FORCE_CONTENT=1 or
  // SMT_PREFER_FLYCUBE_3D=0 (hang-prone hosts / --self-test). Stereo and
  // GDI DEM remain paint fallbacks when attach or present fails.
  if (force_content_mapview_3d()) {
    return false;
  }
  return true;
}

}  // namespace app
