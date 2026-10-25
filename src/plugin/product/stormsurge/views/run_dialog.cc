// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/stormsurge/views/run_dialog.h"

#include <memory>
#include <string>
#include <utility>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/catalog/resource_roots.h"
#include "plugin/runtime/widgets/dialog_fields.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/text/textfield.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace plugin {

StormSurgeRunDialog::StormSurgeRunDialog(content::PluginHost* host)
    : host_(host) {
  constexpr int kW = 520;
  constexpr int kH = 520;
  set_preferred_size({kW, kH});

  const std::string markup =
      plugin::resolve_resource("smartgis.stormsurge", "run.ui.xml");
  ui::views::MarkupRoot* loaded = load_dialog_markup(markup);
  if (!loaded) {
    return;
  }

  dem_path_ = loaded->ids.find_as<ui::views::Textfield>("dem_path");
  coast_path_ = loaded->ids.find_as<ui::views::Textfield>("coast_path");
  tide_path_ = loaded->ids.find_as<ui::views::Textfield>("tide_path");
  output_path_ = loaded->ids.find_as<ui::views::Textfield>("output_path");
  seed_x_ = loaded->ids.find_as<ui::views::Textfield>("seed_x");
  seed_y_ = loaded->ids.find_as<ui::views::Textfield>("seed_y");
  tide_level_ = loaded->ids.find_as<ui::views::Textfield>("tide_level");
  frames_ = loaded->ids.find_as<ui::views::Textfield>("frames");
  frames_dir_ = loaded->ids.find_as<ui::views::Textfield>("frames_dir");
  depth_path_ = loaded->ids.find_as<ui::views::Textfield>("depth_path");
  stats_path_ = loaded->ids.find_as<ui::views::Textfield>("stats_path");
  impact_path_ = loaded->ids.find_as<ui::views::Textfield>("impact_path");

  if (auto* btn = loaded->ids.find_as<ui::views::Button>("browse_dem")) {
    btn->set_click([this] { on_pick_dem(); });
  }
  if (auto* btn = loaded->ids.find_as<ui::views::Button>("browse_coast")) {
    btn->set_click([this] { on_pick_coast(); });
  }
  if (auto* btn = loaded->ids.find_as<ui::views::Button>("browse_output")) {
    btn->set_click([this] { on_pick_output(); });
  }
  if (auto* btn = loaded->ids.find_as<ui::views::Button>("ok")) {
    btn->set_click([this] { on_ok(); });
  }

  ui::views::View* body = take_dialog_markup_root(loaded);
  if (!body) {
    return;
  }
  body->set_preferred_size({kW, kH});
  ui::views::install_fill_layout(this);
  ui::views::view_adopt_child(this, body);

  const std::string base = plugin_sample_dir();
  set_textfield_if_empty(dem_path_, base + "/stormsurge_dem_sample.tif");
  set_textfield_if_empty(coast_path_, base + "/stormsurge_coast_sample.geojson");
  set_textfield_if_empty(tide_path_, base + "/stormsurge_tide_sample.csv");
  set_textfield_if_empty(output_path_, base + "/stormsurge_mask.tif");
  set_textfield_if_empty(depth_path_, base + "/stormsurge_depth.tif");
  set_textfield_if_empty(stats_path_, base + "/stormsurge_stats.json");
  set_textfield_if_empty(impact_path_, base + "/stormsurge_impact_sample.geojson");

  if (host_) {
    host_->present_dataset("smartgis.stormsurge", textfield_utf8(coast_path_),
                           1);
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
  const std::string dem = textfield_utf8(dem_path_);
  const std::string coast = textfield_utf8(coast_path_);
  const std::string tide = textfield_utf8(tide_path_);
  const std::string output = textfield_utf8(output_path_);
  const std::string depth = textfield_utf8(depth_path_);
  const std::string stats = textfield_utf8(stats_path_);
  const std::string impact = textfield_utf8(impact_path_);
  const std::string frames_dir = textfield_utf8(frames_dir_);
  w.Key("dem");
  w.String(dem.c_str());
  w.Key("coast");
  w.String(coast.c_str());
  if (!tide.empty()) {
    w.Key("tide");
    w.String(tide.c_str());
  }
  w.Key("output");
  w.String(output.c_str());
  if (!depth.empty()) {
    w.Key("depth_output");
    w.String(depth.c_str());
  }
  if (!stats.empty()) {
    w.Key("stats_output");
    w.String(stats.c_str());
  }
  if (!impact.empty()) {
    w.Key("impact");
    w.String(impact.c_str());
  }
  w.Key("seed_x");
  w.Double(parse_textfield_double(seed_x_, 0.0));
  w.Key("seed_y");
  w.Double(parse_textfield_double(seed_y_, 0.0));
  w.Key("tide_level");
  w.Double(parse_textfield_double(tide_level_, 36.0));
  w.Key("frames");
  w.Int(static_cast<int>(parse_textfield_double(frames_, 12.0)));
  if (!frames_dir.empty()) {
    w.Key("frames_dir");
    w.String(frames_dir.c_str());
  }
  w.EndObject();
  return buf.GetString();
}

void StormSurgeRunDialog::on_ok() {
  if (!host_) {
    return;
  }
  const std::string coast = textfield_utf8(coast_path_);
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
