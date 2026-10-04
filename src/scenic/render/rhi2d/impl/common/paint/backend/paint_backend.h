// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_IMPL_COMMON_PAINT_BACKEND_PAINT_BACKEND_H_
#define SCENIC_RHI2D_IMPL_COMMON_PAINT_BACKEND_PAINT_BACKEND_H_

#include <cstddef>
#include <new>
#include <windows.h>

#include "scenic/detail/viewport.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/encode/command_buffer.h"

namespace scenic {
namespace detail {

// Backend-agnostic paint lane. Concrete backends live in each port DLL;
// selection is LoadLibrary(chAPI) — not compile macros.
class PaintBackend {
 public:
  virtual ~PaintBackend() = default;

  virtual HDC hdc() const = 0;

  virtual void clear_rect(int x, int y, int w, int h,
                          COLORREF color = RGB(170, 211, 223)) = 0;
  virtual void fill_rect(int x, int y, int w, int h, COLORREF color) = 0;
  virtual void stroke_rect(int x, int y, int w, int h, COLORREF color,
                           int pen_width) = 0;
  virtual void blit(HBITMAP src, int dest_x, int dest_y, int dest_w, int dest_h,
                    int src_x, int src_y, int src_w, int src_h) = 0;

  virtual void set_pen(COLORREF color, int width, int style = PS_SOLID) = 0;
  virtual void set_brush(COLORREF color, int style = BS_SOLID,
                         int hatch = HS_HORIZONTAL) = 0;
  virtual void set_pen(const Rhi2dSetPenArgs& args) = 0;
  virtual void set_brush(const Rhi2dSetBrushArgs& args) = 0;
  virtual void release_style() = 0;

  virtual void polyline(const POINT* pts, int count) = 0;
  virtual void road_polyline(const POINT* pts, int count, COLORREF casing,
                             int casing_w, COLORREF fill, int fill_w) = 0;
  virtual void road_poly_polyline(const POINT* pts, const int* poly_counts,
                                  int n_polys, COLORREF casing, int casing_w,
                                  COLORREF fill, int fill_w) = 0;
  virtual void poly_polygon(const POINT* pts, const int* ring_counts,
                            int n_rings) = 0;
  virtual void poly_polyline(const POINT* pts, const int* poly_counts,
                             int n_polys) = 0;
  virtual void ellipse(int left, int top, int right, int bottom) = 0;

  virtual void draw_cross(long x, long y, long r) = 0;
  virtual void draw_point_disc(long x, long y, int radius) = 0;
  virtual void draw_anno_text(long x, long y, const char* text, int px_h = 14,
                              int halo_px = 2, float angle_deg = 0.f) = 0;
  virtual void draw_text_bytes(int x, int y, const char* text, int n,
                               int height_px, int halo_px,
                               float angle_deg) = 0;
};

// Provided by the loaded device DLL (gdi / gdiplus / skia).
PaintBackend* emplace_paint_backend(void* storage, size_t bytes, HDC hdc);
void destroy_paint_backend(PaintBackend* backend);
base::RenderBaseApi rhi2d_port_api();
const char* rhi2d_port_name();

inline constexpr size_t k_paint_backend_storage_bytes = 1024;

// Stack-scoped backend bound to an HDC (placement-new into port storage).
class ScopedPaintBackend {
 public:
  explicit ScopedPaintBackend(HDC hdc) {
    backend_ = emplace_paint_backend(storage_, sizeof(storage_), hdc);
  }
  ~ScopedPaintBackend() {
    if (backend_) {
      destroy_paint_backend(backend_);
      backend_ = nullptr;
    }
  }

  ScopedPaintBackend(const ScopedPaintBackend&) = delete;
  ScopedPaintBackend& operator=(const ScopedPaintBackend&) = delete;

  PaintBackend* operator->() const { return backend_; }
  PaintBackend& operator*() const { return *backend_; }
  explicit operator bool() const { return backend_ != nullptr; }

 private:
  alignas(std::max_align_t) unsigned char storage_[k_paint_backend_storage_bytes];
  PaintBackend* backend_ = nullptr;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_IMPL_COMMON_PAINT_BACKEND_PAINT_BACKEND_H_
