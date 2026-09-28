// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Console self-test for the public ui::views kernel and primitives.
// Run: out\views_unittests.exe [--self-test]

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/canvas/shell_canvas_backend.h"
#include "ui/gfx/color/color.h"
#include "ui/gfx/display_list/display_list.h"
#include "ui/gfx/raster/paint_stats.h"
#include "tool/command/command.h"
#include "ui/views/gis/shell/ambox_view.h"
#include "ui/views/gis/panel/atmosphere_panel.h"
#include "ui/views/gis/inspect/attribute_table.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/gis/catalog/catalog_view.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/input/combobox.h"
#include "ui/views/dialogs/shell/dialog.h"
#include "ui/views/kernel/shell/dialog_host.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/gis/inspect/feature_info.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/gis/catalog/layer_tree.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/gis/panel/processing_panel.h"
#include "ui/views/primitives/button/radio_button.h"
#include "ui/views/primitives/collection/scroll_view.h"
#include "ui/views/primitives/input/slider.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/views/gis/shell/status_bar.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/text/textfield.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/map/touch_multitouch.h"
#include "ui/views/primitives/collection/tree_view.h"
#include "ui/views/kernel/compositor/shell_compositor.h"
#include "ui/views/kernel/paint/paint_commit.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"

namespace {

using namespace ui::views;
using ui::gfx::paint_counters;
using ui::gfx::reset_paint_counters;

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

MouseEvent mouse_up(int x, int y) {
  MouseEvent e;
  e.type = MouseEvent::Type::kUp;
  e.button = 1;
  e.x = x;
  e.y = y;
  return e;
}

MouseEvent mouse_down(int x, int y) {
  MouseEvent e;
  e.type = MouseEvent::Type::kDown;
  e.button = 1;
  e.x = x;
  e.y = y;
  return e;
}

MouseEvent mouse_move(int x, int y) {
  MouseEvent e;
  e.type = MouseEvent::Type::kMove;
  e.x = x;
  e.y = y;
  return e;
}

KeyEvent key_down(std::uint32_t vk) {
  KeyEvent e;
  e.type = KeyEvent::Type::kDown;
  e.vk = vk;
  return e;
}

void test_utf8_and_theme() {
  expect(utf8_to_wide("ok") == L"ok", "utf8_to_wide");
  expect(wide_to_utf8(L"ok") == "ok", "wide_to_utf8");
  expect(Theme::current().accent == ui::gfx::color_rgb(0, 122, 204),
         "theme accent");
}

void test_shell_canvas_preference() {
  ui::gfx::apply_shell_canvas_preference("gdi");
  expect(ui::gfx::resolved_shell_canvas_backend() ==
             ui::gfx::ShellCanvasBackend::kGdi,
         "prefer gdi");
  const auto skia = ui::gfx::apply_shell_canvas_preference("skia");
  if (ui::gfx::is_skia_backend_available()) {
    expect(skia == ui::gfx::ShellCanvasBackend::kSkia, "skia when linked");
  } else {
    expect(skia == ui::gfx::ShellCanvasBackend::kGdi, "skia falls back to gdi");
  }
  ui::gfx::apply_shell_canvas_preference("gdi");
}

void test_skia_canvas_api() {
  ui::gfx::apply_shell_canvas_preference("gdi");
  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  const int W = 64;
  const int H = 32;
  HBITMAP bmp = CreateCompatibleBitmap(screen, W, H);
  HGDIOBJ old = SelectObject(mem, bmp);

  ui::gfx::Canvas c(mem, W, H);
  c.fill_rect(0, 0, W, H, ui::gfx::color_rgb(0, 0, 0));
  c.stroke_rect(2, 2, 20, 10, ui::gfx::color_rgb(255, 0, 0), 1);
  c.draw_line(0, 0, 10, 10, ui::gfx::color_rgb(0, 255, 0), 1);
  c.save();
  c.clip_rect(8, 8, 16, 16);
  c.fill_rect(0, 0, W, H, ui::gfx::color_rgb(0, 0, 255));
  c.restore();
  const auto sz = c.measure_text(L"Ab");
  expect(sz.width > 0 && sz.height > 0, "measure_text Ab");
  expect(c.measure_text(L"").width == 0, "measure_text empty");
  expect(c.measure_text(nullptr).width == 0, "measure_text null");

  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);

  if (ui::gfx::is_skia_backend_available()) {
    ui::gfx::apply_shell_canvas_preference("skia");
    HDC screen2 = GetDC(nullptr);
    HDC mem2 = CreateCompatibleDC(screen2);
    HBITMAP bmp2 = CreateCompatibleBitmap(screen2, W, H);
    HGDIOBJ old2 = SelectObject(mem2, bmp2);
    ui::gfx::Canvas c2(mem2, W, H);
    c2.fill_rect(0, 0, W, H, ui::gfx::color_rgb(0, 0, 0));
    const auto sz2 = c2.measure_text(L"Ab");
    expect(sz2.width > 0 && sz2.height > 0, "skia measure_text Ab");
    SelectObject(mem2, old2);
    DeleteObject(bmp2);
    DeleteDC(mem2);
    ReleaseDC(nullptr, screen2);
    ui::gfx::apply_shell_canvas_preference("gdi");
  }
}

void test_kernel_visible_enabled_focus_hover() {
  View hidden;
  hidden.set_bounds({0, 0, 40, 40});
  hidden.set_visible(false);
  expect(!hidden.is_visible(), "view hidden");
  expect(hidden.get_view_at(10, 10) == nullptr, "hidden skips hit-test");

  View disabled;
  disabled.set_enabled(false);
  expect(!disabled.is_enabled(), "view disabled");

  Widget widget;
  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, 400, 300});
  auto* button = new Button("Go");
  button->set_bounds({0, 0, 80, 28});
  root->add_child(std::unique_ptr<View>(button));
  widget.set_contents_view(std::move(root));

  expect(button->is_visible(), "button visible");
  expect(button->is_enabled(), "button enabled");
  expect(button->request_focus(), "button focus");
  expect(button->is_focused(), "button is focused");
  expect(widget.focused_view() == button, "widget focused_view");

  widget.send_mouse(mouse_move(10, 10));
  expect(button->is_hovered(), "button hovered");
  expect(widget.hovered_view() == button, "widget hovered_view");

  button->set_enabled(false);
  expect(!button->is_enabled(), "button disabled");
  expect(!button->is_focused(), "disabled clears focus");
  expect(!button->request_focus(), "disabled cannot focus");

  button->set_enabled(true);
  button->set_visible(false);
  expect(!button->is_visible(), "button hidden");
  expect(!button->request_focus(), "hidden cannot focus");
}

void test_tab_focus_traversal() {
  Widget widget;
  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, 400, 300});
  auto* a = new Button("A");
  auto* mid = new Button("hidden");
  auto* b = new Textfield();
  a->set_bounds({0, 0, 80, 28});
  mid->set_bounds({0, 30, 80, 28});
  mid->set_visible(false);
  b->set_bounds({0, 60, 160, 24});
  root->add_child(std::unique_ptr<View>(a));
  root->add_child(std::unique_ptr<View>(mid));
  root->add_child(std::unique_ptr<View>(b));
  widget.set_contents_view(std::move(root));

  expect(a->request_focus(), "tab start A");
  expect(widget.send_key(key_down(VK_TAB)), "tab key");
  expect(b->is_focused(), "tab skips hidden");
  expect(!mid->is_focused(), "hidden not focused");
  expect(widget.advance_focus(true), "shift-tab reverse");
  expect(a->is_focused(), "reverse back to A");
}

