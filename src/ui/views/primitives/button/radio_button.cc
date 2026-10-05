// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/button/radio_button.h"

#include <algorithm>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/primitives/detail/control_paint.h"
#include "ui/views/primitives/detail/primary_input.h"

namespace ui {
namespace views {
namespace {

constexpr int kBoxDip = 14;
constexpr int kGapDip = 8;
constexpr int kPadYDip = 4;
constexpr int kMinHeightDip = 22;

}  // namespace

RadioButton::RadioButton(std::string label, int group_id)
    : label_(std::move(label)), group_id_(group_id) {
  rebuild_preferred_size();
  set_focusable(true);
}

void RadioButton::rebuild_preferred_size() {
  const float scale = detail::device_scale_for(this);
  const Size ink = label_.empty() ? Size{} : measure_text_utf8(label_, scale);
  const int box = dip_to_px(kBoxDip, scale);
  const int gap = dip_to_px(kGapDip, scale);
  const int pad_y = dip_to_px(kPadYDip, scale);
  const int min_h = dip_to_px(kMinHeightDip, scale);
  const int h = (std::max)(min_h, (std::max)(box, ink.height) + pad_y);
  const int w = box + gap + ink.width + dip_to_px(4, scale);
  set_preferred_size({w, h});
}

void RadioButton::exclusive_unselect_peers() {
  View* root = this;
  while (root->parent()) {
    root = root->parent();
  }
  const auto walk = [&](auto& self, View* node) -> void {
    if (!node) {
      return;
    }
    for (size_t i = 0; i < node->child_count(); ++i) {
      View* child = node->child_at(i);
      auto* other = dynamic_cast<RadioButton*>(child);
      if (other && other != this && other->group_id() == group_id_) {
        if (other->selected_) {
          other->selected_ = false;
          other->schedule_paint();
        }
      }
      self(self, child);
    }
  };
  walk(walk, root);
}

void RadioButton::set_selected(bool on) {
  if (selected_ == on) {
    if (on) {
      exclusive_unselect_peers();
    }
    return;
  }
  selected_ = on;
  if (on) {
    exclusive_unselect_peers();
  }
  schedule_paint();
}

void RadioButton::layout() {
  View::layout();
  // Markup sets selected=true before add_child; exclusivity only works once
  // the control is in a tree.
  if (selected_) {
    exclusive_unselect_peers();
  }
}

void RadioButton::on_device_scale_factor_changed(float /*old_scale*/,
                                                 float /*new_scale*/) {
  rebuild_preferred_size();
}

bool RadioButton::is_selected() const {
  return selected_;
}

int RadioButton::group_id() const {
  return group_id_;
}

void RadioButton::set_label(std::string label) {
  if (label == label_) {
    return;
  }
  label_ = std::move(label);
  rebuild_preferred_size();
  schedule_paint();
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
  return detail::handle_primary_click(e, is_enabled(),
                                      [this] { select_from_user(); });
}

bool RadioButton::on_key_event(const KeyEvent& e) {
  return detail::handle_activate_key(e, is_enabled(), true,
                                     [this] { select_from_user(); });
}

void RadioButton::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  const float scale = detail::device_scale_for(this);
  const int box = dip_to_px(kBoxDip, scale);
  const int gap = dip_to_px(kGapDip, scale);
  const int box_y = b.y + std::max(0, (b.height - box) / 2);
  ui::gfx::Color fill = selected_ ? t.accent : t.control_unchecked;
  if (!is_enabled()) {
    fill = t.control_disabled;
  } else if (is_pressed() || is_hovered()) {
    if (!selected_) {
      fill = t.control_hover;
    }
  }
  canvas->fill_rect(b.x, box_y, box, box, fill);
  const Size text = measure_text_utf8(label_, scale);
  const int text_y = detail::centered_text_y(b, text.height);
  const std::wstring w = utf8_to_wide(label_);
  detail::draw_clipped_text(canvas, b, b.x + box + gap, text_y, w.c_str(),
                            is_enabled() ? t.text : t.text_muted);
  if (is_focused()) {
    draw_focus_ring(canvas, {b.x, box_y, box, box});
  }
}

std::string_view RadioButton::paint_role() const {
  return "radio_button";
}

}  // namespace views
}  // namespace ui
