// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/testing/unit/views_unit_helpers.h"

#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

ui::views::MouseEvent mouse_up(int x, int y) {
  ui::views::MouseEvent e;
  e.type = ui::views::MouseEvent::Type::kUp;
  e.button = 1;
  e.x = x;
  e.y = y;
  return e;
}

ui::views::MouseEvent mouse_down(int x, int y) {
  ui::views::MouseEvent e;
  e.type = ui::views::MouseEvent::Type::kDown;
  e.button = 1;
  e.x = x;
  e.y = y;
  return e;
}

ui::views::MouseEvent mouse_move(int x, int y) {
  ui::views::MouseEvent e;
  e.type = ui::views::MouseEvent::Type::kMove;
  e.x = x;
  e.y = y;
  return e;
}

ui::views::KeyEvent key_down(std::uint32_t vk) {
  ui::views::KeyEvent e;
  e.type = ui::views::KeyEvent::Type::kDown;
  e.vk = vk;
  return e;
}

void PaintProbe::paint_self(ui::gfx::Canvas* canvas) {
  ++self_paints;
  if (canvas) {
    canvas->fill_rect(bounds().x, bounds().y, bounds().width, bounds().height,
                      ui::gfx::color_rgb(200, 40, 40));
  }
}

void CountLayout::layout(ui::views::View*) {
  ++layouts;
}

void paint_tree(ui::views::View* root,
                int width,
                int height,
                const ui::views::Rect* clip) {
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
  if (clip && clip->width > 0 && clip->height > 0) {
    IntersectClipRect(mem, clip->x, clip->y, clip->right(), clip->bottom());
  }
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
