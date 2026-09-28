// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_RHI_SESSION_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_RHI_SESSION_H_

#include <cstdint>

#include "content/content_export.h"

namespace content {

// Product Scene3d present engine. Selected at runtime (View menu / API);
// not controlled by environment variables.
enum class Scene3dEngine : uint32_t {
  kFlyCube = 0,   // DX12 RHI (default product SoT)
  kStereoGl = 1,  // Leftover OpenGL stereo
  kGdi = 2,       // Software DEM paint (ContentMapView / placeholder)
};

CONTENT_EXPORT void set_scene3d_engine(Scene3dEngine engine);
CONTENT_EXPORT Scene3dEngine scene3d_engine();

// True when the selected engine is FlyCube RHI.
CONTENT_EXPORT bool prefer_scene3d_flycube();

// True when the selected engine is leftover Stereo/GL.
CONTENT_EXPORT bool prefer_scene3d_stereo_gl();

// True when the selected engine is software GDI DEM.
CONTENT_EXPORT bool prefer_scene3d_gdi();

// Legacy name: any non-FlyCube selection (skip FlyCube attach first).
CONTENT_EXPORT bool force_content_mapview_3d();

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_RHI_SESSION_H_
