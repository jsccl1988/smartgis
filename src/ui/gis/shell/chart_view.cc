// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/shell/chart_view.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/text/label.h"

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

void draw_segment(ui::gfx::Canvas* canvas,
                  int x0,
                  int y0,
                  int x1,
                  int y1,
                  ui::gfx::Color color,
                  int stroke) {
  canvas->draw_line(x0, y0, x1, y1, color, stroke);
}

// Series plot surface; title chrome lives in markup.
class ChartPlotView : public View {
 public:
  explicit ChartPlotView(ChartView* owner) : owner_(owner) {}

  void refresh() { schedule_paint(); }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    if (!canvas || !owner_) {
      return;
    }
    const Theme& t = Theme::current();
    const Rect& b = bounds();
    const float scale =
        widget() ? widget()->device_scale_factor() : 1.f;
    const int hair = std::max(1, dip_to_px(1, scale));
    const int pad_l = dip_to_px(40, scale);
    const int pad_r = dip_to_px(12, scale);
    const int pad_t = dip_to_px(14, scale);
    const int pad_b = dip_to_px(28, scale);
    const int axis = std::max(1, dip_to_px(1, scale));
    const int line_w = std::max(1, dip_to_px(2, scale));

    canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_bg);
    canvas->stroke_rect(b.x, b.y, b.width, b.height, t.panel_header, hair);

    const int left = b.x + pad_l;
    const int right = b.right() - pad_r;
    const int top = b.y + pad_t;
    const int bottom = b.bottom() - pad_b;
    if (right <= left || bottom <= top) {
      return;
    }

    canvas->save();
    canvas->clip_rect(b.x + hair, b.y + hair,
                      std::max(0, b.width - 2 * hair),
                      std::max(0, b.height - 2 * hair));

    const int guide_count = 4;
    for (int g = 1; g < guide_count; ++g) {
      const int gy = top + (bottom - top) * g / guide_count;
      canvas->draw_line(left, gy, right, gy, t.panel_header, hair);
    }

    canvas->draw_line(left, top, left, bottom, t.text_muted, axis);
    canvas->draw_line(left, bottom, right, bottom, t.text_muted, axis);

    const auto& series = owner_->series();
    const double vmax = series_max(series);
    canvas->draw_text(b.x + dip_to_px(4, scale), top,
                      std::to_wstring(static_cast<int>(vmax)).c_str(),
                      t.text_muted);
    canvas->draw_text(b.x + dip_to_px(4, scale),
                      bottom - dip_to_px(16, scale), L"0", t.text_muted);

    if (series.empty()) {
      const wchar_t* empty = L"No series";
      const int cx = left + (right - left) / 2 - dip_to_px(36, scale);
      const int cy = top + (bottom - top) / 2 - dip_to_px(8, scale);
      canvas->draw_text(cx, cy, empty, t.text_muted);
      canvas->restore();
      return;
    }

    const int n = static_cast<int>(series.size());
    const int slot = std::max(1, (right - left) / n);
    const int bar_w = std::max(dip_to_px(4, scale), slot * 2 / 3);
    const int plot_h = bottom - top - dip_to_px(4, scale);
    int prev_x = 0;
    int prev_y = 0;
    bool have_prev = false;

    for (int i = 0; i < n; ++i) {
      const double v = std::max(0.0, series[static_cast<size_t>(i)].value);
      const int bar_h =
          static_cast<int>(std::lround(plot_h * (v / vmax)));
      const int x = left + i * slot + (slot - bar_w) / 2;
      const int y = bottom - hair - bar_h;
      canvas->fill_rect(x, y, bar_w, bar_h, t.accent);
      canvas->fill_rect(x, y, bar_w, hair, t.text_bright);
      const int mid_x = x + bar_w / 2;
      const int mid_y = y;
      if (have_prev) {
        draw_segment(canvas, prev_x, prev_y, mid_x, mid_y, t.text, line_w);
      }
      prev_x = mid_x;
      prev_y = mid_y;
      have_prev = true;

      const std::wstring label =
          utf8_to_wide(series[static_cast<size_t>(i)].label);
      canvas->draw_text(x, bottom + hair, label.c_str(), t.text_muted);
    }
    canvas->restore();
  }

 private:
  ChartView* owner_ = nullptr;
};

}  // namespace

ChartView::ChartView() : title_("Chart") {
  MarkupRoot loaded = load_markup("shell/chart_view.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({320, 220});
    return;
  }
  title_label_ = loaded.ids.find_as<Label>("title");
  View* plot_host = loaded.ids.find("plot");

  auto plot = std::make_unique<ChartPlotView>(this);
  plot_ = plot.get();
  if (plot_host) {
    plot_host->set_layout_manager(std::make_unique<FillLayout>());
    plot_host->add_child(std::move(plot));
  }

  if (title_label_) {
    title_label_->set_text(title_);
    title_label_->set_color(Theme::current().text_bright);
  }

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({320, 220});
  add_child(std::move(loaded.root));
  set_preferred_size({320, 220});
}

void ChartView::set_title(std::string title) {
  title_ = std::move(title);
  if (title_label_) {
    title_label_->set_text(title_);
  }
  schedule_paint();
}

void ChartView::set_series(std::vector<SeriesPoint> series) {
  series_ = std::move(series);
  if (plot_) {
    static_cast<ChartPlotView*>(plot_)->refresh();
  }
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

void ChartView::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
  if (title_label_ && title_label_->is_visible()) {
    const Rect& h = title_label_->bounds();
    canvas->fill_rect(h.x, h.y, std::max(h.width, b.right() - h.x), h.height,
                      t.panel_header);
  }
}

}  // namespace views
}  // namespace ui
