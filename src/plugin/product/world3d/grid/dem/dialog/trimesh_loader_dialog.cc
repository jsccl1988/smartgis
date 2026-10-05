// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/grid/dem/dialog/trimesh_loader_dialog.h"

#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/resources/resource_roots.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/message_box.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/button/radio_button.h"
#include "ui/views/primitives/collection/table_view.h"
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

int parse_int(const std::string& text, int fallback) {
  if (text.empty()) {
    return fallback;
  }
  char* end = nullptr;
  const long v = std::strtol(text.c_str(), &end, 10);
  return end != text.c_str() ? static_cast<int>(v) : fallback;
}

void split_line(std::string_view line, char sep, std::vector<std::string>* out) {
  out->clear();
  std::string field;
  for (char c : line) {
    if (c == sep) {
      out->push_back(field);
      field.clear();
    } else {
      field += c;
    }
  }
  out->push_back(field);
}

void populate_column_combos(ui::views::Combobox* x,
                            ui::views::Combobox* y,
                            ui::views::Combobox* z,
                            int col_count) {
  if (!x || !y || !z || col_count < 1) {
    return;
  }
  x->clear_items();
  y->clear_items();
  z->clear_items();
  for (int i = 0; i < col_count; ++i) {
    const std::string label = "Col " + std::to_string(i + 1);
    x->add_item(label);
    y->add_item(label);
    z->add_item(label);
  }
  x->set_selected_index(0);
  y->set_selected_index(col_count > 1 ? 1 : 0);
  z->set_selected_index(col_count > 2 ? 2 : 0);
}

}  // namespace

TrimeshLoaderDialog::TrimeshLoaderDialog(content::PluginHost* host) : host_(host) {
  constexpr int kW = 640;
  constexpr int kH = 520;
  set_preferred_size({kW, kH});

  ui::views::MarkupRoot loaded;
  const std::string markup =
      plugin::resolve_resource("smartgis.world3d", "trimesh_loader.ui.xml");
  if (!markup.empty()) {
    loaded = ui::views::load_markup(markup);
  }
  if (!loaded.ok()) {
    return;
  }

  vertex_path_ = loaded.ids.find_as<ui::views::Textfield>("vertex_path");
  radio_tab_ = loaded.ids.find_as<ui::views::RadioButton>("radio_tab");
  radio_space_ = loaded.ids.find_as<ui::views::RadioButton>("radio_space");
  radio_comma_ = loaded.ids.find_as<ui::views::RadioButton>("radio_comma");
  head_skip_ = loaded.ids.find_as<ui::views::Textfield>("head_skip");
  line_skip_ = loaded.ids.find_as<ui::views::Textfield>("line_skip");
  col_x_ = loaded.ids.find_as<ui::views::Combobox>("col_x");
  col_y_ = loaded.ids.find_as<ui::views::Combobox>("col_y");
  col_z_ = loaded.ids.find_as<ui::views::Combobox>("col_z");
  x_scale_ = loaded.ids.find_as<ui::views::Textfield>("x_scale");
  y_scale_ = loaded.ids.find_as<ui::views::Textfield>("y_scale");
  z_scale_ = loaded.ids.find_as<ui::views::Textfield>("z_scale");
  use_texture_ = loaded.ids.find_as<ui::views::Checkbox>("use_texture");
  texture_path_ = loaded.ids.find_as<ui::views::Textfield>("texture_path");
  gen_2d_trimesh_ = loaded.ids.find_as<ui::views::Checkbox>("gen_2d_trimesh");
  trimesh_layer_ = loaded.ids.find_as<ui::views::Combobox>("trimesh_layer");
  preview_ = loaded.ids.find_as<ui::views::TableView>("preview");

  if (auto* btn = loaded.ids.find_as<ui::views::Button>("browse_vertex")) {
    btn->set_click([this] { on_pick_vertex(); });
  }
  if (auto* btn = loaded.ids.find_as<ui::views::Button>("browse_texture")) {
    btn->set_click([this] { on_pick_texture(); });
  }
  if (auto* btn = loaded.ids.find_as<ui::views::Button>("ok")) {
    btn->set_click([this] { on_ok(); });
  }
  if (radio_comma_ && !radio_comma_->is_selected()) {
    radio_comma_->set_selected(true);
  }

  set_layout_manager(std::make_unique<ui::views::FillLayout>());
  loaded.root->set_preferred_size({kW, kH});
  add_child(std::move(loaded.root));

  if (host_) {
    host_->present_dataset("smartgis.world3d", "", 1);
  }
}

