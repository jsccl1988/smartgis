// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_PRESENT_SCENE3D_RHI_SESSION_H_
#define APP_VIEWS_PRESENT_SCENE3D_RHI_SESSION_H_

namespace app {

// True when SMT_FORCE_CONTENT_MAPVIEW_3D=1, or SMT_PREFER_FLYCUBE_3D=0.
// Opt-out of the product FlyCube default (ContentMapView + stereo/GDI SoT).
bool force_content_mapview_3d();

// Prefer FlyCube RHI for kScene3d by default unless force_content_mapview_3d().
// MapViewport owns the FlyCube device; this header only exposes SoT policy.
bool prefer_scene3d_flycube();

}  // namespace app

#endif  // APP_VIEWS_PRESENT_SCENE3D_RHI_SESSION_H_
