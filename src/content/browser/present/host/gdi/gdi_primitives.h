// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_HOST_GDI_GDI_PRIMITIVES_H_
#define CONTENT_BROWSER_PRESENT_HOST_GDI_GDI_PRIMITIVES_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace content {
namespace detail {

// Owns an HPEN, selects it on construct, restores and deletes on destroy.
class ScopedGdiPen {
 public:
  ScopedGdiPen(HDC hdc, COLORREF color, int width = 1, int style = PS_SOLID);
  // PS_NULL pen (mesh fill with no outline).
  static ScopedGdiPen null_pen(HDC hdc);
  ~ScopedGdiPen();

  ScopedGdiPen(const ScopedGdiPen&) = delete;
  ScopedGdiPen& operator=(const ScopedGdiPen&) = delete;
  ScopedGdiPen(ScopedGdiPen&& other) noexcept;
  ScopedGdiPen& operator=(ScopedGdiPen&& other) noexcept;

  explicit operator bool() const { return pen_ != nullptr; }
  HPEN get() const { return pen_; }

 private:
  ScopedGdiPen() = default;
  void release();

  HDC hdc_ = nullptr;
  HPEN pen_ = nullptr;
  HGDIOBJ old_ = nullptr;
};

// Owns an HBRUSH, selects it on construct, restores and deletes on destroy.
class ScopedGdiBrush {
 public:
  explicit ScopedGdiBrush(HDC hdc, COLORREF color);
  ~ScopedGdiBrush();

  ScopedGdiBrush(const ScopedGdiBrush&) = delete;
  ScopedGdiBrush& operator=(const ScopedGdiBrush&) = delete;
  ScopedGdiBrush(ScopedGdiBrush&& other) noexcept;
  ScopedGdiBrush& operator=(ScopedGdiBrush&& other) noexcept;

  explicit operator bool() const { return brush_ != nullptr; }
  HBRUSH get() const { return brush_; }

 private:
  void release();

  HDC hdc_ = nullptr;
  HBRUSH brush_ = nullptr;
  HGDIOBJ old_ = nullptr;
};

// Selects an existing GDI object without taking ownership.
class ScopedGdiSelect {
 public:
  ScopedGdiSelect(HDC hdc, HGDIOBJ obj);
  ~ScopedGdiSelect();

  ScopedGdiSelect(const ScopedGdiSelect&) = delete;
  ScopedGdiSelect& operator=(const ScopedGdiSelect&) = delete;

 private:
  HDC hdc_ = nullptr;
  HGDIOBJ old_ = nullptr;
};

void gdi_fill_rect(HDC hdc, int x, int y, int w, int h, COLORREF color);
void gdi_fill_rect(HDC hdc, const RECT& rc, COLORREF color);

// Fill using the currently selected brush (and pen for outline).
void gdi_fill_polygon(HDC hdc, const POINT* pts, int count);
// One-shot solid fill + optional stroke (stroke_w < 1 → NULL_PEN).
void gdi_fill_polygon(HDC hdc, const POINT* pts, int count, COLORREF fill,
                      COLORREF stroke, int stroke_w = 1);

void gdi_stroke_line(HDC hdc, int x0, int y0, int x1, int y1);
// Open chain: pts[0]→pts[1]→…→pts[count-1].
void gdi_stroke_polyline(HDC hdc, const POINT* pts, int count);
// Closed outline: polyline then back to pts[0].
void gdi_stroke_closed(HDC hdc, const POINT* pts, int count);

void gdi_draw_ellipse(HDC hdc, int left, int top, int right, int bottom);
void gdi_draw_disc(HDC hdc, int cx, int cy, int radius);
void gdi_draw_ellipse_disc(HDC hdc, int cx, int cy, int rx, int ry);

// Transparent background. Does not change the selected font.
void gdi_draw_text_w(HDC hdc, int x, int y, const wchar_t* text, int len,
                     COLORREF color);
// Halo then ink (scenic GdiBackend anno fallback shape).
void gdi_draw_halo_text_w(HDC hdc, int x, int y, const wchar_t* text, int len,
                          COLORREF ink, COLORREF halo, int halo_px);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_HOST_GDI_GDI_PRIMITIVES_H_
