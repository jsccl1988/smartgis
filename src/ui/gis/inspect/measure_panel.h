// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_INSPECT_MEASURE_PANEL_H_
#define UI_GIS_INSPECT_MEASURE_PANEL_H_

#include "ui/ui_export.h"

#include <functional>
#include <string>
#include <vector>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Label;
class RadioButton;
class TableView;

// Measure results board: mode radios + host-pushed result rows.
// Rubber-band drawing stays in src/tool; this panel never owns map geometry.
class UI_EXPORT MeasurePanel : public View {
 public:
  enum class Mode { kLength = 0, kArea = 1, kAzimuth = 2 };

  struct ResultRow {
    std::string label;
    std::string value;
  };

  MeasurePanel();
  ~MeasurePanel() override;

  void set_mode(Mode mode);
  Mode mode() const { return mode_; }

  void set_unit_text(std::string unit);
  const std::string& unit_text() const { return unit_text_; }

  void set_results(std::vector<ResultRow> rows);
  size_t result_count() const { return results_.size(); }

  void set_mode_change(std::function<void(Mode)> fn);

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void sync_radios();
  void rebuild_table();
  void on_mode_radio(Mode mode);

  Label* title_ = nullptr;
  Label* unit_label_ = nullptr;
  RadioButton* length_ = nullptr;
  RadioButton* area_ = nullptr;
  RadioButton* azimuth_ = nullptr;
  TableView* table_ = nullptr;

  Mode mode_ = Mode::kLength;
  std::string unit_text_ = "m";
  std::vector<ResultRow> results_;
  std::function<void(Mode)> mode_change_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_INSPECT_MEASURE_PANEL_H_
