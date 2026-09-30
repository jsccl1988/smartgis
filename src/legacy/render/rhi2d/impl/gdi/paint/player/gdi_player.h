// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_IMPL_GDI_PAINT_GDI_PLAYER_H_
#define LEGACY_RENDER_RHI2D_IMPL_GDI_PAINT_GDI_PLAYER_H_

#include <windows.h>

#include "legacy/render/rhi2d/impl/gdi/paint/encode/command_buffer.h"

namespace render {
namespace detail {

// Single HDC play surface for leftover GDI: canvas immediate draws and
// GdiCommandEncoder::replay share this lane (no parallel gdiaux helpers).
class GdiPlayer {
 public:
  explicit GdiPlayer(HDC hdc);
  ~GdiPlayer();

  GdiPlayer(const GdiPlayer&) = delete;
  GdiPlayer& operator=(const GdiPlayer&) = delete;

  HDC hdc() const { return hdc_; }

  void clear_rect(int x, int y, int w, int h,
                  COLORREF color = RGB(170, 211, 223));
  void fill_rect(int x, int y, int w, int h, COLORREF color);
  void stroke_rect(int x, int y, int w, int h, COLORREF color, int pen_width);
  void blit(HBITMAP src, int dest_x, int dest_y, int dest_w, int dest_h,
            int src_x, int src_y, int src_w, int src_h);

  void set_pen(COLORREF color, int width, int style = PS_SOLID);
  void set_brush(COLORREF color, int style = BS_SOLID,
                 int hatch = HS_HORIZONTAL);
  void set_pen(const GdiSetPenArgs& args);
  void set_brush(const GdiSetBrushArgs& args);
  void release_style();

  void polyline(const POINT* pts, int count);
  // Dual-pen road casing then fill (same path for immediate and encode replay).
  void road_polyline(const POINT* pts, int count, COLORREF casing,
                     int casing_w, COLORREF fill, int fill_w);
  void poly_polygon(const POINT* pts, const int* ring_counts, int n_rings);
  // Multiple open polylines in one GDI call (same pen).
  void poly_polyline(const POINT* pts, const int* poly_counts, int n_polys);
  void ellipse(int left, int top, int right, int bottom);

  void draw_cross(long x, long y, long r);
  void draw_point_disc(long x, long y, int radius);

  // UTF-8 (GeoJSON/OGR) or ACP leftover text. Prefers GDI+ AA when available.
  void draw_anno_text(long x, long y, const char* text, int px_h = 14,
                      int halo_px = 2, float angle_deg = 0.f);

  // Encoder blob text (bytes, no NUL required).
  void draw_text_bytes(int x, int y, const char* text, int n, int height_px,
                       int halo_px, float angle_deg);

 private:
  HDC hdc_ = nullptr;
  HPEN pen_ = nullptr;
  HBRUSH brush_ = nullptr;
  HGDIOBJ old_pen_ = nullptr;
  HGDIOBJ old_brush_ = nullptr;
};

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_RHI2D_IMPL_GDI_PAINT_GDI_PLAYER_H_
