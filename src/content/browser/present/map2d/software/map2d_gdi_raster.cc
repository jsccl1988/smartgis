// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// GN-DEP: //src/vista/component/raster:raster

#include "content/browser/present/map2d/software/map2d_gdi_raster.h"

#include "vista/component/raster/blit.h"
#include "vista/component/raster/dib.h"

namespace content {
namespace detail {

bool try_bind_dib(HDC hdc, DibSurface* out) {
  return vista::raster::try_bind_dib(hdc, out);
}

void fill_dib_solid(DibSurface* dib, uint32_t bgra) {
  vista::raster::fill_dib_solid(dib, bgra);
}

bool blit_rgba_quad(HDC hdc, const std::vector<POINT>& pts,
                    const std::vector<uint8_t>& rgba, int tw, int th,
                    float opacity, vista::DrawBlend blend) {
  return vista::raster::blit_rgba_quad(hdc, pts, rgba, tw, th, opacity, blend);
}

bool blit_rgba_quad(HDC hdc, const std::vector<POINT>& pts, const uint8_t* rgba,
                    int tw, int th, float opacity, vista::DrawBlend blend) {
  return vista::raster::blit_rgba_quad(hdc, pts, rgba, tw, th, opacity, blend);
}

}  // namespace detail
}  // namespace content
