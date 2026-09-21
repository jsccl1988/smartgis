// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_SCENE3D_STEREO_HWND_VIEW_H_
#define SMT_LEGACY_RENDER_SCENE3D_STEREO_HWND_VIEW_H_

// C ABI: leftover OpenGL stereo (seed_sample_map_into_scene + SmtScene HUD)
// for product shells that must not link legacy_render (SP5). Call via
// LoadLibrary("legacy_render[_d].dll") + GetProcAddress.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#if defined(SCENE3D_EXPORTS) || defined(LEGACY_RENDER_EXPORTS)
#define SMT_STEREO_HWND_API __declspec(dllexport)
#else
#define SMT_STEREO_HWND_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Create GL device on |hwnd|, seed China DEM + vectors + labels, frame camera.
// Returns opaque handle or nullptr on failure.
SMT_STEREO_HWND_API void* smt_stereo_hwnd_create(HWND hwnd);

SMT_STEREO_HWND_API void smt_stereo_hwnd_destroy(void* view);

// Resize viewport / GL surface. Returns non-zero on success.
SMT_STEREO_HWND_API int smt_stereo_hwnd_resize(void* view, int width_px,
                                               int height_px);

// Orbit params match Scene3dController (yaw/pitch/distance). Renders one
// leftover frame (black clear, hypsometric DEM, draped vectors, labels,
// NorthArray + Fps HUD) and SwapBuffers. Returns non-zero on success.
SMT_STEREO_HWND_API int smt_stereo_hwnd_present(void* view, float yaw,
                                                float pitch, float distance);

// After present, BitBlt the HWND client into |hdc| (for paint_to_dc hosts).
SMT_STEREO_HWND_API int smt_stereo_hwnd_blit(void* view, HDC hdc, int width_px,
                                             int height_px);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // SMT_LEGACY_RENDER_SCENE3D_STEREO_HWND_VIEW_H_