void test_box_layout_skips_hidden() {
  View host;
  host.set_bounds({10, 20, 200, 80});
  auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
  auto left = std::make_unique<View>();
  auto hidden = std::make_unique<View>();
  auto right = std::make_unique<View>();
  View* l = left.get();
  View* h = hidden.get();
  View* r = right.get();
  l->set_preferred_size({40, 0});
  h->set_preferred_size({80, 0});
  h->set_visible(false);
  box->set_flex_for_view(r, 1);
  host.set_layout_manager(std::move(box));
  host.add_child(std::move(left));
  host.add_child(std::move(hidden));
  host.add_child(std::move(right));
  host.layout();
  expect(l->bounds().x == 10, "box child uses host origin");
  expect(l->bounds().y == 20, "box child y");
  expect(r->bounds().x == 50, "hidden child skipped");
  expect(r->bounds().width == 160, "flex leftover ignores hidden");
  expect(h->bounds().width == 0 || !h->is_visible(), "hidden not laid out");
}

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
  expect(short_label.preferred_size().height >= 24, "label min height");

  short_label.set_text("Hello preferred width");
  expect(short_label.preferred_size().width ==
             long_label.preferred_size().width,
         "label set_text refreshes preferred");

  Label empty("");
  expect(empty.preferred_size().width == 8, "label empty pad-only width");
  expect(empty.preferred_size().height >= 24, "label empty min height");

  Button go("Go");
  expect(go.preferred_size().width == measure_text_utf8("Go").width + 16,
         "button width = ink + pad");
  expect(go.preferred_size().width < 96, "button tighter than old fixed 96");
  expect(go.preferred_size().height >= 28, "button min height");

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

void test_tab_strip_switch_page() {
  TabStrip tabs;
  tabs.set_bounds({0, 0, 200, 100});
  auto p0 = std::make_unique<Label>("p0");
  auto p1 = std::make_unique<Label>("p1");
  Label* page0 = p0.get();
  Label* page1 = p1.get();
  int changed = -1;
  tabs.set_change([&](int i) { changed = i; });
  tabs.add_tab("One", std::move(p0));
  tabs.add_tab("Two", std::move(p1));
  expect(tabs.active() == 0, "first tab active");
  expect(tabs.tab_count() == 2, "tab count");
  expect(tabs.on_mouse_event(mouse_up(150, 10)), "click second tab");
  expect(tabs.active() == 1, "second tab active");
  expect(changed == 1, "tab change");
  expect(page1->is_visible(), "active page visible");
  expect(!page0->is_visible(), "inactive page hidden");
  tabs.set_active(0);
  expect(tabs.active() == 0, "set_active");
  expect(page0->is_visible(), "page 0 visible again");
}

void test_table_and_attribute_selection() {
  TableView table;
  table.set_bounds({0, 0, 200, 80});
  table.set_columns({"id", "name"});
  table.add_row({"1", "a"});
  table.add_row({"2", "b"});
  int row = -1;
  table.set_row_click([&](int i) { row = i; });
  // Header/row are 24 DIP at scale 1.0 (see TableView metrics).
  expect(table.header_height() == 24, "table header dip");
  expect(table.row_height() == 24, "table row dip");
  expect(table.on_mouse_event(mouse_up(10, 24 + 10)), "table row click");
  expect(table.selected_row() == 0, "row 0 selected");
  expect(row == 0, "row click callback");
  expect(table.row_count() == 2, "table row_count");

  AttributeTable attrs;
  attrs.set_bounds({0, 0, 200, 80});
  int attr_row = -1;
  int changed = 0;
  attrs.set_selected([&](int i) { attr_row = i; });
  attrs.set_changed([&] { ++changed; });
  attrs.set_columns({"id", "name"});
  attrs.add_row({"1", "a"});
  attrs.add_row({"2", "b"});
  expect(changed >= 2, "attribute add_row changed");
  expect(attrs.on_mouse_event(mouse_up(10, 24 + 10)), "attribute row click");
  expect(attrs.selected_row() == 0, "attribute row 0");
  expect(attr_row == 0, "attribute selected callback");
  attrs.clear();
  expect(attrs.row_count() == 0, "attribute clear");
  expect(attrs.selected_row() == -1, "attribute selection cleared");
}

void test_layer_tree() {
  LayerTree tree;
  tree.set_bounds({0, 0, 200, 80});
  tree.set_visible(true);
  int vis_n = 0;
  bool last_vis = true;
  std::string last_id;
  tree.set_visible_changed([&](const std::string& id, bool visible) {
    ++vis_n;
    last_id = id;
    last_vis = visible;
  });
  tree.add_layer("roads", "Roads", true);
  tree.add_layer("rivers", "Rivers", false);
  expect(tree.layer_count() == 2, "layer count");
  expect(tree.is_visible(), "tree View::is_visible");
  expect(tree.is_layer_visible("roads"), "roads layer on");
  expect(!tree.is_layer_visible("rivers"), "rivers layer off");
  tree.set_visible(false);
  expect(!tree.is_visible(), "tree View hidden");
  expect(tree.is_layer_visible("roads"), "layer on while View hidden");
  tree.set_visible(true);
  tree.layout();

  expect(tree.on_mouse_event(mouse_up(50, 12)), "select roads");
  expect(tree.selected_id() == "roads", "selected_id");
  expect(tree.on_mouse_event(mouse_up(10, 12)), "toggle roads checkbox");
  expect(!tree.is_layer_visible("roads"), "roads toggled off");
  expect(vis_n == 1, "visible_changed");
  expect(last_id == "roads", "visible_changed id");
  expect(!last_vis, "visible_changed value");

  vis_n = 0;
  tree.set_layers({{"a", "A", true, false}, {"b", "B", false, true}});
  expect(tree.layer_count() == 2, "set_layers count");
  expect(tree.selected_id() == "b", "set_layers active");
  expect(tree.is_layer_visible("a"), "set_layers a visible");
  expect(!tree.is_layer_visible("b"), "set_layers b hidden");
  expect(vis_n == 0, "set_layers no visible_changed");
  tree.set_layer_visible("b", true);
  expect(tree.is_layer_visible("b"), "set_layer_visible");
  expect(vis_n == 0, "set_layer_visible silent");
  tree.select_layer("a");
  expect(tree.selected_id() == "a", "select_layer");
}

void test_catalog_view() {
  CatalogView catalog;
  expect(catalog.preferred_size().width == 240, "catalog preferred width");
  expect(catalog.layer_tree() != nullptr, "catalog layer_tree");
  catalog.set_title("Layers");
  expect(catalog.title() == "Layers", "catalog title");
  catalog.set_source_names({"shp", "gdb"});
  expect(catalog.source_tabs() != nullptr, "catalog source_tabs");
  expect(catalog.source_tabs()->page_at(0) != nullptr, "sources page");

  catalog.populate_demo_layers();
  expect(catalog.using_demo_layers(), "demo layers flag");
  expect(catalog.layer_tree()->layer_count() == 1, "demo layer count");
  expect(catalog.layer_tree()->selected_id() == "layer.demo", "demo active");

  catalog.populate_layers(
      {{"roads", "Roads", true, true}, {"rivers", "Rivers", false, false}});
  expect(!catalog.using_demo_layers(), "real layers clear demo");
  expect(catalog.layer_tree()->layer_count() == 2, "real layer count");
  expect(catalog.layer_tree()->selected_id() == "roads", "real active");
  expect(catalog.layer_tree()->is_layer_visible("roads"), "roads visible");
  expect(!catalog.layer_tree()->is_layer_visible("rivers"), "rivers hidden");

  catalog.populate_layers({});
  expect(catalog.using_demo_layers(), "empty populate restores demo");
  expect(catalog.layer_tree()->layer_count() == 1, "demo restored count");
}

