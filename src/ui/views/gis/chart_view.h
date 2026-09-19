// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_GIS_CHART_VIEW_H_
#define UI_VIEWS_GIS_CHART_VIEW_H_

#include <string>
#include <vector>

#if __has_include("ui/views/dialogs/dialog.h")
#include "ui/views/dialogs/dialog.h"
#define UI_VIEWS_CHART_HAS_DIALOG 1
#endif

#include "ui/views/kernel/view.h"

namespace ui {
namespace views {

// Bar + polyline series chart painted through the Skia fill/text stub.
// Replaces leftover SmtChart / CDlg2DXChartView (SmtStaDiagram).
class ChartView : public View {
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
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  std::string title_;
  std::vector<SeriesPoint> series_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_GIS_CHART_VIEW_H_
