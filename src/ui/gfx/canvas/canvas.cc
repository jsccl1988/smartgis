// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gfx/canvas/canvas.h"

#include <cstdio>

#include "ui/gfx/canvas/canvas_backend.h"
#include "ui/gfx/canvas/shell_canvas_backend.h"
#include "ui/gfx/display_list/display_list.h"
#include "ui/gfx/raster/paint_stats.h"

namespace ui {
namespace gfx {

void Canvas::discard_retained_surface() {
  detail::discard_skia_retained_surface();
}

Canvas::Canvas(HDC hdc, int width, int height)
    : hdc_(hdc), width_(width), height_(height) {
  note_canvas_ctor();
  // DisplayList recording uses a null HDC; keep that path lightweight (GDI).
  if (!hdc_) {
    backend_ = detail::create_gdi_canvas_backend(hdc_, width_, height_);
    return;
  }
  if (resolved_shell_canvas_backend() == ShellCanvasBackend::kSkia) {
    backend_ = detail::create_skia_canvas_backend(hdc_, width_, height_);
    if (!backend_) {
      std::fprintf(stderr,
                   "ui::gfx: Skia surface setup failed; falling back to gdi\n");
    }
  }
  if (!backend_) {
    backend_ = detail::create_gdi_canvas_backend(hdc_, width_, height_);
  }
}

Canvas::~Canvas() {
  if (backend_) {
    backend_->present_if_owned();
    delete backend_;
    backend_ = nullptr;
  }
}

void Canvas::fill_rect(int x, int y, int w, int h, Color color) {
  if (DisplayList* rec = display_list_recorder()) {
    rec->fill_rect(x, y, w, h, color);
    return;
  }
  if (backend_) {
    backend_->fill_rect(x, y, w, h, color);
  }
}

void Canvas::stroke_rect(int x, int y, int w, int h, Color color,
                         int stroke_width) {
  if (DisplayList* rec = display_list_recorder()) {
    rec->stroke_rect(x, y, w, h, color, stroke_width);
    return;
  }
  if (backend_) {
    backend_->stroke_rect(x, y, w, h, color, stroke_width);
  }
}

void Canvas::draw_line(int x0, int y0, int x1, int y1, Color color,
                       int stroke_width) {
  if (DisplayList* rec = display_list_recorder()) {
    rec->draw_line(x0, y0, x1, y1, color, stroke_width);
    return;
  }
  if (backend_) {
    backend_->draw_line(x0, y0, x1, y1, color, stroke_width);
  }
}

void Canvas::draw_text(int x, int y, const wchar_t* text, Color color) {
  if (DisplayList* rec = display_list_recorder()) {
    rec->draw_text(x, y, text, color);
    return;
  }
  if (backend_) {
    backend_->draw_text(x, y, text, color);
  }
}

Size Canvas::measure_text(const wchar_t* text) const {
  if (backend_) {
    return backend_->measure_text(text);
  }
  return Size{};
}

void Canvas::clip_rect(int x, int y, int w, int h) {
  if (DisplayList* rec = display_list_recorder()) {
    rec->clip_rect(x, y, w, h);
    return;
  }
  if (backend_) {
    backend_->clip_rect(x, y, w, h);
  }
}

void Canvas::save() {
  if (DisplayList* rec = display_list_recorder()) {
    rec->save();
    return;
  }
  if (backend_) {
    backend_->save();
  }
}

void Canvas::restore() {
  if (DisplayList* rec = display_list_recorder()) {
    rec->restore();
    return;
  }
  if (backend_) {
    backend_->restore();
  }
}

}  // namespace gfx
}  // namespace ui
