// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_SKIA_CANVAS_H_
#define RENDER_SKIA_CANVAS_H_

// GDI-backed paint surface for Views chrome. No Skia checkout.
// See docs/build/ui-views-skia.md.

#include <windows.h>

#include "render/skia/color.h"

namespace render {
namespace skia {

// Immediate-mode canvas used by ui::views chrome. Map pixels stay on
// src/render/{gdi,gl}, not here.
class Canvas {
 public:
  Canvas(HDC hdc, int width, int height);

  void fill_rect(int x, int y, int w, int h, Color color);
  void draw_text(int x, int y, const wchar_t* text, Color color);

  int width() const { return width_; }
  int height() const { return height_; }
  HDC hdc() const { return hdc_; }

 private:
  HDC hdc_;
  int width_;
  int height_;
};

}  // namespace skia
}  // namespace render

#endif  // RENDER_SKIA_CANVAS_H_
