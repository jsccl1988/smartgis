// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/analysis/geoprocessing_history_panel.h"

#include <memory>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/text/label.h"

namespace ui {
namespace views {

GeoprocessingHistoryPanel::GeoprocessingHistoryPanel() {
  MarkupRoot loaded = load_markup("analysis/geoprocessing_history_panel.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({280, 140});
    return;
  }
  title_ = loaded.ids.find_as<Label>("title");
  table_ = loaded.ids.find_as<TableView>("table");
  rerun_ = loaded.ids.find_as<Button>("rerun");
  clear_ = loaded.ids.find_as<Button>("clear");

  if (table_) {
    table_->set_row_click([this](int row) { on_row_click(row); });
  }
  if (rerun_) {
    rerun_->set_click([this]() { on_rerun(); });
  }
  if (clear_) {
    clear_->set_click([this]() { on_clear(); });
  }

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({280, 140});
  add_child(std::move(loaded.root));
  set_preferred_size({280, 140});
}

GeoprocessingHistoryPanel::~GeoprocessingHistoryPanel() {
  if (table_) {
    table_->set_row_click({});
  }
  if (rerun_) {
    rerun_->set_click({});
  }
  if (clear_) {
    clear_->set_click({});
  }
  remove_all_children();
  title_ = nullptr;
  table_ = nullptr;
  rerun_ = nullptr;
  clear_ = nullptr;
}

void GeoprocessingHistoryPanel::set_entries(std::vector<Entry> entries) {
  entries_ = std::move(entries);
  rebuild_table();
}

void GeoprocessingHistoryPanel::append_entry(Entry entry) {
  entries_.push_back(std::move(entry));
  rebuild_table();
}

void GeoprocessingHistoryPanel::set_rerun_handler(
    std::function<void(const std::string& op_id)> fn) {
  rerun_handler_ = std::move(fn);
}

void GeoprocessingHistoryPanel::set_clear_handler(std::function<void()> fn) {
  clear_handler_ = std::move(fn);
}

void GeoprocessingHistoryPanel::rebuild_table() {
  if (!table_) {
    return;
  }
  table_->clear_rows();
  int selected = -1;
  for (size_t i = 0; i < entries_.size(); ++i) {
    table_->add_row(
        {entries_[i].time, entries_[i].op_id, entries_[i].status});
    if (entries_[i].op_id == selected_op_id_) {
      selected = static_cast<int>(i);
    }
  }
  if (selected >= 0) {
    table_->set_selected_row(selected);
  }
}

void GeoprocessingHistoryPanel::on_row_click(int row) {
  if (row < 0 || static_cast<size_t>(row) >= entries_.size()) {
    return;
  }
  selected_op_id_ = entries_[static_cast<size_t>(row)].op_id;
}

void GeoprocessingHistoryPanel::on_rerun() {
  if (rerun_handler_ && !selected_op_id_.empty()) {
    rerun_handler_(selected_op_id_);
  }
}

void GeoprocessingHistoryPanel::on_clear() {
  entries_.clear();
  selected_op_id_.clear();
  rebuild_table();
  if (clear_handler_) {
    clear_handler_();
  }
}

void GeoprocessingHistoryPanel::on_device_scale_factor_changed(
    float old_scale,
    float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  const float s = new_scale > 0.f ? new_scale : 1.f;
  set_preferred_size({dip_to_px(280, s), dip_to_px(140, s)});
}

void GeoprocessingHistoryPanel::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
}

}  // namespace views
}  // namespace ui
