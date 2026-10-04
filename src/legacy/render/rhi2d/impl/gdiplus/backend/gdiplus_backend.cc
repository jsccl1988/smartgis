// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/gdiplus/backend/gdiplus_backend.h"

#include <objidl.h>
#include <gdiplus.h>

#include <string>
#include <vector>

#include "gis/datasource/ogr/ogr_text_encoding.h"
#include "legacy/render/rhi2d/impl/gdiplus/aa/gdiplus.h"

namespace render {
namespace detail {
namespace {

Gdiplus::Color colorref_to_gp(COLORREF c, BYTE a = 255) {
  return Gdiplus::Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

Gdiplus::DashStyle dash_from_pen_style(int style) {
  switch (style) {
    case PS_DASH:
      return Gdiplus::DashStyleDash;
    case PS_DOT:
      return Gdiplus::DashStyleDot;
    case PS_DASHDOT:
      return Gdiplus::DashStyleDashDot;
    case PS_DASHDOTDOT:
      return Gdiplus::DashStyleDashDotDot;
    default:
      return Gdiplus::DashStyleSolid;
  }
}

}  // namespace

GdiPlusBackend::GdiPlusBackend(HDC hdc) : hdc_(hdc) {
  (void)gdiplus_ensure_started();
  ensure_graphics();
}

HDC GdiPlusBackend::hdc() const { return hdc_; }

GdiPlusBackend::~GdiPlusBackend() {
  release_style();
  delete static_cast<Gdiplus::Graphics*>(gfx_);
  gfx_ = nullptr;
}

void GdiPlusBackend::ensure_graphics() {
  if (gfx_ || !hdc_ || !gdiplus_available()) {
    return;
  }
  auto* g = new Gdiplus::Graphics(hdc_);
  g->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  g->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
  g->SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
  gfx_ = g;
}

void GdiPlusBackend::clear_rect(int x, int y, int w, int h, COLORREF color) {
  fill_rect(x, y, w, h, color);
}

void GdiPlusBackend::fill_rect(int x, int y, int w, int h, COLORREF color) {
  ensure_graphics();
  auto* g = static_cast<Gdiplus::Graphics*>(gfx_);
  if (!g || w < 1 || h < 1) {
    return;
  }
  Gdiplus::SolidBrush brush(colorref_to_gp(color));
  g->FillRectangle(&brush, x, y, w, h);
}

void GdiPlusBackend::stroke_rect(int x, int y, int w, int h, COLORREF color,
                                int pen_width) {
  ensure_graphics();
  auto* g = static_cast<Gdiplus::Graphics*>(gfx_);
  if (!g || w < 1 || h < 1) {
    return;
  }
  const int width = pen_width < 1 ? 1 : pen_width;
  Gdiplus::Pen pen(colorref_to_gp(color), static_cast<Gdiplus::REAL>(width));
  g->DrawRectangle(&pen, x, y, w, h);
}

void GdiPlusBackend::blit(HBITMAP src, int dest_x, int dest_y, int dest_w,
                         int dest_h, int src_x, int src_y, int src_w,
                         int src_h) {
  if (!hdc_ || !src || dest_w < 1 || dest_h < 1 || src_w < 1 || src_h < 1) {
    return;
  }
  // Keep BitBlt for HBITMAP present compatibility (same as GDI device).
  HDC src_dc = ::CreateCompatibleDC(hdc_);
  if (!src_dc) {
    return;
  }
  HGDIOBJ old = ::SelectObject(src_dc, src);
  if (dest_w == src_w && dest_h == src_h) {
    ::BitBlt(hdc_, dest_x, dest_y, dest_w, dest_h, src_dc, src_x, src_y,
             SRCCOPY);
  } else {
    ::StretchBlt(hdc_, dest_x, dest_y, dest_w, dest_h, src_dc, src_x, src_y,
                 src_w, src_h, SRCCOPY);
  }
  ::SelectObject(src_dc, old);
  ::DeleteDC(src_dc);
}

void GdiPlusBackend::set_pen(COLORREF color, int width, int style) {
  Rhi2dSetPenArgs args;
  args.color = color;
  args.width = width < 1 ? 1 : width;
  args.style = style;
  set_pen(args);
}

void GdiPlusBackend::set_brush(COLORREF color, int style, int hatch) {
  Rhi2dSetBrushArgs args;
  args.color = color;
  args.style = style;
  args.hatch = hatch;
  set_brush(args);
}

void GdiPlusBackend::set_pen(const Rhi2dSetPenArgs& args) {
  pen_ = args;
  if (pen_.width < 1) {
    pen_.width = 1;
  }
  pen_valid_ = true;
}

void GdiPlusBackend::set_brush(const Rhi2dSetBrushArgs& args) {
  brush_ = args;
  brush_valid_ = true;
  brush_null_ = (args.style == BS_NULL);
}

void GdiPlusBackend::release_style() {
  pen_valid_ = false;
  brush_valid_ = false;
  brush_null_ = false;
}

void GdiPlusBackend::polyline(const POINT* pts, int count) {
  ensure_graphics();
  auto* g = static_cast<Gdiplus::Graphics*>(gfx_);
  if (!g || !pts || count < 2 || !pen_valid_) {
    return;
  }
  Gdiplus::Pen pen(colorref_to_gp(pen_.color),
                   static_cast<Gdiplus::REAL>(pen_.width));
  pen.SetDashStyle(dash_from_pen_style(pen_.style));
  pen.SetLineJoin(Gdiplus::LineJoinRound);
  pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound,
                 Gdiplus::DashCapRound);
  std::vector<Gdiplus::Point> gp(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    gp[static_cast<size_t>(i)] = Gdiplus::Point(pts[i].x, pts[i].y);
  }
  g->DrawLines(&pen, gp.data(), count);
}

void GdiPlusBackend::road_polyline(const POINT* pts, int count, COLORREF casing,
                                  int casing_w, COLORREF fill, int fill_w) {
  if (!pts || count < 2) {
    return;
  }
  if (fill_w <= 1) {
    set_pen(fill, 1);
    polyline(pts, count);
    return;
  }
  set_pen(casing, casing_w < 1 ? 1 : casing_w);
  polyline(pts, count);
  set_pen(fill, fill_w < 1 ? 1 : fill_w);
  polyline(pts, count);
}

void GdiPlusBackend::road_poly_polyline(const POINT* pts,
                                       const int* poly_counts, int n_polys,
                                       COLORREF casing, int casing_w,
                                       COLORREF fill, int fill_w) {
  if (!pts || !poly_counts || n_polys < 1) {
    return;
  }
  if (fill_w <= 1) {
    set_pen(fill, 1);
    poly_polyline(pts, poly_counts, n_polys);
    return;
  }
  set_pen(casing, casing_w < 1 ? 1 : casing_w);
  poly_polyline(pts, poly_counts, n_polys);
  set_pen(fill, fill_w < 1 ? 1 : fill_w);
  poly_polyline(pts, poly_counts, n_polys);
}

void GdiPlusBackend::poly_polygon(const POINT* pts, const int* ring_counts,
                                 int n_rings) {
  ensure_graphics();
  auto* g = static_cast<Gdiplus::Graphics*>(gfx_);
  if (!g || !pts || !ring_counts || n_rings < 1) {
    return;
  }
  int total = 0;
  for (int i = 0; i < n_rings; ++i) {
    if (ring_counts[i] < 1) {
      return;
    }
    total += ring_counts[i];
  }
  std::vector<Gdiplus::Point> gp(static_cast<size_t>(total));
  for (int i = 0; i < total; ++i) {
    gp[static_cast<size_t>(i)] = Gdiplus::Point(pts[i].x, pts[i].y);
  }
  if (!brush_null_ && brush_valid_) {
    Gdiplus::SolidBrush brush(colorref_to_gp(brush_.color));
    g->FillPolygon(&brush, gp.data(), total);
  }
  if (pen_valid_) {
    Gdiplus::Pen pen(colorref_to_gp(pen_.color),
                     static_cast<Gdiplus::REAL>(pen_.width));
    pen.SetDashStyle(dash_from_pen_style(pen_.style));
    g->DrawPolygon(&pen, gp.data(), total);
  }
}

void GdiPlusBackend::poly_polyline(const POINT* pts, const int* poly_counts,
                                  int n_polys) {
  if (!pts || !poly_counts || n_polys < 1) {
    return;
  }
  int offset = 0;
  for (int i = 0; i < n_polys; ++i) {
    const int n = poly_counts[i];
    if (n < 2) {
      return;
    }
    polyline(pts + offset, n);
    offset += n;
  }
}

void GdiPlusBackend::ellipse(int left, int top, int right, int bottom) {
  ensure_graphics();
  auto* g = static_cast<Gdiplus::Graphics*>(gfx_);
  if (!g) {
    return;
  }
  const int w = right - left;
  const int h = bottom - top;
  if (!brush_null_ && brush_valid_) {
    Gdiplus::SolidBrush brush(colorref_to_gp(brush_.color));
    g->FillEllipse(&brush, left, top, w, h);
  }
  if (pen_valid_) {
    Gdiplus::Pen pen(colorref_to_gp(pen_.color),
                     static_cast<Gdiplus::REAL>(pen_.width));
    g->DrawEllipse(&pen, left, top, w, h);
  }
}

void GdiPlusBackend::draw_cross(long x, long y, long r) {
  POINT h[2] = {{x - r, y}, {x + r, y}};
  POINT v[2] = {{x, y - r}, {x, y + r}};
  if (!pen_valid_) {
    set_pen(RGB(0, 0, 0), 1);
  }
  polyline(h, 2);
  polyline(v, 2);
}

void GdiPlusBackend::draw_point_disc(long x, long y, int radius) {
  if (radius < 1) {
    radius = 1;
  }
  const int outer = radius + 1;
  set_brush(RGB(255, 255, 255));
  set_pen(RGB(255, 255, 255), 1);
  ellipse(static_cast<int>(x - outer), static_cast<int>(y - outer),
          static_cast<int>(x + outer + 1), static_cast<int>(y + outer + 1));
  set_brush(RGB(90, 110, 130));
  set_pen(RGB(70, 90, 110), 1);
  ellipse(static_cast<int>(x - radius), static_cast<int>(y - radius),
          static_cast<int>(x + radius + 1), static_cast<int>(y + radius + 1));
}

void GdiPlusBackend::draw_anno_text(long x, long y, const char* text, int px_h,
                                   int halo_px, float angle_deg) {
  if (!hdc_ || !text || !text[0]) {
    return;
  }
  const std::wstring w = gis::datasource::ogr_bytes_to_wide(text);
  if (w.empty()) {
    return;
  }
  if (px_h < 12) {
    px_h = 12;
  }
  if (halo_px < 1) {
    halo_px = 1;
  }
  const COLORREF ink = ::GetTextColor(hdc_);
  const COLORREF ink_use =
      (ink == 0 || ink == RGB(0, 0, 0)) ? RGB(28, 28, 28) : ink;
  GdiplusGraphics gfx(hdc_);
  if (gfx.ok()) {
    gfx.draw_string(static_cast<int>(x), static_cast<int>(y), w.c_str(), px_h,
                    ink_use, RGB(252, 252, 250), halo_px, angle_deg);
  }
}

void GdiPlusBackend::draw_text_bytes(int x, int y, const char* text, int n,
                                    int height_px, int halo_px,
                                    float angle_deg) {
  if (!hdc_ || !text || n < 1) {
    return;
  }
  std::string bytes(text, static_cast<size_t>(n));
  draw_anno_text(x, y, bytes.c_str(), height_px, halo_px, angle_deg);
}

}  // namespace detail
}  // namespace render
