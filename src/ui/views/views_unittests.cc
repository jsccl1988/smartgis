// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Console self-test for the public ui::views kernel and primitives.
// Run: out\views_unittests.exe [--self-test]

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "render/skia/canvas.h"
#include "render/skia/color.h"
#include "ui/views/ambox_view.h"
#include "ui/views/attribute_table.h"
#include "ui/views/button.h"
#include "ui/views/catalog_view.h"
#include "ui/views/checkbox.h"
#include "ui/views/combobox.h"
#include "ui/views/dialog.h"
#include "ui/views/feature_info.h"
#include "ui/views/label.h"
#include "ui/views/layer_tree.h"
#include "ui/views/layout.h"
#include "ui/views/map_viewport.h"
#include "ui/views/menu_bar.h"
#include "ui/views/radio_button.h"
#include "ui/views/scroll_view.h"
#include "ui/views/splitter.h"
#include "ui/views/status_bar.h"
#include "ui/views/tab_strip.h"
#include "ui/views/table_view.h"
#include "ui/views/textfield.h"
#include "ui/views/theme.h"
#include "ui/views/tree_view.h"
#include "ui/views/view.h"
#include "ui/views/widget.h"

namespace {

using namespace ui::views;

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
  expect(Theme::current().accent == render::skia::color_rgb(0, 122, 204),
         "theme accent");
}

void test_skia_canvas_api() {
  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  const int W = 64;
  const int H = 32;
  HBITMAP bmp = CreateCompatibleBitmap(screen, W, H);
  HGDIOBJ old = SelectObject(mem, bmp);

  render::skia::Canvas c(mem, W, H);
  c.fill_rect(0, 0, W, H, render::skia::color_rgb(0, 0, 0));
  c.stroke_rect(2, 2, 20, 10, render::skia::color_rgb(255, 0, 0), 1);
  c.draw_line(0, 0, 10, 10, render::skia::color_rgb(0, 255, 0), 1);
  c.save();
  c.clip_rect(8, 8, 16, 16);
  c.fill_rect(0, 0, W, H, render::skia::color_rgb(0, 0, 255));
  c.restore();
  const auto sz = c.measure_text(L"Ab");
  expect(sz.width > 0 && sz.height > 0, "measure_text Ab");
  expect(c.measure_text(L"").width == 0, "measure_text empty");
  expect(c.measure_text(nullptr).width == 0, "measure_text null");

  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
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
  expect(table.on_mouse_event(mouse_up(10, 22 + 10)), "table row click");
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
  // Default catalog-less populate: Select + Pan (Tools only when catalog
  // contributes non-flash commands).
  expect(box.child_count() == 2, "ambox groups are children");
  expect(box.get_view_at(20, 40) != &box, "ambox hit-test reaches button");
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

void test_dialog_close_noop() {
  // Do not pump a native modal loop in this console test.
  Dialog::close(true);
  expect(true, "dialog close without run_modal");
}

void test_widget_hwnd_and_map_viewport() {
  // Peers do not CreateWindow in this console test; skip Widget::init.
  // MapViewport::attach needs a parent HWND / GPU; do not call it here.
  Widget widget;
  expect(widget.hwnd() == nullptr, "widget hwnd before init");
  expect(widget.contents_view() == nullptr, "widget empty contents");

  MapViewport viewport;
  expect(viewport.attach_mode() == MapViewport::AttachMode::kNone,
         "map_viewport not attached");
}

}  // namespace

int main() {
  test_utf8_and_theme();
  test_skia_canvas_api();
  test_kernel_visible_enabled_focus_hover();
  test_tab_focus_traversal();
  test_box_layout_skips_hidden();
  test_button_send_mouse();
  test_label_button_preferred_from_measure();
  test_textfield_set_text_char_backspace();
  test_checkbox_toggle();
  test_radio_exclusive_group();
  test_combobox_select();
  test_tab_strip_switch_page();
  test_table_and_attribute_selection();
  test_layer_tree();
  test_catalog_view();
  test_feature_info_and_status_bar();
  test_splitter_layout();
  test_splitter_drag_keeps_capture();
  test_layer_tree_add_while_hidden();
  test_ambox_in_view_tree();
  test_tree_view_add_select_check();
  test_scroll_view_wheel();
  test_menu_bar_click();
  test_dialog_close_noop();
  test_widget_hwnd_and_map_viewport();

  if (g_fails) {
    std::fprintf(stderr, "views_unittests: %d failed\n", g_fails);
    return 1;
  }
  std::printf("views_unittests: ok\n");
  return 0;
}
