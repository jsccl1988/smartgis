// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_SHELL_CHART_VIEW_H_
#define UI_GIS_SHELL_CHART_VIEW_H_

#include "ui/ui_export.h"
#include <string>
#include <vector>

#if __has_include("ui/views/dialogs/dialog.h")
#include "ui/views/dialogs/dialog.h"
#define UI_VIEWS_CHART_HAS_DIALOG 1
#endif

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Bar + polyline series chart: title horizon via markup, plot via Skia paint.
// Replaces leftover Chart / CDlg2DXChartView (StaDiagram).
class UI_EXPORT ChartView : public View {
 public:
  struct SeriesPoint {
    std::string label;
    double value = 0;
  };

  ChartView();

  void set_title(std::string title);
  const std::string& title() const { return title_; }

  void set_series(std::vector<SeriesPoint> series);
  const std::vector<SeriesPoint>& series() const { return series_; }

#ifdef UI_VIEWS_CHART_HAS_DIALOG
  static Dialog::Result run_modal(HWND owner,
                                  const wchar_t* title,
                                  const std::vector<SeriesPoint>& series);
#endif

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  std::string title_;
  std::vector<SeriesPoint> series_;
  class Label* title_label_ = nullptr;
  View* plot_ = nullptr;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_SHELL_CHART_VIEW_H_
