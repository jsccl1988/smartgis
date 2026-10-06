// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Console self-test for EventGenerator-driven control interaction.
// Run: out\Debug\views_interactive_tests.exe

#include <cstdio>
#include <memory>
#include <string>

#include "ui/gis/catalog/layer_tree.h"
#include "ui/gis/inspect/attribute_table.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/button/radio_button.h"
#include "ui/views/primitives/collection/scroll_view.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/collection/tree_view.h"
#include "ui/views/primitives/input/combobox.h"
#include "ui/views/primitives/input/slider.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"
#include "ui/views/kernel/shell/event.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/testing/harness/event_generator.h"
#include "ui/views/testing/harness/views_test_base.h"

namespace {

using namespace ui::views;

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void test_button_click_via_generator() {
  TestWidgetRoot host;
  make_test_widget(&host, 400, 300);
  EventGenerator gen(&host.widget);
  auto* button = new Button("Go");
  button->set_bounds({10, 10, 80, 28});
  int clicks = 0;
  button->set_click([&] { ++clicks; });
  host.root->add_child(std::unique_ptr<View>(button));

  expect(gen.click(20, 20), "button click");
  expect(clicks == 1, "button click count");
}

void test_checkbox_toggle_via_generator() {
  TestWidgetRoot host;
  make_test_widget(&host, 400, 300);
  EventGenerator gen(&host.widget);
  auto* box = new Checkbox("On");
  box->set_bounds({0, 70, 160, 24});
  int changes = 0;
  box->set_change([&](bool) { ++changes; });
  host.root->add_child(std::unique_ptr<View>(box));

  expect(!box->is_checked(), "checkbox off");
  expect(gen.click(4, 78), "checkbox click");
  expect(box->is_checked(), "checkbox on");
  expect(changes == 1, "checkbox change");
}

void test_textfield_type_via_generator() {
  TestWidgetRoot host;
  make_test_widget(&host, 400, 300);
  EventGenerator gen(&host.widget);
  auto* field = new Textfield();
  field->set_bounds({0, 40, 160, 24});
  int changes = 0;
  field->set_change([&] { ++changes; });
  host.root->add_child(std::unique_ptr<View>(field));

  expect(field->request_focus(), "textfield focus");
  expect(gen.type_utf8("ab"), "type ab");
  expect(field->text() == "ab", "textfield text");
  expect(changes == 2, "textfield changes");
}

void test_tab_strip_switch_via_generator() {
  TestWidgetRoot host;
  make_test_widget(&host, 400, 300);
  EventGenerator gen(&host.widget);
  auto tabs_ptr = std::make_unique<TabStrip>();
  tabs_ptr->set_bounds({0, 0, 200, 100});
  auto a = std::make_unique<Label>("p0");
  auto b = std::make_unique<Label>("p1");
  Label* page0 = a.get();
  Label* page1 = b.get();
  int changed = -1;
  tabs_ptr->set_change([&](int i) { changed = i; });
  tabs_ptr->add_tab("One", std::move(a));
  tabs_ptr->add_tab("Two", std::move(b));
  TabStrip* tabs = tabs_ptr.get();
  host.root->add_child(std::move(tabs_ptr));
  tabs->layout();

  expect(gen.click(tabs->tab_x_at(1) + 4, 10), "second tab click");
  expect(changed == 1, "tab changed");
  expect(page1->is_visible(), "page1 visible");
  expect(!page0->is_visible(), "page0 hidden");
}

void test_combobox_select_via_generator() {
  TestWidgetRoot host;
  make_test_widget(&host, 400, 300);
  EventGenerator gen(&host.widget);
  auto combo = std::make_unique<Combobox>();
  combo->add_item("one");
  combo->add_item("two");
  combo->set_bounds({0, 0, 200, 24});
  int idx = -1;
  combo->set_change([&](int i) { idx = i; });
  Combobox* combo_ptr = combo.get();
  host.root->add_child(std::move(combo));

  expect(combo_ptr->request_focus(), "combo focus");
  expect(gen.key_press(VK_DOWN), "combo down");
  expect(idx == 1, "combo index");
}

void test_nested_hover_via_generator() {
  TestWidgetRoot host;
  make_test_widget(&host, 400, 300);
  EventGenerator gen(&host.widget);
  auto outer = std::make_unique<View>();
  outer->set_bounds({20, 20, 120, 80});
  auto* inner = new Button("Inner");
  inner->set_bounds({10, 10, 60, 28});
  outer->add_child(std::unique_ptr<View>(inner));
  host.root->add_child(std::move(outer));

  expect(gen.move_to(35, 35), "move onto inner");
  expect(inner->is_hovered(), "inner hovered");
  expect(host.widget.hovered_view() == inner, "widget hover inner");
  expect(gen.move_to(5, 5), "move to root horizon");
  expect(!inner->is_hovered(), "inner unhovered");
}

void test_multi_step_button_updates_label() {
  TestWidgetRoot host;
  make_test_widget(&host, 400, 300);
  EventGenerator gen(&host.widget);
  auto* label = new Label("idle");
  label->set_bounds({0, 0, 120, 24});
  auto* button = new Button("Run");
  button->set_bounds({0, 30, 80, 28});
  int runs = 0;
  button->set_click([&] {
    ++runs;
    label->set_text("runs=" + std::to_string(runs));
  });
  host.root->add_child(std::unique_ptr<View>(label));
  host.root->add_child(std::unique_ptr<View>(button));

  expect(label->text() == "idle", "label idle");
  expect(gen.click(20, 40), "first run click");
  expect(runs == 1, "run count 1");
  expect(label->text() == "runs=1", "label after first");
  expect(gen.click(20, 40), "second run click");
  expect(runs == 2, "run count 2");
  expect(label->text() == "runs=2", "label after second");
}

void test_radio_via_generator() {
  TestWidgetRoot host;
  make_test_widget(&host, 400, 300);
  EventGenerator gen(&host.widget);
  auto* a = new RadioButton("A", 1);
  auto* b = new RadioButton("B", 1);
  a->set_bounds({0, 0, 80, 24});
  b->set_bounds({0, 28, 80, 24});
  host.root->add_child(std::unique_ptr<View>(a));
  host.root->add_child(std::unique_ptr<View>(b));
  expect(gen.click(8, 34), "radio B click");
  expect(b->is_selected(), "radio B selected");
  expect(!a->is_selected(), "radio A cleared");
}

void test_slider_via_generator() {
  TestWidgetRoot host;
  make_test_widget(&host, 400, 300);
  EventGenerator gen(&host.widget);
  auto* slider = new Slider();
  slider->set_range(0.0, 100.0);
  slider->set_value(0.0);
  slider->set_bounds({0, 0, 200, 24});
  host.root->add_child(std::unique_ptr<View>(slider));
  expect(gen.click(100, 12), "slider click");
  expect(slider->value() > 0.0, "slider moved");
}

void test_table_via_generator() {
  TestWidgetRoot host;
  make_test_widget(&host, 400, 300);
  EventGenerator gen(&host.widget);
  auto* table = new TableView();
  table->set_bounds({0, 0, 220, 80});
  table->set_columns({"id", "name"});
  table->add_row({"1", "a"});
  table->add_row({"2", "b"});
  host.root->add_child(std::unique_ptr<View>(table));
  expect(gen.click(10, table->header_height() + 10), "table row click");
  expect(table->selected_row() == 0, "table row 0");
}

void test_tree_via_generator() {
  TestWidgetRoot host;
  make_test_widget(&host, 400, 300);
  EventGenerator gen(&host.widget);
  auto* tree = new TreeView();
  tree->set_bounds({0, 0, 200, 80});
  tree->add_node("", "root", "Root", true);
  tree->layout();
  host.root->add_child(std::unique_ptr<View>(tree));
  expect(gen.click(80, 6), "tree select");
  expect(tree->selected_id() == "root", "tree selected");
}

void test_scroll_via_generator() {
  TestWidgetRoot host;
  make_test_widget(&host, 400, 300);
  auto scroll = std::make_unique<ScrollView>();
  scroll->set_bounds({0, 0, 120, 40});
  auto inner = std::make_unique<View>();
  inner->set_preferred_size({100, 200});
  scroll->add_child(std::move(inner));
  ScrollView* s = scroll.get();
  host.root->add_child(std::move(scroll));
  s->layout();
  MouseEvent wheel;
  wheel.type = MouseEvent::Type::kWheel;
  wheel.x = 10;
  wheel.y = 10;
  wheel.wheel_delta = -120;
  expect(s->on_mouse_event(wheel), "scroll wheel");
  expect(s->scroll_offset() > 0, "scroll moved");
}

void test_menu_bar_via_generator() {
  TestWidgetRoot host;
  make_test_widget(&host, 400, 300);
  EventGenerator gen(&host.widget);
  auto* bar = new MenuBar();
  int n = 0;
  bar->add_item("File", [&] { ++n; });
  bar->set_bounds({0, 0, 200, 28});
  host.root->add_child(std::unique_ptr<View>(bar));
  expect(gen.click(8, 8), "menu bar click");
  expect(n == 1, "menu item invoke");
}

void test_layer_tree_via_generator() {
  TestWidgetRoot host;
  make_test_widget(&host, 400, 300);
  EventGenerator gen(&host.widget);
  auto* tree = new LayerTree();
  tree->set_bounds({0, 0, 200, 80});
  tree->add_layer("roads", "Roads", true);
  tree->layout();
  host.root->add_child(std::unique_ptr<View>(tree));
  expect(gen.click(50, 15), "layer select");
  expect(tree->selected_id() == "roads", "layer selected");
}

void test_status_bar_via_generator() {
  TestWidgetRoot host;
  make_test_widget(&host, 520, 40);
  auto* bar = new StatusBar();
  bar->set_bounds({0, 0, 520, 32});
  bar->set_status("Ready");
  host.root->add_child(std::unique_ptr<View>(bar));
  expect(bar->status() == "Ready", "status text");
}

void test_attribute_table_via_generator() {
  TestWidgetRoot host;
  make_test_widget(&host, 400, 300);
  EventGenerator gen(&host.widget);
  auto* attrs = new AttributeTable();
  attrs->set_bounds({0, 0, 220, 80});
  attrs->set_columns({"id"});
  attrs->add_row({"1"});
  host.root->add_child(std::unique_ptr<View>(attrs));
  expect(gen.click(10, 24 + 10), "attr row");
  expect(attrs->selected_row() == 0, "attr selected");
}

}  // namespace

int main() {
  test_button_click_via_generator();
  test_checkbox_toggle_via_generator();
  test_textfield_type_via_generator();
  test_tab_strip_switch_via_generator();
  test_combobox_select_via_generator();
  test_nested_hover_via_generator();
  test_multi_step_button_updates_label();
  test_radio_via_generator();
  test_slider_via_generator();
  test_table_via_generator();
  test_tree_via_generator();
  test_scroll_via_generator();
  test_menu_bar_via_generator();
  test_layer_tree_via_generator();
  test_status_bar_via_generator();
  test_attribute_table_via_generator();

  if (g_fails != 0) {
    std::fprintf(stderr, "%d test(s) failed.\n", g_fails);
    return g_fails;
  }
  std::printf("views_interactive_tests: OK\n");
  return 0;
}
