// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GFX_CANVAS_CANVAS_H_
#define UI_GFX_CANVAS_CANVAS_H_

// Paint surface for the Views shell. Process preference selects GDI or Skia
// (shell_canvas_backend.h). Both backends may be linked; consumers must not
// #ifdef on HAS_SKIA. See docs/superpowers/ui-views-skia.md.

#include "ui/ui_export.h"
#include <memory>
#include <windows.h>

#include "ui/gfx/color/color.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
namespace gfx {

namespace detail {
class CanvasBackend;
}  // namespace detail

// Immediate-mode canvas used by ui::views. Map pixels stay on content / RHI
// paths, not on this type.
class UI_EXPORT Canvas {
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
  HDC hdc() const { return hdc_; }

  // Drop a SkSurface previously wrapped around a shell DIB that is about to
  // be deleted. GDI path: no-op. Call before DeleteObject on that DIB.
  static void discard_retained_surface();

 private:
  HDC hdc_;
  int width_;
  int height_;
  std::unique_ptr<detail::CanvasBackend> backend_;
};

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_CANVAS_CANVAS_H_
