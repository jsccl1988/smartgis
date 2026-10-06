// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/stormsurge/views/run_dialog.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/catalog/resource_roots.h"
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

StormSurgeRunDialog::StormSurgeRunDialog(content::PluginHost* host)
    : host_(host) {
  constexpr int kW = 520;
  constexpr int kH = 520;
  set_preferred_size({kW, kH});

  ui::views::MarkupRoot loaded;
  const std::string markup =
      plugin::resolve_resource("smartgis.stormsurge", "run.ui.xml");
  if (!markup.empty()) {
    loaded = ui::views::load_markup(markup);
  }
  if (!loaded.ok()) {
    return;
  }

  dem_path_ = loaded.ids.find_as<ui::views::Textfield>("dem_path");
  coast_path_ = loaded.ids.find_as<ui::views::Textfield>("coast_path");
  tide_path_ = loaded.ids.find_as<ui::views::Textfield>("tide_path");
  output_path_ = loaded.ids.find_as<ui::views::Textfield>("output_path");
  seed_x_ = loaded.ids.find_as<ui::views::Textfield>("seed_x");
  seed_y_ = loaded.ids.find_as<ui::views::Textfield>("seed_y");
  tide_level_ = loaded.ids.find_as<ui::views::Textfield>("tide_level");
  frames_ = loaded.ids.find_as<ui::views::Textfield>("frames");
  frames_dir_ = loaded.ids.find_as<ui::views::Textfield>("frames_dir");
  depth_path_ = loaded.ids.find_as<ui::views::Textfield>("depth_path");
  stats_path_ = loaded.ids.find_as<ui::views::Textfield>("stats_path");
  impact_path_ = loaded.ids.find_as<ui::views::Textfield>("impact_path");

  if (auto* btn = loaded.ids.find_as<ui::views::Button>("browse_dem")) {
    btn->set_click([this] { on_pick_dem(); });
  }
  if (auto* btn = loaded.ids.find_as<ui::views::Button>("browse_coast")) {
    btn->set_click([this] { on_pick_coast(); });
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

  const char* sample_dir = base::switch_cstr("plugin-sample-dir");
  const std::string base =
      sample_dir && *sample_dir ? std::string(sample_dir) : "../data/plugin";
  if (dem_path_ && dem_path_->text().empty()) {
    dem_path_->set_text(base + "/stormsurge_dem_sample.tif");
  }
  if (coast_path_ && coast_path_->text().empty()) {
    coast_path_->set_text(base + "/stormsurge_coast_sample.geojson");
  }
  if (tide_path_ && tide_path_->text().empty()) {
    tide_path_->set_text(base + "/stormsurge_tide_sample.csv");
  }
  if (output_path_ && output_path_->text().empty()) {
    output_path_->set_text(base + "/stormsurge_mask.tif");
  }
  if (depth_path_ && depth_path_->text().empty()) {
    depth_path_->set_text(base + "/stormsurge_depth.tif");
  }
  if (stats_path_ && stats_path_->text().empty()) {
    stats_path_->set_text(base + "/stormsurge_stats.json");
  }
  if (impact_path_ && impact_path_->text().empty()) {
    impact_path_->set_text(base + "/stormsurge_impact_sample.geojson");
  }

  if (host_) {
    host_->present_dataset("smartgis.stormsurge",
                           coast_path_ ? coast_path_->text() : "", 1);
  }
}

void StormSurgeRunDialog::on_pick_dem() {
  const ui::views::FilePickerResult picked = ui::views::pick_open_file(
      L"DEM (*.tif;*.tiff;*.img;*.asc)\0"
      L"*.tif;*.tiff;*.img;*.asc\0All (*.*)\0*.*\0");
  if (!picked.accepted || !dem_path_) {
    return;
  }
  dem_path_->set_text(picked.path);
}

void StormSurgeRunDialog::on_pick_coast() {
  const ui::views::FilePickerResult picked = ui::views::pick_open_file(
      L"Coast (*.geojson;*.json;*.shp)\0"
      L"*.geojson;*.json;*.shp\0All (*.*)\0*.*\0");
  if (!picked.accepted || !coast_path_) {
    return;
  }
  coast_path_->set_text(picked.path);
}

void StormSurgeRunDialog::on_pick_output() {
  const ui::views::FilePickerResult picked =
      ui::views::pick_save_file(L"GeoTIFF (*.tif)\0*.tif\0");
  if (!picked.accepted || !output_path_) {
    return;
  }
  output_path_->set_text(picked.path);
}

std::string StormSurgeRunDialog::build_json() const {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("dem");
  w.String(dem_path_ ? dem_path_->text().c_str() : "");
  w.Key("coast");
  w.String(coast_path_ ? coast_path_->text().c_str() : "");
  if (tide_path_ && !tide_path_->text().empty()) {
    w.Key("tide");
    w.String(tide_path_->text().c_str());
  }
  w.Key("output");
  w.String(output_path_ ? output_path_->text().c_str() : "");
  if (depth_path_ && !depth_path_->text().empty()) {
    w.Key("depth_output");
    w.String(depth_path_->text().c_str());
  }
  if (stats_path_ && !stats_path_->text().empty()) {
    w.Key("stats_output");
    w.String(stats_path_->text().c_str());
  }
  if (impact_path_ && !impact_path_->text().empty()) {
    w.Key("impact");
    w.String(impact_path_->text().c_str());
  }
  w.Key("seed_x");
  w.Double(parse_double(seed_x_ ? seed_x_->text() : "", 0.0));
  w.Key("seed_y");
  w.Double(parse_double(seed_y_ ? seed_y_->text() : "", 0.0));
  w.Key("tide_level");
  w.Double(parse_double(tide_level_ ? tide_level_->text() : "", 36.0));
  w.Key("frames");
  w.Int(static_cast<int>(
      parse_double(frames_ ? frames_->text() : "", 12.0)));
  if (frames_dir_ && !frames_dir_->text().empty()) {
    w.Key("frames_dir");
    w.String(frames_dir_->text().c_str());
  }
  w.EndObject();
  return buf.GetString();
}

void StormSurgeRunDialog::on_ok() {
  if (!host_) {
    return;
  }
  const std::string coast =
      coast_path_ ? coast_path_->text() : std::string();
  if (!coast.empty()) {
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> w(buf);
    w.StartObject();
    w.Key("coast");
    w.String(coast.c_str());
    w.EndObject();
    host_->run_processing("stormsurge.load_coast", buf.GetString());
  }
  host_->run_processing("stormsurge.run", build_json());
}

}  // namespace plugin
