// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// TabStrip, Table, Tree, ScrollView, and MenuBar unit tests.

#include <memory>
#include <string>
#include <vector>

#include "ui/gis/inspect/attribute_table.h"
#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/primitives/collection/scroll_view.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/collection/tree_view.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/testing/unit/views_unit_helpers.h"

using namespace ui::views;
using ui::gfx::paint_counters;
using ui::gfx::reset_paint_counters;

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
  expect(tabs.on_mouse_event(mouse_up(tabs.tab_x_at(1) + 4, 10)),
         "click second tab");
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

void test_tree_view_add_select_check() {
  TreeView tree;
  tree.set_bounds({0, 0, 200, 80});
  expect(tree.row_height() == 24, "tree row dip @1x");
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
  // Label of row 0 starts after twisty(14) + checkbox(16) DIP.
  expect(tree.on_mouse_event(mouse_up(80, 6)), "tree select root");
  expect(tree.selected_id() == "root", "tree selected_id");
  expect(sel == "root", "tree selection callback");
  expect(sel_n == 1, "tree selection once");
  // Checkbox of row 0 is at x in [14, 30) DIP.
  expect(tree.on_mouse_event(mouse_up(22, 6)), "tree toggle check");
  expect(chk_id == "root", "tree check id");
  expect(!chk, "tree unchecked");
  expect(chk_n == 1, "tree check once");
  tree.clear();
  expect(tree.selected_id().empty(), "tree cleared");
}

void test_tree_view_dpi_row_height() {
  Widget widget;
  auto root = std::make_unique<View>();
  auto tree = std::make_unique<TreeView>();
  TreeView* t = tree.get();
  root->add_child(std::move(tree));
  widget.set_contents_view(std::move(root));
  expect(t->row_height() == 24, "tree row @96dpi");
  widget.set_device_scale_factor(2.5f);
  expect(t->row_height() == dip_to_px(24, 2.5f), "tree row @250%");
  expect(t->row_height() >= shell_body_font_px(2.5f),
         "tree row fits shell body face");
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

void test_tab_strip_catalog_labels_have_cells() {
  TabStrip tabs;
  tabs.set_bounds({0, 0, 240, 280});
  tabs.add_tab("Layers", std::make_unique<View>());
  tabs.add_tab("Sources", std::make_unique<View>());
  tabs.add_tab("Maps", std::make_unique<View>());
  tabs.layout();
  expect(tabs.tab_width_at(0) >= 40, "Layers tab cell wide enough");
  expect(tabs.tab_x_at(1) == tabs.tab_x_at(0) + tabs.tab_width_at(0),
         "Sources packs after Layers");
  const Size layers = measure_text_utf8("Layers");
  expect(layers.width > 0 && layers.width < tabs.tab_width_at(0) + 8,
         "Layers label fits packed cell");
}

void test_tab_strip_packed_not_equal_width() {
  TabStrip tabs;
  tabs.set_bounds({0, 0, 1200, 200});
  tabs.add_tab("Map", std::make_unique<View>());
  tabs.add_tab("Data", std::make_unique<View>());
  tabs.add_tab("3D", std::make_unique<View>());
  tabs.set_active(0);
  tabs.layout();
  const int map_w = tabs.tab_width_at(0);
  expect(map_w > 0 && map_w < 200, "Map tab content-sized (not strip/3)");
  expect(tabs.on_mouse_event(mouse_up(tabs.tab_x_at(2) + 2, 10)),
         "3D hit near its packed cell");
  expect(tabs.active() == 2, "3D became active");
  expect(!tabs.on_mouse_event(mouse_up(900, 10)),
         "empty header band not a tab");
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

void test_table_row_cache_hit_on_rerecord() {
  TableView table;
  table.set_columns({"n"});
  for (int i = 0; i < 1000; ++i) {
    table.add_row({std::to_string(i)});
  }
  const int view_h = table.header_height() + 10 * table.row_height();
  const int full_h = table.header_height() + 1000 * table.row_height();
  table.set_bounds({0, 0, 180, full_h});
  table.set_exposed_rect({0, 0, 180, view_h});

  ui::gfx::DisplayList first;
  table.invalidate_commands();
  table.append_commands_to(&first);
  expect(table.last_painted_row_count() > 0 &&
             table.last_painted_row_count() <= 12,
         "cache build paints ~10 visible rows");
  expect(!table.last_cache_hit(), "first record rebuilds row cache");

  ui::gfx::DisplayList second;
  table.invalidate_commands();
  table.append_commands_to(&second);
  expect(table.last_cache_hit(), "second record hits row cache");
  expect(table.last_painted_row_count() > 0 &&
             table.last_painted_row_count() <= 12,
         "cache hit keeps viewport row count");
  expect(!second.empty(), "rerecord still emits commands");
}

