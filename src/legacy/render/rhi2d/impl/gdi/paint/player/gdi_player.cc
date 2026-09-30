// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/gdi/paint/player/gdi_player.h"

#include <string>
#include <vector>

#include "gis/datasource/provider/impl/ogr/text/ogr_text_encoding.h"
#include "legacy/render/rhi2d/impl/gdi/paint/gdiplus/gdiplus.h"

namespace render {
namespace detail {

GdiPlayer::GdiPlayer(HDC hdc) : hdc_(hdc) {}

GdiPlayer::~GdiPlayer() { release_style(); }

void GdiPlayer::clear_rect(int x, int y, int w, int h, COLORREF color) {
  fill_rect(x, y, w, h, color);
}

void GdiPlayer::fill_rect(int x, int y, int w, int h, COLORREF color) {
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

void GdiPlayer::stroke_rect(int x, int y, int w, int h, COLORREF color,
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

void GdiPlayer::blit(HBITMAP src, int dest_x, int dest_y, int dest_w,
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

void GdiPlayer::set_pen(COLORREF color, int width, int style) {
  GdiSetPenArgs args;
  args.color = color;
  args.width = width < 1 ? 1 : width;
  args.style = style;
  set_pen(args);
}

void GdiPlayer::set_brush(COLORREF color, int style, int hatch) {
  GdiSetBrushArgs args;
  args.color = color;
  args.style = style;
  args.hatch = hatch;
  set_brush(args);
}

void GdiPlayer::set_pen(const GdiSetPenArgs& args) {
  if (!hdc_) {
    return;
  }
  if (pen_) {
    ::SelectObject(hdc_, old_pen_ ? old_pen_ : ::GetStockObject(BLACK_PEN));
    ::DeleteObject(pen_);
    pen_ = nullptr;
    old_pen_ = nullptr;
  }
  const int width = args.width < 1 ? 1 : args.width;
  pen_ = ::CreatePen(args.style, width, args.color);
  if (pen_) {
    old_pen_ = ::SelectObject(hdc_, pen_);
  }
}

void GdiPlayer::set_brush(const GdiSetBrushArgs& args) {
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

void GdiPlayer::release_style() {
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

void GdiPlayer::polyline(const POINT* pts, int count) {
  if (!hdc_ || !pts || count < 2) {
    return;
  }
  ::MoveToEx(hdc_, pts[0].x, pts[0].y, nullptr);
  ::PolylineTo(hdc_, const_cast<POINT*>(pts), count);
}

void GdiPlayer::road_polyline(const POINT* pts, int count, COLORREF casing,
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

void GdiPlayer::poly_polygon(const POINT* pts, const int* ring_counts,
                             int n_rings) {
  if (!hdc_ || !pts || !ring_counts || n_rings < 1) {
    return;
  }
  ::PolyPolygon(hdc_, const_cast<POINT*>(pts), const_cast<int*>(ring_counts),
                n_rings);
}

void GdiPlayer::poly_polyline(const POINT* pts, const int* poly_counts,
                              int n_polys) {
  if (!hdc_ || !pts || !poly_counts || n_polys < 1) {
    return;
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

void GdiPlayer::ellipse(int left, int top, int right, int bottom) {
  if (!hdc_) {
    return;
  }
  ::Ellipse(hdc_, left, top, right, bottom);
}

void GdiPlayer::draw_cross(long x, long y, long r) {
  if (!hdc_) {
    return;
  }
  ::MoveToEx(hdc_, x - r, y, nullptr);
  ::LineTo(hdc_, x + r, y);
  ::MoveToEx(hdc_, x, y - r, nullptr);
  ::LineTo(hdc_, x, y + r);
}

void GdiPlayer::draw_point_disc(long x, long y, int radius) {
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

void GdiPlayer::draw_anno_text(long x, long y, const char* text, int px_h,
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

void GdiPlayer::draw_text_bytes(int x, int y, const char* text, int n,
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
}  // namespace render
