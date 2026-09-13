// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/chart_view.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "render/skia/canvas.h"
#include "ui/views/theme.h"

namespace ui {
namespace views {
namespace {

double series_max(const std::vector<ChartView::SeriesPoint>& series) {
  double m = 0;
  for (const auto& p : series) {
    m = std::max(m, std::abs(p.value));
  }
  return m > 0 ? m : 1.0;
}

void draw_segment(render::skia::Canvas* canvas,
                  int x0,
                  int y0,
                  int x1,
                  int y1,
                  render::skia::Color color) {
  const int dx = x1 - x0;
  const int dy = y1 - y0;
  const int steps = std::max(1, std::max(std::abs(dx), std::abs(dy)));
  for (int i = 0; i <= steps; ++i) {
    const int x = x0 + dx * i / steps;
    const int y = y0 + dy * i / steps;
    canvas->fill_rect(x, y, 2, 2, color);
  }
}

}  // namespace

ChartView::ChartView() : title_("Chart") {
  set_preferred_size({320, 220});
}

void ChartView::set_title(std::string title) {
  title_ = std::move(title);
  schedule_paint();
}

void ChartView::set_series(std::vector<SeriesPoint> series) {
  series_ = std::move(series);
  schedule_paint();
}

#ifdef UI_VIEWS_CHART_HAS_DIALOG
Dialog::Result ChartView::run_modal(HWND owner,
                                    const wchar_t* title,
                                    const std::vector<SeriesPoint>& series) {
  auto contents = std::make_unique<ChartView>();
  if (title && title[0] != L'\0') {
    contents->set_title(wide_to_utf8(title));
  }
  contents->set_series(series);
  return Dialog::run_modal(owner, title ? title : L"Chart", 480, 320,
                           std::move(contents));
}
#endif

void ChartView::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
  canvas->fill_rect(b.x, b.y, b.width, 28, t.panel_header);
  canvas->draw_text(b.x + 8, b.y + 6, utf8_to_wide(title_).c_str(),
                    t.text_bright);

  const int left = b.x + 36;
  const int right = b.right() - 12;
  const int top = b.y + 40;
  const int bottom = b.bottom() - 28;
  if (right <= left || bottom <= top) {
    return;
  }

  canvas->fill_rect(left, top, 2, bottom - top, t.text_muted);
  canvas->fill_rect(left, bottom - 2, right - left, 2, t.text_muted);

  const double vmax = series_max(series_);
  canvas->draw_text(b.x + 4, top, std::to_wstring(static_cast<int>(vmax)).c_str(),
                    t.text_muted);
  canvas->draw_text(b.x + 4, bottom - 16, L"0", t.text_muted);

  if (series_.empty()) {
    return;
  }

  const int n = static_cast<int>(series_.size());
  const int slot = (right - left) / n;
  const int bar_w = std::max(4, slot * 2 / 3);
  const int plot_h = bottom - top - 4;
  int prev_x = 0;
  int prev_y = 0;
  bool have_prev = false;

  for (int i = 0; i < n; ++i) {
    const double v = std::max(0.0, series_[static_cast<size_t>(i)].value);
    const int bar_h =
        static_cast<int>(std::lround(plot_h * (v / vmax)));
    const int x = left + i * slot + (slot - bar_w) / 2;
    const int y = bottom - 2 - bar_h;
    canvas->fill_rect(x, y, bar_w, bar_h, t.accent);
    const int mid_x = x + bar_w / 2;
    const int mid_y = y;
    if (have_prev) {
      draw_segment(canvas, prev_x, prev_y, mid_x, mid_y, t.text_bright);
    }
    prev_x = mid_x;
    prev_y = mid_y;
    have_prev = true;

    const std::wstring label =
        utf8_to_wide(series_[static_cast<size_t>(i)].label);
    canvas->draw_text(x, bottom + 2, label.c_str(), t.text);
  }
}

}  // namespace views
}  // namespace ui
