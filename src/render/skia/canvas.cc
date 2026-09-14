// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/skia/canvas.h"

#include <cstdint>
#include <vector>

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

struct Canvas::Backend {
  std::vector<int> saved_dcs;
};

Canvas::Canvas(HDC hdc, int width, int height)
    : hdc_(hdc), width_(width), height_(height), backend_(new Backend) {}

Canvas::~Canvas() {
  delete backend_;
  backend_ = nullptr;
}

void Canvas::fill_rect(int x, int y, int w, int h, Color color) {
  if (!hdc_ || w <= 0 || h <= 0) {
    return;
  }
  const RECT rc = {x, y, x + w, y + h};
  const HBRUSH brush = CreateSolidBrush(to_colorref(color));
  FillRect(hdc_, &rc, brush);
  DeleteObject(brush);
}

void Canvas::stroke_rect(int x, int y, int w, int h, Color color,
                         int stroke_width) {
  if (!hdc_ || w <= 0 || h <= 0) {
    return;
  }
  if (stroke_width < 1) {
    stroke_width = 1;
  }
  const HPEN pen = CreatePen(PS_SOLID, stroke_width, to_colorref(color));
  const HGDIOBJ old_pen = SelectObject(hdc_, pen);
  const HGDIOBJ old_brush = SelectObject(hdc_, GetStockObject(NULL_BRUSH));
  Rectangle(hdc_, x, y, x + w, y + h);
  SelectObject(hdc_, old_brush);
  SelectObject(hdc_, old_pen);
  DeleteObject(pen);
}

void Canvas::draw_line(int x0, int y0, int x1, int y1, Color color,
                       int stroke_width) {
  if (!hdc_) {
    return;
  }
  if (stroke_width < 1) {
    stroke_width = 1;
  }
  const HPEN pen = CreatePen(PS_SOLID, stroke_width, to_colorref(color));
  const HGDIOBJ old_pen = SelectObject(hdc_, pen);
  MoveToEx(hdc_, x0, y0, nullptr);
  LineTo(hdc_, x1, y1);
  SelectObject(hdc_, old_pen);
  DeleteObject(pen);
}

void Canvas::draw_text(int x, int y, const wchar_t* text, Color color) {
  if (!hdc_ || !text) {
    return;
  }
  SetBkMode(hdc_, TRANSPARENT);
  SetTextColor(hdc_, to_colorref(color));
  TextOutW(hdc_, x, y, text, lstrlenW(text));
}

Size Canvas::measure_text(const wchar_t* text) const {
  Size out;
  if (!hdc_ || !text || !*text) {
    return out;
  }
  SIZE sz = {};
  if (GetTextExtentPoint32W(hdc_, text, lstrlenW(text), &sz)) {
    out.width = sz.cx;
    out.height = sz.cy;
  }
  return out;
}

void Canvas::clip_rect(int x, int y, int w, int h) {
  if (!hdc_ || w <= 0 || h <= 0) {
    return;
  }
  IntersectClipRect(hdc_, x, y, x + w, y + h);
}

void Canvas::save() {
  if (!hdc_ || !backend_) {
    return;
  }
  const int id = SaveDC(hdc_);
  if (id != 0) {
    backend_->saved_dcs.push_back(id);
  }
}

void Canvas::restore() {
  if (!hdc_ || !backend_ || backend_->saved_dcs.empty()) {
    return;
  }
  const int id = backend_->saved_dcs.back();
  backend_->saved_dcs.pop_back();
  RestoreDC(hdc_, id);
}

}  // namespace skia
}  // namespace render
