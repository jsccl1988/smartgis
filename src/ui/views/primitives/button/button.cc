// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/button/button.h"

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/primitives/detail/control_paint.h"
#include "ui/views/primitives/detail/primary_input.h"

namespace ui {
namespace views {
namespace {

// Matches paint_self text inset (8px x, 4px y each side) at 96 DPI.
constexpr int kPadX = 16;
constexpr int kPadY = 8;
constexpr int kMinHeight = 24;

}  // namespace

Button::Button(std::string text) : text_(std::move(text)) {
  rebuild_text_cache();
  set_focusable(true);
}

void Button::rebuild_text_cache() {
  const float scale = detail::device_scale_for(this);
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

void Button::set_style(Style style) {
  if (style_ == style) {
    return;
  }
  style_ = style;
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
  return detail::handle_primary_click(e, is_enabled(), [this] { activate(); });
}

bool Button::on_key_event(const KeyEvent& e) {
  return detail::handle_activate_key(e, is_enabled(), true,
                                     [this] { activate(); });
}

void Button::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  ui::gfx::Color fill = t.control_fill;
  ui::gfx::Color edge = t.control_border;
  ui::gfx::Color fg = t.text_bright;

  if (style_ == Style::kPrimary) {
    fill = t.accent;
    edge = t.accent;
    fg = t.text_bright;
  } else if (style_ == Style::kDestructive) {
    fill = t.danger;
    edge = t.danger;
    fg = t.text_bright;
  }

  if (!is_enabled()) {
    fill = t.control_disabled;
    edge = t.control_border;
    fg = t.text_muted;
  } else if (is_pressed()) {
    if (style_ == Style::kPrimary) {
      fill = ui::gfx::color_rgb(0, 98, 168);
    } else if (style_ == Style::kDestructive) {
      fill = ui::gfx::color_rgb(160, 32, 20);
    } else {
      fill = t.control_press;
    }
  } else if (is_hovered()) {
    if (style_ == Style::kPrimary) {
      fill = ui::gfx::color_rgb(28, 140, 214);
    } else if (style_ == Style::kDestructive) {
      fill = ui::gfx::color_rgb(214, 60, 42);
    } else {
      fill = t.control_hover;
    }
  }

  canvas->fill_rect(b.x, b.y, b.width, b.height, fill);
  canvas->stroke_rect(b.x, b.y, b.width, b.height, edge, 1);
  if (!wide_.empty()) {
    const float scale = detail::device_scale_for(this);
    const int pad_x = dip_to_px(kPadX / 2, scale);  // 8dip at 96dpi
    const Size ink = measure_text_utf8(text_, scale);
    const int text_y = detail::centered_text_y(b, ink.height);
    detail::draw_clipped_text(canvas, b, b.x + pad_x, text_y, wide_.c_str(), fg);
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