void test_feature_info_and_status_bar() {
  FeatureInfo info;
  info.set_feature_id("42");
  expect(info.feature_id() == "42", "feature id");
  info.set_fields({{"name", "road"}, {"len", "12"}});
  expect(info.field_count() == 2, "feature fields");
  info.clear();
  expect(info.feature_id().empty(), "feature cleared id");
  expect(info.field_count() == 0, "feature cleared fields");

  StatusBar bar;
  bar.set_scale_text("1:1000");
  bar.set_crs_text("EPSG:4326");
  bar.set_coord_text("120.1, 30.2");
  bar.set_status("ready");
  expect(bar.scale_text() == "1:1000", "status scale");
  expect(bar.crs_text() == "EPSG:4326", "status crs");
  expect(bar.coord_text() == "120.1, 30.2", "status coord");
  expect(bar.status() == "ready", "status text");
}

void test_splitter_layout() {
  Splitter split(Splitter::Orientation::kHorizontal);
  split.set_bounds({0, 0, 400, 200});
  auto a = std::make_unique<View>();
  auto b = std::make_unique<View>();
  View* left = a.get();
  View* right = b.get();
  left->set_preferred_size({120, 0});
  right->set_preferred_size({280, 0});
  split.add_child(std::move(a));
  split.add_child(std::move(b));
  split.layout();
  expect(left->bounds().x == 0, "split left x");
  expect(right->bounds().x == left->bounds().right() + 6, "split bar 6px");
  expect(left->bounds().width + 6 + right->bounds().width == 400,
         "split panes fill");
  expect(left->bounds().width >= 40, "split min left");
  expect(right->bounds().width >= 40, "split min right");
  expect(!split.is_collapsed(), "split open");

  split.set_collapsed(true);
  expect(split.is_collapsed(), "split collapsed");
  expect(right->bounds().width == 0, "split second pane 0");
  expect(left->bounds().width + 6 == 400, "split primary fills when collapsed");

  Splitter vert(Splitter::Orientation::kVertical);
  vert.set_bounds({10, 20, 200, 300});
  auto t = std::make_unique<View>();
  auto bot = std::make_unique<View>();
  View* top = t.get();
  View* bottom = bot.get();
  top->set_preferred_size({0, 100});
  bottom->set_preferred_size({0, 200});
  vert.add_child(std::move(t));
  vert.add_child(std::move(bot));
  vert.layout();
  expect(top->bounds().y == 20, "vsplit top y");
  expect(bottom->bounds().y == top->bounds().bottom() + 6, "vsplit bar");
  expect(top->bounds().height + 6 + bottom->bounds().height == 300,
         "vsplit panes fill");
}

void test_splitter_host_resize_grows_flex_pane() {
  // BrowserView work strip: flexible catalog+map | fixed ambox preferred width.
  Splitter work(Splitter::Orientation::kHorizontal);
  work.set_bounds({0, 0, 800, 400});
  auto map_side = std::make_unique<View>();
  auto ambox = std::make_unique<View>();
  View* primary = map_side.get();
  View* secondary = ambox.get();
  map_side->set_preferred_size({0, 0});
  ambox->set_preferred_size({200, 0});
  work.add_child(std::move(map_side));
  work.add_child(std::move(ambox));
  work.layout();
  expect(secondary->bounds().width == 200, "ambox keeps preferred");
  expect(primary->bounds().width == 800 - 6 - 200, "map takes leftover");

  work.set_bounds({0, 0, 1200, 400});
  work.layout();
  expect(secondary->bounds().width == 200, "ambox stays fixed on grow");
  expect(primary->bounds().width == 1200 - 6 - 200,
         "map grows with host width");

  // BrowserView columns: flexible work | fixed inspector preferred height.
  Splitter columns(Splitter::Orientation::kVertical);
  columns.set_bounds({0, 0, 800, 600});
  auto work_pane = std::make_unique<View>();
  auto inspector = std::make_unique<View>();
  View* top = work_pane.get();
  View* bottom = inspector.get();
  work_pane->set_preferred_size({0, 0});
  inspector->set_preferred_size({0, 160});
  columns.add_child(std::move(work_pane));
  columns.add_child(std::move(inspector));
  columns.layout();
  expect(bottom->bounds().height == 160, "inspector keeps preferred");
  expect(top->bounds().height == 600 - 6 - 160, "work takes leftover");

  columns.set_bounds({0, 0, 800, 900});
  columns.layout();
  expect(bottom->bounds().height == 160, "inspector stays fixed on grow");
  expect(top->bounds().height == 900 - 6 - 160, "work grows with host height");

  // Catalog (fixed) | map (flex): secondary must absorb growth.
  Splitter catalog_map(Splitter::Orientation::kHorizontal);
  catalog_map.set_bounds({0, 0, 800, 400});
  auto catalog = std::make_unique<View>();
  auto map_tabs = std::make_unique<View>();
  View* left = catalog.get();
  View* right = map_tabs.get();
  catalog->set_preferred_size({240, 0});
  map_tabs->set_preferred_size({0, 0});
  catalog_map.add_child(std::move(catalog));
  catalog_map.add_child(std::move(map_tabs));
  catalog_map.layout();
  expect(left->bounds().width == 240, "catalog keeps preferred");
  catalog_map.set_bounds({0, 0, 1100, 400});
  catalog_map.layout();
  expect(left->bounds().width == 240, "catalog stays fixed on grow");
  expect(right->bounds().width == 1100 - 6 - 240, "map tabs grow");
}

void test_splitter_drag_keeps_capture() {
  Widget widget;
  auto split = std::make_unique<Splitter>(Splitter::Orientation::kHorizontal);
  Splitter* host = split.get();
  host->set_bounds({0, 0, 400, 200});
  auto a = std::make_unique<View>();
  auto b = std::make_unique<View>();
  View* left = a.get();
  View* right = b.get();
  left->set_preferred_size({120, 0});
  right->set_preferred_size({280, 0});
  host->add_child(std::move(a));
  host->add_child(std::move(b));
  widget.set_contents_view(std::move(split));
  host->layout();
  const int bar_x = left->bounds().right() + 3;
  const int start_w = left->bounds().width;
  expect(widget.send_mouse(mouse_down(bar_x, 80)), "split press bar");
  expect(widget.send_mouse(mouse_move(bar_x + 80, 80)), "split drag off bar");
  expect(left->bounds().width > start_w, "split capture grows primary");
  expect(widget.send_mouse(mouse_up(bar_x + 80, 80)), "split release");
}

