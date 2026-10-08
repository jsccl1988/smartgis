// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_HDC_SCENE3D_HDC_LOGO_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_HDC_SCENE3D_HDC_LOGO_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace content {
namespace detail {

// Engine badge painted into an HDC (mem-DC BitBlt / soft GDI paths).
bool measure_engine_logo(HDC hdc, const char* engine_name, SIZE* out_box);
void paint_engine_logo_at(HDC hdc, int x, int y, const char* engine_name);
void paint_engine_logo_corner(HDC hdc, int width_px, int height_px,
                              const char* engine_name);

// WS_CHILD badge for DXGI/GL flip surfaces that ignore GDI on the present HWND.
void hide_engine_logo_overlay(HWND* logo_hwnd);
void release_engine_logo_overlay(HWND* logo_hwnd);
void sync_engine_logo_overlay(HWND* logo_hwnd, HWND parent, int width_px,
                              int height_px, const char* label);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_HDC_SCENE3D_HDC_LOGO_H_
