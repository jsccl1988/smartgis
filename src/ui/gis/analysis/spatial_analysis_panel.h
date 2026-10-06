// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_ANALYSIS_SPATIAL_ANALYSIS_PANEL_H_
#define UI_GIS_ANALYSIS_SPATIAL_ANALYSIS_PANEL_H_

#include "ui/ui_export.h"

#include <functional>
#include <string>
#include <vector>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Button;
class GeoprocessingHistoryPanel;
class Label;
class ScrollView;
class TableView;
class Textfield;

// Spatial analysis toolbox: categorized operators, params, progress, history.
// Host runs PluginHost / OpsRunner; panel stays map-agnostic.
class UI_EXPORT SpatialAnalysisPanel : public View {
 public:
  struct Operator {
    std::string id;
    std::string title;
    std::string category;
  };

  struct Param {
    std::string name;
    std::string value;
    std::string hint;
  };

  using RunFn = std::function<void(const std::string& id,
                                   const std::vector<Param>& params)>;

  SpatialAnalysisPanel();
  ~SpatialAnalysisPanel() override;

  void set_operators(std::vector<Operator> ops);
  size_t operator_count() const { return operators_.size(); }
  const std::string& selected_id() const { return selected_id_; }
  bool select_id(const std::string& id);

  void set_params(std::vector<Param> params);
  const std::vector<Param>& params() const { return params_; }

  void set_progress(double fraction, std::string message);
  double progress() const { return progress_; }

  void set_run_handler(RunFn fn);
  void set_cancel_handler(std::function<void()> fn);

  GeoprocessingHistoryPanel* history() { return history_; }
  const GeoprocessingHistoryPanel* history() const { return history_; }

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void rebuild_ops_table();
  void rebuild_params_table();
  void on_op_row(int row);
  void on_param_row(int row);
  void on_param_commit();
  void on_run();
  void on_cancel();
  void refresh_progress_label();

  Label* title_ = nullptr;
  TableView* ops_table_ = nullptr;
  ScrollView* ops_scroll_ = nullptr;
  TableView* params_table_ = nullptr;
  ScrollView* params_scroll_ = nullptr;
  Textfield* param_edit_ = nullptr;
  Label* progress_label_ = nullptr;
  Button* run_ = nullptr;
  Button* cancel_ = nullptr;
  GeoprocessingHistoryPanel* history_ = nullptr;

  std::vector<Operator> operators_;
  std::string selected_id_;
  std::vector<Param> params_;
  int selected_param_row_ = -1;
  double progress_ = 0.0;
  std::string progress_message_;
  RunFn run_handler_;
  std::function<void()> cancel_handler_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_ANALYSIS_SPATIAL_ANALYSIS_PANEL_H_
