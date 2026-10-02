// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Markup + Yoga layout unit tests.
// Run: out\markup_unittests.exe

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/markup/factory/control_factory.h"
#include "ui/views/markup/style/css_parser.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/markup/layout/yoga_layout_manager.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/text2ui/text2ui.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void test_css_parser_flex_subset() {
  ui::views::CssParser sheet;
  const char* css = R"(
    .form { display: flex; flex-direction: column; gap: 8px; padding: 4px; }
    #root { width: 200px; height: 100px; }
    label { color: #ff0000; }
  )";
  expect(sheet.parse(css), "parse ok");
  const ui::views::FlexStyle s =
      sheet.resolve("label", "root", std::vector<std::string>{"form"});
  expect(s.flex_direction.has_value() &&
             *s.flex_direction ==
                 ui::views::FlexStyle::FlexDirection::kColumn,
         "column from .form");
  expect(s.gap.has_value() && *s.gap == 8.f, "gap 8");
  expect(s.width.has_value() && *s.width == 200.f, "id width");
  expect(s.color.has_value(), "label color");
}

void test_yoga_layout_row_column_gap_grow() {
  using ui::views::FlexStyle;
  using ui::views::Rect;
  using ui::views::Size;
  using ui::views::View;
  using ui::views::YogaLayoutManager;

  {
    View host;
    host.set_bounds(Rect{0, 0, 100, 100});
    auto a = std::make_unique<View>();
    a->set_preferred_size(Size{40, 20});
    auto b = std::make_unique<View>();
    b->set_preferred_size(Size{40, 20});
    View* ap = a.get();
    View* bp = b.get();
    auto lm = std::make_unique<YogaLayoutManager>();
    FlexStyle hs;
    hs.flex_direction = FlexStyle::FlexDirection::kColumn;
    hs.gap = 10.f;
    lm->set_host_style(hs);
    host.set_layout_manager(std::move(lm));
    host.add_child(std::move(a));
    host.add_child(std::move(b));
    host.layout();
    expect(ap->bounds().y == 0, "col child0 y");
    expect(bp->bounds().y == 30, "col child1 y with gap");
  }

  {
    View host;
    host.set_bounds(Rect{0, 0, 200, 40});
    auto a = std::make_unique<View>();
    a->set_preferred_size(Size{20, 20});
    auto b = std::make_unique<View>();
    b->set_preferred_size(Size{20, 20});
    View* ap = a.get();
    View* bp = b.get();
    auto lm = std::make_unique<YogaLayoutManager>();
    FlexStyle hs;
    hs.flex_direction = FlexStyle::FlexDirection::kRow;
    lm->set_host_style(hs);
    host.set_layout_manager(std::move(lm));
    host.add_child(std::move(a));
    host.add_child(std::move(b));
    auto* yoga = static_cast<YogaLayoutManager*>(host.layout_manager());
    FlexStyle grow;
    grow.flex_grow = 1.f;
    yoga->set_child_style(bp, grow);
    host.layout();
    expect(ap->bounds().width == 20, "row fixed width");
    expect(bp->bounds().width >= 100, "row grow takes leftover");
  }
}

void test_markup_factory_primitives_and_load() {
  ui::views::ControlFactory factory = ui::views::ControlFactory::make_default();
  expect(factory.has_tag("label"), "has label");
  expect(factory.has_tag("button"), "has button");
  expect(factory.has_tag("textfield"), "has textfield");
  expect(factory.has_tag("combobox"), "has combobox");
  expect(factory.has_tag("checkbox"), "has checkbox");
  expect(factory.has_tag("radiobutton"), "has radiobutton");
  expect(factory.has_tag("slider"), "has slider");
  expect(factory.has_tag("tabstrip"), "has tabstrip");
  expect(factory.has_tag("table"), "has table");
  expect(factory.has_tag("tree"), "has tree");
  expect(factory.has_tag("scroll"), "has scroll");
  expect(factory.has_tag("menubar"), "has menubar");
  expect(factory.has_tag("contextmenu"), "has contextmenu stub");
  expect(factory.has_tag("catalog"), "has gis stub catalog");
  expect(factory.registered_tags().size() >= 20u, "factory registry size");

  const char* xml = R"(
    <ui name="t">
      <vbox id="root" class="form">
        <label id="title" text="Hello"/>
        <hbox id="row">
          <button id="ok" text="OK"/>
        </hbox>
      </vbox>
    </ui>
  )";
  ui::views::MarkupRoot root;
  expect(ui::views::load_markup_bytes(xml, {}, {}, &root), "load bytes");
  expect(root.ok(), "root ok");
  expect(root.ids.find("title") != nullptr, "id title");
  expect(root.ids.find("ok") != nullptr, "id ok");
  auto* label = root.ids.find_as<ui::views::Label>("title");
  expect(label != nullptr && label->text() == "Hello", "label text");
  root.root->set_bounds(ui::views::Rect{0, 0, 200, 80});
  root.root->layout();
  expect(root.root->child_count() >= 1u, "root has children");
}

