// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/proj/map_prj_grid_page.h"

#include <cstdlib>
#include <memory>
#include <string>

#include "content/public/plugin_host.h"
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

long parse_long_field(const ui::views::Textfield* field, long fallback) {
  if (!field) {
    return fallback;
  }
  const char* start = field->text().c_str();
  char* end = nullptr;
  const long v = std::strtol(start, &end, 10);
  if (end == start) {
    return fallback;
  }
  return v;
}

std::string build_transform_grid_json(double dl, double db, double lmin,
                                      double bmin, double lmax, double bmax,
                                      long scale_ruler) {
  return std::string("{\"dL\":") + std::to_string(dl) + ",\"dB\":" +
         std::to_string(db) + ",\"Lmin\":" + std::to_string(lmin) +
         ",\"Bmin\":" + std::to_string(bmin) + ",\"Lmax\":" +
         std::to_string(lmax) + ",\"Bmax\":" + std::to_string(bmax) +
         ",\"scale_ruler\":" + std::to_string(scale_ruler) + "}";
}

}  // namespace

MapPrjGridPage::MapPrjGridPage(content::PluginHost* host) : host_(host) {
  set_preferred_size({480, 320});
  auto layout = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kVertical);
  set_layout_manager(std::move(layout));

  add_child(std::make_unique<ui::views::Label>("dL (lon step)"));
  auto dl = std::make_unique<ui::views::Textfield>();
  dl->set_text("0.125");
  dl_field_ = dl.get();
  add_child(std::move(dl));

  add_child(std::make_unique<ui::views::Label>("dB (lat step)"));
  auto db = std::make_unique<ui::views::Textfield>();
  db->set_text("0.08333333333333333");
  db_field_ = db.get();
  add_child(std::move(db));

  add_child(std::make_unique<ui::views::Label>("Lmin"));
  auto lmin = std::make_unique<ui::views::Textfield>();
  lmin->set_text("117");
  lmin_field_ = lmin.get();
  add_child(std::move(lmin));

  add_child(std::make_unique<ui::views::Label>("Bmin"));
  auto bmin = std::make_unique<ui::views::Textfield>();
  bmin->set_text("36");
  bmin_field_ = bmin.get();
  add_child(std::move(bmin));

  add_child(std::make_unique<ui::views::Label>("Lmax"));
  auto lmax = std::make_unique<ui::views::Textfield>();
  lmax->set_text("118");
  lmax_field_ = lmax.get();
  add_child(std::move(lmax));

  add_child(std::make_unique<ui::views::Label>("Bmax"));
  auto bmax = std::make_unique<ui::views::Textfield>();
  bmax->set_text("37");
  bmax_field_ = bmax.get();
  add_child(std::move(bmax));

  add_child(std::make_unique<ui::views::Label>("Scale ruler (1:N)"));
  auto scale = std::make_unique<ui::views::Textfield>();
  scale->set_text("10000");
  scale_field_ = scale.get();
  add_child(std::move(scale));

  auto apply = std::make_unique<ui::views::Button>("Apply");
  apply->set_click([this] { on_apply(); });
  add_child(std::move(apply));
}

long MapPrjGridPage::scale_ruler() const {
  return parse_long_field(scale_field_, 10000);
}

void MapPrjGridPage::on_apply() {
  if (!host_) {
    return;
  }
  const double dl = parse_field(dl_field_, 0.125);
  const double db = parse_field(db_field_, 1.0 / 12.0);
  const double lmin = parse_field(lmin_field_, 117.0);
  const double bmin = parse_field(bmin_field_, 36.0);
  const double lmax = parse_field(lmax_field_, 118.0);
  const double bmax = parse_field(bmax_field_, 37.0);
  const long scale = scale_ruler();
  const std::string json = build_transform_grid_json(
      dl, db, lmin, bmin, lmax, bmax, scale);
  host_->run_processing("proj.transform_grid", json);
}

}  // namespace plugin
