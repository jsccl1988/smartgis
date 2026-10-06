// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/mine/views/interpolate_dialog.h"

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

InterpolateDialog::InterpolateDialog(content::PluginHost* host) : host_(host) {
  constexpr int kW = 520;
  constexpr int kH = 360;
  set_preferred_size({kW, kH});

  ui::views::MarkupRoot loaded;
  const std::string markup =
      plugin::resolve_resource("smartgis.mine", "interpolate.ui.xml");
  if (!markup.empty()) {
    loaded = ui::views::load_markup(markup);
  }
  if (!loaded.ok()) {
    return;
  }

  csv_path_ = loaded.ids.find_as<ui::views::Textfield>("csv_path");
  output_path_ = loaded.ids.find_as<ui::views::Textfield>("output_path");
  stratum_id_ = loaded.ids.find_as<ui::views::Textfield>("stratum_id");
  top_stratum_ = loaded.ids.find_as<ui::views::Textfield>("top_stratum");
  bottom_stratum_ = loaded.ids.find_as<ui::views::Textfield>("bottom_stratum");

  if (auto* btn = loaded.ids.find_as<ui::views::Button>("browse_csv")) {
    btn->set_click([this] { on_pick_csv(); });
  }
  if (auto* btn = loaded.ids.find_as<ui::views::Button>("browse_output")) {
    btn->set_click([this] { on_pick_output(); });
  }
  if (auto* btn = loaded.ids.find_as<ui::views::Button>("ok")) {
    btn->set_click([this] { on_ok(); });
  }
  if (auto* btn = loaded.ids.find_as<ui::views::Button>("prism")) {
    btn->set_click([this] { on_prism(); });
  }

  set_layout_manager(std::make_unique<ui::views::FillLayout>());
  loaded.root->set_preferred_size({kW, kH});
  add_child(std::move(loaded.root));

  const char* sample_dir = base::switch_cstr("plugin-sample-dir");
  const std::string base =
      sample_dir && *sample_dir ? std::string(sample_dir) : "../data/plugin";
  if (csv_path_ && csv_path_->text().empty()) {
    csv_path_->set_text(base + "/mine_boreholes.csv");
  }
  if (output_path_ && output_path_->text().empty()) {
    output_path_->set_text(base + "/mine_stratum_tin.json");
  }
  if (stratum_id_ && stratum_id_->text().empty()) {
    stratum_id_->set_text("clay");
  }
  if (top_stratum_ && top_stratum_->text().empty()) {
    top_stratum_->set_text("clay");
  }
  if (bottom_stratum_ && bottom_stratum_->text().empty()) {
    bottom_stratum_->set_text("sand");
  }

  if (host_) {
    host_->present_dataset("smartgis.mine", "", 1);
  }
}

void InterpolateDialog::on_pick_csv() {
  const ui::views::FilePickerResult picked = ui::views::pick_open_file(
      L"Borehole CSV (*.csv)\0*.csv\0All (*.*)\0*.*\0");
  if (!picked.accepted || !csv_path_) {
    return;
  }
  csv_path_->set_text(picked.path);
}

void InterpolateDialog::on_pick_output() {
  const ui::views::FilePickerResult picked =
      ui::views::pick_save_file(L"JSON (*.json)\0*.json\0");
  if (!picked.accepted || !output_path_) {
    return;
  }
  output_path_->set_text(picked.path);
}

std::string InterpolateDialog::build_interpolate_json() const {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("input");
  w.String(csv_path_ ? csv_path_->text().c_str() : "");
  w.Key("stratum_id");
  w.String(stratum_id_ ? stratum_id_->text().c_str() : "clay");
  w.Key("output");
  w.String(output_path_ ? output_path_->text().c_str() : "");
  w.EndObject();
  return buf.GetString();
}

std::string InterpolateDialog::build_prism_json() const {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("input");
  w.String(csv_path_ ? csv_path_->text().c_str() : "");
  w.Key("top_stratum_id");
  w.String(top_stratum_ ? top_stratum_->text().c_str() : "clay");
  w.Key("bottom_stratum_id");
  w.String(bottom_stratum_ ? bottom_stratum_->text().c_str() : "sand");
  if (output_path_ && !output_path_->text().empty()) {
    const std::string vol_out = output_path_->text() + ".volume.json";
    w.Key("output");
    w.String(vol_out.c_str());
  }
  w.EndObject();
  return buf.GetString();
}

void InterpolateDialog::on_ok() {
  if (!host_) {
    return;
  }
  host_->run_processing("mine.interpolate_stratum", build_interpolate_json());
}

void InterpolateDialog::on_prism() {
  if (!host_) {
    return;
  }
  host_->run_processing("mine.prism_volume", build_prism_json());
}

}  // namespace plugin
