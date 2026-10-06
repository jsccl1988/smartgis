// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_DETAIL_CONTROL_PAINT_H_
#define UI_VIEWS_PRIMITIVES_DETAIL_CONTROL_PAINT_H_

#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/color/color.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {
namespace detail {

// Widget DPI for a control, or 1.0 when headless (unit tests without HWND).
inline float device_scale_for(const View* view) {
  if (view && view->widget()) {
    return view->widget()->device_scale_factor();
  }
  return 1.f;
}

// Vertically center |ink_h| inside |b|; never above b.y.
inline int centered_text_y(const Rect& b, int ink_h) {
  if (ink_h <= 0 || b.height <= ink_h) {
    return b.y;
  }
  return b.y + (b.height - ink_h) / 2;
}

// Draw UTF-16 text clipped to |clip| so horizon does not bleed into siblings.
inline void draw_clipped_text(ui::gfx::Canvas* canvas, const Rect& clip, int x,
                              int y, const wchar_t* text, ui::gfx::Color color) {
  if (!canvas || !text || clip.width <= 0 || clip.height <= 0) {
    return;
  }
  canvas->save();
  canvas->clip_rect(clip.x, clip.y, clip.width, clip.height);
  canvas->draw_text(x, y, text, color);
  canvas->restore();
}

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_DETAIL_CONTROL_PAINT_H_
