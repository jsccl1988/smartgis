// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_IMPL_SKIA_BACKEND_SKIA_BACKEND_H_
#define SCENIC_RHI2D_IMPL_SKIA_BACKEND_SKIA_BACKEND_H_

#include <memory>
#include <windows.h>

#include "scenic/render/rhi2d/impl/common/paint/backend/paint_backend.h"

namespace scenic {
namespace detail {

// Skia port paint lane. Without Skia pin, bootstraps via GDI+ inside Impl.
class SkiaBackend : public PaintBackend {
 public:
  explicit SkiaBackend(HDC hdc);
  ~SkiaBackend() override;

  SkiaBackend(const SkiaBackend&) = delete;
  SkiaBackend& operator=(const SkiaBackend&) = delete;

  HDC hdc() const override;

  void clear_rect(int x, int y, int w, int h,
                  COLORREF color = RGB(170, 211, 223)) override;
  void fill_rect(int x, int y, int w, int h, COLORREF color) override;
  void stroke_rect(int x, int y, int w, int h, COLORREF color,
                   int pen_width) override;
  void blit(HBITMAP src, int dest_x, int dest_y, int dest_w, int dest_h,
            int src_x, int src_y, int src_w, int src_h) override;

  void set_pen(COLORREF color, int width, int style = PS_SOLID) override;
  void set_brush(COLORREF color, int style = BS_SOLID,
                 int hatch = HS_HORIZONTAL) override;
  void set_pen(const Rhi2dSetPenArgs& args) override;
  void set_brush(const Rhi2dSetBrushArgs& args) override;
  void release_style() override;

  void polyline(const POINT* pts, int count) override;
  void road_polyline(const POINT* pts, int count, COLORREF casing, int casing_w,
                     COLORREF fill, int fill_w) override;
  void road_poly_polyline(const POINT* pts, const int* poly_counts, int n_polys,
                          COLORREF casing, int casing_w, COLORREF fill,
                          int fill_w) override;
  void poly_polygon(const POINT* pts, const int* ring_counts,
                    int n_rings) override;
  void poly_polyline(const POINT* pts, const int* poly_counts,
                     int n_polys) override;
  void ellipse(int left, int top, int right, int bottom) override;

  void draw_cross(long x, long y, long r) override;
  void draw_point_disc(long x, long y, int radius) override;
  void draw_anno_text(long x, long y, const char* text, int px_h = 14,
                      int halo_px = 2, float angle_deg = 0.f) override;
  void draw_text_bytes(int x, int y, const char* text, int n, int height_px,
                       int halo_px, float angle_deg) override;

 private:
  struct Impl;
  HDC hdc_ = nullptr;
  std::unique_ptr<Impl> impl_;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_IMPL_SKIA_BACKEND_SKIA_BACKEND_H_
