// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/raster/dib.h"

#include <algorithm>
#include <cstdlib>

namespace vista {
namespace raster {

bool try_bind_dib(HDC hdc, DibSurface* out) {
  if (!hdc || !out) {
    return false;
  }
  const HBITMAP bmp = static_cast<HBITMAP>(GetCurrentObject(hdc, OBJ_BITMAP));
  if (!bmp) {
    return false;
  }
  DIBSECTION ds{};
  if (GetObjectW(bmp, sizeof(ds), &ds) <
      static_cast<int>(sizeof(DIBSECTION))) {
    return false;
  }
  if (!ds.dsBm.bmBits || ds.dsBm.bmBitsPixel != 32 || ds.dsBm.bmWidth <= 0 ||
      ds.dsBm.bmHeight == 0 || ds.dsBm.bmWidthBytes < 4) {
    return false;
  }
  out->pixels = static_cast<uint32_t*>(ds.dsBm.bmBits);
  out->width = ds.dsBm.bmWidth;
  out->height = std::abs(ds.dsBm.bmHeight);
  out->stride_px = ds.dsBm.bmWidthBytes / 4;
  // CreateDIBSection(..., biHeight=-H) stores top-down bits. GetObjectW on
  // some stacks reports dsBmih.biHeight=+H while the buffer stays top-down —
  // trusting that positive sign inverted DibSurface::row (map2d_hdc_frame_test
  // red quad landed at y=19..59 instead of 300..340). Map2d paint/export HDCs
  // are always created top-down (see Map2dHdcPainter::export_bmp /
  // ensure_present_cache_dib); do not bind bottom-up DIBs here.
  out->top_down = true;
  return out->valid();
}

void fill_dib_solid(DibSurface* dib, uint32_t bgra) {
  if (!dib || !dib->valid()) {
    return;
  }
  const size_t width = static_cast<size_t>(dib->width);
  for (int y = 0; y < dib->height; ++y) {
    uint32_t* row = dib->row(y);
    std::fill(row, row + width, bgra);
  }
}

}  // namespace raster
}  // namespace vista
