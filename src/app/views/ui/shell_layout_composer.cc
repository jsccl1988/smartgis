// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/shell_layout_composer.h"

#include "app/views/ui/browser_view.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include "app/views/browser/browser.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/debug/diagnostic_tools_panel.h"
#include "ui/gis/inspect/attribute_table.h"
#include "ui/gis/inspect/feature_info.h"
#include "ui/gis/shell/ambox_view.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/kernel/frame/frame_view.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/primitives/menu/menu_bar.h"

namespace app {
namespace {

void clear_children(ui::views::View* host) {
  if (!host) {
    return;
  }
  while (host->child_count() > 0) {
    (void)host->remove_child(host->child_at(0));
  }
}

void mount_fill(ui::views::View* host, std::unique_ptr<ui::views::View> child) {
  if (!host || !child) {
    return;
  }
  clear_children(host);
  host->set_layout_manager(std::make_unique<ui::views::FillLayout>());
  host->add_child(std::move(child));
}

std::unique_ptr<ui::views::View> make_inspector_placeholder() {
  auto page = std::make_unique<ui::views::View>();
  page->set_preferred_size({280, 180});
  return page;
}

}  // namespace

ShellLayoutComposer::ShellLayoutComposer(BrowserView* host) : host_(host) {}

void ShellLayoutComposer::build_contents() {
  if (build_from_markup()) {
    return;
  }
  std::fprintf(stderr,
               "shell: main_app.ui.xml failed — imperative build_contents\n");
  build_imperative();
}

bool ShellLayoutComposer::build_from_markup() {
  if (!host_) {
    return false;
  }
  ui::views::MarkupRoot loaded =
      ui::views::load_markup("shell/main_app.ui.xml", {});
  if (!loaded.ok() || !loaded.root) {
    if (!loaded.error.empty()) {
      std::fprintf(stderr, "shell: load_markup: %s\n", loaded.error.c_str());
    }
    return false;
  }

  auto* menu_bar = loaded.ids.find_as<ui::views::MenuBar>("menu_bar");
  auto* catalog_host = loaded.ids.find("catalog_host");
  auto* map_tabs_host = loaded.ids.find("map_tabs_host");
  auto* tool_bar_host = loaded.ids.find("tool_bar_host");
  auto* inspector_host = loaded.ids.find("inspector_host");
  auto* diagnostic_host = loaded.ids.find("diagnostic_host");
  auto* status_host = loaded.ids.find("status_host");
  auto* catalog_map = loaded.ids.find_as<ui::views::Splitter>("catalog_map");
  auto* main_split = loaded.ids.find_as<ui::views::Splitter>("main_split");
  auto* work = loaded.ids.find_as<ui::views::Splitter>("work");
  if (!menu_bar || !catalog_host || !map_tabs_host || !tool_bar_host ||
      !inspector_host || !diagnostic_host || !status_host || !catalog_map ||
      !main_split || !work) {
    std::fprintf(stderr, "shell: main_app.ui.xml missing required hosts\n");
    return false;
  }

  host_->menu_bar_ = menu_bar;
  // rebuild_menus → MenuBar::refresh_preferred_size (non-zero width). Do not
  // pin {0, 32}: Yoga measure with width 0 collapses File/Edit/View/Layer
  // (CSS width:100% is rejected by the px parser).
  host_->rebuild_menus();

  auto catalog = std::make_unique<ui::views::CatalogView>();
  catalog->set_preferred_size({300, 0});
  catalog->set_title("Catalog");
  host_->catalog_ = catalog.get();
  // Top headers (QGIS/ArcGIS Pro): bottom-aligned catalog+map+inspector strips
  // merged into one overcrowded band with dual active underlines.
  if (host_->catalog_->source_tabs()) {
    host_->catalog_->source_tabs()->set_header_placement(
        ui::views::TabStrip::HeaderPlacement::kTop);
  }
  catalog_host->set_preferred_size({300, 0});
  mount_fill(catalog_host, std::move(catalog));
  host_->wire_catalog();

  auto map_tabs = std::make_unique<ui::views::TabStrip>();
  auto map_edit = std::make_unique<ui::views::DrawHost>();
  auto map_scene = std::make_unique<ui::views::DrawHost>();
  host_->map_edit_ = map_edit.get();
  host_->map_data_ = nullptr;
  host_->map_scene_ = map_scene.get();
  host_->map_edit_->set_role(ui::views::DrawHost::Role::kMapEdit);
  host_->map_scene_->set_role(ui::views::DrawHost::Role::kScene3d);
  map_tabs->add_tab("Map", std::move(map_edit));
  map_tabs->add_tab("3D", std::move(map_scene));
  map_tabs->set_header_placement(ui::views::TabStrip::HeaderPlacement::kTop);
  map_tabs->set_preferred_size({0, 0});
  map_tabs->set_change([this](int i) { host_->switch_map_tab(i); });
  host_->map_tabs_ = map_tabs.get();
  map_tabs_host->set_preferred_size({0, 0});
  mount_fill(map_tabs_host, std::move(map_tabs));

  catalog_map->set_preferred_size({0, 0});
  host_->catalog_map_ = catalog_map;

  auto tool_bar = std::make_unique<ui::views::AmboxView>();
  tool_bar->set_orientation(ui::views::AmboxView::Orientation::kHorizontal);
  // 48 DIP: glyph+label row must clear clip at 125–200% DPI (was 40).
  // Width 0 lets Yoga shrink-wrap packed chips on the map stack.
  tool_bar->set_preferred_size({0, 48});
  host_->ambox_ = tool_bar.get();
  host_->ambox_->set_command_handler([this](const std::string& id) {
    if (host_->browser_->plugins() && host_->browser_->plugins()->execute(id)) {
      return;
    }
    host_->browser_->run_tool_command(id);
  });
  tool_bar_host->set_preferred_size({0, 48});
  mount_fill(tool_bar_host, std::move(tool_bar));
  host_->populate_ambox();

  auto side = std::make_unique<ui::views::TabStrip>();
  // Idle Feature copy fits ~280 DIP; 400 squeezed the map for empty identify.
  constexpr int kInspectorWDip = 280;
  side->set_preferred_size({kInspectorWDip, 0});
  auto ambox_page = std::make_unique<ui::views::AmboxView>();
  ambox_page->set_preferred_size({kInspectorWDip, 0});
  ambox_page->set_command_handler([this](const std::string& id) {
    if (host_->browser_->plugins() && host_->browser_->plugins()->execute(id)) {
      return;
    }
    host_->browser_->run_tool_command(id);
  });
  host_->side_ambox_ = ambox_page.get();
  side->set_header_placement(ui::views::TabStrip::HeaderPlacement::kTop);
  side->add_tab("Tools", std::move(ambox_page));
  host_->feature_info_tab_ =
      side->add_tab("Feature", make_inspector_placeholder());
  side->add_tab("Attrs", make_inspector_placeholder());
  host_->measure_tab_ = side->add_tab("Measure", make_inspector_placeholder());
  host_->selection_tab_ =
      side->add_tab("Selection", make_inspector_placeholder());
  host_->layer_props_tab_ = side->add_tab("Layer", make_inspector_placeholder());
  host_->legend_tab_ = side->add_tab("Legend", make_inspector_placeholder());
  host_->spatial_analysis_tab_ =
      side->add_tab("Analysis", make_inspector_placeholder());
  host_->processing_tab_ =
      side->add_tab("Processing", make_inspector_placeholder());
  host_->playback_tab_ = side->add_tab("Play", make_inspector_placeholder());
  host_->report_tab_ = side->add_tab("Report", make_inspector_placeholder());
  host_->atmosphere_tab_ =
      side->add_tab("Atmosphere", make_inspector_placeholder());
  side->set_change([this](int i) { host_->ensure_inspector_tab(i); });
  side->set_active(0);  // Tools. Identify switches to Feature.
  host_->inspector_tabs_ = side.get();
  inspector_host->set_preferred_size({kInspectorWDip, 0});
  mount_fill(inspector_host, std::move(side));

  work->set_preferred_size({0, 0});
  main_split->set_preferred_size({0, 0});
  // Flex map column absorbs growth; inspector stays preferred dock width.
  // tool_bar CSS uses min-height (not fixed height) so Yoga measures from
  // preferred_size — which scales with DPI — instead of locking 40 physical px
  // that let Catalog paint through an undersized strip.
  if (ui::views::View* map_column = loaded.ids.find("map_column")) {
    map_column->set_preferred_size({0, 0});
  }

  auto diagnostic_tools = ui::views::make_diagnostic_tools_panel();
  host_->diagnostic_tools_ = diagnostic_tools.get();
  // Product shell shows Diagnostic Tools. Default Output — Trace gantt
  // previously consumed the map work area on interact/scene/data captures.
  host_->diagnostic_tools_->set_visible_tools(true);
  host_->diagnostic_tools_->set_active_tab(0);
  // Prefer panel DIP→px metrics; do not pin a smaller host that clips the
  // Output/Console/Trace/Memory body under the toolbar.
  {
    const int kDiagH =
        host_->diagnostic_tools_->preferred_size().height > 0
            ? host_->diagnostic_tools_->preferred_size().height
            : 200;
    host_->diagnostic_tools_->set_preferred_size({0, kDiagH});
    diagnostic_host->set_preferred_size({0, kDiagH});
  }
  mount_fill(diagnostic_host, std::move(diagnostic_tools));
  host_->wire_debug_console();
  host_->attach_plugin_shell_ui();

  auto status = std::make_unique<ui::views::StatusBar>();
  host_->status_bar_ = status.get();
  status_host->set_preferred_size({0, 32});
  mount_fill(status_host, std::move(status));
  if (host_->status_bar_) {
    host_->status_bar_->set_message("Ready");
    host_->status_bar_->set_scale_text("1:1000");
    host_->status_bar_->set_crs_text("CRS");
    host_->status_bar_->set_coord_text("XY");
  }

  // Prefer flex fill over designer CSS fixed canvas size.
  loaded.root->set_preferred_size({0, 0});

  auto frame = std::make_unique<ui::views::FrameView>();
  frame->set_title("SmartGIS Views");
  frame->set_can_maximize(true);
  frame->set_client(std::move(loaded.root));
  host_->widget_.set_contents_view(std::move(frame));
  // After attach, preferred sizes are already DPI-scaled. Re-pin with
  // dip_to_px — raw DIP constants here crushed the Feature dock under HiDPI
  // (visual_review: "Click the map to ide…").
  const float scale = host_->widget_.device_scale_factor();
  const int inspector_w = ui::views::dip_to_px(kInspectorWDip, scale);
  // Reseed after preferred sizes settle so catalog is not locked at kMinPanePx
  // from a create-time zero/tiny host.
  catalog_map->reseed();
  // Re-pin inspector dock width before work reseed so TabStrip layout measure
  // cannot inflate the secondary pane past the product dock.
  inspector_host->set_preferred_size({inspector_w, 0});
  if (host_->inspector_tabs_) {
    host_->inspector_tabs_->set_preferred_size({inspector_w, 0});
  }
  work->reseed();
  main_split->reseed();
  // Re-pin diagnostic dock after reseed — keep host+child aligned.
  if (host_->diagnostic_tools_) {
    const int kDiagH =
        host_->diagnostic_tools_->preferred_size().height > 0
            ? host_->diagnostic_tools_->preferred_size().height
            : ui::views::dip_to_px(200, scale);
    diagnostic_host->set_preferred_size({0, kDiagH});
    host_->diagnostic_tools_->set_preferred_size({0, kDiagH});
    main_split->reseed();
  }
  return true;
}

void ShellLayoutComposer::build_imperative() {
  auto root = std::make_unique<ui::views::View>();
  auto root_box = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kVertical);

