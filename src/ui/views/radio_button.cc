// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/radio_button.h"

#include "render/skia/canvas.h"
#include "ui/views/theme.h"

namespace ui {
namespace views {

RadioButton::RadioButton(std::string label, int group_id)
    : label_(std::move(label)), group_id_(group_id) {
  set_preferred_size({200, 24});
  set_focusable(true);
}

void RadioButton::set_selected(bool on) {
  if (selected_ == on) {
    return;
  }
  selected_ = on;
  if (on && parent()) {
    for (size_t i = 0; i < parent()->child_count(); ++i) {
      auto* other = dynamic_cast<RadioButton*>(parent()->child_at(i));
      if (other && other != this && other->group_id() == group_id_) {
        if (other->selected_) {
          other->selected_ = false;
          other->schedule_paint();
        }
      }
    }
  }
  schedule_paint();
}

bool RadioButton::is_selected() const {
  return selected_;
}

int RadioButton::group_id() const {
  return group_id_;
}

const std::string& RadioButton::label() const {
  return label_;
}

void RadioButton::set_change(std::function<void()> fn) {
  change_ = std::move(fn);
}

void RadioButton::select_from_user() {
  if (!is_enabled()) {
    return;
  }
  const bool was = selected_;
  set_selected(true);
  if (!was && change_) {
    change_();
  }
}

bool RadioButton::on_mouse_event(const MouseEvent& e) {
  if (!is_enabled()) {
    return false;
  }
  if (e.type == MouseEvent::Type::kUp && e.button == 1) {
    select_from_user();
    return true;
  }
  return e.type == MouseEvent::Type::kDown && e.button == 1;
}

bool RadioButton::on_key_event(const KeyEvent& e) {
  if (!is_enabled() || e.type != KeyEvent::Type::kDown) {
    return false;
  }
  if (e.vk == VK_SPACE || e.vk == VK_RETURN) {
    select_from_user();
    return true;
  }
  return false;
}

void RadioButton::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  render::skia::Color box = selected_ ? t.accent : t.control_unchecked;
  if (!is_enabled()) {
    box = t.control_disabled;
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
