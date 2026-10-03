// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/shell/status_bar.h"

#include <algorithm>
#include <format>
#include <memory>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/text/label.h"

namespace ui {
namespace views {

StatusBar::StatusBar() {
  MarkupRoot loaded = load_markup("shell/status_bar.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({400, 24});
    return;
  }
  scale_ = loaded.ids.find_as<Label>("scale");
  crs_ = loaded.ids.find_as<Label>("crs");
  coord_ = loaded.ids.find_as<Label>("coord");
  status_ = loaded.ids.find_as<Label>("status");
  const Theme& theme = Theme::current();
  const ui::gfx::Color ink = theme.text_bright;
  const ui::gfx::Color muted = theme.text_muted;
  if (scale_) {
    scale_->set_color(ink);
  }
  if (crs_) {
    crs_->set_color(ink);
  }
  if (coord_) {
    coord_->set_color(ink);
  }
  if (status_) {
    status_->set_color(muted);
    status_->set_text("Ready");
  }
  // Single tab stop for the strip; arrows cycle field highlight.
  set_focusable(true);

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({400, 32});
  add_child(std::move(loaded.root));
  set_preferred_size({400, 32});
}

Label* StatusBar::field_at(int index) const {
  switch (index) {
    case 0:
      return scale_;
    case 1:
      return crs_;
    case 2:
      return coord_;
    case 3:
      return status_;
    default:
      return nullptr;
  }
}

void StatusBar::on_focus() {
  schedule_paint();
}

void StatusBar::on_blur() {
  schedule_paint();
}

bool StatusBar::on_key_event(const KeyEvent& event) {
  if (event.type != KeyEvent::Type::kDown) {
    return false;
  }
  if (event.vk == VK_LEFT) {
    highlight_index_ = (highlight_index_ + kFieldCount - 1) % kFieldCount;
    schedule_paint();
    return true;
  }
  if (event.vk == VK_RIGHT) {
    highlight_index_ = (highlight_index_ + 1) % kFieldCount;
    schedule_paint();
    return true;
  }
  if (event.vk == VK_HOME) {
    highlight_index_ = 0;
    schedule_paint();
    return true;
  }
  if (event.vk == VK_END) {
    highlight_index_ = kFieldCount - 1;
    schedule_paint();
    return true;
  }
  if (event.vk == VK_SPACE || event.vk == VK_RETURN) {
    return true;
  }
  return false;
}

void StatusBar::set_xy(double x, double y) {
  set_coord_text(std::format("X: {:.3f}  Y: {:.3f}", x, y));
}

void StatusBar::set_scale(double scale) {
  set_scale_text(std::format("1:{:.0f}", scale));
}

void StatusBar::set_message(std::string text) {
  set_status(std::move(text));
}

void StatusBar::set_scale_text(std::string text) {
  if (scale_) {
    scale_->set_text(std::move(text));
  }
}

void StatusBar::set_crs_text(std::string text) {
  if (crs_) {
    crs_->set_text(std::move(text));
  }
}

void StatusBar::set_coord_text(std::string text) {
  if (coord_) {
    coord_->set_text(std::move(text));
  }
}

void StatusBar::set_status(std::string text) {
  if (!status_) {
    return;
  }
  const Theme& t = Theme::current();
  if (text.empty() || text == "Ready") {
    if (text.empty()) {
      text = "Ready";
    }
    status_->set_color(t.text_muted);
  } else {
    status_->set_color(t.text_bright);
  }
  status_->set_text(std::move(text));
}

const std::string& StatusBar::scale_text() const {
  static const std::string kEmpty;
  return scale_ ? scale_->text() : kEmpty;
}

const std::string& StatusBar::crs_text() const {
  static const std::string kEmpty;
  return crs_ ? crs_->text() : kEmpty;
}

const std::string& StatusBar::coord_text() const {
  static const std::string kEmpty;
  return coord_ ? coord_->text() : kEmpty;
}

const std::string& StatusBar::status() const {
  static const std::string kEmpty;
  return status_ ? status_->text() : kEmpty;
}

void StatusBar::on_device_scale_factor_changed(float old_scale,
                                              float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  const float s = new_scale > 0.f ? new_scale : 1.f;
  set_preferred_size({dip_to_px(400, s), dip_to_px(32, s)});
  layout();
  schedule_paint();
}

void StatusBar::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);

  const float scale = widget() ? widget()->device_scale_factor() : 1.f;
  const int hair = std::max(1, dip_to_px(1, scale));
  canvas->fill_rect(b.x, b.y, b.width, hair, t.panel_header);

  const int inset = std::max(2, dip_to_px(6, scale));
  auto paint_field_gap = [&](Label* left, Label* right) {
    if (!left || !right) {
      return;
    }
    const Rect& a = left->bounds();
    const Rect& c = right->bounds();
    if (c.x <= a.right()) {
      return;
    }
    const int mid = (a.right() + c.x) / 2;
    canvas->fill_rect(mid, b.y + inset, hair,
                      std::max(0, b.height - inset * 2), t.panel_header);
  };
  paint_field_gap(scale_, crs_);
  paint_field_gap(crs_, coord_);
  paint_field_gap(coord_, status_);

  if (is_focused()) {
    if (Label* lab = field_at(highlight_index_)) {
      const Rect& fb = lab->bounds();
      const int fi = std::max(1, dip_to_px(1, scale));
      canvas->fill_rect(fb.x, fb.y, fb.width, fb.height, t.panel_header);
      draw_focus_ring(canvas, {fb.x + fi, fb.y + fi,
                               std::max(0, fb.width - 2 * fi),
                               std::max(0, fb.height - 2 * fi)});
    } else {
      draw_focus_ring(canvas, b);
    }
  }
}

}  // namespace views
}  // namespace ui