  auto menu = std::make_unique<ui::views::MenuBar>();
  host_->menu_bar_ = menu.get();
  host_->rebuild_menus();

  auto catalog = std::make_unique<ui::views::CatalogView>();
  catalog->set_preferred_size({300, 0});
  catalog->set_title("Catalog");
  host_->catalog_ = catalog.get();
  if (host_->catalog_->source_tabs()) {
    host_->catalog_->source_tabs()->set_header_placement(
        ui::views::TabStrip::HeaderPlacement::kTop);
  }
  host_->wire_catalog();

  auto map_tabs = std::make_unique<ui::views::TabStrip>();
  auto map_edit = std::make_unique<ui::views::DrawHost>();
  auto map_scene = std::make_unique<ui::views::DrawHost>();
  host_->map_edit_ = map_edit.get();
  host_->map_data_ = nullptr;
  host_->map_scene_ = map_scene.get();
  host_->map_edit_->set_role(ui::views::DrawHost::Role::kMapEdit);
  host_->map_scene_->set_role(ui::views::DrawHost::Role::kScene3d);
  map_tabs->add_tab("Map", std::move(map_edit));
  map_tabs->add_tab("3D", std::move(map_scene));
  map_tabs->set_header_placement(ui::views::TabStrip::HeaderPlacement::kTop);
  map_tabs->set_preferred_size({0, 0});
  map_tabs->set_change([this](int i) { host_->switch_map_tab(i); });
  host_->map_tabs_ = map_tabs.get();

