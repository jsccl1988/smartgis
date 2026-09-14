// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_SKIA_CANVAS_H_
#define RENDER_SKIA_CANVAS_H_

// Paint surface for Views chrome. Default backend is GDI (canvas.cc).
// With smt_has_skia + local pin, the same API is implemented by canvas_skia.cc.
// Layout and destructor are identical for both backends (opaque Backend*):
// consumers must not #ifdef on SMT_HAS_SKIA. See docs/build/ui-views-skia.md.

#include <windows.h>

#include "render/render_export.h"
#include "render/skia/color.h"

namespace render {
namespace skia {

// Pixel size returned by Canvas::measure_text.
struct Size {
  int width = 0;
  int height = 0;
};

// Immediate-mode canvas used by ui::views chrome. Map pixels stay on
// leftover / RHI paths, not here.
class RENDER_EXPORT Canvas {
 public:
  Canvas(HDC hdc, int width, int height);
  ~Canvas();

  Canvas(const Canvas&) = delete;
  Canvas& operator=(const Canvas&) = delete;

  void fill_rect(int x, int y, int w, int h, Color color);
  void stroke_rect(int x, int y, int w, int h, Color color,
                   int stroke_width = 1);
  void draw_line(int x0, int y0, int x1, int y1, Color color,
                 int stroke_width = 1);
  void draw_text(int x, int y, const wchar_t* text, Color color);

  // Returns ink size in pixels; empty / null text → {0,0}.
  Size measure_text(const wchar_t* text) const;

  // Intersect with the current clip. Non-positive size is a no-op.
  void clip_rect(int x, int y, int w, int h);
  void save();
  void restore();

  int width() const { return width_; }
  int height() const { return height_; }
  // GDI stub returns the paint HDC. Real Skia may still expose it for
  // present/blit; callers must not use it to draw GIS.
  HDC hdc() const { return hdc_; }

 private:
  // Opaque backend (GDI save-stack or Skia raster). Same size for both
  // implementations so dllimport callers match the render DLL layout.
  struct Backend;

  HDC hdc_;
  int width_;
  int height_;
  Backend* backend_ = nullptr;
};

}  // namespace skia
}  // namespace render

#endif  // RENDER_SKIA_CANVAS_H_
