// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/testing/harness/views_test_base.h"

#include "ui/gfx/canvas/canvas.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ui {
namespace views {

void make_test_widget(TestWidgetRoot* out, int width, int height) {
  if (!out) {
    return;
  }
  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, width, height});
  out->root = root.get();
  out->widget.set_contents_view(std::move(root));
}

View* find_child_at(View* root, int x, int y) {
  if (!root) {
    return nullptr;
  }
  return root->get_view_at(x, y);
}

void paint_offscreen(View* root, int width, int height) {
  if (width <= 0 || height <= 0) {
    return;
  }
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = width;
  bi.bmiHeader.biHeight = -height;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  HGDIOBJ old_bmp = bmp ? SelectObject(mem, bmp) : nullptr;
  HFONT font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
  HGDIOBJ old_font = font ? SelectObject(mem, font) : nullptr;
  {
    ui::gfx::Canvas canvas(mem, width, height);
    if (root) {
      root->paint(&canvas);
    }
  }
  if (font) {
    SelectObject(mem, old_font);
    DeleteObject(font);
  }
  if (bmp) {
    SelectObject(mem, old_bmp);
    DeleteObject(bmp);
  }
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
}

}  // namespace views
}  // namespace ui
