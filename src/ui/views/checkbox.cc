// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/checkbox.h"

#include "render/skia/canvas.h"
#include "ui/views/theme.h"

namespace ui {
namespace views {

Checkbox::Checkbox(std::string label) : label_(std::move(label)) {
  set_preferred_size({200, 24});
  set_focusable(true);
}

void Checkbox::set_checked(bool on) {
  if (checked_ == on) {
    return;
  }
  checked_ = on;
  schedule_paint();
}

bool Checkbox::is_checked() const {
  return checked_;
}

void Checkbox::set_label(std::string label) {
  label_ = std::move(label);
  schedule_paint();
}

const std::string& Checkbox::label() const {
  return label_;
}

void Checkbox::set_change(std::function<void(bool)> fn) {
  change_ = std::move(fn);
}

void Checkbox::toggle() {
  if (!is_enabled()) {
    return;
  }
  checked_ = !checked_;
  schedule_paint();
  if (change_) {
    change_(checked_);
  }
}

bool Checkbox::on_mouse_event(const MouseEvent& e) {
  if (!is_enabled()) {
    return false;
  }
  if (e.type == MouseEvent::Type::kUp && e.button == 1) {
    toggle();
    return true;
  }
  return e.type == MouseEvent::Type::kDown && e.button == 1;
}

bool Checkbox::on_key_event(const KeyEvent& e) {
  if (!is_enabled() || e.type != KeyEvent::Type::kDown) {
    return false;
  }
  if (e.vk == VK_SPACE) {
    toggle();
    return true;
  }
  return false;
}

void Checkbox::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  render::skia::Color box = checked_ ? t.accent : t.control_unchecked;
  if (!is_enabled()) {
    box = t.control_disabled;
  } else if (is_pressed() || is_hovered()) {
    if (!checked_) {
      box = t.control_hover;
    }
  }
  canvas->fill_rect(b.x, b.y + 4, 16, 16, box);
  const std::wstring w = utf8_to_wide(label_);
  canvas->draw_text(b.x + 22, b.y + 4, w.c_str(),
                    is_enabled() ? t.text : t.text_muted);
  if (is_focused()) {
    draw_focus_ring(canvas, {b.x, b.y, 16, 20});
  }
}

}  // namespace views
}  // namespace ui
