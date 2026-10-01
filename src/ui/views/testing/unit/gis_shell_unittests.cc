// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// GIS shell catalog / layer tree / ambox / status unit tests.

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "tool/command/command.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/catalog/layer_tree.h"
#include "ui/gis/inspect/feature_info.h"
#include "ui/gis/shell/ambox_view.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/collection/scroll_view.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/testing/forensics/ui_forensics.h"
#include "ui/views/testing/unit/views_unit_helpers.h"

using namespace ui::views;

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

void test_ambox_buttons_not_collapsed() {
  AmboxView ambox;
  ambox.set_bounds({0, 0, 200, 400});
  ambox.populate_from_commands(nullptr);
  ambox.layout();
  expect(ambox.groups().size() >= 2, "ambox groups");
  expect(ambox.preferred_size().height >= 28, "ambox preferred height");
  std::vector<std::string> issues;
  expect(collect_layout_violations(&ambox, &issues) == 0,
         "ambox layout clean");
}

void test_forensics_dump_writes_manifest() {
  View root;
  root.set_bounds({0, 0, 120, 80});
  auto child = std::make_unique<View>();
  child->set_bounds({8, 8, 40, 24});
  root.add_child(std::move(child));
  ForensicsDumpOptions opt;
  opt.root = std::filesystem::temp_directory_path() / "smartgis_ui_forensics";
  opt.run_id = "unit_probe";
  opt.write_png = true;
  opt.frame_width = 120;
  opt.frame_height = 80;
  const ForensicsDumpResult r = dump_ui_forensics(&root, {}, opt);
  expect(r.ok, "forensics dump ok");
  expect(std::filesystem::exists(r.dir / "manifest.json"), "manifest exists");
  expect(std::filesystem::exists(r.dir / "layout_issues.txt"), "issues exists");
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

