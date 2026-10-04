// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Button / input / slider control unit tests.

#include <memory>
#include <string>
#include <vector>

#include "ui/gis/shell/atmosphere_panel.h"
#include "ui/gis/analysis/geoprocessing_history_panel.h"
#include "ui/gis/analysis/processing_panel.h"
#include "ui/gis/analysis/spatial_analysis_panel.h"
#include "ui/gis/inspect/measure_panel.h"
#include "ui/gis/inspect/selection_panel.h"
#include "ui/gis/style/layer_properties_panel.h"
#include "ui/gis/style/legend_panel.h"
#include "ui/gis/style/symbology_panel.h"
#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/color/color.h"
#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/button/radio_button.h"
#include "ui/views/primitives/input/combobox.h"
#include "ui/views/primitives/input/slider.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"
#include "ui/views/testing/unit/views_unit_helpers.h"

using namespace ui::views;
using ui::gfx::paint_counters;
using ui::gfx::reset_paint_counters;

void test_button_send_mouse() {
  Widget widget;
  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, 400, 300});
  auto* button = new Button("Go");
  button->set_bounds({0, 0, 80, 28});
  int clicks = 0;
  button->set_click([&] { ++clicks; });
  root->add_child(std::unique_ptr<View>(button));
  widget.set_contents_view(std::move(root));

  expect(widget.send_mouse(mouse_up(10, 10)), "button send_mouse");
  expect(clicks == 1, "button click via send_mouse");

  expect(button->request_focus(), "button focus");
  expect(widget.send_key(key_down(VK_SPACE)), "button space");
  expect(clicks == 2, "button click via space");

  Button dead("x");
  dead.set_enabled(false);
  int n = 0;
  dead.set_click([&] { ++n; });
  expect(!dead.on_mouse_event(mouse_up(0, 0)), "disabled button ignores click");
  expect(n == 0, "disabled no callback");
}

void test_label_button_preferred_from_measure() {
  const Size short_ink = measure_text_utf8("Hi");
  const Size long_ink = measure_text_utf8("Hello preferred width");
  expect(short_ink.width > 0 && short_ink.height > 0, "measure short ink");
  expect(long_ink.width > short_ink.width, "measure long wider than short");

  Label short_label("Hi");
  Label long_label("Hello preferred width");
  expect(short_label.preferred_size().width == short_ink.width + 8,
         "label short width = ink + pad");
  expect(long_label.preferred_size().width == long_ink.width + 8,
         "label long width = ink + pad");
  expect(long_label.preferred_size().width >
             short_label.preferred_size().width,
         "label long preferred wider");
  expect(short_label.preferred_size().width < 160,
         "label tighter than old fixed 160");
  // Label::kMinHeight is 22 DIP; Button::kMinHeight is 24 DIP.
  expect(short_label.preferred_size().height >= 22, "label min height");

  short_label.set_text("Hello preferred width");
  expect(short_label.preferred_size().width ==
             long_label.preferred_size().width,
         "label set_text refreshes preferred");

  Label empty("");
  expect(empty.preferred_size().width == 8, "label empty pad-only width");
  expect(empty.preferred_size().height >= 22, "label empty min height");

  Button go("Go");
  expect(go.preferred_size().width == measure_text_utf8("Go").width + 16,
         "button width = ink + pad");
  expect(go.preferred_size().width < 96, "button tighter than old fixed 96");
  expect(go.preferred_size().height >= 24, "button min height");

  go.set_text("Much longer button caption");
  expect(go.preferred_size().width >
             measure_text_utf8("Go").width + 16,
         "button set_text refreshes preferred");
}

void test_textfield_set_text_char_backspace() {
  Widget widget;
  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, 400, 300});
  auto* field = new Textfield();
  field->set_bounds({0, 40, 160, 24});
  int changes = 0;
  field->set_change([&] { ++changes; });
  root->add_child(std::unique_ptr<View>(field));
  widget.set_contents_view(std::move(root));

  field->set_text("hi");
  expect(field->text() == "hi", "set_text");

  expect(field->request_focus(), "textfield focus");
  CharEvent ch;
  ch.ch = L'A';
  expect(widget.send_char(ch), "char A");
  expect(field->text() == "hiA", "textfield char");
  expect(changes == 1, "textfield change after char");
  expect(widget.send_key(key_down(VK_BACK)), "backspace");
  expect(field->text() == "hi", "textfield backspace");
  expect(changes == 2, "textfield change after backspace");
}

