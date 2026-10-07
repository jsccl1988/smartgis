// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_RHI_SESSION_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_RHI_SESSION_H_

#include <cstdint>

#include "content/content_export.h"

namespace content {

// Product Scene3d present engine. Selected at runtime (View menu / API),
// `--scene3d-engine`, or harness env SCENE3D_ENGINE (switch wins if both set).
// Leftover GL vs D3D11 under kStereoGl is STEREO_API / SCENE3D_SHOWCASE_D3D.
//
// Prep parallel (GPUSCENE_PREP_PARALLEL / --gpuscene-prep-parallel) stays
// product default OFF until frustum cull is honest (see vista prep_cull +
// equal-profile plan M3). Scene3d session must not force it on.
enum class Scene3dEngine : uint32_t {
  kFlyCube = 0,   // DX12 RHI (default product SoT)
  kStereoGl = 1,  // Leftover stereo (OpenGL or D3D11 via STEREO_API)
  kGdi = 2,       // Software DEM paint (product HWND / Scene3dPresenter)
  kScenic = 3,    // Content-hosted scenic::Engine (scenic.dll)
};

CONTENT_EXPORT void set_scene3d_engine(Scene3dEngine engine);
CONTENT_EXPORT Scene3dEngine scene3d_engine();

// Apply `--scene3d-engine` or SCENE3D_ENGINE when set. Values (case-insensitive):
//   flycube | dx12
//   stereo_gl | opengl | gl   → kStereoGl + force STEREO_API=OpenGL
//   stereo_d3d | d3d | direct3d → kStereoGl + force STEREO_API=Direct3D
//   gdi
//   scenic
// Returns true when the switch or env selected an engine (callers should not
// override). Empty/unset switch falls through to SCENE3D_ENGINE.
CONTENT_EXPORT bool apply_scene3d_engine_from_env();

// True when the selected engine is FlyCube RHI.
CONTENT_EXPORT bool prefer_scene3d_flycube();

// True when the selected engine is leftover Stereo (GL or D3D11).
CONTENT_EXPORT bool prefer_scene3d_stereo_gl();

// True when leftover stereo is on and STEREO_API selects OpenGL.
CONTENT_EXPORT bool prefer_scene3d_stereo_opengl();

// True when leftover stereo is on and STEREO_API selects Direct3D (default).
CONTENT_EXPORT bool prefer_scene3d_stereo_d3d();

// True when the selected engine is software GDI DEM.
CONTENT_EXPORT bool prefer_scene3d_gdi();

// True when --scene3d-engine=scenic, SCENE3D_ENGINE=scenic, or kScenic.
// set_scene3d_engine(kScenic) loads scenic.dll; ScenicScene3dHost then
// creates scenic::Engine and presents. This predicate only reports the
// selection. Default product engine remains FlyCube.
CONTENT_EXPORT bool prefer_scene3d_scenic();

// Map scenic.dll beside this PE (debug stem scenic_d.dll). Idempotent.
// Called when the engine becomes kScenic, and again by ScenicScene3dHost
// before scenic::create_scene3d_engine. Does not construct the Engine.
CONTENT_EXPORT bool load_scene3d_scenic_dll();

// True only for leftover stereo (ContentMapView HWND). Scenic/GDI use the
// product HWND + Scene3dPresenter — not a SharedSurface fallback.
CONTENT_EXPORT bool force_content_mapview_3d();

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_RHI_SESSION_H_
