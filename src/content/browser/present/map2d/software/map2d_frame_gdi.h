// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_FRAME_GDI_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_FRAME_GDI_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <functional>
#include <vector>

#include "vista/map/frame.h"

namespace content {
namespace detail {

// 0xAARRGGBB → COLORREF. Call only at the HDC edge.
inline COLORREF rgba_to_colorref(uint32_t rgba) {
  return RGB(static_cast<uint8_t>((rgba >> 16) & 0xff),
             static_cast<uint8_t>((rgba >> 8) & 0xff),
             static_cast<uint8_t>(rgba & 0xff));
}

// Rasters a MapFrame to an HDC (world items projected; pixel_space as-is).
// |load_raster| resolves DrawKind::kRaster texture_key → tightly packed RGBA8.
void paint_map_frame_gdi(
    HDC hdc, const vista::MapFrame& frame, const vista::View& view,
    bool fill_background,
    const std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba,
                             int* w, int* h)>& load_raster = {});

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_FRAME_GDI_H_
