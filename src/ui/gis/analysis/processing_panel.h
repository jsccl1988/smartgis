// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_ANALYSIS_PROCESSING_PANEL_H_
#define UI_GIS_ANALYSIS_PROCESSING_PANEL_H_

#include "ui/ui_export.h"
#include <functional>
#include <string>
#include <vector>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Button;
class Label;
class ScrollView;
class TableView;

// Lists PluginHost processing operators and fires a run callback for the
// selected id. Hosts wire GisScene write-back; this panel stays map-agnostic.
class UI_EXPORT ProcessingPanel : public View {
 public:
  struct Operator {
    std::string id;
    std::string title;
  };

  ProcessingPanel();
  ~ProcessingPanel() override;

  void set_operators(std::vector<Operator> ops);
  size_t operator_count() const { return operators_.size(); }
  const std::string& selected_id() const { return selected_id_; }
  bool select_id(const std::string& id);

  void set_run_handler(std::function<void(const std::string& id)> fn);

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void rebuild_table();
  void on_row_click(int row);
  void on_run_clicked();

  Label* title_ = nullptr;
  TableView* table_ = nullptr;
  ScrollView* scroll_ = nullptr;
  Button* run_ = nullptr;
  std::vector<Operator> operators_;
  std::string selected_id_;
  std::function<void(const std::string&)> run_handler_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_ANALYSIS_PROCESSING_PANEL_H_
