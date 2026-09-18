#include "legacy/render/gdi/gdi_aux_api.h"

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

void draw_anno_text(HDC hdc, long x, long y, const char* text) {
  if (!hdc || !text || !text[0]) {
    return;
  }
  wchar_t wide[512];
  int n = MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, 512);
  if (n <= 1) {
    n = MultiByteToWideChar(CP_ACP, 0, text, -1, wide, 512);
  }
  if (n <= 1) {
    return;
  }
  HFONT font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                           L"Microsoft YaHei");
  HGDIOBJ old = font ? SelectObject(hdc, font) : nullptr;
  SetBkMode(hdc, TRANSPARENT);
  TextOutW(hdc, x, y, wide, n - 1);
  if (font) {
    SelectObject(hdc, old);
    DeleteObject(font);
  }
}
