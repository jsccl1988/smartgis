// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/analysis/spatial_analysis_panel.h"

#include <memory>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/gis/analysis/geoprocessing_history_panel.h"
#include "ui/gis/scroll_table.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"
#include "ui/views/markup/loader/markup_loader.h"

namespace ui {
namespace views {

SpatialAnalysisPanel::SpatialAnalysisPanel() {
  MarkupRoot loaded = load_markup("analysis/spatial_analysis_panel.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({300, 420});
    return;
  }
  title_ = loaded.ids.find_as<Label>("title");
  ops_table_ = loaded.ids.find_as<TableView>("ops");
  params_table_ = loaded.ids.find_as<TableView>("params");
  param_edit_ = loaded.ids.find_as<Textfield>("param_edit");
  progress_label_ = loaded.ids.find_as<Label>("progress");
  run_ = loaded.ids.find_as<Button>("run");
  cancel_ = loaded.ids.find_as<Button>("cancel");
  View* history_host = loaded.ids.find("history_host");

  if (ops_table_) {
    ops_table_->set_row_click([this](int row) { on_op_row(row); });
    ops_scroll_ = wrap_markup_table_in_scroll(ops_table_);
  }
  if (params_table_) {
    params_table_->set_row_click([this](int row) { on_param_row(row); });
    params_scroll_ = wrap_markup_table_in_scroll(params_table_);
  }
  if (param_edit_) {
    param_edit_->set_submit([this]() { on_param_commit(); });
  }
  if (run_) {
    run_->set_click([this]() { on_run(); });
  }
  if (cancel_) {
    cancel_->set_click([this]() { on_cancel(); });
  }

  auto history = std::make_unique<GeoprocessingHistoryPanel>();
  history_ = history.get();
  if (history_host) {
    history_host->set_layout_manager(std::make_unique<FillLayout>());
    history_host->add_child(std::move(history));
  }

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({300, 420});
  add_child(std::move(loaded.root));
  set_preferred_size({300, 420});
}

SpatialAnalysisPanel::~SpatialAnalysisPanel() {
  if (ops_table_) {
    ops_table_->set_row_click({});
  }
  if (params_table_) {
    params_table_->set_row_click({});
  }
  if (param_edit_) {
    param_edit_->set_submit({});
  }
  if (run_) {
    run_->set_click({});
  }
  if (cancel_) {
    cancel_->set_click({});
  }
  remove_all_children();
  title_ = nullptr;
  ops_table_ = nullptr;
  ops_scroll_ = nullptr;
  params_table_ = nullptr;
  params_scroll_ = nullptr;
  param_edit_ = nullptr;
  progress_label_ = nullptr;
  run_ = nullptr;
  cancel_ = nullptr;
  history_ = nullptr;
}

void SpatialAnalysisPanel::set_operators(std::vector<Operator> ops) {
  operators_ = std::move(ops);
  selected_id_.clear();
  if (!operators_.empty()) {
    selected_id_ = operators_.front().id;
  }
  rebuild_ops_table();
}

bool SpatialAnalysisPanel::select_id(const std::string& id) {
  for (size_t i = 0; i < operators_.size(); ++i) {
    if (operators_[i].id == id) {
      selected_id_ = id;
      if (ops_table_) {
        ops_table_->set_selected_row(static_cast<int>(i));
      }
      return true;
    }
  }
  return false;
}

void SpatialAnalysisPanel::set_params(std::vector<Param> params) {
  params_ = std::move(params);
  selected_param_row_ = params_.empty() ? -1 : 0;
  rebuild_params_table();
  if (selected_param_row_ >= 0 && param_edit_) {
    param_edit_->set_text(
        params_[static_cast<size_t>(selected_param_row_)].value);
  }
}

void SpatialAnalysisPanel::set_progress(double fraction, std::string message) {
  if (fraction < 0.0) {
    fraction = 0.0;
  }
  if (fraction > 1.0) {
    fraction = 1.0;
  }
  progress_ = fraction;
  progress_message_ = std::move(message);
  refresh_progress_label();
}

void SpatialAnalysisPanel::set_run_handler(RunFn fn) {
  run_handler_ = std::move(fn);
}

void SpatialAnalysisPanel::set_cancel_handler(std::function<void()> fn) {
  cancel_handler_ = std::move(fn);
}

void SpatialAnalysisPanel::rebuild_ops_table() {
  if (!ops_table_) {
    return;
  }
  ops_table_->clear_rows();
  int selected = -1;
  for (size_t i = 0; i < operators_.size(); ++i) {
    ops_table_->add_row(
        {operators_[i].category, operators_[i].id, operators_[i].title});
    if (operators_[i].id == selected_id_) {
      selected = static_cast<int>(i);
    }
  }
  if (selected >= 0) {
    ops_table_->set_selected_row(selected);
  }
  sync_scroll_table_content(ops_table_, ops_scroll_);
}

void SpatialAnalysisPanel::rebuild_params_table() {
  if (!params_table_) {
    return;
  }
  params_table_->clear_rows();
  for (const auto& p : params_) {
    params_table_->add_row({p.name, p.value});
  }
  if (selected_param_row_ >= 0) {
    params_table_->set_selected_row(selected_param_row_);
  }
  sync_scroll_table_content(params_table_, params_scroll_);
}

void SpatialAnalysisPanel::on_op_row(int row) {
  if (row < 0 || static_cast<size_t>(row) >= operators_.size()) {
    return;
  }
  selected_id_ = operators_[static_cast<size_t>(row)].id;
}

void SpatialAnalysisPanel::on_param_row(int row) {
  if (row < 0 || static_cast<size_t>(row) >= params_.size()) {
    return;
  }
  selected_param_row_ = row;
  if (param_edit_) {
    param_edit_->set_text(params_[static_cast<size_t>(row)].value);
  }
}

void SpatialAnalysisPanel::on_param_commit() {
  if (selected_param_row_ < 0 ||
      static_cast<size_t>(selected_param_row_) >= params_.size() ||
      !param_edit_) {
    return;
  }
  params_[static_cast<size_t>(selected_param_row_)].value = param_edit_->text();
  rebuild_params_table();
}

void SpatialAnalysisPanel::on_run() {
  on_param_commit();
  if (run_handler_ && !selected_id_.empty()) {
    run_handler_(selected_id_, params_);
  }
}

void SpatialAnalysisPanel::on_cancel() {
  if (cancel_handler_) {
    cancel_handler_();
  }
}

void SpatialAnalysisPanel::refresh_progress_label() {
  if (!progress_label_) {
    return;
  }
  const int pct = static_cast<int>(progress_ * 100.0 + 0.5);
  std::string text = "Progress: " + std::to_string(pct) + "%";
  if (!progress_message_.empty()) {
    text += " - ";
    text += progress_message_;
  }
  progress_label_->set_text(std::move(text));
}

void SpatialAnalysisPanel::on_device_scale_factor_changed(float old_scale,
                                                          float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  const float s = new_scale > 0.f ? new_scale : 1.f;
  set_preferred_size({dip_to_px(300, s), dip_to_px(420, s)});
}

void SpatialAnalysisPanel::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
}

}  // namespace views
}  // namespace ui
