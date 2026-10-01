// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_GDI_DRAW_DEVICE_H_
#define SMT_LEGACY_RENDER_GDI_DRAW_DEVICE_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace render {
namespace detail {

class Rhi2dCartoDraw;

// Device-space playback helpers (post LP→DP prep) for Rhi2dCartoDraw.
class GdiDeviceDraw {
 public:
  explicit GdiDeviceDraw(Rhi2dCartoDraw* carto_draw) : c_(carto_draw) {}

  int draw_device_polyline(const POINT* pts, int n);
  int draw_device_polylines(const POINT* pts, const int* poly_counts,
                            int n_polys);
  int draw_device_polygon(const POINT* pts, const int* ring_counts,
                          int n_rings);
  int draw_device_point(int x, int y);
  int draw_device_anno(int x, int y, const char* text);

 private:
  Rhi2dCartoDraw* c_;
};

}  // namespace detail
}  // namespace render

#endif  // SMT_LEGACY_RENDER_GDI_DRAW_DEVICE_H_
