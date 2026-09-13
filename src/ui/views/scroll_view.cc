// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/scroll_view.h"

#include <algorithm>

#include "render/skia/canvas.h"
#include "ui/views/theme.h"

namespace ui {
namespace views {
namespace {

constexpr int kWheelStep = 40;

}  // namespace

ScrollView::ScrollView() {
  set_preferred_size({160, 120});
}

View* ScrollView::content() const {
  return child_count() > 0 ? child_at(0) : nullptr;
}

void ScrollView::set_scroll_offset(int y) {
  scroll_y_ = y;
  clamp_scroll();
  apply_content_bounds();
  schedule_paint();
}

void ScrollView::clamp_scroll() {
  View* child = content();
  const int content_h = child ? child->preferred_size().height : 0;
  const int max_y = std::max(0, content_h - bounds().height);
  if (scroll_y_ < 0) {
    scroll_y_ = 0;
  } else if (scroll_y_ > max_y) {
    scroll_y_ = max_y;
  }
}

void ScrollView::apply_content_bounds() {
  View* child = content();
  if (!child) {
    return;
  }
  const Rect& b = bounds();
  const Size pref = child->preferred_size();
  const int w = std::max(b.width, pref.width);
  const int h = std::max(pref.height, 0);
  child->set_bounds({b.x, b.y - scroll_y_, w, h});
  child->layout();
  child->sync_native_bounds();
}

void ScrollView::layout() {
  clamp_scroll();
  apply_content_bounds();
}

void ScrollView::paint(render::skia::Canvas* canvas) {
  if (!canvas || !is_visible()) {
    return;
  }
  paint_self(canvas);
  View* child = content();
  if (!child || !child->is_visible() || child->native_view()) {
    return;
  }
  HDC hdc = canvas->hdc();
  if (!hdc) {
    child->paint(canvas);
    return;
  }
  const Rect& b = bounds();
  const int saved = SaveDC(hdc);
  IntersectClipRect(hdc, b.x, b.y, b.right(), b.bottom());
  child->paint(canvas);
  RestoreDC(hdc, saved);
}

bool ScrollView::on_mouse_event(const MouseEvent& event) {
  if (!is_visible() || !is_enabled()) {
    return false;
  }
  if (event.type == MouseEvent::Type::kWheel &&
      bounds().contains(event.x, event.y)) {
    const int steps = event.wheel_delta / 120;
    const int delta = (steps == 0 && event.wheel_delta != 0)
                          ? (event.wheel_delta > 0 ? 1 : -1)
                          : steps;
    set_scroll_offset(scroll_y_ - delta * kWheelStep);
    return true;
  }
  return View::on_mouse_event(event);
}

void ScrollView::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_bg);
}

}  // namespace views
}  // namespace ui
