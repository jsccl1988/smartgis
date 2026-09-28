// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_PAINT_PAINTER_H_
#define UI_VIEWS_KERNEL_PAINT_PAINTER_H_

#include "ui/ui_export.h"

namespace ui {
namespace gfx {
class Canvas;
}
namespace views {

class View;

// Draws a View for a registered paint_role(). Reads Theme::current() as needed.
class UI_EXPORT Painter {
 public:
  virtual ~Painter() = default;
  virtual void paint(View* view, ui::gfx::Canvas* canvas) = 0;
};

// Optional per-instance decoration around the type Painter / paint_self.
class UI_EXPORT PaintDelegate {
 public:
  virtual ~PaintDelegate() = default;
  virtual void paint_before(View* view, ui::gfx::Canvas* canvas) {
    (void)view;
    (void)canvas;
  }
  virtual void paint_after(View* view, ui::gfx::Canvas* canvas) {
    (void)view;
    (void)canvas;
  }
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_PAINT_PAINTER_H_