void test_layer_tree_add_while_hidden() {
  View parent;
  parent.set_visible(true);
  auto tree = std::make_unique<LayerTree>();
  LayerTree* layers = tree.get();
  layers->set_bounds({0, 0, 200, 80});
  parent.add_child(std::move(tree));
  layers->add_layer("roads", "Roads", true);
  parent.set_visible(false);
  layers->add_layer("rivers", "Rivers", false);
  expect(layers->layer_count() == 2, "add layer while ancestor hidden");
  expect(layers->is_layer_visible("roads"), "first layer kept");
  expect(!layers->is_layer_visible("rivers"), "second layer added");
}

void test_ambox_in_view_tree() {
  AmboxView box;
  box.set_bounds({0, 0, 180, 240});
  box.layout();
  // Default catalog-less populate: Select + Edit inside a ScrollView.
  expect(box.child_count() == 1, "ambox hosts scroll");
  expect(box.get_view_at(20, 40) != &box, "ambox hit-test reaches button");
  expect(box.groups().size() == 2, "ambox group list");
  expect(box.groups()[1].name == "Edit", "ambox edit group");
  std::vector<std::string> issues;
  expect(collect_layout_violations(&box, &issues) == 0,
         "ambox scroll layout clean");
}

bool group_has_id(const AmboxView::Group& group, const char* id) {
  for (const auto& item : group.items) {
    if (item.id == id) {
      return true;
    }
  }
  return false;
}

void test_ambox_populate_from_commands() {
  tool::CommandCatalog catalog;
  expect(catalog.add("selection.point",
                     [](const tool::CommandArgs&) { return true; }),
         "ambox add selection.point");
  expect(catalog.add("view.pan", [](const tool::CommandArgs&) { return true; }),
         "ambox add view.pan");
  expect(catalog.add("dem.load_tin",
                     [](const tool::CommandArgs&) { return true; }),
         "ambox add dem.load_tin");
  expect(catalog.add("edit.append.point",
                     [](const tool::CommandArgs&) { return true; }),
         "ambox add edit.append.point");

  AmboxView box;
  box.populate_from_commands(&catalog);
  expect(box.groups().size() == 3, "ambox groups with Tools");
  expect(box.groups()[0].name == "Select", "select group name");
  expect(group_has_id(box.groups()[0], "selection.point"),
         "select has selection.point");
  expect(group_has_id(box.groups()[0], "select"), "select keeps Select");
  expect(!group_has_id(box.groups()[0], "identify"),
         "select drops identify placeholder");
  expect(box.groups()[1].name == "Edit", "edit group name");
  expect(group_has_id(box.groups()[1], "edit.append.point"),
         "edit has append.point");
  expect(box.groups()[2].name == "Tools", "tools group name");
  expect(group_has_id(box.groups()[2], "dem.load_tin"),
         "tools has dem.load_tin");

  // Multi-catalog merge (Workspace + PluginHost style).
  tool::CommandCatalog plugins;
  expect(plugins.add("proj.set_map",
                     [](const tool::CommandArgs&) { return true; }),
         "ambox add plugin cmd");
  std::vector<tool::CommandCatalog*> catalogs{&catalog, &plugins};
  box.populate_from_commands(catalogs);
  expect(box.groups().size() == 3, "merged still three groups");
  expect(group_has_id(box.groups()[2], "dem.load_tin"), "merge keeps dem");
  expect(group_has_id(box.groups()[2], "proj.set_map"), "merge adds proj");

  // Null host installs Select and Edit only.
  AmboxView dummy;
  dummy.populate_from_plugin_host(nullptr);
  expect(dummy.groups().size() == 2, "null host dummy groups");
  expect(!group_has_id(dummy.groups()[0], "selection.point"),
         "dummy select without catalog id");
}

void test_ambox_plugin_groups() {
  tool::CommandCatalog catalog;
  expect(catalog.add("dem.load_tin",
                     [](const tool::CommandArgs&) { return true; }),
         "plugin group catalog dem");
  expect(catalog.add("proj.do_prj",
                     [](const tool::CommandArgs&) { return true; }),
         "plugin group catalog proj");
  expect(catalog.add("edit.undo",
                     [](const tool::CommandArgs&) { return true; }),
         "plugin group catalog edit");

  AmboxView::Group dem;
  dem.name = "DEM";
  dem.items.push_back({"dem.load_tin", "Load TIN"});
  AmboxView::Group proj;
  proj.name = "Map Project";
  proj.items.push_back({"proj.do_prj", "Projection"});
  std::vector<AmboxView::Group> plugins;
  plugins.push_back(std::move(dem));
  plugins.push_back(std::move(proj));

  AmboxView box;
  std::vector<tool::CommandCatalog*> catalogs{&catalog};
  box.populate_from_commands(catalogs, std::move(plugins));

  bool saw_dem = false;
  bool saw_proj = false;
  bool dem_in_tools = false;
  for (const auto& group : box.groups()) {
    if (group.name == "DEM") {
      saw_dem = group_has_id(group, "dem.load_tin") &&
                group.items.size() == 1 &&
                group.items[0].label == "Load TIN";
    } else if (group.name == "Map Project") {
      saw_proj = group_has_id(group, "proj.do_prj");
    } else if (group.name == "Tools") {
      dem_in_tools = group_has_id(group, "dem.load_tin");
    } else if (group.name == "Edit") {
      expect(group_has_id(group, "edit.undo"), "edit group kept");
    }
  }
  expect(saw_dem, "DEM group from enabled plugin");
  expect(saw_proj, "proj group from enabled plugin");
  expect(!dem_in_tools, "plugin command leaves Tools");
}

void test_tree_view_add_select_check() {
  TreeView tree;
  tree.set_bounds({0, 0, 200, 80});
  std::string sel;
  std::string chk_id;
  bool chk = true;
  int sel_n = 0;
  int chk_n = 0;
  tree.set_selection_changed([&](const TreeView::NodeId& id) {
    ++sel_n;
    sel = id;
  });
  tree.set_checked_changed([&](const TreeView::NodeId& id, bool on) {
    ++chk_n;
    chk_id = id;
    chk = on;
  });
  tree.add_node("", "root", "Root", true);
  tree.add_node("root", "child", "Child", false);
  tree.layout();
  expect(tree.selected_id().empty(), "tree no selection");
  // Label of row 0 starts after twisty(14) + checkbox(16).
  expect(tree.on_mouse_event(mouse_up(80, 6)), "tree select root");
  expect(tree.selected_id() == "root", "tree selected_id");
  expect(sel == "root", "tree selection callback");
  expect(sel_n == 1, "tree selection once");
  // Checkbox of row 0 is at x in [14, 30).
  expect(tree.on_mouse_event(mouse_up(22, 6)), "tree toggle check");
  expect(chk_id == "root", "tree check id");
  expect(!chk, "tree unchecked");
  expect(chk_n == 1, "tree check once");
  tree.clear();
  expect(tree.selected_id().empty(), "tree cleared");
}

void test_scroll_view_wheel() {
  ScrollView sc;
  sc.set_bounds({0, 0, 100, 40});
  auto c = std::make_unique<View>();
  View* child = c.get();
  child->set_preferred_size({100, 200});
  sc.add_child(std::move(c));
  sc.layout();
  expect(child->bounds().y == 0, "scroll top");
  expect(sc.scroll_offset() == 0, "scroll offset 0");
  MouseEvent wheel;
  wheel.type = MouseEvent::Type::kWheel;
  wheel.x = 10;
  wheel.y = 10;
  wheel.wheel_delta = -120;
  expect(sc.on_mouse_event(wheel), "scroll wheel");
  expect(sc.scroll_offset() == 40, "scroll down");
  expect(child->bounds().y == -40, "content offset");
}

