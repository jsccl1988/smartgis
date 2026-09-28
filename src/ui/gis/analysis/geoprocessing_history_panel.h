// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_ANALYSIS_GEOPROCESSING_HISTORY_PANEL_H_
#define UI_GIS_ANALYSIS_GEOPROCESSING_HISTORY_PANEL_H_

#include "ui/ui_export.h"

#include <functional>
#include <string>
#include <vector>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Button;
class Label;
class TableView;

// Process-local geoprocessing history list with Rerun / Clear.
class UI_EXPORT GeoprocessingHistoryPanel : public View {
 public:
  struct Entry {
    std::string time;
    std::string op_id;
    std::string status;
  };

  GeoprocessingHistoryPanel();
  ~GeoprocessingHistoryPanel() override;

  void set_entries(std::vector<Entry> entries);
  void append_entry(Entry entry);
  size_t entry_count() const { return entries_.size(); }
  const std::string& selected_op_id() const { return selected_op_id_; }

  void set_rerun_handler(std::function<void(const std::string& op_id)> fn);
  void set_clear_handler(std::function<void()> fn);

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void rebuild_table();
  void on_row_click(int row);
  void on_rerun();
  void on_clear();

  Label* title_ = nullptr;
  TableView* table_ = nullptr;
  Button* rerun_ = nullptr;
  Button* clear_ = nullptr;

  std::vector<Entry> entries_;
  std::string selected_op_id_;
  std::function<void(const std::string&)> rerun_handler_;
  std::function<void()> clear_handler_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_ANALYSIS_GEOPROCESSING_HISTORY_PANEL_H_
