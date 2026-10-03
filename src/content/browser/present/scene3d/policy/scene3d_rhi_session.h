// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_RHI_SESSION_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_RHI_SESSION_H_

#include <cstdint>

#include "content/content_export.h"

namespace content {

// Product Scene3d present engine. Selected at runtime (View menu / API), or
// via harness env SMT_SCENE3D_ENGINE (see apply_scene3d_engine_from_env).
// Leftover GL vs D3D11 under kStereoGl is SMT_STEREO_API / SMT_SCENE3D_SHOWCASE_D3D.
enum class Scene3dEngine : uint32_t {
  kFlyCube = 0,   // DX12 RHI (default product SoT)
  kStereoGl = 1,  // Leftover stereo (OpenGL or D3D11 via SMT_STEREO_API)
  kGdi = 2,       // Software DEM paint (ContentMapView / placeholder)
};

CONTENT_EXPORT void set_scene3d_engine(Scene3dEngine engine);
CONTENT_EXPORT Scene3dEngine scene3d_engine();

// Apply SMT_SCENE3D_ENGINE when set. Values (case-insensitive):
//   flycube | dx12
//   stereo_gl | opengl | gl   → kStereoGl + force SMT_STEREO_API=OpenGL
//   stereo_d3d | d3d | direct3d → kStereoGl + force SMT_STEREO_API=Direct3D
//   gdi
// Returns true when the env selected an engine (callers should not override).
CONTENT_EXPORT bool apply_scene3d_engine_from_env();

// True when the selected engine is FlyCube RHI.
CONTENT_EXPORT bool prefer_scene3d_flycube();

// True when the selected engine is leftover Stereo (GL or D3D11).
CONTENT_EXPORT bool prefer_scene3d_stereo_gl();

// True when leftover stereo is on and SMT_STEREO_API selects OpenGL.
CONTENT_EXPORT bool prefer_scene3d_stereo_opengl();

// True when leftover stereo is on and SMT_STEREO_API selects Direct3D (default).
CONTENT_EXPORT bool prefer_scene3d_stereo_d3d();

// True when the selected engine is software GDI DEM.
CONTENT_EXPORT bool prefer_scene3d_gdi();

// Legacy name: any non-FlyCube selection (skip FlyCube attach first).
CONTENT_EXPORT bool force_content_mapview_3d();

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_RHI_SESSION_H_