void test_menu_bar_click() {
  MenuBar bar;
  bar.set_bounds({0, 0, 200, 28});
  int n = 0;
  bar.add_item("File", [&] { ++n; });
  bar.add_item("Edit", [&] {});
  expect(bar.item_count() == 2, "menu item_count");
  expect(bar.on_mouse_event(mouse_up(10, 10)), "menu click");
  expect(n == 1, "menu invoke");
}

void test_menu_bar_add_menu() {
  MenuBar bar;
  bar.set_bounds({0, 0, 400, 28});
  std::vector<MenuItem> file;
  MenuItem open;
  open.label = "Open";
  MenuItem save;
  save.label = "Save";
  file.push_back(std::move(open));
  file.push_back(std::move(save));
  bar.add_menu("File", std::move(file));
  int flat = 0;
  bar.add_item("Help", [&] { ++flat; });
  expect(bar.item_count() == 2, "add_menu counts as a top item");
  expect(bar.menu_items(0).size() == 2, "add_menu stores child list");
  expect(bar.menu_items(0)[0].label == "Open", "stored child Open");
  expect(bar.menu_items(0)[1].label == "Save", "stored child Save");
  expect(bar.menu_items(1).empty(), "add_item has no child list");
  expect(bar.last_opened_menu() == -1, "no dropdown opened yet");
  expect(bar.on_mouse_event(mouse_up(10, 10)), "activate File");
  expect(bar.last_opened_menu() == 0, "File activation observable");
  expect(flat == 0, "dropdown activation does not run add_item");
}

bool ambox_has_id(const AmboxView& box, const char* id) {
  for (const auto& group : box.groups()) {
    if (group_has_id(group, id)) {
      return true;
    }
  }
  return false;
}

bool ambox_has_label(const AmboxView& box, const char* label) {
  for (const auto& group : box.groups()) {
    if (group.name == label) {
      return true;
    }
    for (const auto& item : group.items) {
      if (item.label == label) {
        return true;
      }
    }
  }
  return false;
}

void test_ambox_skips_view_navigation() {
  tool::CommandCatalog catalog;
  expect(catalog.add("view.zoom_in",
                     [](const tool::CommandArgs&) { return true; }),
         "ambox add view.zoom_in");
  expect(catalog.add("view.pan", [](const tool::CommandArgs&) { return true; }),
         "ambox add view.pan");
  expect(catalog.add("view.full",
                     [](const tool::CommandArgs&) { return true; }),
         "ambox add view.full");

  AmboxView box;
  box.populate_from_commands(&catalog);
  for (const auto& group : box.groups()) {
    for (const auto& item : group.items) {
      expect(item.id.compare(0, 5, "view.") != 0, "no view.* item");
    }
  }
  expect(!ambox_has_id(box, "view.zoom_in"), "no view.zoom_in");
  expect(!ambox_has_id(box, "view.pan"), "no view.pan");
  expect(!ambox_has_id(box, "view.full"), "no view.full");
  expect(!ambox_has_label(box, "Pan"), "no Pan placeholder");
  expect(!ambox_has_id(box, "pan"), "no pan id");
  expect(!ambox_has_label(box, "Identify"), "no Identify placeholder");
  expect(!ambox_has_id(box, "identify"), "no identify id");
  expect(ambox_has_id(box, "select"), "Select remains");
  expect(box.groups()[0].name == "Select", "Select group remains");
}

void test_layout_invariants_smoke() {
  View root;
  root.set_bounds({0, 0, 400, 300});
  auto child = std::make_unique<View>();
  child->set_bounds({10, 10, 100, 40});
  child->set_preferred_size({100, 40});
  root.add_child(std::move(child));
  std::vector<std::string> issues;
  expect(collect_layout_violations(&root, &issues) == 0, "clean tree ok");

  View bad;
  bad.set_bounds({0, 0, 50, 50});
  auto outside = std::make_unique<View>();
  outside->set_bounds({40, 40, 30, 30});
  outside->set_preferred_size({30, 30});
  bad.add_child(std::move(outside));
  issues.clear();
  expect(collect_layout_violations(&bad, &issues) > 0, "outside child fails");
  expect(!issues.empty(), "outside reports code");

  // ScrollView content may extend past the clip rect; that is not a violation.
  ScrollView scroller;
  scroller.set_bounds({0, 0, 100, 40});
  auto tall = std::make_unique<View>();
  tall->set_preferred_size({100, 200});
  scroller.add_child(std::move(tall));
  scroller.layout();
  issues.clear();
  expect(collect_layout_violations(&scroller, &issues) == 0,
         "scroll content exempt");

  expect(rect_non_negative({0, 0, 1, 1}), "non-neg ok");
  expect(!rect_non_negative({0, 0, -1, 1}), "neg width fails");
  expect(menu_item_metrics_ok(40, 28, 1.f), "menu metrics 1x");
  expect(!menu_item_metrics_ok(8, 10, 1.5f), "menu metrics too small");
}

void test_tab_strip_page_bounds_align() {
  TabStrip tabs;
  tabs.set_bounds({100, 50, 300, 200});
  auto a = std::make_unique<View>();
  auto b = std::make_unique<View>();
  View* pa = a.get();
  View* pb = b.get();
  tabs.add_tab("A", std::move(a));
  tabs.add_tab("B", std::move(b));
  tabs.layout();
  expect(pa->bounds().height > 0, "page has body");
  expect(pa->bounds().y > tabs.bounds().y, "page below tab header");
  expect(pa->bounds().x == tabs.bounds().x, "page x aligns with strip");
  expect(pa->bounds().width == tabs.bounds().width, "page width matches");
  expect(pb->bounds().width == pa->bounds().width, "inactive page sized");
  expect(rect_contains_rect(tabs.bounds(), pa->bounds()), "page inside strip");
}

void test_box_layout_insets_and_spacing() {
  View host;
  host.set_bounds({0, 0, 200, 40});
  auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
  box->set_inside_border(10);
  box->set_between_child_spacing(8);
  auto a = std::make_unique<View>();
  auto b = std::make_unique<View>();
  View* left = a.get();
  View* right = b.get();
  left->set_preferred_size({40, 0});
  right->set_preferred_size({40, 0});
  host.set_layout_manager(std::move(box));
  host.add_child(std::move(a));
  host.add_child(std::move(b));
  host.layout();
  expect(left->bounds().x == 10, "inset left");
  expect(left->bounds().y == 10, "inset top");
  expect(right->bounds().x == 10 + 40 + 8, "between-child spacing");
  expect(left->bounds().height == 20, "cross axis minus insets");
}

void test_box_layout_flex_keeps_preferred() {
  View host;
  host.set_bounds({0, 0, 300, 40});
  auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
  auto fixed = std::make_unique<View>();
  auto flex = std::make_unique<View>();
  View* a = fixed.get();
  View* b = flex.get();
  a->set_preferred_size({60, 24});
  b->set_preferred_size({80, 24});
  box->set_flex_for_view(b, 1);
  host.set_layout_manager(std::move(box));
  host.add_child(std::move(fixed));
  host.add_child(std::move(flex));
  host.layout();
  expect(a->bounds().width == 60, "fixed keeps preferred");
  // leftover = 300 - (60+80) = 160 → flex = 80 + 160
  expect(b->bounds().width == 240, "flex preferred + leftover");
  expect(a->bounds().x + a->bounds().width == b->bounds().x,
         "no overlap between siblings");
}

