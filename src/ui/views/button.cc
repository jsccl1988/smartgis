// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/button.h"

#include "render/skia/canvas.h"
#include "ui/views/theme.h"

namespace ui {
namespace views {

Button::Button(std::string text) : text_(std::move(text)) {
  set_preferred_size({96, 28});
  set_focusable(true);
}

void Button::set_text(std::string text) {
  text_ = std::move(text);
  schedule_paint();
}

const std::string& Button::text() const {
  return text_;
}

void Button::set_click(std::function<void()> fn) {
  click_ = std::move(fn);
}

void Button::activate() {
  if (!is_enabled()) {
    return;
  }
  if (click_) {
    click_();
  }
}

bool Button::on_mouse_event(const MouseEvent& e) {
  if (!is_enabled()) {
    return false;
  }
  if (e.type == MouseEvent::Type::kUp && e.button == 1) {
    activate();
    return true;
  }
  return e.type == MouseEvent::Type::kDown && e.button == 1;
}

bool Button::on_key_event(const KeyEvent& e) {
  if (!is_enabled() || e.type != KeyEvent::Type::kDown) {
    return false;
  }
  if (e.vk == VK_SPACE || e.vk == VK_RETURN) {
    activate();
    return true;
  }
  return false;
}

void Button::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  render::skia::Color fill = t.control_fill;
  if (!is_enabled()) {
    fill = t.control_disabled;
  } else if (is_pressed()) {
    fill = t.control_press;
  } else if (is_hovered()) {
    fill = t.control_hover;
  }
  canvas->fill_rect(b.x, b.y, b.width, b.height, fill);
  const std::wstring w = utf8_to_wide(text_);
  const render::skia::Color fg =
      is_enabled() ? t.text_bright : t.text_muted;
  canvas->draw_text(b.x + 8, b.y + 6, w.c_str(), fg);
  if (is_focused()) {
    draw_focus_ring(canvas, b);
  }
}

}  // namespace views
}  // namespace ui
