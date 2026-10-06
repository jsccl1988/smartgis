// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/analysis/processing_panel.h"

#include <memory>
#include <utility>

#include "ui/gis/scroll_table.h"
#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/markup/loader/markup_loader.h"

namespace ui {
namespace views {

ProcessingPanel::ProcessingPanel() {
  MarkupRoot loaded = load_markup("analysis/processing_panel.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({280, 200});
    return;
  }
  title_ = loaded.ids.find_as<Label>("title");
  table_ = loaded.ids.find_as<TableView>("table");
  run_ = loaded.ids.find_as<Button>("run");

  if (table_) {
    table_->set_row_click([this](int row) { on_row_click(row); });
    scroll_ = wrap_markup_table_in_scroll(table_);
  }
  if (run_) {
    run_->set_click([this]() { on_run_clicked(); });
  }

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({280, 200});
  add_child(std::move(loaded.root));
  set_preferred_size({280, 200});
}

ProcessingPanel::~ProcessingPanel() {
  if (table_) {
    table_->set_row_click({});
  }
  if (run_) {
    run_->set_click({});
  }
  remove_all_children();
  title_ = nullptr;
  table_ = nullptr;
  scroll_ = nullptr;
  run_ = nullptr;
}

void ProcessingPanel::set_operators(std::vector<Operator> ops) {
  operators_ = std::move(ops);
  selected_id_.clear();
  if (!operators_.empty()) {
    selected_id_ = operators_.front().id;
  }
  rebuild_table();
}

bool ProcessingPanel::select_id(const std::string& id) {
  for (size_t i = 0; i < operators_.size(); ++i) {
    if (operators_[i].id == id) {
      selected_id_ = id;
      if (table_) {
        table_->set_selected_row(static_cast<int>(i));
      }
      return true;
    }
  }
  return false;
}

void ProcessingPanel::set_run_handler(
    std::function<void(const std::string& id)> fn) {
  run_handler_ = std::move(fn);
}

void ProcessingPanel::rebuild_table() {
  if (!table_) {
    return;
  }
  table_->clear_rows();
  int selected = -1;
  for (size_t i = 0; i < operators_.size(); ++i) {
    table_->add_row({operators_[i].id, operators_[i].title});
    if (operators_[i].id == selected_id_) {
      selected = static_cast<int>(i);
    }
  }
  if (selected >= 0) {
    table_->set_selected_row(selected);
  }
  sync_scroll_table_content(table_, scroll_);
}

void ProcessingPanel::on_row_click(int row) {
  if (row < 0 || static_cast<size_t>(row) >= operators_.size()) {
    return;
  }
  selected_id_ = operators_[static_cast<size_t>(row)].id;
}

void ProcessingPanel::on_run_clicked() {
  if (run_handler_ && !selected_id_.empty()) {
    run_handler_(selected_id_);
  }
}

void ProcessingPanel::on_device_scale_factor_changed(float old_scale,
                                                    float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  const float s = new_scale > 0.f ? new_scale : 1.f;
  set_preferred_size({dip_to_px(280, s), dip_to_px(200, s)});
}

void ProcessingPanel::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
}

}  // namespace views
}  // namespace ui
