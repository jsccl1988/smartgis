// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/gis/status_bar.h"

#include <format>
#include <memory>
#include <utility>

#include "render/skia/canvas.h"
#include "ui/views/kernel/dpi.h"
#include "ui/views/primitives/label.h"
#include "ui/views/kernel/layout.h"
#include "ui/views/kernel/theme.h"

namespace ui {
namespace views {

StatusBar::StatusBar() {
  auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
  auto scale = std::make_unique<Label>("Scale");
  scale->set_preferred_size({90, 22});
  scale_ = scale.get();
  auto crs = std::make_unique<Label>("CRS");
  crs->set_preferred_size({110, 22});
  crs_ = crs.get();
  auto coord = std::make_unique<Label>("XY");
  coord->set_preferred_size({180, 22});
  coord_ = coord.get();
  auto status = std::make_unique<Label>("");
  status->set_preferred_size({160, 22});
  status_ = status.get();
  box->set_flex_for_view(status_, 1);
  set_layout_manager(std::move(box));
  add_child(std::move(scale));
  add_child(std::move(crs));
  add_child(std::move(coord));
  add_child(std::move(status));
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
  // Keep the chrome strip height tied to DIPs after preferred_size scaling.
  set_preferred_size(
      {preferred_size().width, dip_to_px(24, new_scale > 0.f ? new_scale : 1.f)});
}

void StatusBar::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_header);
}

}  // namespace views
}  // namespace ui