void test_box_layout_flex_share_no_stack() {
  View host;
  host.set_bounds({0, 0, 200, 40});
  auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
  auto left = std::make_unique<View>();
  auto right = std::make_unique<View>();
  View* a = left.get();
  View* b = right.get();
  a->set_preferred_size({0, 24});
  b->set_preferred_size({0, 24});
  box->set_flex_for_view(a, 1);
  box->set_flex_for_view(b, 1);
  host.set_layout_manager(std::move(box));
  host.add_child(std::move(left));
  host.add_child(std::move(right));
  host.layout();
  expect(a->bounds().width == 100, "flex a half");
  expect(b->bounds().width == 100, "flex b half");
  expect(b->bounds().x == 100, "flex siblings do not stack");
}

void test_box_layout_overflow_fits_host() {
  View host;
  host.set_bounds({0, 0, 100, 40});
  auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
  auto fixed = std::make_unique<View>();
  auto flex = std::make_unique<View>();
  View* a = fixed.get();
  View* b = flex.get();
  a->set_preferred_size({60, 24});
  b->set_preferred_size({80, 24});
  box->set_flex_for_view(b, 1);
  host.set_layout_manager(std::move(box));
  host.add_child(std::move(fixed));
  host.add_child(std::move(flex));
  host.layout();
  expect(a->bounds().width == 60, "fixed preferred before shrink flex");
  expect(b->bounds().width == 40, "flex shrinks to fit host");
  expect(a->bounds().width + b->bounds().width == 100, "children fit host");
  expect(b->bounds().x + b->bounds().width == 100, "no overflow past host");

  std::vector<std::string> issues;
  expect(collect_layout_violations(&host, &issues) == 0,
         "overflow layout stays inside host");
}

void test_box_layout_preferred_size_from_children() {
  View host;
  auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
  box->set_inside_border(4);
  box->set_between_child_spacing(2);
  auto top = std::make_unique<View>();
  auto bottom = std::make_unique<View>();
  top->set_preferred_size({120, 30});
  bottom->set_preferred_size({80, 50});
  host.set_layout_manager(std::move(box));
  host.add_child(std::move(top));
  host.add_child(std::move(bottom));
  const Size pref = host.get_preferred_size();
  expect(pref.width == 120 + 8, "cross = max child + insets");
  expect(pref.height == 30 + 50 + 2 + 8, "main = sum + spacing + insets");
}

void test_dialog_close_noop() {
  // Do not pump a native modal loop in this console test.
  Dialog::close(true);
  expect(true, "dialog close without run_modal");
}

void test_dialog_host_geometry() {
  RECT owner = {100, 200, 500, 600};  // 400脳400
  const OwnedPopupGeom g = center_outer_on_owner_rect(owner, 200, 100);
  expect(g.x == 200, "popup x centered on owner");
  expect(g.y == 350, "popup y centered on owner");
  expect(g.outer_width == 200, "outer width kept");
  expect(g.outer_height == 100, "outer height kept");

  RECT popup = {g.x, g.y, g.x + g.outer_width, g.y + g.outer_height};
  expect(rect_approximately_centered(popup, owner, 1),
         "screen centers align");

  int outer_w = 0;
  int outer_h = 0;
  client_to_outer_size(380, 160, kOwnedDialogStyle, 0, &outer_w, &outer_h);
  expect(outer_w > 380, "caption expands width");
  expect(outer_h > 160, "caption expands height");

  const OwnedPopupGeom placed =
      place_owned_dialog(nullptr, 380, 160, kOwnedDialogStyle, 0);
  expect(placed.outer_width >= outer_w - 2, "place uses outer size");
  expect(placed.outer_height >= outer_h - 2, "place uses outer height");
}

void test_layout_center_helper() {
  Rect outer = {0, 0, 400, 300};
  Rect inner = {100, 75, 200, 150};
  expect(rect_approximately_centered(inner, outer, 0), "view centers match");
  Rect skewed = {0, 0, 200, 150};
  expect(!rect_approximately_centered(skewed, outer, 10), "skew fails");
}

void test_widget_hwnd_and_map_viewport() {
  // Peers do not CreateWindow in this console test; skip Widget::init.
  // MapViewport::attach needs a parent HWND / GPU; do not call it here.
  Widget widget;
  expect(widget.hwnd() == nullptr, "widget hwnd before init");
  expect(widget.contents_view() == nullptr, "widget empty contents");
  expect(widget.device_scale_factor() == 1.f, "widget default scale 1");
  expect(widget.dpi() == kDefaultDpi, "widget default dpi 96");

  MapViewport viewport;
  expect(viewport.attach_mode() == MapViewport::AttachMode::kNone,
         "map_viewport not attached");
}

void test_touch_multitouch_midpoint() {
  TouchMultitouchTracker tracker;
  content::InputEvent e{};

  // Single finger: no multitouch sample; mouse path stays authoritative.
  expect(!tracker.on_contact_down(1, 10, 20, &e), "single down silent");
  expect(tracker.contact_count() == 1, "one contact");
  expect(!tracker.suppress_mouse(), "single no suppress");
  expect(!tracker.on_contact_move(1, 12, 22, &e), "single move silent");

  // Second finger: ldown at midpoint, suppress mouse synthesis.
  expect(tracker.on_contact_down(2, 30, 40, &e), "two-finger down");
  expect(e.kind == content::InputEvent::Kind::kLDown, "down kind");
  expect(e.pointer_count >= 2, "down pointer_count");
  // After single-finger move to (12,22), mid with (30,40) is (21,31).
  expect(e.x_px == 21 && e.y_px == 31, "down midpoint");
  expect(tracker.suppress_mouse(), "multitouch suppresses mouse");

  expect(tracker.on_contact_move(1, 14, 24, &e), "two-finger move");
  expect(e.kind == content::InputEvent::Kind::kMouseMove, "move kind");
  expect(e.pointer_count >= 2, "move pointer_count");
  expect(e.x_px == 22 && e.y_px == 32, "move midpoint");

  expect(tracker.on_contact_up(2, 30, 40, &e), "two-finger up");
  expect(e.kind == content::InputEvent::Kind::kLUp, "up kind");
  expect(e.pointer_count >= 2, "up pointer_count");
  expect(!tracker.suppress_mouse(), "up clears suppress");
  expect(tracker.contact_count() == 1, "one contact remains");

  // Remaining single finger: silent again.
  expect(!tracker.on_contact_move(1, 16, 26, &e), "post-up single silent");
  expect(!tracker.on_contact_up(1, 16, 26, &e), "last up silent");
  expect(tracker.contact_count() == 0, "cleared");
}

void test_dpi_scale_math() {
  expect(scale_factor_from_dpi(96) == 1.f, "96 dpi 鈫?1.0");
  expect(scale_factor_from_dpi(144) == 1.5f, "144 dpi 鈫?1.5");
  expect(scale_factor_from_dpi(192) == 2.f, "192 dpi 鈫?2.0");
  expect(scale_factor_from_dpi(0) == 1.f, "0 dpi 鈫?1.0");
  expect(dip_to_px(100, 1.5f) == 150, "dip_to_px 100@1.5");
  expect(dip_to_px(10, 1.25f) == 13, "dip_to_px rounds 12.5鈫?3");
  expect(px_to_dip(150, 1.5f) == 100, "px_to_dip 150@1.5");
  expect(dpi_for_hwnd(nullptr) >= 96u, "dpi_for_hwnd screen fallback");
}

