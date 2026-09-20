// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/slider.h"

#include <algorithm>
#include <cmath>

#include "render/skia/canvas.h"
#include "ui/views/kernel/theme.h"

namespace ui {
namespace views {
namespace {

constexpr int kTrackHeight = 4;
constexpr int kThumbW = 10;
constexpr int kThumbH = 16;

}  // namespace

Slider::Slider() {
  set_preferred_size({200, 24});
  set_focusable(true);
}

void Slider::set_range(double min_value, double max_value) {
  if (max_value < min_value) {
    std::swap(min_value, max_value);
  }
  min_ = min_value;
  max_ = max_value;
  set_value(value_);
}

void Slider::set_value(double value) {
  const double clamped = std::clamp(value, min_, max_);
  if (clamped == value_) {
    return;
  }
  value_ = clamped;
  schedule_paint();
}

void Slider::set_change(std::function<void(double)> fn) {
  change_ = std::move(fn);
}

int Slider::thumb_center_x() const {
  const Rect& b = bounds();
  const int track_w = (std::max)(1, b.width - kThumbW);
  const double span = max_ - min_;
  const double t = span > 0.0 ? (value_ - min_) / span : 0.0;
  return b.x + kThumbW / 2 + static_cast<int>(std::lround(t * track_w));
}

void Slider::set_value_from_x(int local_x, bool notify) {
  const Rect& b = bounds();
  const int track_w = (std::max)(1, b.width - kThumbW);
  const int rel = std::clamp(local_x - kThumbW / 2, 0, track_w);
  const double span = max_ - min_;
  const double t = span > 0.0 ? static_cast<double>(rel) / track_w : 0.0;
  const double next = min_ + t * span;
  const double prev = value_;
  set_value(next);
  if (notify && change_) {
    // Notify even when clamped to the same value so hosts can refresh labels.
    (void)prev;
    change_(value_);
  }
}

bool Slider::on_mouse_event(const MouseEvent& e) {
  if (!is_enabled()) {
    return false;
  }
  const Rect& b = bounds();
  const int local_x = e.x - b.x;
  if (e.type == MouseEvent::Type::kDown && e.button == 1) {
    dragging_ = true;
    set_pressed(true);
    set_value_from_x(local_x, true);
    return true;
  }
  if (e.type == MouseEvent::Type::kMove && dragging_) {
    set_value_from_x(local_x, true);
    return true;
  }
  if (e.type == MouseEvent::Type::kUp && e.button == 1) {
    if (dragging_) {
      dragging_ = false;
      set_pressed(false);
      set_value_from_x(local_x, true);
      return true;
    }
  }
  return false;
}

bool Slider::on_key_event(const KeyEvent& e) {
  if (!is_enabled() || e.type != KeyEvent::Type::kDown) {
    return false;
  }
  const double span = max_ - min_;
  if (span <= 0.0) {
    return false;
  }
  const double step = span / 20.0;
  if (e.vk == VK_LEFT || e.vk == VK_DOWN) {
    const double prev = value_;
    set_value(value_ - step);
    if (change_ && value_ != prev) {
      change_(value_);
    }
    return true;
  }
  if (e.vk == VK_RIGHT || e.vk == VK_UP) {
    const double prev = value_;
    set_value(value_ + step);
    if (change_ && value_ != prev) {
      change_(value_);
    }
    return true;
  }
  return false;
}

void Slider::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  const int track_y = b.y + (b.height - kTrackHeight) / 2;
  canvas->fill_rect(b.x + kThumbW / 2, track_y, (std::max)(1, b.width - kThumbW),
                    kTrackHeight,
                    is_enabled() ? t.control_fill : t.control_disabled);
  const int cx = thumb_center_x();
  const int thumb_x = cx - kThumbW / 2;
  const int thumb_y = b.y + (b.height - kThumbH) / 2;
  render::skia::Color thumb = t.accent;
  if (!is_enabled()) {
    thumb = t.control_disabled;
  } else if (is_pressed() || is_hovered() || dragging_) {
    thumb = t.control_hover;
  }
  canvas->fill_rect(thumb_x, thumb_y, kThumbW, kThumbH, thumb);
  if (is_focused()) {
    draw_focus_ring(canvas, {thumb_x - 1, thumb_y - 1, kThumbW + 2, kThumbH + 2});
  }
}

}  // namespace views
}  // namespace ui
