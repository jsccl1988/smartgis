// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/geochem/views/analyze_dialog.h"

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

AnalyzeDialog::AnalyzeDialog(content::PluginHost* host) : host_(host) {
  constexpr int kW = 540;
  constexpr int kH = 420;
  set_preferred_size({kW, kH});

  ui::views::MarkupRoot loaded;
  const std::string markup =
      plugin::resolve_resource("smartgis.geochem", "analyze.ui.xml");
  if (!markup.empty()) {
    loaded = ui::views::load_markup(markup);
  }
  if (!loaded.ok()) {
    return;
  }

  input_path_ = loaded.ids.find_as<ui::views::Textfield>("input_path");
  vector_path_ = loaded.ids.find_as<ui::views::Textfield>("vector_path");
  output_path_ = loaded.ids.find_as<ui::views::Textfield>("output_path");
  element_ = loaded.ids.find_as<ui::views::Textfield>("element");
  correlate_ = loaded.ids.find_as<ui::views::Textfield>("correlate");
  threshold_ = loaded.ids.find_as<ui::views::Textfield>("threshold");
  cells_ = loaded.ids.find_as<ui::views::Textfield>("cells");
  classes_ = loaded.ids.find_as<ui::views::Textfield>("classes");

  if (auto* btn = loaded.ids.find_as<ui::views::Button>("browse_input")) {
    btn->set_click([this] { on_pick_input(); });
  }
  if (auto* btn = loaded.ids.find_as<ui::views::Button>("browse_vector")) {
    btn->set_click([this] {
      const ui::views::FilePickerResult picked = ui::views::pick_open_file(
          L"Vector (*.geojson;*.shp;*.gpkg)\0"
          L"*.geojson;*.shp;*.gpkg\0All (*.*)\0*.*\0");
      if (picked.accepted && vector_path_) {
        vector_path_->set_text(picked.path);
      }
    });
  }
  if (auto* btn = loaded.ids.find_as<ui::views::Button>("browse_output")) {
    btn->set_click([this] { on_pick_output(); });
  }
  if (auto* btn = loaded.ids.find_as<ui::views::Button>("ok")) {
    btn->set_click([this] { on_ok(); });
  }
  if (auto* btn = loaded.ids.find_as<ui::views::Button>("stats")) {
    btn->set_click([this] { on_stats(); });
  }

  set_layout_manager(std::make_unique<ui::views::FillLayout>());
  loaded.root->set_preferred_size({kW, kH});
  add_child(std::move(loaded.root));

  const char* sample_dir = std::getenv("SMT_PLUGIN_SAMPLE_DIR");
  const std::string base =
      sample_dir && *sample_dir ? std::string(sample_dir) : "../data/plugin";
  if (input_path_ && input_path_->text().empty()) {
    input_path_->set_text(base + "/geochem/geochem_samples.csv");
  }
  if (output_path_ && output_path_->text().empty()) {
    output_path_->set_text(base + "/geochem_idw.tif");
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
  const std::string vector =
      vector_path_ ? vector_path_->text() : std::string();
  if (!vector.empty()) {
    w.Key("vector");
    w.String(vector.c_str());
  } else {
    w.Key("input");
    w.String(input_path_ ? input_path_->text().c_str() : "");
  }
  w.Key("element");
  w.String(element_ ? element_->text().c_str() : "Cu");
  if (correlate_ && !correlate_->text().empty()) {
    w.Key("correlate");
    w.String(correlate_->text().c_str());
  }
  w.Key("output");
  w.String(output_path_ ? output_path_->text().c_str() : "");
  w.Key("cells");
  w.Int(static_cast<int>(parse_double(cells_ ? cells_->text() : "", 64.0)));
  w.Key("classes");
  w.Int(static_cast<int>(
      parse_double(classes_ ? classes_->text() : "", 5.0)));
  if (threshold_ && !threshold_->text().empty()) {
    w.Key("threshold");
    w.Double(parse_double(threshold_->text(), 0.0));
  }
  w.EndObject();
  return buf.GetString();
}

std::string AnalyzeDialog::build_stats_json() const {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  const std::string vector =
      vector_path_ ? vector_path_->text() : std::string();
  if (!vector.empty()) {
    w.Key("vector");
    w.String(vector.c_str());
  } else {
    w.Key("input");
    w.String(input_path_ ? input_path_->text().c_str() : "");
  }
  w.Key("element");
  w.String(element_ ? element_->text().c_str() : "Cu");
  if (correlate_ && !correlate_->text().empty()) {
    w.Key("correlate");
    w.String(correlate_->text().c_str());
  }
  w.Key("classes");
  w.Int(static_cast<int>(
      parse_double(classes_ ? classes_->text() : "", 5.0)));
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
