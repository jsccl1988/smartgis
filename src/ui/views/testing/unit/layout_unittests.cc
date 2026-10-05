// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// BoxLayout, Splitter, and layout-check unit tests.

#include <cmath>
#include <memory>

#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/primitives/collection/scroll_view.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/testing/forensics/ui_forensics.h"
#include "ui/views/testing/unit/views_unit_helpers.h"

using namespace ui::views;

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
  constexpr int kBar = Splitter::kBarPx;
  expect(left->bounds().x == 0, "split left x");
  expect(right->bounds().x == left->bounds().right() + kBar, "split bar width");
  expect(left->bounds().width + kBar + right->bounds().width == 400,
         "split panes fill");
  expect(left->bounds().width >= 40, "split min left");
  expect(right->bounds().width >= 40, "split min right");
  expect(!split.is_collapsed(), "split open");

  split.set_collapsed(true);
  expect(split.is_collapsed(), "split collapsed");
  expect(right->bounds().width == 0, "split second pane 0");
  expect(left->bounds().width + kBar == 400,
         "split primary fills when collapsed");

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
  expect(bottom->bounds().y == top->bounds().bottom() + kBar, "vsplit bar");
  expect(top->bounds().height + kBar + bottom->bounds().height == 300,
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
  constexpr int kBar = Splitter::kBarPx;
  expect(secondary->bounds().width == 200, "ambox keeps preferred");
  expect(primary->bounds().width == 800 - kBar - 200, "map takes leftover");

  work.set_bounds({0, 0, 1200, 400});
  work.layout();
  expect(secondary->bounds().width == 200, "ambox stays fixed on grow");
  expect(primary->bounds().width == 1200 - kBar - 200,
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
  expect(top->bounds().height == 600 - kBar - 160, "work takes leftover");

  columns.set_bounds({0, 0, 800, 900});
  columns.layout();
  expect(bottom->bounds().height == 160, "inspector stays fixed on grow");
  expect(top->bounds().height == 900 - kBar - 160,
         "work grows with host height");

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
  expect(right->bounds().width == 1100 - kBar - 240, "map tabs grow");

  // Late preferred on primary: both-flex seed must recover Catalog width.
  Splitter late_catalog(Splitter::Orientation::kHorizontal);
  late_catalog.set_bounds({0, 0, 900, 400});
  auto late_a = std::make_unique<View>();
  auto late_b = std::make_unique<View>();
  View* late_left = late_a.get();
  View* late_right = late_b.get();
  late_a->set_preferred_size({0, 0});
  late_b->set_preferred_size({0, 0});
  late_catalog.add_child(std::move(late_a));
  late_catalog.add_child(std::move(late_b));
  late_catalog.layout();
  expect(late_right->bounds().width == 0, "both-flex map starts at 0");
  late_left->set_preferred_size({288, 0});
  late_catalog.layout();
  expect(late_left->bounds().width == 288, "reseed locks catalog preferred");
  expect(late_right->bounds().width == 900 - kBar - 288,
         "map recovers leftover");

  // Markup shell: catalog_host preferred_size=288 but FillLayout+child reports
  // get_preferred_size=0 (Yoga width:100%). Must still leave Map|Data|3D room.
  Splitter markup_catalog_map(Splitter::Orientation::kHorizontal);
  markup_catalog_map.set_bounds({0, 0, 1200, 400});
  auto cat_host = std::make_unique<View>();
  auto map_host = std::make_unique<View>();
  View* cat_pane = cat_host.get();
  View* map_pane = map_host.get();
  cat_host->set_preferred_size({288, 0});
  cat_host->set_layout_manager(std::make_unique<FillLayout>());
  auto cat_inner = std::make_unique<View>();
  cat_inner->set_preferred_size({0, 0});
  cat_host->add_child(std::move(cat_inner));
  map_host->set_preferred_size({0, 0});
  map_host->set_layout_manager(std::make_unique<FillLayout>());
  auto map_inner = std::make_unique<View>();
  map_inner->set_preferred_size({0, 0});
  map_host->add_child(std::move(map_inner));
  markup_catalog_map.add_child(std::move(cat_host));
  markup_catalog_map.add_child(std::move(map_host));
  markup_catalog_map.layout();
  expect(cat_pane->bounds().width == 288,
         "markup catalog hint wins over layout preferred 0");
  expect(map_pane->bounds().width == 1200 - Splitter::kBarPx - 288,
         "map tabs keep leftover beside markup catalog");

  // Collapsed DiagnosticToolsPanel: preferred {0,0} secondary must not keep a
  // kMinPanePx remnant that paints into the status bar.
  Splitter tools_host(Splitter::Orientation::kVertical);
  tools_host.set_bounds({0, 0, 800, 600});
  auto tools_primary = std::make_unique<View>();
  auto tools_secondary = std::make_unique<View>();
  View* tools_work = tools_primary.get();
  View* tools_pane = tools_secondary.get();
  tools_primary->set_preferred_size({0, 0});
  tools_secondary->set_preferred_size({0, 0});
  tools_host.add_child(std::move(tools_primary));
  tools_host.add_child(std::move(tools_secondary));
  tools_host.layout();
  expect(tools_pane->bounds().height == 0, "collapsed tools height 0");
  expect(tools_work->bounds().height == 600 - Splitter::kBarPx,
         "work fills when tools 0");
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

  // Collapsed splitter secondary: preferred leaf under a zero-size parent
  // must not fail (DiagnosticToolsPanel starts at preferred {0,0}).
  View collapsed;
  collapsed.set_bounds({0, 200, 400, 0});
  auto leaf = std::make_unique<View>();
  leaf->set_preferred_size({120, 24});
  leaf->set_bounds({0, 200, 400, 0});
  collapsed.add_child(std::move(leaf));
  issues.clear();
  expect(collect_layout_violations(&collapsed, &issues) == 0,
         "zero-size under collapsed parent ok");

  expect(rect_non_negative({0, 0, 1, 1}), "non-neg ok");
  expect(!rect_non_negative({0, 0, -1, 1}), "neg width fails");
  expect(menu_item_metrics_ok(40, 28, 1.f), "menu metrics 1x");
  expect(!menu_item_metrics_ok(8, 10, 1.5f), "menu metrics too small");
}

void test_sibling_overlap_detection() {
  View host;
  host.set_bounds({0, 0, 200, 100});
  auto a = std::make_unique<View>();
  auto b = std::make_unique<View>();
  a->set_bounds({10, 10, 80, 40});
  b->set_bounds({50, 20, 80, 40});  // overlaps a
  host.add_child(std::move(a));
  host.add_child(std::move(b));
  std::vector<std::string> issues;
  expect(collect_sibling_overlaps(&host, &issues) > 0, "overlap detected");
  expect(!issues.empty(), "overlap code present");

  View clean;
  clean.set_bounds({0, 0, 200, 100});
  auto c = std::make_unique<View>();
  auto d = std::make_unique<View>();
  c->set_bounds({0, 0, 80, 40});
  d->set_bounds({0, 50, 80, 40});
  clean.add_child(std::move(c));
  clean.add_child(std::move(d));
  issues.clear();
  expect(collect_sibling_overlaps(&clean, &issues) == 0, "no overlap clean");
}

void test_gantt_lane_geom_spaced() {
  GanttLaneGeom g{};
  expect(compute_gantt_lane_geom(0, 200, 8, 5, &g), "geom ok");
  expect(g.lane_h >= 14, "min lane height");
  expect(g.lane_top == 8, "chrome inset");
  // Five labels must not share the same y.
  const int y0 = g.lane_top;
  const int y1 = g.lane_top + g.lane_h;
  expect(y1 - y0 >= 14, "lanes vertically spaced");
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
  // leftover = 300 - (60+80) = 160; flex = 80 + 160
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

void test_add_child_during_layout_keeps_dirty() {
  class AddOnceLayout : public LayoutManager {
   public:
    void layout(View* host) override {
      if (!host || host->child_count() != 1) {
        return;
      }
      auto extra = std::make_unique<View>();
      extra->set_preferred_size({10, 10});
      host->add_child(std::move(extra));
    }
  };

  View host;
  host.set_bounds({0, 0, 100, 80});
  host.set_layout_manager(std::make_unique<AddOnceLayout>());
  host.add_child(std::make_unique<View>());
  host.layout();
  expect(host.child_count() == 2, "layout can add a child");
  expect(host.needs_layout(), "add_child during layout stays dirty");
  host.layout();
  expect(!host.needs_layout(), "second layout pass clears dirty");
}

void test_layout_center_helper() {
  Rect outer = {0, 0, 400, 300};
  Rect inner = {100, 75, 200, 150};
  expect(rect_approximately_centered(inner, outer, 0), "view centers match");
  Rect skewed = {0, 0, 200, 150};
  expect(!rect_approximately_centered(skewed, outer, 10), "skew fails");
}

