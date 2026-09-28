// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/shell/status_bar.h"

#include <format>
#include <memory>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
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

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({400, 24});
  add_child(std::move(loaded.root));
  set_preferred_size({400, 24});
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
  if (status_) {
    status_->set_text(std::move(text));
  }
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
  set_preferred_size({dip_to_px(400, s), dip_to_px(24, s)});
}

void StatusBar::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
}

}  // namespace views
}  // namespace ui
