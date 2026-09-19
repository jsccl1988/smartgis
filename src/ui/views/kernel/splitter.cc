// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/splitter.h"

#include <algorithm>

#include "render/skia/canvas.h"
#include "ui/views/kernel/theme.h"

namespace ui {
namespace views {

Splitter::Splitter(Orientation orientation) : orientation_(orientation) {
  set_preferred_size({200, 200});
}

void Splitter::set_collapsed(bool collapsed) {
  if (collapsed_ == collapsed) {
    return;
  }
  if (collapsed) {
    saved_primary_ = primary_extent_;
  } else if (saved_primary_ > 0) {
    primary_extent_ = saved_primary_;
  }
  collapsed_ = collapsed;
  layout();
  schedule_paint();
}

int Splitter::main_extent() const {
  return is_horizontal() ? bounds().width : bounds().height;
}

Rect Splitter::bar_rect() const {
  const Rect& b = bounds();
  if (is_horizontal()) {
    return {b.x + primary_extent_, b.y, kBarPx, b.height};
  }
  return {b.x, b.y + primary_extent_, b.width, kBarPx};
}

void Splitter::seed_split_if_needed() {
  if (split_seeded_ || child_count() < 2) {
    return;
  }
  // layout() can run before the host has a real size; do not lock a 0 split.
  if (main_extent() <= kBarPx) {
    return;
  }
  View* a = child_at(0);
  View* b = child_at(1);
  const int pa = is_horizontal() ? a->preferred_size().width
                                 : a->preferred_size().height;
  const int pb = is_horizontal() ? b->preferred_size().width
                                 : b->preferred_size().height;
  const int inner = std::max(0, main_extent() - kBarPx);
  fixed_secondary_px_ = 0;
  if (pa <= 0 && pb <= 0) {
    primary_extent_ = inner / 2;
    resize_policy_ = ResizePolicy::kProportional;
  } else if (pa <= 0) {
    // BrowserView pattern: flexible map/work pane + fixed ambox/inspector.
    primary_extent_ = inner - pb;
    fixed_secondary_px_ = pb;
    resize_policy_ = ResizePolicy::kSecondaryFixed;
  } else if (pb <= 0) {
    // Catalog (fixed preferred) + map tabs (flex).
    primary_extent_ = pa;
    resize_policy_ = ResizePolicy::kPrimaryFixed;
  } else {
    primary_extent_ = inner * pa / (pa + pb);
    resize_policy_ = ResizePolicy::kProportional;
  }
  split_seeded_ = true;
  last_main_ = main_extent();
}

void Splitter::adjust_for_host_resize() {
  const int main = main_extent();
  if (!split_seeded_ || collapsed_ || dragging_ || main <= kBarPx) {
    if (main > kBarPx) {
      last_main_ = main;
    }
    return;
  }
  if (last_main_ <= kBarPx) {
    last_main_ = main;
    return;
  }
  if (main == last_main_) {
    return;
  }

  const int inner = std::max(0, main - kBarPx);
  const int last_inner = std::max(0, last_main_ - kBarPx);
  if (user_adjusted_ || resize_policy_ == ResizePolicy::kProportional) {
    if (last_inner > 0) {
      primary_extent_ = inner * primary_extent_ / last_inner;
    }
  } else if (resize_policy_ == ResizePolicy::kSecondaryFixed) {
    primary_extent_ = inner - fixed_secondary_px_;
  }
  // kPrimaryFixed: keep primary_extent_; secondary absorbs growth.
  last_main_ = main;
}

void Splitter::clamp_primary() {
  const int main = main_extent();
  if (collapsed_) {
    primary_extent_ = std::max(0, main - kBarPx);
    return;
  }
  const int max_primary = main - kBarPx - kMinPanePx;
  if (main < kMinPanePx * 2 + kBarPx) {
    primary_extent_ = std::max(0, std::min(primary_extent_, main - kBarPx));
    return;
  }
  primary_extent_ = std::max(kMinPanePx, std::min(primary_extent_, max_primary));
}

void Splitter::apply_child_bounds() {
  const Rect& b = bounds();
  View* a = child_count() > 0 ? child_at(0) : nullptr;
  View* sec = child_count() > 1 ? child_at(1) : nullptr;
  if (is_horizontal()) {
    if (a) {
      a->set_bounds({b.x, b.y, primary_extent_, b.height});
    }
    if (sec) {
      const int x = b.x + primary_extent_ + kBarPx;
      sec->set_bounds({x, b.y, std::max(0, b.right() - x), b.height});
    }
  } else {
    if (a) {
      a->set_bounds({b.x, b.y, b.width, primary_extent_});
    }
    if (sec) {
      const int y = b.y + primary_extent_ + kBarPx;
      sec->set_bounds({b.x, y, b.width, std::max(0, b.bottom() - y)});
    }
  }
}

void Splitter::layout() {
  seed_split_if_needed();
  adjust_for_host_resize();
  clamp_primary();
  apply_child_bounds();
  for (size_t i = 0; i < child_count(); ++i) {
    View* child = child_at(i);
    if (child && child->is_visible()) {
      child->layout();
      child->sync_native_bounds();
    }
  }
}

void Splitter::begin_drag(int pos) {
  dragging_ = true;
  user_adjusted_ = true;
  drag_origin_ = pos;
  drag_primary_origin_ = primary_extent_;
  if (collapsed_) {
    collapsed_ = false;
    saved_primary_ = 0;
  }
}

void Splitter::update_drag(int pos) {
  primary_extent_ = drag_primary_origin_ + (pos - drag_origin_);
  clamp_primary();
  apply_child_bounds();
  for (size_t i = 0; i < child_count(); ++i) {
    View* child = child_at(i);
    if (child && child->is_visible()) {
      child->layout();
      child->sync_native_bounds();
    }
  }
  schedule_paint();
}

bool Splitter::on_mouse_event(const MouseEvent& event) {
  if (!is_enabled() || !is_visible()) {
    return false;
  }
  const int pos = is_horizontal() ? event.x : event.y;
  if (dragging_) {
    if (event.type == MouseEvent::Type::kMove ||
        event.type == MouseEvent::Type::kUp) {
      update_drag(pos);
      if (event.type == MouseEvent::Type::kUp) {
        dragging_ = false;
      }
      return true;
    }
  }
  if (bar_rect().contains(event.x, event.y)) {
    SetCursor(LoadCursor(nullptr,
                         is_horizontal() ? IDC_SIZEWE : IDC_SIZENS));
    if (event.type == MouseEvent::Type::kDown && event.button == 1) {
      begin_drag(pos);
      return true;
    }
    return event.type == MouseEvent::Type::kMove ||
           event.type == MouseEvent::Type::kDown;
  }
  return View::on_mouse_event(event);
}

void Splitter::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
  const Rect bar = bar_rect();
  render::skia::Color fill = t.control_fill;
  if (dragging_ || is_pressed()) {
    fill = t.control_press;
  } else if (is_hovered()) {
    fill = t.control_hover;
  }
  canvas->fill_rect(bar.x, bar.y, bar.width, bar.height, fill);
}

}  // namespace views
}  // namespace ui
