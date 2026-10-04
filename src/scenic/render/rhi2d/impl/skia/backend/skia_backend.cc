// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/skia/backend/skia_backend.h"

#include "scenic/render/rhi2d/impl/gdiplus/backend/gdiplus_backend.h"

#if defined(RHI2D_HAS_SKIA)
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkSurface.h"
#include "include/core/SkColor.h"
#endif

namespace scenic {
namespace detail {

struct SkiaBackend::Impl {
#if defined(RHI2D_HAS_SKIA)
  sk_sp<SkSurface> surface;
  SkCanvas* canvas = nullptr;
  SkPaint stroke;
  SkPaint fill;
  bool brush_null = false;
#endif
  // Used when Skia pin is off, or HDC has no DIBSECTION to wrap.
  std::unique_ptr<GdiPlusBackend> fallback;
};

#if defined(RHI2D_HAS_SKIA)
namespace {

SkColor colorref_to_sk(COLORREF c) {
  return SkColorSetRGB(GetRValue(c), GetGValue(c), GetBValue(c));
}

bool bind_canvas_from_hdc(HDC hdc, sk_sp<SkSurface>* out_surface,
                          SkCanvas** out_canvas) {
  if (!hdc || !out_surface || !out_canvas) {
    return false;
  }
  HBITMAP hbmp =
      static_cast<HBITMAP>(::GetCurrentObject(hdc, OBJ_BITMAP));
  if (!hbmp) {
    return false;
  }
  DIBSECTION dib = {};
  if (::GetObject(hbmp, sizeof(dib), &dib) < sizeof(DIBSECTION) ||
      !dib.dsBm.bmBits || dib.dsBm.bmWidth < 1 || dib.dsBm.bmHeight < 1) {
    return false;
  }
  const int w = dib.dsBm.bmWidth;
  const int h = dib.dsBm.bmHeight;
  // DIBSection map buffers are bottom-up BGRA in this tree.
  SkImageInfo info =
      SkImageInfo::Make(w, h, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  SkBitmap bitmap;
  if (!bitmap.installPixels(info, dib.dsBm.bmBits, dib.dsBm.bmWidthBytes)) {
    return false;
  }
  *out_surface = SkSurfaces::WrapPixels(info, dib.dsBm.bmBits,
                                        dib.dsBm.bmWidthBytes);
  if (!*out_surface) {
    return false;
  }
  *out_canvas = (*out_surface)->getCanvas();
  if (*out_canvas) {
    // Win32 DIB is bottom-up; flip Skia to match GDI y-down.
    (*out_canvas)->scale(1.f, -1.f);
    (*out_canvas)->translate(0.f, -static_cast<float>(h));
  }
  return *out_canvas != nullptr;
}

}  // namespace
#endif

SkiaBackend::SkiaBackend(HDC hdc) : hdc_(hdc), impl_(std::make_unique<Impl>()) {
#if defined(RHI2D_HAS_SKIA)
  if (!bind_canvas_from_hdc(hdc_, &impl_->surface, &impl_->canvas)) {
    impl_->fallback = std::make_unique<GdiPlusBackend>(hdc_);
  } else {
    impl_->stroke.setAntiAlias(true);
    impl_->stroke.setStyle(SkPaint::kStroke_Style);
    impl_->stroke.setStrokeCap(SkPaint::kRound_Cap);
    impl_->stroke.setStrokeJoin(SkPaint::kRound_Join);
    impl_->fill.setAntiAlias(true);
    impl_->fill.setStyle(SkPaint::kFill_Style);
  }
#else
  impl_->fallback = std::make_unique<GdiPlusBackend>(hdc_);
#endif
}

HDC SkiaBackend::hdc() const { return hdc_; }

SkiaBackend::~SkiaBackend() = default;

void SkiaBackend::clear_rect(int x, int y, int w, int h, COLORREF color) {
  fill_rect(x, y, w, h, color);
}

void SkiaBackend::fill_rect(int x, int y, int w, int h, COLORREF color) {
#if defined(RHI2D_HAS_SKIA)
  if (impl_->fallback) {
    impl_->fallback->fill_rect(x, y, w, h, color);
    return;
  }
  if (!impl_->canvas || w < 1 || h < 1) {
    return;
  }
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor(colorref_to_sk(color));
  impl_->canvas->drawRect(
      SkRect::MakeXYWH(static_cast<SkScalar>(x), static_cast<SkScalar>(y),
                       static_cast<SkScalar>(w), static_cast<SkScalar>(h)),
      p);
#else
  impl_->fallback->fill_rect(x, y, w, h, color);
#endif
}

void SkiaBackend::stroke_rect(int x, int y, int w, int h, COLORREF color,
                             int pen_width) {
#if defined(RHI2D_HAS_SKIA)
  if (impl_->fallback) {
    impl_->fallback->stroke_rect(x, y, w, h, color, pen_width);
    return;
  }
  if (!impl_->canvas || w < 1 || h < 1) {
    return;
  }
  SkPaint p = impl_->stroke;
  p.setColor(colorref_to_sk(color));
  p.setStrokeWidth(static_cast<SkScalar>(pen_width < 1 ? 1 : pen_width));
  impl_->canvas->drawRect(
      SkRect::MakeXYWH(static_cast<SkScalar>(x), static_cast<SkScalar>(y),
                       static_cast<SkScalar>(w), static_cast<SkScalar>(h)),
      p);
#else
  impl_->fallback->stroke_rect(x, y, w, h, color, pen_width);
#endif
}

void SkiaBackend::blit(HBITMAP src, int dest_x, int dest_y, int dest_w,
                      int dest_h, int src_x, int src_y, int src_w, int src_h) {
  // Present-compatible blit stays on GDI regardless of Skia pin.
  if (!hdc_ || !src || dest_w < 1 || dest_h < 1 || src_w < 1 || src_h < 1) {
    return;
  }
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

void SkiaBackend::set_pen(COLORREF color, int width, int style) {
  Rhi2dSetPenArgs args;
  args.color = color;
  args.width = width < 1 ? 1 : width;
  args.style = style;
  set_pen(args);
}

void SkiaBackend::set_brush(COLORREF color, int style, int hatch) {
  Rhi2dSetBrushArgs args;
  args.color = color;
  args.style = style;
  args.hatch = hatch;
  set_brush(args);
}

void SkiaBackend::set_pen(const Rhi2dSetPenArgs& args) {
#if defined(RHI2D_HAS_SKIA)
  if (impl_->fallback) {
    impl_->fallback->set_pen(args);
    return;
  }
  impl_->stroke.setColor(colorref_to_sk(args.color));
  impl_->stroke.setStrokeWidth(
      static_cast<SkScalar>(args.width < 1 ? 1 : args.width));
#else
  impl_->fallback->set_pen(args);
#endif
}

void SkiaBackend::set_brush(const Rhi2dSetBrushArgs& args) {
#if defined(RHI2D_HAS_SKIA)
  if (impl_->fallback) {
    impl_->fallback->set_brush(args);
    return;
  }
  impl_->brush_null = (args.style == BS_NULL);
  impl_->fill.setColor(colorref_to_sk(args.color));
#else
  impl_->fallback->set_brush(args);
#endif
}

void SkiaBackend::release_style() {
#if defined(RHI2D_HAS_SKIA)
  if (impl_->fallback) {
    impl_->fallback->release_style();
  }
#else
  impl_->fallback->release_style();
#endif
}

void SkiaBackend::polyline(const POINT* pts, int count) {
#if defined(RHI2D_HAS_SKIA)
  if (impl_->fallback) {
    impl_->fallback->polyline(pts, count);
    return;
  }
  if (!impl_->canvas || !pts || count < 2) {
    return;
  }
  SkPath path;
  path.moveTo(static_cast<SkScalar>(pts[0].x),
              static_cast<SkScalar>(pts[0].y));
  for (int i = 1; i < count; ++i) {
    path.lineTo(static_cast<SkScalar>(pts[i].x),
                static_cast<SkScalar>(pts[i].y));
  }
  impl_->canvas->drawPath(path, impl_->stroke);
#else
  impl_->fallback->polyline(pts, count);
#endif
}

void SkiaBackend::road_polyline(const POINT* pts, int count, COLORREF casing,
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

void SkiaBackend::road_poly_polyline(const POINT* pts, const int* poly_counts,
                                    int n_polys, COLORREF casing, int casing_w,
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

void SkiaBackend::poly_polygon(const POINT* pts, const int* ring_counts,
                              int n_rings) {
#if defined(RHI2D_HAS_SKIA)
  if (impl_->fallback) {
    impl_->fallback->poly_polygon(pts, ring_counts, n_rings);
    return;
  }
  if (!impl_->canvas || !pts || !ring_counts || n_rings < 1) {
    return;
  }
  SkPath path;
  int offset = 0;
  for (int r = 0; r < n_rings; ++r) {
    const int n = ring_counts[r];
    if (n < 1) {
      return;
    }
    path.moveTo(static_cast<SkScalar>(pts[offset].x),
                static_cast<SkScalar>(pts[offset].y));
    for (int i = 1; i < n; ++i) {
      path.lineTo(static_cast<SkScalar>(pts[offset + i].x),
                  static_cast<SkScalar>(pts[offset + i].y));
    }
    path.close();
    offset += n;
  }
  if (!impl_->brush_null) {
    impl_->canvas->drawPath(path, impl_->fill);
  }
  impl_->canvas->drawPath(path, impl_->stroke);
#else
  impl_->fallback->poly_polygon(pts, ring_counts, n_rings);
#endif
}

void SkiaBackend::poly_polyline(const POINT* pts, const int* poly_counts,
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

void SkiaBackend::ellipse(int left, int top, int right, int bottom) {
#if defined(RHI2D_HAS_SKIA)
  if (impl_->fallback) {
    impl_->fallback->ellipse(left, top, right, bottom);
    return;
  }
  if (!impl_->canvas) {
    return;
  }
  SkRect rect = SkRect::MakeLTRB(static_cast<SkScalar>(left),
                                 static_cast<SkScalar>(top),
                                 static_cast<SkScalar>(right),
                                 static_cast<SkScalar>(bottom));
  if (!impl_->brush_null) {
    impl_->canvas->drawOval(rect, impl_->fill);
  }
  impl_->canvas->drawOval(rect, impl_->stroke);
#else
  impl_->fallback->ellipse(left, top, right, bottom);
#endif
}

void SkiaBackend::draw_cross(long x, long y, long r) {
#if defined(RHI2D_HAS_SKIA)
  if (impl_->fallback) {
    impl_->fallback->draw_cross(x, y, r);
    return;
  }
  POINT h[2] = {{x - r, y}, {x + r, y}};
  POINT v[2] = {{x, y - r}, {x, y + r}};
  polyline(h, 2);
  polyline(v, 2);
#else
  impl_->fallback->draw_cross(x, y, r);
#endif
}

void SkiaBackend::draw_point_disc(long x, long y, int radius) {
#if defined(RHI2D_HAS_SKIA)
  if (impl_->fallback) {
    impl_->fallback->draw_point_disc(x, y, radius);
    return;
  }
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
#else
  impl_->fallback->draw_point_disc(x, y, radius);
#endif
}

void SkiaBackend::draw_anno_text(long x, long y, const char* text, int px_h,
                                int halo_px, float angle_deg) {
  // Text stays on GDI+ AA helper (same as GDI device) until SkFont lands.
#if defined(RHI2D_HAS_SKIA)
  if (impl_->fallback) {
    impl_->fallback->draw_anno_text(x, y, text, px_h, halo_px, angle_deg);
    return;
  }
#endif
  GdiPlusBackend text_lane(hdc_);
  text_lane.draw_anno_text(x, y, text, px_h, halo_px, angle_deg);
}

void SkiaBackend::draw_text_bytes(int x, int y, const char* text, int n,
                                 int height_px, int halo_px, float angle_deg) {
#if defined(RHI2D_HAS_SKIA)
  if (impl_->fallback) {
    impl_->fallback->draw_text_bytes(x, y, text, n, height_px, halo_px,
                                     angle_deg);
    return;
  }
#endif
  GdiPlusBackend text_lane(hdc_);
  text_lane.draw_text_bytes(x, y, text, n, height_px, halo_px, angle_deg);
}

}  // namespace detail
}  // namespace scenic
