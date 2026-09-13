// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/skia/canvas.h"

namespace render {
namespace skia {
namespace {

COLORREF to_colorref(Color color) {
  const std::uint8_t r = static_cast<std::uint8_t>((color >> 16) & 0xFF);
  const std::uint8_t g = static_cast<std::uint8_t>((color >> 8) & 0xFF);
  const std::uint8_t b = static_cast<std::uint8_t>(color & 0xFF);
  return RGB(r, g, b);
}

}  // namespace

Canvas::Canvas(HDC hdc, int width, int height)
    : hdc_(hdc), width_(width), height_(height) {}

void Canvas::fill_rect(int x, int y, int w, int h, Color color) {
  if (!hdc_ || w <= 0 || h <= 0) {
    return;
  }
  const RECT rc = {x, y, x + w, y + h};
  const HBRUSH brush = CreateSolidBrush(to_colorref(color));
  FillRect(hdc_, &rc, brush);
  DeleteObject(brush);
}

void Canvas::draw_text(int x, int y, const wchar_t* text, Color color) {
  if (!hdc_ || !text) {
    return;
  }
  SetBkMode(hdc_, TRANSPARENT);
  SetTextColor(hdc_, to_colorref(color));
  TextOutW(hdc_, x, y, text, lstrlenW(text));
}

}  // namespace skia
}  // namespace render