void TrimeshLoaderDialog::on_pick_vertex() {
  const ui::views::FilePickerResult picked = ui::views::pick_open_file(
      L"Data (*.dat;*.txt)\0*.dat;*.txt\0All (*.*)\0*.*\0");
  if (!picked.accepted) {
    return;
  }
  if (vertex_path_) {
    vertex_path_->set_text(picked.path);
  }
  update_preview();
}

void TrimeshLoaderDialog::on_pick_texture() {
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

void TrimeshLoaderDialog::update_preview() {
  if (!preview_ || !vertex_path_) {
    return;
  }
  preview_->clear_rows();

  std::ifstream fin(vertex_path_->text());
  if (!fin.is_open()) {
    return;
  }

  char sep = ',';
  if (radio_tab_ && radio_tab_->is_selected()) {
    sep = '\t';
  } else if (radio_space_ && radio_space_->is_selected()) {
    sep = ' ';
  }

  std::vector<std::string> lines;
  std::string line;
  for (int i = 0; i < 3 && std::getline(fin, line); ++i) {
    lines.push_back(line);
  }

  int col_count = 0;
  std::vector<std::string> fields;
  if (!lines.empty()) {
    split_line(lines[0], sep, &fields);
    col_count = static_cast<int>(fields.size());
  }

  if (col_count >= 3) {
    populate_column_combos(col_x_, col_y_, col_z_, col_count);
  }

  std::vector<std::string> columns;
  columns.push_back("Row");
  for (int i = 0; i < col_count; ++i) {
    columns.push_back("Col " + std::to_string(i + 1));
  }
  preview_->set_columns(columns);

  for (size_t row = 0; row < lines.size(); ++row) {
    split_line(lines[row], sep, &fields);
    std::vector<std::string> cells;
    cells.push_back(std::to_string(row + 1));
    for (const auto& f : fields) {
      cells.push_back(f);
    }
    preview_->add_row(cells);
  }
}

std::string TrimeshLoaderDialog::separator_name() const {
  if (radio_tab_ && radio_tab_->is_selected()) {
    return "tab";
  }
  if (radio_space_ && radio_space_->is_selected()) {
    return "space";
  }
  return "comma";
}

std::string TrimeshLoaderDialog::build_json() const {
  const int ix = col_x_ ? col_x_->selected_index() : 0;
  const int iy = col_y_ ? col_y_->selected_index() : 1;
  const int iz = col_z_ ? col_z_->selected_index() : 2;

  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  const std::string vertex =
      vertex_path_ ? vertex_path_->text() : std::string();
  w.Key("vertex_path");
  w.String(vertex.c_str(), static_cast<rapidjson::SizeType>(vertex.size()));
  const std::string sep = separator_name();
  w.Key("separator");
  w.String(sep.c_str(), static_cast<rapidjson::SizeType>(sep.size()));
  w.Key("head_skip");
  w.Int(parse_int(head_skip_ ? head_skip_->text() : "", 1));
  w.Key("line_skip");
  w.Int(parse_int(line_skip_ ? line_skip_->text() : "", 4));
  w.Key("col_x");
  w.Int(ix);
  w.Key("col_y");
  w.Int(iy);
  w.Key("col_z");
  w.Int(iz);
  w.Key("x_scale");
  w.Double(parse_float(x_scale_ ? x_scale_->text() : "", 0.05f));
  w.Key("y_scale");
  w.Double(parse_float(y_scale_ ? y_scale_->text() : "", 0.05f));
  w.Key("z_scale");
  w.Double(parse_float(z_scale_ ? z_scale_->text() : "", 0.05f));
  w.Key("use_texture");
  w.Bool(use_texture_ && use_texture_->is_checked());
  const std::string texture =
      texture_path_ ? texture_path_->text() : std::string();
  w.Key("texture_path");
  w.String(texture.c_str(), static_cast<rapidjson::SizeType>(texture.size()));
  w.Key("gen_2d_trimesh");
  w.Bool(gen_2d_trimesh_ && gen_2d_trimesh_->is_checked());
  const std::string trimesh =
      trimesh_layer_ ? trimesh_layer_->selected_text() : std::string();
  w.Key("trimesh_layer");
  w.String(trimesh.c_str(), static_cast<rapidjson::SizeType>(trimesh.size()));
  w.EndObject();
  return std::string(buf.GetString(), buf.GetSize());
}

void TrimeshLoaderDialog::on_ok() {
  if (!host_) {
    return;
  }
  const int ix = col_x_ ? col_x_->selected_index() : 0;
  const int iy = col_y_ ? col_y_->selected_index() : 1;
  if (ix >= 0 && iy >= 0 && ix == iy) {
    ui::views::show_message_box(ui::views::MessageBoxKind::kError,
                                "X and Y must use different columns.");
    return;
  }
  host_->run_processing("world3d.trimesh_from_xyz", build_json());
}

}  // namespace plugin
