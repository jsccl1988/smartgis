// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gfx/canvas/canvas_backend.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace ui {
namespace gfx {
namespace detail {
namespace {

COLORREF to_colorref(Color color) {
  const std::uint8_t r = static_cast<std::uint8_t>((color >> 16) & 0xFF);
  const std::uint8_t g = static_cast<std::uint8_t>((color >> 8) & 0xFF);
  const std::uint8_t b = static_cast<std::uint8_t>(color & 0xFF);
  return RGB(r, g, b);
}

int clamp_stroke(int stroke_width) {
  return stroke_width < 1 ? 1 : stroke_width;
}

class GdiCanvasBackend final : public CanvasBackend {
 public:
  explicit GdiCanvasBackend(HDC hdc) : hdc_(hdc) {}

  void fill_rect(int x, int y, int w, int h, Color color) override {
    if (!hdc_ || w <= 0 || h <= 0) {
      return;
    }
    const RECT rc = {x, y, x + w, y + h};
    const COLORREF prev = SetDCBrushColor(hdc_, to_colorref(color));
    FillRect(hdc_, &rc, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    SetDCBrushColor(hdc_, prev);
  }

  void stroke_rect(int x, int y, int w, int h, Color color,
                   int stroke_width) override {
    if (!hdc_ || w <= 0 || h <= 0) {
      return;
    }
    stroke_width = clamp_stroke(stroke_width);
    const HPEN pen = CreatePen(PS_SOLID, stroke_width, to_colorref(color));
    const HGDIOBJ old_pen =
        SelectObject(hdc_, pen ? pen : GetStockObject(DC_PEN));
    const HGDIOBJ old_brush = SelectObject(hdc_, GetStockObject(NULL_BRUSH));
    Rectangle(hdc_, x, y, x + w, y + h);
    SelectObject(hdc_, old_brush);
    SelectObject(hdc_, old_pen);
    if (pen) {
      DeleteObject(pen);
    }
  }

  void draw_line(int x0, int y0, int x1, int y1, Color color,
                 int stroke_width) override {
    if (!hdc_) {
      return;
    }
    stroke_width = clamp_stroke(stroke_width);
    const HPEN pen = CreatePen(PS_SOLID, stroke_width, to_colorref(color));
    const HGDIOBJ old_pen =
        SelectObject(hdc_, pen ? pen : GetStockObject(DC_PEN));
    MoveToEx(hdc_, x0, y0, nullptr);
    LineTo(hdc_, x1, y1);
    SelectObject(hdc_, old_pen);
    if (pen) {
      DeleteObject(pen);
    }
  }

  void draw_text(int x, int y, const wchar_t* text, Color color) override {
    if (!hdc_ || !text) {
      return;
    }
    SetBkMode(hdc_, TRANSPARENT);
    SetTextColor(hdc_, to_colorref(color));
    TextOutW(hdc_, x, y, text, lstrlenW(text));
  }

  Size measure_text(const wchar_t* text) const override {
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

  void clip_rect(int x, int y, int w, int h) override {
    if (!hdc_ || w <= 0 || h <= 0) {
      return;
    }
    IntersectClipRect(hdc_, x, y, x + w, y + h);
  }

  void save() override {
    if (!hdc_) {
      return;
    }
    const int id = SaveDC(hdc_);
    if (id != 0) {
      saved_dcs_.push_back(id);
    }
  }

  void restore() override {
    if (!hdc_ || saved_dcs_.empty()) {
      return;
    }
    const int id = saved_dcs_.back();
    saved_dcs_.pop_back();
    RestoreDC(hdc_, id);
  }

 private:
  HDC hdc_ = nullptr;
  std::vector<int> saved_dcs_;
};

}  // namespace

std::unique_ptr<CanvasBackend> create_gdi_canvas_backend(HDC hdc, int /*width*/,
                                                         int /*height*/) {
  return std::make_unique<GdiCanvasBackend>(hdc);
}

}  // namespace detail
}  // namespace gfx
}  // namespace ui
