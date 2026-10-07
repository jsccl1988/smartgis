// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// 32bpp DIB pixel buffer for the content software fallback. Not Scenic rhi2d.

#ifndef VISTA_COMPONENT_RASTER_DIB_H_
#define VISTA_COMPONENT_RASTER_DIB_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>

namespace vista {
namespace raster {

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

}  // namespace raster
}  // namespace vista

#endif  // VISTA_COMPONENT_RASTER_DIB_H_
