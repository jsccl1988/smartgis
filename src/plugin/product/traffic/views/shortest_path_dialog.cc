// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/traffic/views/shortest_path_dialog.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/resources/resource_roots.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/text/textfield.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include "base/process/switches.h"

namespace plugin {
namespace {

double parse_double(const std::string& text, double fallback) {
  if (text.empty()) {
    return fallback;
  }
  char* end = nullptr;
  const double v = std::strtod(text.c_str(), &end);
  return end != text.c_str() ? v : fallback;
}

}  // namespace

ShortestPathDialog::ShortestPathDialog(content::PluginHost* host)
    : host_(host) {
  constexpr int kW = 520;
  constexpr int kH = 360;
  set_preferred_size({kW, kH});

  ui::views::MarkupRoot loaded;
  const std::string markup =
      plugin::resolve_resource("smartgis.traffic", "shortest_path.ui.xml");
  if (!markup.empty()) {
    loaded = ui::views::load_markup(markup);
  }
  if (!loaded.ok()) {
    return;
  }

  network_path_ = loaded.ids.find_as<ui::views::Textfield>("network_path");
  output_path_ = loaded.ids.find_as<ui::views::Textfield>("output_path");
  start_x_ = loaded.ids.find_as<ui::views::Textfield>("start_x");
  start_y_ = loaded.ids.find_as<ui::views::Textfield>("start_y");
  end_x_ = loaded.ids.find_as<ui::views::Textfield>("end_x");
  end_y_ = loaded.ids.find_as<ui::views::Textfield>("end_y");
  weight_field_ = loaded.ids.find_as<ui::views::Textfield>("weight_field");
  frames_ = loaded.ids.find_as<ui::views::Textfield>("frames");

  if (auto* btn = loaded.ids.find_as<ui::views::Button>("browse_network")) {
    btn->set_click([this] { on_pick_network(); });
  }
  if (auto* btn = loaded.ids.find_as<ui::views::Button>("browse_output")) {
    btn->set_click([this] { on_pick_output(); });
  }
  if (auto* btn = loaded.ids.find_as<ui::views::Button>("ok")) {
    btn->set_click([this] { on_ok(); });
  }

  set_layout_manager(std::make_unique<ui::views::FillLayout>());
  loaded.root->set_preferred_size({kW, kH});
  add_child(std::move(loaded.root));

  // Sample import defaults.
  const char* sample_dir = base::switch_cstr("plugin-sample-dir");
  const std::string base =
      sample_dir && *sample_dir ? std::string(sample_dir) : "../data/plugin";
  if (network_path_ && network_path_->text().empty()) {
    network_path_->set_text(base + "/traffic_network_sample.geojson");
  }
  if (output_path_ && output_path_->text().empty()) {
    output_path_->set_text(base + "/traffic_path.geojson");
  }
  if (start_x_ && start_x_->text().empty()) {
    start_x_->set_text("116.35");
  }
  if (start_y_ && start_y_->text().empty()) {
    start_y_->set_text("39.90");
  }
  if (end_x_ && end_x_->text().empty()) {
    end_x_->set_text("116.45");
  }
  if (end_y_ && end_y_->text().empty()) {
    end_y_->set_text("39.95");
  }

  if (host_ && network_path_ && !network_path_->text().empty()) {
    host_->present_dataset("smartgis.traffic", network_path_->text(), 0);
  }
}

void ShortestPathDialog::on_pick_network() {
  const ui::views::FilePickerResult picked = ui::views::pick_open_file(
      L"Network (*.shp;*.geojson;*.json)\0"
      L"*.shp;*.geojson;*.json\0All (*.*)\0*.*\0");
  if (!picked.accepted || !network_path_) {
    return;
  }
  network_path_->set_text(picked.path);
}

void ShortestPathDialog::on_pick_output() {
  const ui::views::FilePickerResult picked =
      ui::views::pick_save_file(L"GeoJSON (*.geojson)\0*.geojson\0");
  if (!picked.accepted || !output_path_) {
    return;
  }
  output_path_->set_text(picked.path);
}

std::string ShortestPathDialog::build_json() const {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("network");
  w.String(network_path_ ? network_path_->text().c_str() : "");
  w.Key("output");
  w.String(output_path_ ? output_path_->text().c_str() : "");
  w.Key("start_x");
  w.Double(parse_double(start_x_ ? start_x_->text() : "", 0.0));
  w.Key("start_y");
  w.Double(parse_double(start_y_ ? start_y_->text() : "", 0.0));
  w.Key("end_x");
  w.Double(parse_double(end_x_ ? end_x_->text() : "", 1.0));
  w.Key("end_y");
  w.Double(parse_double(end_y_ ? end_y_->text() : "", 1.0));
  if (weight_field_ && !weight_field_->text().empty()) {
    w.Key("weight_field");
    w.String(weight_field_->text().c_str());
  }
  w.Key("frames");
  w.Int(static_cast<int>(
      parse_double(frames_ ? frames_->text() : "", 24.0)));
  w.EndObject();
  return buf.GetString();
}

void ShortestPathDialog::on_ok() {
  if (!host_) {
    return;
  }
  host_->run_processing("traffic.cost_path", build_json());
}

}  // namespace plugin
