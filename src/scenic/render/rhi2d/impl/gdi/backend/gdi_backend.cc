// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/gdi/backend/gdi_backend.h"

#include <cstddef>
#include <string>
#include <vector>

#include "gis/datasource/ogr/ogr_text_encoding.h"
#include "scenic/render/rhi2d/impl/gdiplus/aa/gdiplus.h"

namespace scenic {
namespace detail {
namespace {

// Resolve stroke args for GDI+ AA. Prefer backend set_pen cache; otherwise read
// the HDC's current pen (ExtCreatePen / CreatePen from carto style).
bool resolve_stroke_pen(HDC hdc, bool pen_args_valid, COLORREF cached_color,
                        int cached_width, int cached_style, COLORREF* color,
                        int* width, int* style) {
  if (pen_args_valid) {
    *color = cached_color;
    *width = cached_width < 1 ? 1 : cached_width;
    *style = cached_style;
    return true;
  }
  if (!hdc) {
    return false;
  }
  HGDIOBJ obj = ::GetCurrentObject(hdc, OBJ_PEN);
  if (!obj || obj == ::GetStockObject(NULL_PEN)) {
    return false;
  }
  const int need = ::GetObject(obj, 0, nullptr);
  if (need >= static_cast<int>(offsetof(EXTLOGPEN, elpStyleEntry))) {
    std::vector<BYTE> buf(static_cast<size_t>(need));
    if (::GetObject(obj, need, buf.data()) > 0) {
      const auto* elp = reinterpret_cast<const EXTLOGPEN*>(buf.data());
      *color = elp->elpColor;
      *width = static_cast<int>(elp->elpWidth);
      if (*width < 1) {
        *width = 1;
      }
      *style = static_cast<int>(elp->elpPenStyle & PS_STYLE_MASK);
      return true;
    }
  }
  LOGPEN lp = {};
  if (::GetObject(obj, sizeof(lp), &lp) == sizeof(lp)) {
    *color = lp.lopnColor;
    *width = lp.lopnWidth.x;
    if (*width < 1) {
      *width = 1;
    }
    *style = lp.lopnStyle & PS_STYLE_MASK;
    return true;
  }
  return false;
}

}  // namespace

GdiBackend::GdiBackend(HDC hdc) : hdc_(hdc) {}

GdiBackend::~GdiBackend() { release_style(); }

HDC GdiBackend::hdc() const { return hdc_; }

void GdiBackend::clear_rect(int x, int y, int w, int h, COLORREF color) {
  fill_rect(x, y, w, h, color);
}

void GdiBackend::fill_rect(int x, int y, int w, int h, COLORREF color) {
  if (!hdc_ || w < 1 || h < 1) {
    return;
  }
  RECT rc;
  rc.left = x;
  rc.top = y;
  rc.right = x + w;
  rc.bottom = y + h;
  HBRUSH brush = ::CreateSolidBrush(color);
  if (!brush) {
    return;
  }
  ::FillRect(hdc_, &rc, brush);
  ::DeleteObject(brush);
}

void GdiBackend::stroke_rect(int x, int y, int w, int h, COLORREF color,
                            int pen_width) {
  if (!hdc_ || w < 1 || h < 1) {
    return;
  }
  const int width = pen_width < 1 ? 1 : pen_width;
  HPEN pen = ::CreatePen(PS_SOLID, width, color);
  if (!pen) {
    return;
  }
  HGDIOBJ old_pen = ::SelectObject(hdc_, pen);
  HGDIOBJ old_brush = ::SelectObject(hdc_, ::GetStockObject(NULL_BRUSH));
  ::Rectangle(hdc_, x, y, x + w, y + h);
  ::SelectObject(hdc_, old_brush);
  ::SelectObject(hdc_, old_pen);
  ::DeleteObject(pen);
}

void GdiBackend::blit(HBITMAP src, int dest_x, int dest_y, int dest_w,
                     int dest_h, int src_x, int src_y, int src_w, int src_h) {
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

void GdiBackend::set_pen(COLORREF color, int width, int style) {
  Rhi2dSetPenArgs args;
  args.color = color;
  args.width = width < 1 ? 1 : width;
  args.style = style;
  set_pen(args);
}

void GdiBackend::set_brush(COLORREF color, int style, int hatch) {
  Rhi2dSetBrushArgs args;
  args.color = color;
  args.style = style;
  args.hatch = hatch;
  set_brush(args);
}

void GdiBackend::set_pen(const Rhi2dSetPenArgs& args) {
  if (!hdc_) {
    return;
  }
  const int width = args.width < 1 ? 1 : args.width;
  // Same pen already selected — skip CreatePen/DeleteObject churn.
  if (pen_args_valid_ && pen_ && pen_color_ == args.color &&
      pen_width_ == width && pen_style_ == args.style) {
    return;
  }
  if (pen_) {
    ::SelectObject(hdc_, old_pen_ ? old_pen_ : ::GetStockObject(BLACK_PEN));
    ::DeleteObject(pen_);
    pen_ = nullptr;
    old_pen_ = nullptr;
  }
  pen_ = ::CreatePen(args.style, width, args.color);
  if (pen_) {
    old_pen_ = ::SelectObject(hdc_, pen_);
    pen_args_valid_ = true;
    pen_color_ = args.color;
    pen_width_ = width;
    pen_style_ = args.style;
  } else {
    pen_args_valid_ = false;
  }
}

void GdiBackend::set_brush(const Rhi2dSetBrushArgs& args) {
  if (!hdc_) {
    return;
  }
  if (brush_) {
    ::SelectObject(hdc_,
                   old_brush_ ? old_brush_ : ::GetStockObject(NULL_BRUSH));
    ::DeleteObject(brush_);
    brush_ = nullptr;
    old_brush_ = nullptr;
  }
  if (args.style == BS_NULL) {
    old_brush_ = ::SelectObject(hdc_, ::GetStockObject(NULL_BRUSH));
    return;
  }
  if (args.style == BS_HATCHED) {
    brush_ = ::CreateHatchBrush(args.hatch, args.color);
  } else {
    brush_ = ::CreateSolidBrush(args.color);
  }
  if (brush_) {
    old_brush_ = ::SelectObject(hdc_, brush_);
  }
}

void GdiBackend::release_style() {
  pen_args_valid_ = false;
  if (!hdc_) {
    pen_ = nullptr;
    brush_ = nullptr;
    old_pen_ = nullptr;
    old_brush_ = nullptr;
    return;
  }
  if (pen_) {
    ::SelectObject(hdc_, old_pen_ ? old_pen_ : ::GetStockObject(BLACK_PEN));
    ::DeleteObject(pen_);
    pen_ = nullptr;
    old_pen_ = nullptr;
  }
  if (brush_) {
    ::SelectObject(hdc_,
                   old_brush_ ? old_brush_ : ::GetStockObject(NULL_BRUSH));
    ::DeleteObject(brush_);
    brush_ = nullptr;
    old_brush_ = nullptr;
  }
}

void GdiBackend::polyline(const POINT* pts, int count) {
  if (!hdc_ || !pts || count < 2) {
    return;
  }
  // Prefer GDI+ AA strokes so leftover GDI port matches GDI+/Skia line quality.
  COLORREF color = 0;
  int width = 1;
  int style = PS_SOLID;
  if (resolve_stroke_pen(hdc_, pen_args_valid_, pen_color_, pen_width_,
                         pen_style_, &color, &width, &style)) {
    GdiplusGraphics gfx(hdc_);
    if (gfx.ok() && gfx.draw_polyline(pts, count, color, width, style)) {
      return;
    }
  }
  ::Polyline(hdc_, pts, count);
}

void GdiBackend::road_polyline(const POINT* pts, int count, COLORREF casing,
                              int casing_w, COLORREF fill, int fill_w) {
  if (!hdc_ || !pts || count < 2) {
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

void GdiBackend::road_poly_polyline(const POINT* pts, const int* poly_counts,
                                   int n_polys, COLORREF casing, int casing_w,
                                   COLORREF fill, int fill_w) {
  if (!hdc_ || !pts || !poly_counts || n_polys < 1) {
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

void GdiBackend::poly_polygon(const POINT* pts, const int* ring_counts,
                             int n_rings) {
  if (!hdc_ || !pts || !ring_counts || n_rings < 1) {
    return;
  }
  ::PolyPolygon(hdc_, const_cast<POINT*>(pts), const_cast<int*>(ring_counts),
                n_rings);
}

void GdiBackend::poly_polyline(const POINT* pts, const int* poly_counts,
                              int n_polys) {
  if (!hdc_ || !pts || !poly_counts || n_polys < 1) {
    return;
  }
  COLORREF color = 0;
  int width = 1;
  int style = PS_SOLID;
  if (resolve_stroke_pen(hdc_, pen_args_valid_, pen_color_, pen_width_,
                         pen_style_, &color, &width, &style)) {
    GdiplusGraphics gfx(hdc_);
    if (gfx.ok()) {
      int offset = 0;
      bool drew_any = false;
      bool all_ok = true;
      for (int i = 0; i < n_polys; ++i) {
        const int n = poly_counts[i];
        if (n < 2) {
          all_ok = false;
          break;
        }
        if (!gfx.draw_polyline(pts + offset, n, color, width, style)) {
          all_ok = false;
          break;
        }
        drew_any = true;
        offset += n;
      }
      if (all_ok) {
        return;
      }
      // Partial AA already on the DC — do not double-draw via Win32.
      if (drew_any) {
        return;
      }
    }
  }
  // Win32 PolyPolyline wants DWORD counts.
  thread_local std::vector<DWORD> counts;
  counts.resize(static_cast<size_t>(n_polys));
  for (int i = 0; i < n_polys; ++i) {
    if (poly_counts[i] < 2) {
      return;
    }
    counts[static_cast<size_t>(i)] = static_cast<DWORD>(poly_counts[i]);
  }
  ::PolyPolyline(hdc_, pts, counts.data(), static_cast<DWORD>(n_polys));
}

void GdiBackend::ellipse(int left, int top, int right, int bottom) {
  if (!hdc_) {
    return;
  }
  ::Ellipse(hdc_, left, top, right, bottom);
}

void GdiBackend::draw_cross(long x, long y, long r) {
  if (!hdc_) {
    return;
  }
  ::MoveToEx(hdc_, x - r, y, nullptr);
  ::LineTo(hdc_, x + r, y);
  ::MoveToEx(hdc_, x, y - r, nullptr);
  ::LineTo(hdc_, x, y + r);
}

void GdiBackend::draw_point_disc(long x, long y, int radius) {
  if (!hdc_) {
    return;
  }
  if (radius < 1) {
    radius = 1;
  }
  const int outer = radius + 1;
  HBRUSH ring = ::CreateSolidBrush(RGB(255, 255, 255));
  HPEN ring_pen = ::CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
  HGDIOBJ old_b = ::SelectObject(hdc_, ring);
  HGDIOBJ old_p = ::SelectObject(hdc_, ring_pen);
  ::Ellipse(hdc_, x - outer, y - outer, x + outer + 1, y + outer + 1);
  HBRUSH fill = ::CreateSolidBrush(RGB(90, 110, 130));
  HPEN fill_pen = ::CreatePen(PS_SOLID, 1, RGB(70, 90, 110));
  ::SelectObject(hdc_, fill);
  ::SelectObject(hdc_, fill_pen);
  ::Ellipse(hdc_, x - radius, y - radius, x + radius + 1, y + radius + 1);
  ::SelectObject(hdc_, old_b);
  ::SelectObject(hdc_, old_p);
  ::DeleteObject(fill);
  ::DeleteObject(fill_pen);
  ::DeleteObject(ring);
  ::DeleteObject(ring_pen);
}

void GdiBackend::draw_anno_text(long x, long y, const char* text, int px_h,
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
  {
    GdiplusGraphics gfx(hdc_);
    if (gfx.ok() &&
        gfx.draw_string(static_cast<int>(x), static_cast<int>(y), w.c_str(),
                        px_h, ink_use, RGB(252, 252, 250), halo_px,
                        angle_deg)) {
      return;
    }
  }
  HFONT font = ::CreateFontW(-px_h, 0, 0, 0, px_h >= 16 ? FW_SEMIBOLD : FW_NORMAL,
                             FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                             L"Microsoft YaHei");
  HGDIOBJ old = font ? ::SelectObject(hdc_, font) : nullptr;
  ::SetBkMode(hdc_, TRANSPARENT);
  const int n = static_cast<int>(w.size());
  ::SetTextColor(hdc_, RGB(252, 252, 250));
  for (int dy = -halo_px; dy <= halo_px; ++dy) {
    for (int dx = -halo_px; dx <= halo_px; ++dx) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      ::TextOutW(hdc_, x + dx, y + dy, w.c_str(), n);
    }
  }
  ::SetTextColor(hdc_, ink_use);
  ::TextOutW(hdc_, x, y, w.c_str(), n);
  if (font) {
    ::SelectObject(hdc_, old);
    ::DeleteObject(font);
  }
}

void GdiBackend::draw_text_bytes(int x, int y, const char* text, int n,
                                int height_px, int halo_px, float angle_deg) {
  if (!hdc_ || !text || n < 1) {
    return;
  }
  // Encoder blobs are not NUL-terminated; same decode path as draw_anno_text
  // (UTF-8 / GBK via ogr_bytes_to_wide). TextOutA here caused mojibake.
  std::string bytes(text, static_cast<size_t>(n));
  draw_anno_text(x, y, bytes.c_str(), height_px, halo_px, angle_deg);
}

}  // namespace detail
}  // namespace scenic