void test_checkbox_toggle() {
  Widget widget;
  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, 400, 300});
  auto* box = new Checkbox("On");
  box->set_bounds({0, 70, 160, 24});
  int checks = 0;
  bool last = false;
  box->set_change([&](bool on) {
    ++checks;
    last = on;
  });
  root->add_child(std::unique_ptr<View>(box));
  widget.set_contents_view(std::move(root));

  expect(!box->is_checked(), "checkbox starts off");
  expect(box->request_focus(), "checkbox focus");
  expect(widget.send_key(key_down(VK_SPACE)), "checkbox space");
  expect(box->is_checked(), "checkbox checked");
  expect(last, "checkbox change on");
  expect(checks == 1, "checkbox change");
  expect(box->on_mouse_event(mouse_up(4, 8)), "checkbox click");
  expect(!box->is_checked(), "checkbox toggled off");
  expect(!last, "checkbox change off");
  expect(checks == 2, "checkbox change twice");
}

void test_slider_and_atmosphere_panel() {
  Slider slider;
  slider.set_bounds({0, 0, 200, 24});
  slider.set_range(0.0, 100.0);
  int changes = 0;
  double last = -1.0;
  slider.set_change([&](double v) {
    ++changes;
    last = v;
  });
  slider.set_value(40.0);
  expect(std::abs(slider.value() - 40.0) < 1e-9, "slider set_value");
  expect(slider.on_mouse_event(mouse_down(100, 12)), "slider drag start");
  expect(changes >= 1, "slider change on drag");
  expect(last >= 0.0 && last <= 100.0, "slider value in range");

  AtmospherePanel panel;
  panel.set_bounds({0, 0, 280, 180});
  int time_n = 0;
  int ocean_n = 0;
  double time_last = -1.0;
  bool ocean_last = false;
  panel.set_time_range(0.0, 60.0);
  panel.set_time_change([&](double t) {
    ++time_n;
    time_last = t;
  });
  panel.set_ocean_change([&](bool on) {
    ++ocean_n;
    ocean_last = on;
  });
  panel.set_time_sec(15.0);
  expect(std::abs(panel.time_sec() - 15.0) < 1e-9, "panel time");
  panel.set_ocean_checked(true);
  expect(panel.ocean_checked(), "panel ocean checked");
  // Toggle via checkbox child is covered by checkbox tests; exercise setters.
  expect(ocean_n == 0, "setter does not fire change");
  (void)time_n;
  (void)time_last;
  (void)ocean_last;

  ProcessingPanel proc;
  proc.set_bounds({0, 0, 280, 200});
  std::vector<ProcessingPanel::Operator> ops;
  for (int i = 0; i < 11; ++i) {
    ops.push_back({"native.op" + std::to_string(i), "Op " + std::to_string(i)});
  }
  ops[0].id = "native.buffer";
  ops[0].title = "Buffer";
  ops[1].id = "native.clip";
  ops[1].title = "Clip";
  proc.set_operators(std::move(ops));
  expect(proc.operator_count() >= 10, "processing panel >= 10");
  expect(proc.select_id("native.buffer"), "select buffer");
  expect(proc.selected_id() == "native.buffer", "selected buffer id");
  int run_n = 0;
  std::string ran;
  proc.set_run_handler([&](const std::string& id) {
    ++run_n;
    ran = id;
  });
  expect(proc.select_id("native.clip"), "select clip");
  expect(proc.selected_id() == "native.clip", "selected clip id");
  (void)run_n;
  (void)ran;

  MeasurePanel measure;
  measure.set_mode(MeasurePanel::Mode::kArea);
  expect(measure.mode() == MeasurePanel::Mode::kArea, "measure mode area");
  int mode_n = 0;
  measure.set_mode_change([&](MeasurePanel::Mode) { ++mode_n; });
  measure.set_results({{"total", "12.5"}});
  expect(measure.result_count() == 1u, "measure results");
  (void)mode_n;

  SelectionPanel selection;
  selection.set_count(3);
  expect(selection.count() == 3, "selection count");
  selection.set_layers({{"l1", "Roads", 2}});
  expect(selection.layer_count() == 1u, "selection layers");
  std::string sel_cmd;
  selection.set_command([&](const std::string& id) { sel_cmd = id; });
  (void)sel_cmd;

  SymbologyPanel symbology;
  symbology.set_layer("layer.1", "line");
  expect(symbology.layer_token() == "layer.1", "symbology layer");
  symbology.set_fields({"name", "type"});
  symbology.set_paint({{"line-color", "#336699"}, {"line-width", "2"}});
  expect(symbology.paint().size() == 2u, "symbology paint");
  int apply_n = 0;
  symbology.set_apply_handler(
      [&](const SymbologyPanel::PaintKv&) { ++apply_n; });
  (void)apply_n;

  LegendPanel legend;
  legend.set_entries({{"e1", "Class A", "#f00", true}});
  expect(legend.entry_count() == 1u, "legend entries");

  LayerPropertiesPanel layer_props;
  expect(layer_props.symbology() != nullptr, "layer props symbology");
  layer_props.set_source_text("file:demo.gpkg");
  expect(layer_props.source_text() == "file:demo.gpkg", "layer props source");

  SpatialAnalysisPanel analysis;
  analysis.set_operators({{"native.buffer", "Buffer", "overlay"},
                          {"native.clip", "Clip", "overlay"}});
  expect(analysis.operator_count() == 2u, "analysis ops");
  expect(analysis.select_id("native.buffer"), "analysis select");
  analysis.set_params({{"distance", "10", "meters"}});
  expect(analysis.params().size() == 1u, "analysis params");
  analysis.set_progress(0.5, "running");
  expect(std::abs(analysis.progress() - 0.5) < 1e-9, "analysis progress");
  expect(analysis.history() != nullptr, "analysis history");
  analysis.history()->append_entry({"12:00", "native.buffer", "ok"});
  expect(analysis.history()->entry_count() == 1u, "history entry");
}

