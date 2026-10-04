// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/button/checkbox.h"

#include <algorithm>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/event.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ui {
namespace views {
namespace {

constexpr int kBoxDip = 14;
constexpr int kGapDip = 8;
constexpr int kPadYDip = 4;
constexpr int kMinHeightDip = 22;

float checkbox_scale(const View* view) {
  if (view && view->widget()) {
    return view->widget()->device_scale_factor();
  }
  return 1.f;
}

}  // namespace

Checkbox::Checkbox(std::string label) : label_(std::move(label)) {
  rebuild_preferred_size();
  set_focusable(true);
}

void Checkbox::rebuild_preferred_size() {
  const float scale = checkbox_scale(this);
  const Size ink =
      label_.empty() ? Size{} : measure_text_utf8(label_, scale);
  const int box = dip_to_px(kBoxDip, scale);
  const int gap = dip_to_px(kGapDip, scale);
  const int pad_y = dip_to_px(kPadYDip, scale);
  const int min_h = dip_to_px(kMinHeightDip, scale);
  const int h = (std::max)(min_h, (std::max)(box, ink.height) + pad_y);
  const int w = box + gap + ink.width + dip_to_px(4, scale);
  set_preferred_size({w, h});
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
  if (label == label_) {
    return;
  }
  label_ = std::move(label);
  rebuild_preferred_size();
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

void Checkbox::on_device_scale_factor_changed(float /*old_scale*/,
                                             float /*new_scale*/) {
  rebuild_preferred_size();
}

void Checkbox::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  const float scale = checkbox_scale(this);
  const int box = dip_to_px(kBoxDip, scale);
  const int gap = dip_to_px(kGapDip, scale);
  const int box_y = b.y + std::max(0, (b.height - box) / 2);
  ui::gfx::Color fill = checked_ ? t.accent : t.control_unchecked;
  if (!is_enabled()) {
    fill = t.control_disabled;
  } else if (is_pressed() || is_hovered()) {
    if (!checked_) {
      fill = t.control_hover;
    }
  }
  canvas->fill_rect(b.x, box_y, box, box, fill);
  const Size text = measure_text_utf8(label_, scale);
  const int text_y = b.y + std::max(0, (b.height - text.height) / 2);
  canvas->save();
  canvas->clip_rect(b.x, b.y, b.width, b.height);
  canvas->draw_text(b.x + box + gap, text_y, utf8_to_wide(label_).c_str(),
                    is_enabled() ? t.text : t.text_muted);
  canvas->restore();
  if (is_focused()) {
    draw_focus_ring(canvas, {b.x, box_y, box, box});
  }
}

std::string_view Checkbox::paint_role() const {
  return "checkbox";
}

}  // namespace views
}  // namespace ui
