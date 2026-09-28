// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GFX_CANVAS_CANVAS_BACKEND_H_
#define UI_GFX_CANVAS_CANVAS_BACKEND_H_

// Internal Canvas raster backends. Not for Views paint_self.
// Symbols stay inside ui_views.dll (no UI_EXPORT).

#include <windows.h>

#include "ui/gfx/color/color.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
namespace gfx {
namespace detail {

// Immediate-mode raster used by ui::gfx::Canvas after DisplayList recording.
class CanvasBackend {
 public:
  virtual ~CanvasBackend() = default;

  virtual void fill_rect(int x, int y, int w, int h, Color color) = 0;
  virtual void stroke_rect(int x, int y, int w, int h, Color color,
                           int stroke_width) = 0;
  virtual void draw_line(int x0, int y0, int x1, int y1, Color color,
                         int stroke_width) = 0;
  virtual void draw_text(int x, int y, const wchar_t* text, Color color) = 0;
  virtual Size measure_text(const wchar_t* text) const = 0;
  virtual void clip_rect(int x, int y, int w, int h) = 0;
  virtual void save() = 0;
  virtual void restore() = 0;
  // Skia may BitBlt a privately owned DIB; GDI no-op.
  virtual void present_if_owned() {}
};

CanvasBackend* create_gdi_canvas_backend(HDC hdc, int width, int height);
// Returns nullptr when Skia is not linked or surface setup fails.
CanvasBackend* create_skia_canvas_backend(HDC hdc, int width, int height);
bool skia_canvas_backend_linked();
void discard_skia_retained_surface();

}  // namespace detail
}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_CANVAS_CANVAS_BACKEND_H_
