// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/proj/map_prj_xy_page.h"

#include <cstdlib>
#include <memory>
#include <string>

#include "content/public/plugin_host.h"
#include "plugin/proj/proj_commands.h"
#include "ui/views/button.h"
#include "ui/views/label.h"
#include "ui/views/layout.h"
#include "ui/views/textfield.h"

namespace plugin {
namespace {

double parse_field(const ui::views::Textfield* field, double fallback) {
  if (!field) {
    return fallback;
  }
  const char* start = field->text().c_str();
  char* end = nullptr;
  const double v = std::strtod(start, &end);
  if (end == start) {
    return fallback;
  }
  return v;
}

std::string build_transform_xy_json(double l, double b, double x, double y,
                                    long scale_ruler) {
  return std::string("{\"L\":") + std::to_string(l) + ",\"B\":" +
         std::to_string(b) + ",\"X\":" + std::to_string(x) + ",\"Y\":" +
         std::to_string(y) + ",\"scale_ruler\":" +
         std::to_string(scale_ruler) + "}";
}

}  // namespace

MapPrjXyPage::MapPrjXyPage(content::PluginHost* host) : host_(host) {
  set_preferred_size({480, 320});
  auto layout = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kVertical);
  set_layout_manager(std::move(layout));

  add_child(std::make_unique<ui::views::Label>("L (longitude)"));
  auto l = std::make_unique<ui::views::Textfield>();
  l->set_text("120");
  l_field_ = l.get();
  add_child(std::move(l));

  add_child(std::make_unique<ui::views::Label>("B (latitude)"));
  auto b = std::make_unique<ui::views::Textfield>();
  b->set_text("36");
  b_field_ = b.get();
  add_child(std::move(b));

  add_child(std::make_unique<ui::views::Label>("X (projected)"));
  auto x = std::make_unique<ui::views::Textfield>();
  x->set_text("0");
  x_field_ = x.get();
  add_child(std::move(x));

  add_child(std::make_unique<ui::views::Label>("Y (projected)"));
  auto y = std::make_unique<ui::views::Textfield>();
  y->set_text("0");
  y_field_ = y.get();
  add_child(std::move(y));

  auto scale = std::make_unique<ui::views::Label>("Scale ruler: 1:1");
  scale_label_ = scale.get();
  add_child(std::move(scale));

  auto apply = std::make_unique<ui::views::Button>("Apply");
  apply->set_click([this] { on_apply(); });
  add_child(std::move(apply));
}

void MapPrjXyPage::set_scale_ruler(long scale_ruler) {
  scale_ruler_ = (scale_ruler > 0) ? scale_ruler : 1;
  refresh_scale_label();
}

void MapPrjXyPage::refresh_scale_label() {
  if (!scale_label_) {
    return;
  }
  scale_label_->set_text("Scale ruler: 1:" + std::to_string(scale_ruler_));
}

void MapPrjXyPage::on_apply() {
  if (!host_) {
    return;
  }
  const double l = parse_field(l_field_, 120.0);
  const double b = parse_field(b_field_, 36.0);
  const double x = parse_field(x_field_, 0.0);
  const double y = parse_field(y_field_, 0.0);
  const std::string json =
      build_transform_xy_json(l, b, x, y, scale_ruler_);
  if (!host_->run_processing("proj.transform_xy", json)) {
    return;
  }
  const TransformXyOutput out = consume_transform_xy_output();
  if (out.valid) {
    if (x_field_) {
      x_field_->set_text(std::to_string(out.x));
    }
    if (y_field_) {
      y_field_->set_text(std::to_string(out.y));
    }
  }
}

}  // namespace plugin
