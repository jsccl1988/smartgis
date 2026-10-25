// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/flood/views/inundate_dialog.h"

#include <string>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/catalog/resource_roots.h"
#include "plugin/runtime/widgets/dialog_fields.h"
#include "plugin/runtime/widgets/present_surface_picker.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/text/textfield.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace plugin {

InundateDialog::InundateDialog(content::PluginHost* host) : host_(host) {
  constexpr int kW = 520;
  constexpr int kH = 380;
  set_preferred_size({kW, kH});

  const std::string markup =
      plugin::resolve_resource("smartgis.flood", "inundate.ui.xml");
  ui::views::MarkupRoot* loaded = load_dialog_markup(markup);
  if (!loaded) {
    return;
  }

  dem_path_ = loaded->ids.find_as<ui::views::Textfield>("dem_path");
  output_path_ = loaded->ids.find_as<ui::views::Textfield>("output_path");
  seed_x_ = loaded->ids.find_as<ui::views::Textfield>("seed_x");
  seed_y_ = loaded->ids.find_as<ui::views::Textfield>("seed_y");
  water_level_ = loaded->ids.find_as<ui::views::Textfield>("water_level");
  water_depth_ = loaded->ids.find_as<ui::views::Textfield>("water_depth");
  frames_ = loaded->ids.find_as<ui::views::Textfield>("frames");
  frames_dir_ = loaded->ids.find_as<ui::views::Textfield>("frames_dir");

  if (auto* btn = loaded->ids.find_as<ui::views::Button>("browse_dem")) {
    btn->set_click([this] { on_pick_dem(); });
  }
  if (auto* btn = loaded->ids.find_as<ui::views::Button>("browse_output")) {
    btn->set_click([this] { on_pick_output(); });
  }
  // frames_dir is typed; no folder picker in Views yet.
  if (auto* btn = loaded->ids.find_as<ui::views::Button>("ok")) {
    btn->set_click([this] { on_ok(); });
  }

  const std::string base = plugin_sample_dir();
  sample_path_ = base + "/flood_valley.geojson";
  set_textfield_if_empty(dem_path_, base + "/flood_basin_sample.tif");
  set_textfield_if_empty(output_path_, base + "/flood_mask.tif");

  ui::views::View* body = take_dialog_markup_root(loaded);
  if (!body) {
    return;
  }
  body->set_preferred_size({kW, kH});

  ui::views::BoxLayout* box = ui::views::install_box_layout(
      this, ui::views::BoxLayout::Orientation::kVertical);
  auto* picker = new PresentSurfacePicker(host_);
  picker->set_on_surface_change([this](int) { present_sample(); });
  if (box) {
    box->set_flex_for_view(body, 1);
  }
  ui::views::view_adopt_child(this, picker);
  ui::views::view_adopt_child(this, body);
  set_preferred_size({kW, kH + 32});

  // Defer GeoJSON present until after the dialog tree is live. Calling
  // present_sample() mid-ctor raced map rebuild with markup teardown and
  // made HEAP_CORRUPTION look like a PROJ/GDAL fault.
}

void InundateDialog::present_sample() {
  if (host_) {
    host_->present_dataset("smartgis.flood", sample_path_, 0);
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
  w.Double(parse_field_double(seed_x_ ? seed_x_->text() : "", 0.0));
  w.Key("seed_y");
  w.Double(parse_field_double(seed_y_ ? seed_y_->text() : "", 0.0));
  const std::string depth =
      water_depth_ ? water_depth_->text() : std::string();
  if (!depth.empty()) {
    w.Key("water_depth");
    w.Double(parse_field_double(depth, 0.0));
  } else {
    w.Key("water_level");
    w.Double(parse_field_double(water_level_ ? water_level_->text() : "", 100.0));
  }
  w.Key("frames");
  w.Int(static_cast<int>(
      parse_field_double(frames_ ? frames_->text() : "", 12.0)));
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