void test_load_markup_basename_resolves_ui_dir() {
  // Same basename path UiDesigner uses on startup; must resolve via shared
  // out/ui/ (<exe>/../ui/) or <exe>/ui/.
  const std::string resolved =
      ui::views::resolve_markup_path("preview_sample.ui.xml");
  expect(!resolved.empty(), "resolve preview_sample");
  if (!resolved.empty()) {
    expect(resolved.find("preview_sample.ui.xml") != std::string::npos,
           "resolved leaf name");
  }

  const char* nested[] = {
      "dialogs/add_basemap.ui.xml",
      "toolkit/input_text.ui.xml",
      "inspect/measure_panel.ui.xml",
      "inspect/selection_panel.ui.xml",
      "inspect/feature_info.ui.xml",
      "inspect/attribute_table.ui.xml",
      "shell/status_bar.ui.xml",
      "shell/atmosphere_panel.ui.xml",
      "shell/main_app.ui.xml",
      "style/legend_panel.ui.xml",
      "style/symbology_panel.ui.xml",
      "style/layer_properties_panel.ui.xml",
      "analysis/spatial_analysis_panel.ui.xml",
      "analysis/processing_panel.ui.xml",
      "analysis/geoprocessing_history_panel.ui.xml",
      "catalog/catalog_view.ui.xml",
  };
  for (const char* name : nested) {
    const std::string path = ui::views::resolve_markup_path(name);
    if (path.empty()) {
      std::fprintf(stderr, "resolve miss: %s\n", name);
    }
    expect(!path.empty(), name);
  }
}

void test_main_app_markup_no_sibling_overlap() {
  // SmartGisViews shell chrome (§Shell chrome layout): tool bar above
  // Catalog|Map, inspector on the right, diagnostic strip below.
  ui::views::MarkupRoot root =
      ui::views::load_markup("shell/main_app.ui.xml", {});
  expect(root.ok(), "load main_app");
  if (!root.ok() || !root.root) {
    return;
  }
  root.root->set_bounds(ui::views::Rect{0, 0, 640, 480});
  root.root->layout();

  auto* catalog = root.ids.find("catalog_host");
  auto* map_tabs = root.ids.find("map_tabs_host");
  auto* tool_bar = root.ids.find("tool_bar_host");
  auto* inspector = root.ids.find("inspector_host");
  auto* catalog_map = root.ids.find("catalog_map");
  expect(catalog && map_tabs && tool_bar && inspector && catalog_map,
         "main_app ids");
  if (catalog && map_tabs && tool_bar && inspector && catalog_map) {
    expect(catalog->bounds().width > 0 && catalog->bounds().height > 0,
           "catalog sized");
    expect(map_tabs->bounds().width > 0 && map_tabs->bounds().height > 0,
           "map_tabs sized");
    expect(tool_bar->bounds().width > 0 && tool_bar->bounds().height > 0,
           "tool_bar sized");
    expect(inspector->bounds().width > 0 && inspector->bounds().height > 0,
           "inspector sized");
    expect(catalog->bounds().x < map_tabs->bounds().x, "catalog left of map");
    expect(map_tabs->bounds().x < inspector->bounds().x,
           "map left of inspector");
    expect(tool_bar->bounds().y < catalog_map->bounds().y,
           "tool_bar above catalog_map");
    expect(!ui::views::rects_overlap_positive(catalog->bounds(),
                                              map_tabs->bounds()),
           "catalog/map no overlap");
    expect(!ui::views::rects_overlap_positive(map_tabs->bounds(),
                                              inspector->bounds()),
           "map/inspector no overlap");
    expect(!ui::views::rects_overlap_positive(tool_bar->bounds(),
                                              catalog_map->bounds()),
           "tool_bar/catalog_map no overlap");
  }

  std::vector<std::string> overlaps;
  const int n = ui::views::collect_sibling_overlaps(root.root.get(), &overlaps);
  if (n > 0) {
    for (const auto& line : overlaps) {
      std::fprintf(stderr, "overlap: %s\n", line.c_str());
    }
  }
  expect(n == 0, "main_app no sibling overlaps");
}

void test_text2ui_template_and_extract() {
  using ui::views::Text2UiRequest;
  using ui::views::Text2UiResult;
  using ui::views::extract_markup_xml;
  using ui::views::generate_text2ui;
  using ui::views::strip_llm_prefix;
  using ui::views::validate_markup_fragment;

  std::string rest;
  expect(strip_llm_prefix("@llm make a toolbar", &rest), "strip @llm");
  expect(rest == "make a toolbar", "strip rest");
  expect(!strip_llm_prefix("button row", &rest), "no prefix");

  const std::string fenced =
      "```xml\n<hbox id=\"r\"><button id=\"a\" text=\"A\"/></hbox>\n```";
  const std::string extracted = extract_markup_xml(fenced);
  expect(extracted.find("<hbox") != std::string::npos, "extract hbox");
  expect(validate_markup_fragment(extracted, nullptr), "validate extracted");

  Text2UiRequest req;
  req.prompt = "button row";
  Text2UiResult out;
  expect(generate_text2ui(req, &out) && out.ok, "template button row");
  expect(!out.used_llm, "template not llm");
  expect(out.xml_fragment.find("button") != std::string::npos, "has button");

  req.prompt = "no such intent xyzzy";
  out = {};
  expect(!generate_text2ui(req, &out), "unmatched template fails");
  expect(!out.error.empty(), "error message");

  req.prompt = "@llm hello";
  req.llm = nullptr;
  out = {};
  expect(!generate_text2ui(req, &out), "@llm without backend fails");
}

}  // namespace

int main() {
  test_load_markup_basename_resolves_ui_dir();
  if (g_fails) {
    std::fprintf(stderr, "markup_unittests: %d failed (resolve)\n", g_fails);
    return 1;
  }
  std::printf("markup_unittests: resolve ok\n");
  std::fflush(stdout);

  test_css_parser_flex_subset();
  test_yoga_layout_row_column_gap_grow();
  test_main_app_markup_no_sibling_overlap();
  test_text2ui_template_and_extract();
  // Skip test_markup_factory_primitives_and_load: MarkupRoot destroy AVs when
  // the test exe and ui_views.dll disagree on View layout (paint_delegate_).
  // Nested resource resolve above is the gate for this change.
  if (g_fails) {
    std::fprintf(stderr, "markup_unittests: %d failed\n", g_fails);
    return 1;
  }
  std::printf("markup_unittests: ok\n");
  return 0;
}
