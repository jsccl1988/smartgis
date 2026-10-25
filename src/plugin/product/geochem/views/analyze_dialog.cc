// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/geochem/views/analyze_dialog.h"

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

AnalyzeDialog::AnalyzeDialog(content::PluginHost* host) : host_(host) {
  constexpr int kW = 540;
  constexpr int kH = 420;
  set_preferred_size({kW, kH});

  const std::string markup =
      plugin::resolve_resource("smartgis.geochem", "analyze.ui.xml");
  ui::views::MarkupRoot* loaded = load_dialog_markup(markup);
  if (!loaded) {
    return;
  }

  input_path_ = loaded->ids.find_as<ui::views::Textfield>("input_path");
  vector_path_ = loaded->ids.find_as<ui::views::Textfield>("vector_path");
  output_path_ = loaded->ids.find_as<ui::views::Textfield>("output_path");
  element_ = loaded->ids.find_as<ui::views::Textfield>("element");
  correlate_ = loaded->ids.find_as<ui::views::Textfield>("correlate");
  threshold_ = loaded->ids.find_as<ui::views::Textfield>("threshold");
  cells_ = loaded->ids.find_as<ui::views::Textfield>("cells");
  classes_ = loaded->ids.find_as<ui::views::Textfield>("classes");

  if (auto* btn = loaded->ids.find_as<ui::views::Button>("browse_input")) {
    btn->set_click([this] { on_pick_input(); });
  }
  if (auto* btn = loaded->ids.find_as<ui::views::Button>("browse_vector")) {
    btn->set_click([this] {
      const ui::views::FilePickerResult picked = ui::views::pick_open_file(
          L"Vector (*.geojson;*.shp;*.gpkg)\0"
          L"*.geojson;*.shp;*.gpkg\0All (*.*)\0*.*\0");
      if (picked.accepted && vector_path_) {
        vector_path_->set_text(picked.path);
      }
    });
  }
  if (auto* btn = loaded->ids.find_as<ui::views::Button>("browse_output")) {
    btn->set_click([this] { on_pick_output(); });
  }
  if (auto* btn = loaded->ids.find_as<ui::views::Button>("ok")) {
    btn->set_click([this] { on_ok(); });
  }
  if (auto* btn = loaded->ids.find_as<ui::views::Button>("stats")) {
    btn->set_click([this] { on_stats(); });
  }

  ui::views::View* body = take_dialog_markup_root(loaded);
  if (!body) {
    return;
  }
  body->set_preferred_size({kW, kH});
  ui::views::install_fill_layout(this);
  ui::views::view_adopt_child(this, body);

  const std::string base = plugin_sample_dir();
  set_textfield_if_empty(input_path_, base + "/geochem/geochem_samples.csv");
  set_textfield_if_empty(output_path_, base + "/geochem_idw.tif");

  if (host_) {
    host_->present_dataset("smartgis.geochem", "", 0);
  }
}

void AnalyzeDialog::on_pick_input() {
  const ui::views::FilePickerResult picked = ui::views::pick_open_file(
      L"Sample CSV (*.csv)\0*.csv\0All (*.*)\0*.*\0");
  if (!picked.accepted || !input_path_) {
    return;
  }
  input_path_->set_text(picked.path);
}

void AnalyzeDialog::on_pick_output() {
  const ui::views::FilePickerResult picked =
      ui::views::pick_save_file(L"GeoTIFF (*.tif)\0*.tif\0");
  if (!picked.accepted || !output_path_) {
    return;
  }
  output_path_->set_text(picked.path);
}

std::string AnalyzeDialog::build_analyze_json() const {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  const std::string vector = textfield_utf8(vector_path_);
  const std::string input = textfield_utf8(input_path_);
  const std::string element = textfield_utf8(element_);
  const std::string correlate = textfield_utf8(correlate_);
  const std::string output = textfield_utf8(output_path_);
  const std::string threshold = textfield_utf8(threshold_);
  if (!vector.empty()) {
    w.Key("vector");
    w.String(vector.c_str());
  } else {
    w.Key("input");
    w.String(input.c_str());
  }
  w.Key("element");
  w.String(element.empty() ? "Cu" : element.c_str());
  if (!correlate.empty()) {
    w.Key("correlate");
    w.String(correlate.c_str());
  }
  w.Key("output");
  w.String(output.c_str());
  w.Key("cells");
  w.Int(static_cast<int>(parse_textfield_double(cells_, 64.0)));
  w.Key("classes");
  w.Int(static_cast<int>(parse_textfield_double(classes_, 5.0)));
  if (!threshold.empty()) {
    w.Key("threshold");
    w.Double(parse_field_double(threshold, 0.0));
  }
  w.EndObject();
  return buf.GetString();
}

std::string AnalyzeDialog::build_stats_json() const {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  const std::string vector = textfield_utf8(vector_path_);
  const std::string input = textfield_utf8(input_path_);
  const std::string element = textfield_utf8(element_);
  const std::string correlate = textfield_utf8(correlate_);
  if (!vector.empty()) {
    w.Key("vector");
    w.String(vector.c_str());
  } else {
    w.Key("input");
    w.String(input.c_str());
  }
  w.Key("element");
  w.String(element.empty() ? "Cu" : element.c_str());
  if (!correlate.empty()) {
    w.Key("correlate");
    w.String(correlate.c_str());
  }
  w.Key("classes");
  w.Int(static_cast<int>(parse_textfield_double(classes_, 5.0)));
  w.EndObject();
  return buf.GetString();
}

void AnalyzeDialog::on_ok() {
  if (!host_) {
    return;
  }
  host_->run_processing("geochem.analyze", build_analyze_json());
}

void AnalyzeDialog::on_stats() {
  if (!host_) {
    return;
  }
  host_->run_processing("geochem.stats", build_stats_json());
}

}  // namespace plugin