void test_device_scale_recomputes_preferred() {
  Widget widget;
  auto root = std::make_unique<View>();
  auto* root_ptr = root.get();
  auto button = std::make_unique<Button>("Scale");
  auto* button_ptr = button.get();
  root->add_child(std::move(button));
  const int w96 = button_ptr->preferred_size().width;
  const int h96 = button_ptr->preferred_size().height;
  expect(w96 > 0 && h96 >= 28, "button preferred at 96dpi");

  widget.set_contents_view(std::move(root));
  expect(root_ptr->widget() == &widget, "contents widget wired");

  widget.set_device_scale_factor(1.5f);
  expect(widget.device_scale_factor() == 1.5f, "widget scale 1.5");
  expect(widget.dpi() == 144u, "widget dpi 144");
  expect(button_ptr->preferred_size().width >
             w96,
         "button preferred grows with scale");
  expect(button_ptr->preferred_size().height >= dip_to_px(28, 1.5f),
         "button min height scales");

  const int w150 = button_ptr->preferred_size().width;
  widget.set_device_scale_factor(2.f);
  expect(button_ptr->preferred_size().width > w150,
         "button preferred grows again at 2x");

  auto fixed = std::make_unique<View>();
  fixed->set_preferred_size({100, 40});
  auto* fixed_ptr = fixed.get();
  root_ptr->add_child(std::move(fixed));
  // Child added after scale bump still holds DIP sizes until notified.
  fixed_ptr->propagate_device_scale_factor_changed(1.f, 2.f);
  expect(fixed_ptr->preferred_size().width == 200, "view preferred *2 width");
  expect(fixed_ptr->preferred_size().height == 80, "view preferred *2 height");
}

void test_combobox_dpi_row_geometry() {
  Widget widget;
  auto root = std::make_unique<View>();
  auto combo = std::make_unique<Combobox>();
  Combobox* c = combo.get();
  c->add_item("a");
  c->add_item("b");
  root->add_child(std::move(combo));
  widget.set_contents_view(std::move(root));
  widget.set_device_scale_factor(1.5f);
  c->set_bounds({0, 0, dip_to_px(200, 1.5f), dip_to_px(24, 1.5f)});
  expect(c->on_mouse_event(mouse_up(10, 10)), "combo open @1.5");
  expect(c->is_open(), "combo open state");
  expect(c->bounds().height >= dip_to_px(24, 1.5f) + dip_to_px(22, 1.5f) * 2,
         "open height includes scaled rows");
  expect(c->preferred_size().height == dip_to_px(24, 1.5f),
         "preferred stays header-sized");
  std::vector<std::string> issues;
  expect(collect_layout_violations(c, &issues) == 0,
         "open combo rows exempt from outside-parent");
}

void test_status_bar_dpi_height() {
  Widget widget;
  auto bar = std::make_unique<StatusBar>();
  StatusBar* b = bar.get();
  widget.set_contents_view(std::move(bar));
  expect(b->preferred_size().height == 24, "status 96dpi height");
  widget.set_device_scale_factor(1.5f);
  expect(b->preferred_size().height == dip_to_px(24, 1.5f),
         "status 150% height");
}

void test_paint_fingerprint_locked_scene() {
  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, 80, 40});
  auto btn = std::make_unique<Button>("OK");
  btn->set_bounds({8, 8, 64, 24});
  root->add_child(std::move(btn));
  const std::uint32_t a = paint_fingerprint(root.get(), 80, 40);
  const std::uint32_t b = paint_fingerprint(root.get(), 80, 40);
  expect(a != 0, "fingerprint non-zero");
  expect(a == b, "fingerprint stable");
  // Different size must not collide with the locked 80脳40 scene (best-effort).
  const std::uint32_t c = paint_fingerprint(root.get(), 81, 40);
  expect(c != a, "fingerprint size-sensitive");
}

void test_ambox_scroll_content_taller_than_pane() {
  AmboxView ambox;
  ambox.set_bounds({0, 0, 200, 120});
  ambox.populate_from_commands(nullptr);  // dummy Select/Edit groups
  ambox.layout();
  std::vector<std::string> issues;
  expect(collect_layout_violations(&ambox, &issues) == 0,
         "ambox scroll layout clean");
  expect(ambox.groups().size() >= 2, "ambox has groups");
}

void test_dialog_host_clamps_to_work_area() {
  // Extremely large dialog should still produce a finite positive box.
  const OwnedPopupGeom huge =
      place_owned_dialog(nullptr, 4000, 3000, kOwnedDialogStyle, 0);
  expect(huge.outer_width > 0 && huge.outer_height > 0, "huge dialog sized");
  expect(huge.x > -100000 && huge.y > -100000, "clamped coords finite");
}

class PaintProbe : public View {
 public:
  int self_paints = 0;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    ++self_paints;
    if (canvas) {
      canvas->fill_rect(bounds().x, bounds().y, bounds().width, bounds().height,
                        ui::gfx::color_rgb(200, 40, 40));
    }
  }
};

class CountLayout : public LayoutManager {
 public:
  int layouts = 0;
  void layout(View*) override { ++layouts; }
};

void paint_tree(View* root, int width, int height, const Rect* clip) {
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = width;
  bi.bmiHeader.biHeight = -height;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  HGDIOBJ old_bmp = bmp ? SelectObject(mem, bmp) : nullptr;
  HFONT font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
  HGDIOBJ old_font = font ? SelectObject(mem, font) : nullptr;
  if (clip && clip->width > 0 && clip->height > 0) {
    IntersectClipRect(mem, clip->x, clip->y, clip->right(), clip->bottom());
  }
  {
    ui::gfx::Canvas canvas(mem, width, height);
    if (root) {
      root->paint(&canvas);
    }
  }
  if (font) {
    SelectObject(mem, old_font);
    DeleteObject(font);
  }
  if (bmp) {
    SelectObject(mem, old_bmp);
    DeleteObject(bmp);
  }
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
}

void test_hover_paint_skips_unrelated_views() {
  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, 200, 40});
  auto left = std::make_unique<PaintProbe>();
  auto right = std::make_unique<PaintProbe>();
  PaintProbe* a = left.get();
  PaintProbe* b = right.get();
  a->set_bounds({0, 0, 40, 40});
  b->set_bounds({140, 0, 40, 40});
  root->add_child(std::move(left));
  root->add_child(std::move(right));
  a->set_hovered(true);
  const Rect dirty = a->bounds();
  paint_tree(root.get(), 200, 40, &dirty);
  expect(a->self_paints == 1, "hovered view paints");
  expect(b->self_paints == 0, "unrelated view skipped");
}

