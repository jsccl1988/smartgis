// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Remaining GIS panel / chart / debug / playback unit tests.

#include <memory>
#include <string>
#include <vector>

#include "ui/gis/analysis/geoprocessing_history_panel.h"
#include "ui/gis/analysis/result_playback_panel.h"
#include "ui/gis/debug/debug_console_panel.h"
#include "ui/gis/debug/diagnostic_tools_panel.h"
#include "ui/gis/debug/render_trace_panel.h"
#include "ui/gis/inspect/attribute_schema_dialog.h"
#include "ui/gis/shell/chart_view.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/testing/unit/views_unit_helpers.h"

using namespace ui::views;

void test_chart_view_series_and_paint() {
  ChartView chart;
  chart.set_bounds({0, 0, 320, 200});
  chart.set_title("DEM");
  expect(chart.title() == "DEM", "chart title");
  chart.set_series({{"a", 1.0}, {"b", 3.5}, {"c", 2.0}});
  expect(chart.series().size() == 3u, "chart series");
  chart.layout();
  paint_tree(&chart, 320, 200, nullptr);
  expect(chart.preferred_size().width > 0, "chart preferred");
}

void test_result_playback_panel_scrub() {
  ResultPlaybackPanel panel;
  panel.set_bounds({0, 0, 360, 80});
  panel.layout();
  panel.set_frame_range(8);
  expect(panel.frame_count() == 8, "playback frame count");
  panel.set_frame_index(3);
  expect(panel.frame_index() == 3, "playback index");
  int last = -1;
  panel.set_frame_change([&](int i) { last = i; });
  panel.set_frame_index(5);
  expect(panel.frame_index() == 5, "playback set index");
  (void)last;
  panel.set_playing(true);
  expect(panel.playing(), "playback playing");
  panel.set_playing(false);
  expect(!panel.playing(), "playback paused");
  panel.set_looping(false);
  panel.set_status_text("frame 5");
}

void test_geoprocessing_history_panel() {
  GeoprocessingHistoryPanel hist;
  hist.set_bounds({0, 0, 280, 160});
  hist.append_entry({"12:00", "native.buffer", "ok"});
  hist.append_entry({"12:01", "native.clip", "ok"});
  expect(hist.entry_count() == 2u, "history count");
  hist.set_entries({{"12:02", "native.union", "fail"}});
  expect(hist.entry_count() == 1u, "history replace");
  hist.layout();
  paint_tree(&hist, 280, 160, nullptr);
}

void test_debug_console_and_diagnostic_tools() {
  DebugConsolePanel console;
  console.set_pane_mode(DebugConsolePanel::PaneMode::kOutput);
  expect(console.pane_mode() == DebugConsolePanel::PaneMode::kOutput,
         "console output pane");
  console.set_visible_console(true);
  expect(console.is_console_visible(), "console visible");
  console.append_line("hello");
  console.append_line("warn");
  console.clear_output();
  int submits = 0;
  console.set_submit_handler([&](const std::string&) { ++submits; });
  (void)submits;
  console.set_bounds({0, 0, 400, 180});
  console.layout();
  paint_tree(&console, 400, 180, nullptr);

  auto tools = make_diagnostic_tools_panel();
  expect(tools != nullptr, "diagnostic tools factory");
  tools->set_bounds({0, 0, 640, 200});
  tools->set_visible_tools(true);
  expect(tools->is_tools_visible(), "tools visible");
  if (tools->output_pane()) {
    tools->set_active_tab(0);
    expect(tools->active_tab() == 0, "tools output tab");
    tools->set_active_tab(1);
  }
  tools->layout();
  paint_tree(tools.get(), 640, 200, nullptr);

  auto trace = make_render_trace_panel();
  expect(trace != nullptr, "render trace factory");
  trace->set_embedded(true);
  expect(trace->is_embedded(), "trace embedded");
  trace->set_bounds({0, 0, 480, 160});
  trace->layout();
  paint_tree(trace.get(), 480, 160, nullptr);
}

void test_attribute_field_schema_row() {
  AttributeField f;
  f.name = "elev";
  f.type = "Double";
  expect(f.name == "elev", "schema field name");
  expect(f.type == "Double", "schema field type");
  std::vector<AttributeField> rows{f, {"name", "String"}};
  expect(rows.size() == 2u, "schema rows");
}
