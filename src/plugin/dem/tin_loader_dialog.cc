// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/dem/tin_loader_dialog.h"

#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "content/public/plugin_host.h"
#include "ui/views/button.h"
#include "ui/views/checkbox.h"
#include "ui/views/combobox.h"
#include "ui/views/file_picker.h"
#include "ui/views/label.h"
#include "ui/views/message_box.h"
#include "ui/views/radio_button.h"
#include "ui/views/table_view.h"
#include "ui/views/textfield.h"

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

TinLoaderDialog::TinLoaderDialog(content::PluginHost* host) : host_(host) {
  set_preferred_size({640, 520});

  add_child(std::make_unique<ui::views::Label>("Vertex file"));
  {
    auto field = std::make_unique<ui::views::Textfield>();
    vertex_path_ = field.get();
    add_child(std::move(field));
  }
  {
    auto btn = std::make_unique<ui::views::Button>("Browse...");
    btn->set_click([this] { on_pick_vertex(); });
    add_child(std::move(btn));
  }

  add_child(std::make_unique<ui::views::Label>("Separator"));
  {
    auto tab = std::make_unique<ui::views::RadioButton>("Tab", 1);
    radio_tab_ = tab.get();
    add_child(std::move(tab));
  }
  {
    auto space = std::make_unique<ui::views::RadioButton>("Space", 1);
    radio_space_ = space.get();
    add_child(std::move(space));
  }
  {
    auto comma = std::make_unique<ui::views::RadioButton>("Comma", 1);
    radio_comma_ = comma.get();
    comma->set_selected(true);
    add_child(std::move(comma));
  }

  add_child(std::make_unique<ui::views::Label>("Head skip"));
  {
    auto field = std::make_unique<ui::views::Textfield>();
    field->set_text("1");
    head_skip_ = field.get();
    add_child(std::move(field));
  }
  add_child(std::make_unique<ui::views::Label>("Line skip"));
  {
    auto field = std::make_unique<ui::views::Textfield>();
    field->set_text("4");
    line_skip_ = field.get();
    add_child(std::move(field));
  }

  add_child(std::make_unique<ui::views::Label>("X column"));
  {
    auto cmb = std::make_unique<ui::views::Combobox>();
    col_x_ = cmb.get();
    add_child(std::move(cmb));
  }
  add_child(std::make_unique<ui::views::Label>("Y column"));
  {
    auto cmb = std::make_unique<ui::views::Combobox>();
    col_y_ = cmb.get();
    add_child(std::move(cmb));
  }
  add_child(std::make_unique<ui::views::Label>("Z column"));
  {
    auto cmb = std::make_unique<ui::views::Combobox>();
    col_z_ = cmb.get();
    add_child(std::move(cmb));
  }

  add_child(std::make_unique<ui::views::Label>("X scale"));
  {
    auto field = std::make_unique<ui::views::Textfield>();
    field->set_text("0.05");
    x_scale_ = field.get();
    add_child(std::move(field));
  }
  add_child(std::make_unique<ui::views::Label>("Y scale"));
  {
    auto field = std::make_unique<ui::views::Textfield>();
    field->set_text("0.05");
    y_scale_ = field.get();
    add_child(std::move(field));
  }
  add_child(std::make_unique<ui::views::Label>("Z scale"));
  {
    auto field = std::make_unique<ui::views::Textfield>();
    field->set_text("0.05");
    z_scale_ = field.get();
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

  {
    auto ck = std::make_unique<ui::views::Checkbox>("Generate 2D TIN layer");
    gen_2d_tin_ = ck.get();
    add_child(std::move(ck));
  }
  {
    auto cmb = std::make_unique<ui::views::Combobox>();
    cmb->add_item("(none)");
    tin_layer_ = cmb.get();
    add_child(std::move(cmb));
  }

  {
    auto table = std::make_unique<ui::views::TableView>();
    preview_ = table.get();
    add_child(std::move(table));
  }

  {
    auto ok = std::make_unique<ui::views::Button>("OK");
    ok->set_click([this] { on_ok(); });
    add_child(std::move(ok));
  }
  add_child(std::make_unique<ui::views::Button>("Cancel"));
}

void TinLoaderDialog::on_pick_vertex() {
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

void TinLoaderDialog::on_pick_texture() {
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

void TinLoaderDialog::update_preview() {
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

std::string TinLoaderDialog::separator_name() const {
  if (radio_tab_ && radio_tab_->is_selected()) {
    return "tab";
  }
  if (radio_space_ && radio_space_->is_selected()) {
    return "space";
  }
  return "comma";
}

std::string TinLoaderDialog::build_json() const {
  const int ix = col_x_ ? col_x_->selected_index() : 0;
  const int iy = col_y_ ? col_y_->selected_index() : 1;
  const int iz = col_z_ ? col_z_->selected_index() : 2;

  std::ostringstream json;
  json << '{';
  json << "\"vertex_path\":\"" << json_escape(vertex_path_ ? vertex_path_->text() : "")
       << '"';
  json << ",\"separator\":\"" << separator_name() << '"';
  json << ",\"head_skip\":" << parse_int(head_skip_ ? head_skip_->text() : "", 1);
  json << ",\"line_skip\":" << parse_int(line_skip_ ? line_skip_->text() : "", 4);
  json << ",\"col_x\":" << ix;
  json << ",\"col_y\":" << iy;
  json << ",\"col_z\":" << iz;
  json << ",\"x_scale\":" << parse_float(x_scale_ ? x_scale_->text() : "", 0.05f);
  json << ",\"y_scale\":" << parse_float(y_scale_ ? y_scale_->text() : "", 0.05f);
  json << ",\"z_scale\":" << parse_float(z_scale_ ? z_scale_->text() : "", 0.05f);
  json << ",\"use_texture\":"
       << ((use_texture_ && use_texture_->is_checked()) ? "true" : "false");
  json << ",\"texture_path\":\""
       << json_escape(texture_path_ ? texture_path_->text() : "") << '"';
  json << ",\"gen_2d_tin\":"
       << ((gen_2d_tin_ && gen_2d_tin_->is_checked()) ? "true" : "false");
  json << ",\"tin_layer\":\""
       << json_escape(tin_layer_ ? tin_layer_->selected_text() : "") << '"';
  json << '}';
  return json.str();
}

void TinLoaderDialog::on_ok() {
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
  host_->run_processing("dem.tin_from_xyz", build_json());
}

}  // namespace plugin
