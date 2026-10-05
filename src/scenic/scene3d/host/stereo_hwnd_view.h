// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_STEREO_HWND_VIEW_H_
#define SCENIC_SCENE3D_STEREO_HWND_VIEW_H_

// C ABI: leftover stereo (seed_sample_map_into_scene + Scene HUD) for
// product shells that must not link scenic_impl (SP5). Call via
// LoadLibrary("scenic_impl[_d].dll") + GetProcAddress. Create loads
// scenic_render_d3d / scenic_render_gl for the device factory.
// Default backend is D3D11 (CreateD3DRenderDevice). Set STEREO_API=OpenGL
// or SCENE3D_SHOWCASE_D3D=0 to use Create3DRenderDevice (OpenGL).

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#if defined(SCENIC_IMPL_EXPORTS)
#define STEREO_HWND_API __declspec(dllexport)
#else
#define STEREO_HWND_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Create GL/D3D device on |hwnd|, seed per SCENE3D_SHOWCASE_MODE
// (default china DEM + vectors + labels), frame camera.
// Returns opaque handle or nullptr on failure.
STEREO_HWND_API void* stereo_hwnd_create(HWND hwnd);

STEREO_HWND_API void stereo_hwnd_destroy(void* view);

// Resize viewport / GL surface. Returns non-zero on success.
STEREO_HWND_API int stereo_hwnd_resize(void* view, int width_px,
                                               int height_px);

// Orbit params match Scene3dController (yaw/pitch/distance). Renders one
// leftover frame (black clear, hypsometric DEM, draped vectors, labels,
// NorthArray + Fps HUD) and SwapBuffers. Returns non-zero on success.
STEREO_HWND_API int stereo_hwnd_present(void* view, float yaw,
                                                float pitch, float distance);

// After present, BitBlt the HWND client into |hdc| (for paint_to_dc hosts).
STEREO_HWND_API int stereo_hwnd_blit(void* view, HDC hdc, int width_px,
                                             int height_px);

// Read back the front buffer as tightly packed BGR24 (bottom-up, like BMP).
// |out_bgr24| must hold width_px * height_px * 3 bytes. Returns non-zero on
// success. Prefer this over GDI BitBlt — leftover GL has no PFD_SUPPORT_GDI.
STEREO_HWND_API int stereo_hwnd_capture_bgr24(void* view,
                                                      unsigned char* out_bgr24,
                                                      int width_px,
                                                      int height_px);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // SCENIC_SCENE3D_STEREO_HWND_VIEW_H_
