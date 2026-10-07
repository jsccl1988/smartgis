// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_GDI_RASTER_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_GDI_RASTER_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <vector>

#include "vista/component/map/ir.h"
#include "vista/component/raster/dib.h"

namespace content {
namespace detail {

// Pixel buffer for the software DIB fallback. Kernels live in vista::raster.
using DibSurface = vista::raster::DibSurface;

bool try_bind_dib(HDC hdc, DibSurface* out);
void fill_dib_solid(DibSurface* dib, uint32_t bgra);

// Stretch tightly packed RGBA8 into the axis-aligned bbox of |pts|.
// Clips to the HDC bitmap, then bilinear-samples coverage (keeps alpha).
// kMultiply bakes luma into the coverage then multiplies the snapped land.
// kOver is AlphaBlend (SRC_OVER).
bool blit_rgba_quad(HDC hdc, const std::vector<POINT>& pts,
                    const std::vector<uint8_t>& rgba, int tw, int th,
                    float opacity, vista::DrawBlend blend);
bool blit_rgba_quad(HDC hdc, const std::vector<POINT>& pts, const uint8_t* rgba,
                    int tw, int th, float opacity, vista::DrawBlend blend);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_GDI_RASTER_H_