  auto tool_bar = std::make_unique<ui::views::AmboxView>();
  tool_bar->set_orientation(ui::views::AmboxView::Orientation::kHorizontal);
  tool_bar->set_preferred_size({0, 48});
  host_->ambox_ = tool_bar.get();
  host_->ambox_->set_command_handler([this](const std::string& id) {
    if (host_->browser_->plugins() && host_->browser_->plugins()->execute(id)) {
      return;
    }
    host_->browser_->run_tool_command(id);
  });
  host_->populate_ambox();

  auto catalog_map = std::make_unique<ui::views::Splitter>(
      ui::views::Splitter::Orientation::kHorizontal);
  catalog_map->set_preferred_size({0, 0});
  catalog_map->add_child(std::move(catalog));
  catalog_map->add_child(std::move(map_tabs));
  host_->catalog_map_ = catalog_map.get();

  auto map_column = std::make_unique<ui::views::View>();
  auto map_column_box = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kVertical);
  map_column_box->set_flex_for_view(catalog_map.get(), 1);
  map_column->set_layout_manager(std::move(map_column_box));
  map_column->set_preferred_size({0, 0});
  map_column->add_child(std::move(tool_bar));
  map_column->add_child(std::move(catalog_map));

  constexpr int kInspectorWDip = 280;
  auto side = std::make_unique<ui::views::TabStrip>();
  side->set_preferred_size({kInspectorWDip, 0});
  auto ambox_page = std::make_unique<ui::views::AmboxView>();
  ambox_page->set_preferred_size({kInspectorWDip, 0});
  ambox_page->set_command_handler([this](const std::string& id) {
    if (host_->browser_->plugins() && host_->browser_->plugins()->execute(id)) {
      return;
    }
    host_->browser_->run_tool_command(id);
  });
  host_->side_ambox_ = ambox_page.get();
  side->set_header_placement(ui::views::TabStrip::HeaderPlacement::kTop);
  side->add_tab("Tools", std::move(ambox_page));
  host_->feature_info_tab_ =
      side->add_tab("Feature", make_inspector_placeholder());
  side->add_tab("Attrs", make_inspector_placeholder());
  host_->measure_tab_ = side->add_tab("Measure", make_inspector_placeholder());
  host_->selection_tab_ =
      side->add_tab("Selection", make_inspector_placeholder());
  host_->layer_props_tab_ = side->add_tab("Layer", make_inspector_placeholder());
  host_->legend_tab_ = side->add_tab("Legend", make_inspector_placeholder());
  host_->spatial_analysis_tab_ =
      side->add_tab("Analysis", make_inspector_placeholder());
  host_->processing_tab_ =
      side->add_tab("Processing", make_inspector_placeholder());
  host_->playback_tab_ = side->add_tab("Play", make_inspector_placeholder());
  host_->report_tab_ = side->add_tab("Report", make_inspector_placeholder());
  host_->atmosphere_tab_ =
      side->add_tab("Atmosphere", make_inspector_placeholder());
  side->set_change([this](int i) { host_->ensure_inspector_tab(i); });
  side->set_active(0);  // Tools. Identify switches to Feature.
  host_->inspector_tabs_ = side.get();

  auto work = std::make_unique<ui::views::Splitter>(
      ui::views::Splitter::Orientation::kHorizontal);
  work->set_preferred_size({0, 0});
  work->add_child(std::move(map_column));
  work->add_child(std::move(side));

  auto diagnostic_tools = ui::views::make_diagnostic_tools_panel();
  host_->diagnostic_tools_ = diagnostic_tools.get();
  host_->diagnostic_tools_->set_visible_tools(true);
  host_->diagnostic_tools_->set_active_tab(0);
  host_->wire_debug_console();
  host_->attach_plugin_shell_ui();

  auto main_split = std::make_unique<ui::views::Splitter>(
      ui::views::Splitter::Orientation::kVertical);
  main_split->set_preferred_size({0, 0});
  main_split->add_child(std::move(work));
  main_split->add_child(std::move(diagnostic_tools));

  auto status = std::make_unique<ui::views::StatusBar>();
  host_->status_bar_ = status.get();
  if (host_->status_bar_) {
    host_->status_bar_->set_message("Ready");
    host_->status_bar_->set_scale_text("1:1000");
    host_->status_bar_->set_crs_text("CRS");
    host_->status_bar_->set_coord_text("XY");
  }

  root_box->set_flex_for_view(main_split.get(), 1);
  root->set_layout_manager(std::move(root_box));
  root->add_child(std::move(menu));
  root->add_child(std::move(main_split));
  root->add_child(std::move(status));

  auto frame = std::make_unique<ui::views::FrameView>();
  frame->set_title("SmartGIS Views");
  frame->set_can_maximize(true);
  frame->set_client(std::move(root));
  host_->widget_.set_contents_view(std::move(frame));
}

}  // namespace app
