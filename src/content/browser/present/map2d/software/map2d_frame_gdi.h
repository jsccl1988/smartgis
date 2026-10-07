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

#include "vista/component/map/ir.h"

namespace content {
namespace detail {

// 0xAARRGGBB → COLORREF. Call only at the HDC edge.
inline COLORREF rgba_to_colorref(uint32_t rgba) {
  return RGB(static_cast<uint8_t>((rgba >> 16) & 0xff),
             static_cast<uint8_t>((rgba >> 8) & 0xff),
             static_cast<uint8_t>(rgba & 0xff));
}

// Rasters a MapIR to an HDC (world items projected; pixel_space as-is).
// |borrow_raster| (preferred) or |load_raster| resolves kRaster texture_key
// → tightly packed RGBA8. Borrow avoids copying the DEM hillshade bake.
void paint_map_frame_gdi(
    HDC hdc, const vista::MapIR& frame, const vista::View& view,
    bool fill_background,
    const std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba,
                             int* w, int* h)>& load_raster = {},
    const std::function<bool(uint32_t texture_key, const uint8_t** rgba, int* w,
                             int* h)>& borrow_raster = {});

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_FRAME_GDI_H_
