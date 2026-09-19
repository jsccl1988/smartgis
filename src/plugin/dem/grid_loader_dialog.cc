// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/dem/grid_loader_dialog.h"

#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>

#include "content/public/plugin_host.h"
#include "ui/views/primitives/button.h"
#include "ui/views/primitives/checkbox.h"
#include "ui/views/primitives/combobox.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/primitives/label.h"
#include "ui/views/primitives/textfield.h"

namespace plugin {
namespace {

std::string json_escape(std::string_view s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

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
  set_preferred_size({480, 420});

  add_child(std::make_unique<ui::views::Label>("Heightmap file"));
  {
    auto field = std::make_unique<ui::views::Textfield>();
    heightmap_path_ = field.get();
    add_child(std::move(field));
  }
  {
    auto btn = std::make_unique<ui::views::Button>("Browse...");
    btn->set_click([this] { on_pick_heightmap(); });
    add_child(std::move(btn));
  }

  add_child(std::make_unique<ui::views::Label>("X scale"));
  {
    auto field = std::make_unique<ui::views::Textfield>();
    field->set_text("1");
    x_scale_ = field.get();
    add_child(std::move(field));
  }
  add_child(std::make_unique<ui::views::Label>("Y scale"));
  {
    auto field = std::make_unique<ui::views::Textfield>();
    field->set_text("1");
    y_scale_ = field.get();
    add_child(std::move(field));
  }
  add_child(std::make_unique<ui::views::Label>("Z scale"));
  {
    auto field = std::make_unique<ui::views::Textfield>();
    field->set_text("0.1");
    z_scale_ = field.get();
    add_child(std::move(field));
  }

  add_child(std::make_unique<ui::views::Label>("X start"));
  {
    auto field = std::make_unique<ui::views::Textfield>();
    field->set_text("0");
    x_start_ = field.get();
    add_child(std::move(field));
  }
  add_child(std::make_unique<ui::views::Label>("Y start"));
  {
    auto field = std::make_unique<ui::views::Textfield>();
    field->set_text("0");
    y_start_ = field.get();
    add_child(std::move(field));
  }
  add_child(std::make_unique<ui::views::Label>("Z start"));
  {
    auto field = std::make_unique<ui::views::Textfield>();
    field->set_text("0");
    z_start_ = field.get();
    add_child(std::move(field));
  }

  {
    auto ck = std::make_unique<ui::views::Checkbox>("Use texture");
    use_texture_ = ck.get();
    add_child(std::move(ck));
  }
  {
    auto field = std::make_unique<ui::views::Textfield>();
    texture_path_ = field.get();
    add_child(std::move(field));
  }
  {
    auto btn = std::make_unique<ui::views::Button>("Texture...");
    btn->set_click([this] { on_pick_texture(); });
    add_child(std::move(btn));
  }

  add_child(std::make_unique<ui::views::Label>("Color type"));
  {
    auto cmb = std::make_unique<ui::views::Combobox>();
    cmb->add_item("none");
    cmb->add_item("equal_interval");
    cmb->add_item("three_band");
    cmb->set_selected_index(1);
    color_type_ = cmb.get();
    add_child(std::move(cmb));
  }

  {
    auto ok = std::make_unique<ui::views::Button>("OK");
    ok->set_click([this] { on_ok(); });
    add_child(std::move(ok));
  }
  add_child(std::make_unique<ui::views::Button>("Cancel"));
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
  std::ostringstream json;
  json << '{';
  json << "\"heightmap_path\":\""
       << json_escape(heightmap_path_ ? heightmap_path_->text() : "") << '"';
  json << ",\"x_scale\":" << parse_float(x_scale_ ? x_scale_->text() : "", 1.f);
  json << ",\"y_scale\":" << parse_float(y_scale_ ? y_scale_->text() : "", 1.f);
  json << ",\"z_scale\":" << parse_float(z_scale_ ? z_scale_->text() : "", 0.1f);
  json << ",\"x_start\":" << parse_float(x_start_ ? x_start_->text() : "", 0.f);
  json << ",\"y_start\":" << parse_float(y_start_ ? y_start_->text() : "", 0.f);
  json << ",\"z_start\":" << parse_float(z_start_ ? z_start_->text() : "", 0.f);
  json << ",\"use_texture\":"
       << ((use_texture_ && use_texture_->is_checked()) ? "true" : "false");
  json << ",\"texture_path\":\""
       << json_escape(texture_path_ ? texture_path_->text() : "") << '"';
  json << ",\"color_type\":" << color_type_value();
  json << ",\"color_type_label\":\""
       << json_escape(color_type_ ? color_type_->selected_text() : "") << '"';
  json << '}';
  return json.str();
}

void GridLoaderDialog::on_ok() {
  if (!host_) {
    return;
  }
  host_->run_processing("dem.grid_from_heightmap", build_json());
}

}  // namespace plugin
