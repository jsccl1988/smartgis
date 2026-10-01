// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/views/grid_loader_dialog.h"

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
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/input/combobox.h"
#include "ui/views/primitives/text/textfield.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace plugin {
namespace {

float parse_float(const std::string& text, float fallback) {
  if (text.empty()) {
    return fallback;
  }
  char* end = nullptr;
  const float v = std::strtof(text.c_str(), &end);
  return end != text.c_str() ? v : fallback;
}

}  // namespace

GridLoaderDialog::GridLoaderDialog(content::PluginHost* host) : host_(host) {
  constexpr int kW = 480;
  constexpr int kH = 420;
  set_preferred_size({kW, kH});

  ui::views::MarkupRoot loaded;
  const std::string markup =
      plugin::resolve_resource("smartgis.world3d", "grid_loader.ui.xml");
  if (!markup.empty()) {
    loaded = ui::views::load_markup(markup);
  }
  if (!loaded.ok()) {
    return;
  }

  heightmap_path_ = loaded.ids.find_as<ui::views::Textfield>("heightmap_path");
  x_scale_ = loaded.ids.find_as<ui::views::Textfield>("x_scale");
  y_scale_ = loaded.ids.find_as<ui::views::Textfield>("y_scale");
  z_scale_ = loaded.ids.find_as<ui::views::Textfield>("z_scale");
  x_start_ = loaded.ids.find_as<ui::views::Textfield>("x_start");
  y_start_ = loaded.ids.find_as<ui::views::Textfield>("y_start");
  z_start_ = loaded.ids.find_as<ui::views::Textfield>("z_start");
  use_texture_ = loaded.ids.find_as<ui::views::Checkbox>("use_texture");
  texture_path_ = loaded.ids.find_as<ui::views::Textfield>("texture_path");
  color_type_ = loaded.ids.find_as<ui::views::Combobox>("color_type");

  if (auto* btn = loaded.ids.find_as<ui::views::Button>("browse_heightmap")) {
    btn->set_click([this] { on_pick_heightmap(); });
  }
  if (auto* btn = loaded.ids.find_as<ui::views::Button>("browse_texture")) {
    btn->set_click([this] { on_pick_texture(); });
  }
  if (auto* btn = loaded.ids.find_as<ui::views::Button>("ok")) {
    btn->set_click([this] { on_ok(); });
  }
  if (color_type_ && color_type_->item_count() > 1) {
    color_type_->set_selected_index(1);
  }

  set_layout_manager(std::make_unique<ui::views::FillLayout>());
  loaded.root->set_preferred_size({kW, kH});
  add_child(std::move(loaded.root));
}

void GridLoaderDialog::on_pick_heightmap() {
  const ui::views::FilePickerResult picked = ui::views::pick_open_file(
      L"Raster (*.bmp;*.tif;*.tiff;*.img;*.asc)\0"
      L"*.bmp;*.tif;*.tiff;*.img;*.asc\0All (*.*)\0*.*\0");
  if (!picked.accepted) {
    return;
  }
  if (heightmap_path_) {
    heightmap_path_->set_text(picked.path);
  }
}

void GridLoaderDialog::on_pick_texture() {
  const ui::views::FilePickerResult picked =
      ui::views::pick_open_file(L"Bitmap (*.bmp)\0*.bmp\0");
  if (!picked.accepted) {
    return;
  }
  if (texture_path_) {
    texture_path_->set_text(picked.path);
  }
  if (use_texture_) {
    use_texture_->set_checked(true);
  }
}

int GridLoaderDialog::color_type_value() const {
  if (!color_type_) {
    return 2;
  }
  const std::string& label = color_type_->selected_text();
  if (label == "none") {
    return 1;
  }
  if (label == "three_band") {
    return 3;
  }
  return 2;
}

std::string GridLoaderDialog::build_json() const {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  const std::string heightmap =
      heightmap_path_ ? heightmap_path_->text() : std::string();
  w.Key("heightmap_path");
  w.String(heightmap.c_str(),
           static_cast<rapidjson::SizeType>(heightmap.size()));
  w.Key("x_scale");
  w.Double(parse_float(x_scale_ ? x_scale_->text() : "", 1.f));
  w.Key("y_scale");
  w.Double(parse_float(y_scale_ ? y_scale_->text() : "", 1.f));
  w.Key("z_scale");
  w.Double(parse_float(z_scale_ ? z_scale_->text() : "", 0.1f));
  w.Key("x_start");
  w.Double(parse_float(x_start_ ? x_start_->text() : "", 0.f));
  w.Key("y_start");
  w.Double(parse_float(y_start_ ? y_start_->text() : "", 0.f));
  w.Key("z_start");
  w.Double(parse_float(z_start_ ? z_start_->text() : "", 0.f));
  w.Key("use_texture");
  w.Bool(use_texture_ && use_texture_->is_checked());
  const std::string texture =
      texture_path_ ? texture_path_->text() : std::string();
  w.Key("texture_path");
  w.String(texture.c_str(), static_cast<rapidjson::SizeType>(texture.size()));
  w.Key("color_type");
  w.Int(color_type_value());
  const std::string color_label =
      color_type_ ? color_type_->selected_text() : std::string();
  w.Key("color_type_label");
  w.String(color_label.c_str(),
           static_cast<rapidjson::SizeType>(color_label.size()));
  w.EndObject();
  return std::string(buf.GetString(), buf.GetSize());
}

void GridLoaderDialog::on_ok() {
  if (!host_) {
    return;
  }
  host_->run_processing("world3d.grid_from_heightmap", build_json());
}

}  // namespace plugin
