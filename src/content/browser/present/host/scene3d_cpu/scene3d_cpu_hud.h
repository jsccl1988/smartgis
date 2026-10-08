// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_HDC_SCENE3D_HDC_HUD_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_HDC_SCENE3D_HDC_HUD_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace content {

class AtmosphereSession;
class OrbitFrame;
class Scene3dGpuPresent;

namespace detail {

void paint_soft_wind_arrows(HDC hdc, int width_px, int height_px,
                            AtmosphereSession* atmosphere,
                            Scene3dGpuPresent* gpu, const OrbitFrame* orbit);

void paint_soft_legacy_place_labels(HDC hdc, int width_px, int height_px,
                                    Scene3dGpuPresent* gpu,
                                    const OrbitFrame* orbit, HFONT* font_slot);

// Compass + status lines (no wireframe, no engine badge).
void paint_soft_hud_status(HDC hdc, int width_px, int height_px,
                           Scene3dGpuPresent* gpu,
                           AtmosphereSession* atmosphere,
                           bool hosts_shared_scene);

// Bottom-right engine badge (WS_CHILD overlay on flip surfaces).
void paint_soft_hud_engine_badge(HDC hdc, int width_px, int height_px,
                                 Scene3dGpuPresent* gpu, HWND* logo_hwnd);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_HDC_SCENE3D_HDC_HUD_H_