void test_paint_commit_snapshot_isolation() {
  auto root = std::make_unique<PaintProbe>();
  root->set_bounds({0, 0, 80, 24});
  PaintCommit frame;
  expect(commit_view_tree(root.get(), Rect{0, 0, 80, 24}, 80, 24, 12,
                          ui::gfx::color_rgb(30, 30, 30), &frame),
         "commit_view_tree ok");
  expect(frame.generation != 0, "commit generation assigned");
  expect(!frame.display_list.empty(), "commit copied commands");
  expect(root->self_paints == 1, "commit records paint_self once");

  // Mutate the live View list; committed snapshot must keep prior commands.
  root->invalidate_commands();
  root->set_bounds({0, 0, 10, 10});
  ui::gfx::DisplayList live;
  root->append_commands_to(&live);
  expect(root->self_paints == 2, "re-record after invalidate");
  expect(!frame.display_list.empty(), "committed list survives mutation");

  ui::gfx::DisplayList clone = frame.display_list.clone();
  expect(!clone.empty(), "DisplayList::clone keeps commands");
  frame.display_list.clear();
  expect(!clone.empty(), "clone independent of source clear");
}

namespace {

const wchar_t kShellWakeClass[] = L"SmartGisViewsShellWakeTest";
int g_shell_wake_posts = 0;

LRESULT CALLBACK shell_wake_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                     LPARAM lparam) {
  if (msg == kShellPublishedMessage) {
    ++g_shell_wake_posts;
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace

void test_shell_compositor_async_publish_wake() {
  // Focused: Commit does not require UI wait_published; worker posts
  // kShellPublishedMessage when the generation is front.
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = shell_wake_wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kShellWakeClass;
    registered = RegisterClassExW(&wc) != 0;
  }
  expect(registered, "shell wake test class registered");
  if (!registered) {
    return;
  }

  HWND hwnd =
      CreateWindowExW(0, kShellWakeClass, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE,
                      nullptr, GetModuleHandleW(nullptr), nullptr);
  expect(hwnd != nullptr, "shell wake message-only hwnd");
  if (!hwnd) {
    return;
  }

  g_shell_wake_posts = 0;
  ShellCompositor compositor;
  compositor.start();

  PaintCommit frame;
  frame.width_px = 16;
  frame.height_px = 16;
  frame.dirty = Rect{0, 0, 16, 16};
  frame.font_px = 12;
  frame.clear_color = ui::gfx::color_rgb(40, 40, 40);
  frame.generation = 7;
  compositor.notify_when_published(frame.generation, hwnd);
  compositor.commit(std::move(frame));

  // Product path must not block; this test only syncs to observe the wake.
  expect(compositor.wait_published(7), "async publish completes");
  expect(compositor.published_generation() == 7, "published generation 7");

  for (int i = 0; i < 200 && g_shell_wake_posts == 0; ++i) {
    MSG msg = {};
    while (PeekMessageW(&msg, hwnd, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    if (g_shell_wake_posts == 0) {
      Sleep(1);
    }
  }
  expect(g_shell_wake_posts >= 1, "worker posted kShellPublishedMessage");

  compositor.shutdown();
  DestroyWindow(hwnd);
}

void test_set_layers_layouts_once() {
  LayerTree tree;
  tree.set_bounds({0, 0, 220, 400});
  std::vector<LayerTree::LayerDesc> layers(6);
  for (int i = 0; i < 6; ++i) {
    layers[static_cast<size_t>(i)].id = "L" + std::to_string(i);
    layers[static_cast<size_t>(i)].name = layers[static_cast<size_t>(i)].id;
  }
  reset_paint_counters();
  tree.set_layers(layers);
  expect(tree.layer_count() == 6, "set_layers row count");
  expect(paint_counters().layout_count == 1, "set_layers layouts once");
}

void test_scroll_skips_layout_when_preferred_unchanged() {
  auto content = std::make_unique<View>();
  auto layout = std::make_unique<CountLayout>();
  CountLayout* counted = layout.get();
  content->set_preferred_size({100, 500});
  content->set_layout_manager(std::move(layout));
  ScrollView scroll;
  scroll.set_bounds({0, 0, 100, 80});
  scroll.add_child(std::move(content));
  scroll.layout();
  const int before = counted->layouts;
  expect(before >= 1, "initial scroll lays content out");
  scroll.set_scroll_offset(30);
  expect(scroll.scroll_offset() == 30, "scroll offset applied");
  expect(counted->layouts == before, "scroll does not layout");
}

void test_table_paints_viewport_rows_only() {
  auto run = [](int row_count) {
    TableView table;
    table.set_columns({"n"});
    for (int i = 0; i < row_count; ++i) {
      table.add_row({std::to_string(i)});
    }
    const int view_h = table.header_height() + 4 * table.row_height();
    const int full_h =
        table.header_height() + row_count * table.row_height();
    table.set_bounds({0, 0, 180, full_h});
    table.set_exposed_rect({0, 0, 180, view_h});
    paint_tree(&table, std::max(full_h, 1), std::max(full_h, 1), nullptr);
    return table.last_painted_row_count();
  };
  const int few = run(20);
  const int many = run(200);
  expect(few > 0 && few <= 5, "viewport draws a handful of rows");
  expect(many == few, "row draw count stable for fixed viewport");
  expect(many < 200, "draw count does not follow row_count");
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

}  // namespace

int main() {
  test_utf8_and_theme();
  test_shell_canvas_preference();
  test_skia_canvas_api();
  test_kernel_visible_enabled_focus_hover();
  test_tab_focus_traversal();
  test_box_layout_skips_hidden();
  test_button_send_mouse();
  test_label_button_preferred_from_measure();
  test_textfield_set_text_char_backspace();
  test_checkbox_toggle();
  test_slider_and_atmosphere_panel();
  test_radio_exclusive_group();
  test_combobox_select();
  test_tab_strip_switch_page();
  test_table_and_attribute_selection();
  test_layer_tree();
  test_catalog_view();
  test_feature_info_and_status_bar();
  test_splitter_layout();
  test_splitter_host_resize_grows_flex_pane();
  test_splitter_drag_keeps_capture();
  test_layer_tree_add_while_hidden();
  test_ambox_in_view_tree();
  test_ambox_populate_from_commands();
  test_ambox_plugin_groups();
  test_tree_view_add_select_check();
  test_scroll_view_wheel();
  test_menu_bar_click();
  test_menu_bar_add_menu();
  test_ambox_skips_view_navigation();
  test_layout_invariants_smoke();
  test_tab_strip_page_bounds_align();
  test_box_layout_insets_and_spacing();
  test_box_layout_flex_keeps_preferred();
  test_box_layout_flex_share_no_stack();
  test_box_layout_overflow_fits_host();
  test_box_layout_preferred_size_from_children();
  test_dialog_close_noop();
  test_dialog_host_geometry();
  test_layout_center_helper();
  test_widget_hwnd_and_map_viewport();
  test_touch_multitouch_midpoint();
  test_dpi_scale_math();
  test_device_scale_recomputes_preferred();
  test_combobox_dpi_row_geometry();
  test_status_bar_dpi_height();
  test_paint_fingerprint_locked_scene();
  test_ambox_scroll_content_taller_than_pane();
  test_dialog_host_clamps_to_work_area();
  test_hover_paint_skips_unrelated_views();
  test_paint_commit_snapshot_isolation();
  test_shell_compositor_async_publish_wake();
  test_set_layers_layouts_once();
  test_scroll_skips_layout_when_preferred_unchanged();
  test_table_paints_viewport_rows_only();
  test_set_text_caches_measure();

  if (g_fails) {
    std::fprintf(stderr, "views_unittests: %d failed\n", g_fails);
    return 1;
  }
  std::printf("views_unittests: ok\n");
  return 0;
}
