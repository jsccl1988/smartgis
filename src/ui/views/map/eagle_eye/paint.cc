// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map/eagle_eye/paint.h"

#include <algorithm>

#include "ui/views/map/eagle_eye/geom.h"

namespace ui {
namespace views {
namespace detail {

void eagle_eye_paint(HDC hdc,
                     int frame_w,
                     int frame_h,
                     const content::Extent2& world,
                     const content::Extent2& view) {
  if (frame_w <= 0 || frame_h <= 0 || !hdc) {
    return;
  }
  HDC mem = CreateCompatibleDC(hdc);
  if (!mem) {
    return;
  }
  HBITMAP bmp = CreateCompatibleBitmap(hdc, frame_w, frame_h);
  if (!bmp) {
    DeleteDC(mem);
    return;
  }
  HGDIOBJ old_bmp = SelectObject(mem, bmp);

  RECT rc = {0, 0, frame_w, frame_h};
  HBRUSH bg = CreateSolidBrush(RGB(42, 48, 58));
  FillRect(mem, &rc, bg);
  DeleteObject(bg);

  HPEN border = CreatePen(PS_SOLID, 1, RGB(90, 110, 140));
  HGDIOBJ old_pen = SelectObject(mem, border);
  HGDIOBJ old_brush = SelectObject(mem, GetStockObject(NULL_BRUSH));
  Rectangle(mem, 0, 0, frame_w, frame_h);

  SetBkMode(mem, TRANSPARENT);
  SetTextColor(mem, RGB(200, 210, 220));
  TextOutW(mem, 6, 2, L"Overview", 8);

  const EagleEyeRectPx frame{0, 0, frame_w, frame_h};
  const EagleEyeRectPx content = eagle_eye_content_inset(frame);
  if (content.w > 0 && content.h > 0) {
    RECT world_rc = {content.x, content.y, content.x + content.w,
                     content.y + content.h};
    HBRUSH world_brush = CreateSolidBrush(RGB(58, 72, 88));
    FillRect(mem, &world_rc, world_brush);
    DeleteObject(world_brush);

    const int inset = (std::max)(4, content.w / 16);
    RECT land = {content.x + inset, content.y + inset,
                 content.x + content.w - inset, content.y + content.h - inset};
    HBRUSH land_brush = CreateSolidBrush(RGB(72, 96, 78));
    FillRect(mem, &land, land_brush);
    DeleteObject(land_brush);

    const EagleEyeRectPx view_px =
        eagle_eye_view_rect_px(world, view, content);
    if (view_px.w > 0 && view_px.h > 0) {
      HPEN view_pen = CreatePen(PS_SOLID, 2, RGB(255, 196, 64));
      SelectObject(mem, view_pen);
      Rectangle(mem, view_px.x, view_px.y, view_px.x + view_px.w,
                view_px.y + view_px.h);
      DeleteObject(view_pen);

      HBRUSH handle = CreateSolidBrush(RGB(255, 220, 100));
      const int hs = kEagleEyeHandlePx;
      auto draw_handle = [&](int hx, int hy) {
        RECT hr = {hx - hs / 2, hy - hs / 2, hx + hs / 2 + 1, hy + hs / 2 + 1};
        FillRect(mem, &hr, handle);
      };
      draw_handle(view_px.x, view_px.y);
      draw_handle(view_px.x + view_px.w, view_px.y);
      draw_handle(view_px.x, view_px.y + view_px.h);
      draw_handle(view_px.x + view_px.w, view_px.y + view_px.h);
      DeleteObject(handle);
    }
  }

  SelectObject(mem, old_pen);
  SelectObject(mem, old_brush);
  DeleteObject(border);

  BitBlt(hdc, 0, 0, frame_w, frame_h, mem, 0, 0, SRCCOPY);
  SelectObject(mem, old_bmp);
  DeleteObject(bmp);
  DeleteDC(mem);
}

}  // namespace detail
}  // namespace views
}  // namespace ui