void test_radio_exclusive_group() {
  View host;
  auto a = std::make_unique<RadioButton>("A", 1);
  auto b = std::make_unique<RadioButton>("B", 1);
  auto c = std::make_unique<RadioButton>("C", 2);
  RadioButton* ra = a.get();
  RadioButton* rb = b.get();
  RadioButton* rc = c.get();
  host.add_child(std::move(a));
  host.add_child(std::move(b));
  host.add_child(std::move(c));
  int n = 0;
  rb->set_change([&] { ++n; });
  rb->set_bounds({0, 0, 100, 24});
  expect(rb->on_mouse_event(mouse_up(0, 0)), "radio click");
  expect(rb->is_selected(), "radio B selected");
  expect(!ra->is_selected(), "radio A cleared");
  expect(n == 1, "radio change");

  rc->set_selected(true);
  expect(rc->is_selected(), "other group selected");
  expect(rb->is_selected(), "group 1 still selected");
}

void test_combobox_select() {
  Combobox combo;
  combo.add_item("one");
  combo.add_item("two");
  combo.add_item("three");
  combo.set_bounds({0, 0, 200, 24});
  int idx = -1;
  combo.set_change([&](int i) { idx = i; });
  expect(combo.selected_index() == 0, "combo default first");
  expect(combo.on_key_event(key_down(VK_DOWN)), "combo down");
  expect(combo.selected_index() == 1, "combo cycled");
  expect(idx == 1, "combo change");
  expect(combo.selected_text() == "two", "combo selected_text");
  combo.set_selected_index(2);
  expect(combo.selected_index() == 2, "combo select");
  expect(combo.on_mouse_event(mouse_up(10, 10)), "combo open");
  expect(combo.is_open(), "combo popup open");
}

void test_set_text_caches_measure() {
  Label label("Hello");
  label.set_bounds({0, 0, 120, 24});
  reset_paint_counters();
  paint_tree(&label, 120, 24, nullptr);
  const ui::gfx::PaintCounters after = paint_counters();
  expect(after.measure_text == 0, "paint does not remeasure");
  expect(after.utf8_conversions == 0, "paint does not convert utf8");

  reset_paint_counters();
  ui::gfx::Color ink = ui::gfx::color_rgb(1, 2, 3);
  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  HBITMAP bmp = CreateCompatibleBitmap(screen, 16, 16);
  HGDIOBJ old = SelectObject(mem, bmp);
  {
    ui::gfx::Canvas canvas(mem, 16, 16);
    canvas.fill_rect(0, 0, 8, 8, ink);
    const auto mid = paint_counters().create_brush;
    canvas.fill_rect(0, 0, 8, 8, ink);
    expect(paint_counters().create_brush == mid, "brush cache hit");
  }
  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);

  reset_paint_counters();
  (void)measure_text_utf8("Aa");
  const auto fonts = paint_counters().create_font;
  (void)measure_text_utf8("Bb");
  expect(paint_counters().create_font == fonts, "font cache hit");
}

