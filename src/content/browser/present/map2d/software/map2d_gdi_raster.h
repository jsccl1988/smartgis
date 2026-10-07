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

namespace content {
namespace detail {

// Export / present paint into a 32bpp DIB section — write land fills here
// instead of GDI PolyPolygon (chunked PolyPolygon is still ~150ms on china).
struct DibSurface {
  uint32_t* pixels = nullptr;  // little-endian BGRA
  int width = 0;
  int height = 0;
  int stride_px = 0;
  bool top_down = true;

  bool valid() const {
    return pixels != nullptr && width > 0 && height > 0 && stride_px >= width;
  }

  uint32_t* row(int y) const {
    if (!top_down) {
      y = height - 1 - y;
    }
    return pixels + static_cast<size_t>(y) * static_cast<size_t>(stride_px);
  }
};

bool try_bind_dib(HDC hdc, DibSurface* out);
void fill_dib_solid(DibSurface* dib, uint32_t bgra);

// Stretch tightly packed RGBA8 into the axis-aligned bbox of |pts|.
// Clips to the HDC bitmap, then bilinear-samples coverage (keeps alpha).
// kMultiply bakes luma into the coverage then multiplies the snapped land.
// kOver is AlphaBlend (SRC_OVER).
bool blit_rgba_quad(HDC hdc, const std::vector<POINT>& pts,
                    const std::vector<uint8_t>& rgba, int tw, int th,
                    float opacity, vista::DrawBlend blend);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_GDI_RASTER_H_
