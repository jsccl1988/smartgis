#include "legacy/render/gdi/gdi_aux_api.h"

#include "gis/datasource/gdal/ogr_text_encoding.h"
#include "legacy/render/gdi/gdi_gdiplus.h"

#include <string>

using render::GdiplusGraphics;

void clear_rect(HDC hDC, int x, int y, int w, int h, COLORREF clr) {
  // GDI FillRect requires top < bottom in MM_TEXT.
  RECT rect;
  rect.left = x;
  rect.top = y;
  rect.right = x + w;
  rect.bottom = y + h;

  HBRUSH hBrush = CreateSolidBrush(clr);
  HBRUSH hOldBrush = (HBRUSH)::SelectObject(hDC, hBrush);
  ::FillRect(hDC, &rect, hBrush);
  ::SelectObject(hDC, hOldBrush);
  ::DeleteObject(hBrush);
}

void draw_rect(HDC hDC, RECT& rect, BOOL exclusive) {
  MoveToEx(hDC, rect.left, rect.top, NULL);
  LineTo(hDC, rect.right, rect.top);
  LineTo(hDC, rect.right, rect.bottom);
  LineTo(hDC, rect.left, rect.bottom);
  LineTo(hDC, rect.left, rect.top);
}

void draw_rect(HDC hDC, lRect& lrect, BOOL exclusive) {
  MoveToEx(hDC, lrect.lb.x, lrect.lb.y, NULL);
  LineTo(hDC, lrect.rt.x, lrect.lb.y);
  LineTo(hDC, lrect.rt.x, lrect.rt.y);
  LineTo(hDC, lrect.lb.x, lrect.rt.y);
  LineTo(hDC, lrect.lb.x, lrect.lb.y);
}

void draw_line(HDC hDC, base::lPoint* plPoints, int nCount, BOOL exclusive) {
  if (plPoints == NULL) {
    return;
  }

  MoveToEx(hDC, plPoints[0].x, plPoints[0].y, NULL);
  PolylineTo(hDC, (POINT*)plPoints, nCount);
}

void draw_line(HDC hDC, POINT* pPoints, int nCount, BOOL exclusive) {
  if (pPoints == NULL) {
    return;
  }

  MoveToEx(hDC, pPoints[0].x, pPoints[0].y, NULL);
  PolylineTo(hDC, pPoints, nCount);
}

void draw_cross(HDC hDC, long lX, long lY, long r, BOOL exclusive) {
  MoveToEx(hDC, lX - r, lY, NULL);
  LineTo(hDC, lX + r, lY);

  MoveToEx(hDC, lX, lY, NULL);
  LineTo(hDC, lX, lY);
}

void draw_point_disc(HDC hdc, long x, long y, int radius) {
  if (!hdc) {
    return;
  }
  if (radius < 1) {
    radius = 1;
  }
  const int outer = radius + 1;
  HBRUSH ring = CreateSolidBrush(RGB(255, 255, 255));
  HPEN ring_pen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
  HGDIOBJ old_b = SelectObject(hdc, ring);
  HGDIOBJ old_p = SelectObject(hdc, ring_pen);
  Ellipse(hdc, x - outer, y - outer, x + outer + 1, y + outer + 1);
  // Soft Baidu-like POI fill (not solid black).
  HBRUSH fill = CreateSolidBrush(RGB(90, 110, 130));
  HPEN fill_pen = CreatePen(PS_SOLID, 1, RGB(70, 90, 110));
  SelectObject(hdc, fill);
  SelectObject(hdc, fill_pen);
  Ellipse(hdc, x - radius, y - radius, x + radius + 1, y + radius + 1);
  SelectObject(hdc, old_b);
  SelectObject(hdc, old_p);
  DeleteObject(fill);
  DeleteObject(fill_pen);
  DeleteObject(ring);
  DeleteObject(ring_pen);
}

void draw_anno_text(HDC hdc, long x, long y, const char* text, int px_h,
                    int halo_px) {
  draw_anno_text(hdc, x, y, text, px_h, halo_px, 0.f);
}

void draw_anno_text(HDC hdc, long x, long y, const char* text, int px_h,
                    int halo_px, float angle_deg) {
  if (!hdc || !text || !text[0]) {
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
  const COLORREF ink = GetTextColor(hdc);
  const COLORREF ink_use =
      (ink == 0 || ink == RGB(0, 0, 0)) ? RGB(28, 28, 28) : ink;
  {
    GdiplusGraphics gfx(hdc);
    if (gfx.ok() &&
        gfx.draw_string(static_cast<int>(x), static_cast<int>(y), w.c_str(),
                        px_h, ink_use, RGB(252, 252, 250), halo_px,
                        angle_deg)) {
      return;
    }
  }
  // GDI fallback (no rotation).
  HFONT font = CreateFontW(-px_h, 0, 0, 0, px_h >= 16 ? FW_SEMIBOLD : FW_NORMAL,
                           FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
  HGDIOBJ old = font ? SelectObject(hdc, font) : nullptr;
  SetBkMode(hdc, TRANSPARENT);
  const int n = static_cast<int>(w.size());
  SetTextColor(hdc, RGB(252, 252, 250));
  for (int dy = -halo_px; dy <= halo_px; ++dy) {
    for (int dx = -halo_px; dx <= halo_px; ++dx) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      TextOutW(hdc, x + dx, y + dy, w.c_str(), n);
    }
  }
  SetTextColor(hdc, ink_use);
  TextOutW(hdc, x, y, w.c_str(), n);
  if (font) {
    SelectObject(hdc, old);
    DeleteObject(font);
  }
}
