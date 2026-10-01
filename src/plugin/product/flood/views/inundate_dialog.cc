// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/flood/views/inundate_dialog.h"

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

InundateDialog::InundateDialog(content::PluginHost* host) : host_(host) {
  constexpr int kW = 520;
  constexpr int kH = 380;
  set_preferred_size({kW, kH});

  ui::views::MarkupRoot loaded;
  const std::string markup =
      plugin::resolve_resource("smartgis.flood", "inundate.ui.xml");
  if (!markup.empty()) {
    loaded = ui::views::load_markup(markup);
  }
  if (!loaded.ok()) {
    return;
  }

  dem_path_ = loaded.ids.find_as<ui::views::Textfield>("dem_path");
  output_path_ = loaded.ids.find_as<ui::views::Textfield>("output_path");
  seed_x_ = loaded.ids.find_as<ui::views::Textfield>("seed_x");
  seed_y_ = loaded.ids.find_as<ui::views::Textfield>("seed_y");
  water_level_ = loaded.ids.find_as<ui::views::Textfield>("water_level");
  water_depth_ = loaded.ids.find_as<ui::views::Textfield>("water_depth");
  frames_ = loaded.ids.find_as<ui::views::Textfield>("frames");
  frames_dir_ = loaded.ids.find_as<ui::views::Textfield>("frames_dir");

  if (auto* btn = loaded.ids.find_as<ui::views::Button>("browse_dem")) {
    btn->set_click([this] { on_pick_dem(); });
  }
  if (auto* btn = loaded.ids.find_as<ui::views::Button>("browse_output")) {
    btn->set_click([this] { on_pick_output(); });
  }
  // frames_dir is typed; no folder picker in Views yet.
  if (auto* btn = loaded.ids.find_as<ui::views::Button>("ok")) {
    btn->set_click([this] { on_ok(); });
  }

  set_layout_manager(std::make_unique<ui::views::FillLayout>());
  loaded.root->set_preferred_size({kW, kH});
  add_child(std::move(loaded.root));

  const char* sample_dir = std::getenv("SMT_PLUGIN_SAMPLE_DIR");
  const std::string base =
      sample_dir && *sample_dir ? std::string(sample_dir) : "../data/plugin";
  if (dem_path_ && dem_path_->text().empty()) {
    dem_path_->set_text(base + "/flood_basin_sample.tif");
  }
  if (output_path_ && output_path_->text().empty()) {
    output_path_->set_text(base + "/flood_mask.tif");
  }
}

void InundateDialog::on_pick_dem() {
  const ui::views::FilePickerResult picked = ui::views::pick_open_file(
      L"DEM (*.tif;*.tiff;*.img;*.asc)\0"
      L"*.tif;*.tiff;*.img;*.asc\0All (*.*)\0*.*\0");
  if (!picked.accepted || !dem_path_) {
    return;
  }
  dem_path_->set_text(picked.path);
}

void InundateDialog::on_pick_output() {
  const ui::views::FilePickerResult picked =
      ui::views::pick_save_file(L"GeoTIFF (*.tif)\0*.tif\0");
  if (!picked.accepted || !output_path_) {
    return;
  }
  output_path_->set_text(picked.path);
}

std::string InundateDialog::build_json() const {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("dem");
  w.String(dem_path_ ? dem_path_->text().c_str() : "");
  w.Key("output");
  w.String(output_path_ ? output_path_->text().c_str() : "");
  w.Key("seed_x");
  w.Double(parse_double(seed_x_ ? seed_x_->text() : "", 0.0));
  w.Key("seed_y");
  w.Double(parse_double(seed_y_ ? seed_y_->text() : "", 0.0));
  const std::string depth =
      water_depth_ ? water_depth_->text() : std::string();
  if (!depth.empty()) {
    w.Key("water_depth");
    w.Double(parse_double(depth, 0.0));
  } else {
    w.Key("water_level");
    w.Double(parse_double(water_level_ ? water_level_->text() : "", 100.0));
  }
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

void InundateDialog::on_ok() {
  if (!host_) {
    return;
  }
  host_->run_processing("flood.inundate", build_json());
}

}  // namespace plugin
