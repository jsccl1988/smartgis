// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/button/button.h"

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {
namespace {

// Matches paint_self text inset (8px x, 6px y each side) at 96 DPI.
constexpr int kPadX = 16;
constexpr int kPadY = 12;
constexpr int kMinHeight = 28;

float scale_for(const View* view) {
  if (view && view->widget()) {
    return view->widget()->device_scale_factor();
  }
  return 1.f;
}

}  // namespace

Button::Button(std::string text) : text_(std::move(text)) {
  rebuild_text_cache();
  set_focusable(true);
}

void Button::rebuild_text_cache() {
  const float scale = scale_for(this);
  wide_ = utf8_to_wide(text_);
  const Size ink = text_.empty() ? Size{} : measure_text_utf8(text_, scale);
  const int pad_x = dip_to_px(kPadX, scale);
  const int pad_y = dip_to_px(kPadY, scale);
  const int min_h = dip_to_px(kMinHeight, scale);
  const int h = ink.height + pad_y;
  set_preferred_size({ink.width + pad_x, h > min_h ? h : min_h});
  invalidate_commands();
}

void Button::set_text(std::string text) {
  if (text == text_) {
    return;
  }
  text_ = std::move(text);
  rebuild_text_cache();
  schedule_paint();
}

void Button::on_device_scale_factor_changed(float /*old_scale*/,
                                          float /*new_scale*/) {
  rebuild_text_cache();
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

void Button::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  ui::gfx::Color fill = t.control_fill;
  if (!is_enabled()) {
    fill = t.control_disabled;
  } else if (is_pressed()) {
    fill = t.control_press;
  } else if (is_hovered()) {
    fill = t.control_hover;
  }
  canvas->fill_rect(b.x, b.y, b.width, b.height, fill);
  const ui::gfx::Color fg =
      is_enabled() ? t.text_bright : t.text_muted;
  if (!wide_.empty()) {
    canvas->draw_text(b.x + 8, b.y + 6, wide_.c_str(), fg);
  }
  if (is_focused()) {
    draw_focus_ring(canvas, b);
  }
}


std::string_view Button::paint_role() const {
  return "button";
}
}  // namespace views
}  // namespace ui
