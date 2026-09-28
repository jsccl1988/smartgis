// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/inspect/measure_panel.h"

#include <memory>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/radio_button.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/text/label.h"

namespace ui {
namespace views {

MeasurePanel::MeasurePanel() {
  MarkupRoot loaded = load_markup("inspect/measure_panel.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({280, 220});
    return;
  }
  title_ = loaded.ids.find_as<Label>("title");
  unit_label_ = loaded.ids.find_as<Label>("unit");
  length_ = loaded.ids.find_as<RadioButton>("length");
  area_ = loaded.ids.find_as<RadioButton>("area");
  azimuth_ = loaded.ids.find_as<RadioButton>("azimuth");
  table_ = loaded.ids.find_as<TableView>("table");

  if (length_) {
    length_->set_change([this]() { on_mode_radio(Mode::kLength); });
  }
  if (area_) {
    area_->set_change([this]() { on_mode_radio(Mode::kArea); });
  }
  if (azimuth_) {
    azimuth_->set_change([this]() { on_mode_radio(Mode::kAzimuth); });
  }

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({280, 220});
  add_child(std::move(loaded.root));
  set_preferred_size({280, 220});
  sync_radios();
}

MeasurePanel::~MeasurePanel() {
  if (length_) {
    length_->set_change({});
  }
  if (area_) {
    area_->set_change({});
  }
  if (azimuth_) {
    azimuth_->set_change({});
  }
  remove_all_children();
  title_ = nullptr;
  unit_label_ = nullptr;
  length_ = nullptr;
  area_ = nullptr;
  azimuth_ = nullptr;
  table_ = nullptr;
}

void MeasurePanel::set_mode(Mode mode) {
  mode_ = mode;
  sync_radios();
}

void MeasurePanel::set_unit_text(std::string unit) {
  unit_text_ = std::move(unit);
  if (unit_label_) {
    unit_label_->set_text(std::string("Unit: ") + unit_text_);
  }
}

void MeasurePanel::set_results(std::vector<ResultRow> rows) {
  results_ = std::move(rows);
  rebuild_table();
}

void MeasurePanel::set_mode_change(std::function<void(Mode)> fn) {
  mode_change_ = std::move(fn);
}

void MeasurePanel::sync_radios() {
  if (length_) {
    length_->set_selected(mode_ == Mode::kLength);
  }
  if (area_) {
    area_->set_selected(mode_ == Mode::kArea);
  }
  if (azimuth_) {
    azimuth_->set_selected(mode_ == Mode::kAzimuth);
  }
}

void MeasurePanel::rebuild_table() {
  if (!table_) {
    return;
  }
  table_->clear_rows();
  for (const auto& row : results_) {
    table_->add_row({row.label, row.value});
  }
}

void MeasurePanel::on_mode_radio(Mode mode) {
  mode_ = mode;
  sync_radios();
  if (mode_change_) {
    mode_change_(mode_);
  }
}

void MeasurePanel::on_device_scale_factor_changed(float old_scale,
                                                 float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  const float s = new_scale > 0.f ? new_scale : 1.f;
  set_preferred_size({dip_to_px(280, s), dip_to_px(220, s)});
}

void MeasurePanel::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
}

}  // namespace views
}  // namespace ui
