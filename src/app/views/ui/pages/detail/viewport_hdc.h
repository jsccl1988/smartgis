// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_PAGES_DETAIL_VIEWPORT_HDC_H_
#define APP_VIEWS_UI_PAGES_DETAIL_VIEWPORT_HDC_H_

#include <cstdint>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "ui/gfx/raster/shell_raster.h"
#include "ui/views/map/viewport/draw_host.h"

namespace app {
namespace detail {

// Opaque fill used while GpuPresent waits for the first DXGI frame.
// Soft MapIR→HDC / soft DEM SoT paths are gone; cold start is fill + request.
inline void fill_hdc_rgb(HDC hdc, int w, int h, COLORREF rgb) {
  if (!hdc || w <= 0 || h <= 0) {
    return;
  }
  HBRUSH bg = CreateSolidBrush(rgb);
  RECT full = {0, 0, w, h};
  FillRect(hdc, &full, bg);
  DeleteObject(bg);
}

inline void fill_hdc_and_request_frame(HDC hdc,
                                       int w,
                                       int h,
                                       COLORREF rgb,
                                       ui::views::DrawHost* pane) {
  fill_hdc_rgb(hdc, w, h, rgb);
  if (pane) {
    pane->request_frame();
  }
}

// Borrow shell overlay BGRA for a DrawHost gpu_present callback.
// Returns null when the pane has no published shell raster.
inline const ui::gfx::ShellRaster* borrow_pane_shell(
    ui::views::DrawHost* pane,
    std::vector<uint8_t>* copy,
    ui::gfx::ShellRaster* shell,
    uint64_t* gen) {
  if (!pane || !copy || !shell || !gen) {
    return nullptr;
  }
  if (!pane->snapshot_shell_overlay(copy, shell, gen)) {
    return nullptr;
  }
  return shell;
}

// Product-default Map Edit cold-start ocean tint (matches china carto bg).
inline constexpr COLORREF kMap2dColdFillRgb = RGB(170, 211, 223);
// FlyCube Scene3d waiting for first present_gpu.
inline constexpr COLORREF kScene3dColdFillRgb = RGB(120, 165, 210);
// Explicit GDI engine placeholder (no soft DEM bake).
inline constexpr COLORREF kScene3dGdiPlaceholderRgb = RGB(40, 48, 56);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_UI_PAGES_DETAIL_VIEWPORT_HDC_H_
