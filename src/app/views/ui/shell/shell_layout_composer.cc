// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/shell/shell_layout_composer.h"

#include "app/views/ui/browser_view.h"
#include "app/views/ui/horizon/inspector_group_bar.h"
#include "app/views/ui/horizon/inspector_host_composer.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include "app/views/browser/browser.h"
#include "app/views/browser/plugin/shell/plugin_shell.h"
#include "base/trace/event/process_trace.h"
#include "ui/gis/catalog/catalog_view.h"
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
#include "ui/gis/shell/main_app_markup.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/primitives/menu/menu_bar.h"

namespace app {
namespace {

// Must match show_shell re-pin (catalog 288 / inspector 240). Wider Feature
// docks eat the right 22% map band (ui_shell_dark inspector_not_huge).
constexpr int kCatalogWDip = 288;
constexpr int kInspectorWDip = 240;

ui::views::View* make_inspector_placeholder() {
  ui::views::View* page = ui::views::view_new();
  page->set_preferred_size({280, 180});
  return page;
}

ui::views::CatalogView* make_catalog_page(const char* title) {
  ui::views::CatalogView* catalog = ui::views::CatalogView::create_raw();
  catalog->set_preferred_size({kCatalogWDip, 0});
  catalog->set_title(title ? title : "Catalog");
  return catalog;
}

// Cross-DLL AmboxView callback: no std::function from EXE into ui_views.dll.
void ambox_command_trampoline(void* baton, const char* id) {
  auto* bv = static_cast<BrowserView*>(baton);
  if (!bv || !id || !id[0]) {
    return;
  }
  Browser* browser = bv->browser();
  if (!browser) {
    return;
  }
  if (browser->plugins() && browser->plugins()->execute(id)) {
    return;
  }
  browser->run_tool_command(id);
}

}  // namespace

ShellLayoutComposer::ShellLayoutComposer(BrowserView* host) : host_(host) {}

void ShellLayoutComposer::map_tab_change_trampoline(void* baton, int index) {
  auto* bv = static_cast<BrowserView*>(baton);
  if (bv) {
    bv->switch_map_tab(index);
  }
}

void ShellLayoutComposer::inspector_tab_change_trampoline(void* baton,
                                                          int index) {
  auto* bv = static_cast<BrowserView*>(baton);
  if (bv) {
    bv->on_inspector_tab_changed(index);
  }
}

ui::views::TabStrip* ShellLayoutComposer::make_catalog_mode_tabs() {
  ui::views::TabStrip* mode = ui::views::tab_strip_new();
  mode->set_preferred_size({kCatalogWDip, 0});
  mode->set_header_placement(ui::views::TabStrip::HeaderPlacement::kTop);
  ui::views::CatalogView* c2d = make_catalog_page("Catalog");
  ui::views::CatalogView* c3d = make_catalog_page("Catalog");
  host_->catalog_2d_ = c2d;
  host_->catalog_3d_ = c3d;
  host_->catalog_ = host_->catalog_2d_;
  ui::views::tab_strip_adopt_tab(mode, "2D", c2d);
  ui::views::tab_strip_adopt_tab(mode, "3D", c3d);
  host_->catalog_mode_tabs_ = mode;
  return mode;
}

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
  ui::views::MainAppMarkupSession* session = nullptr;
  {
    BASE_TRACE_EVENT("BuildContents.MainMarkup", "startup");
    session = ui::views::main_app_markup_load("shell/main_app.ui.xml");
    if (!session || !ui::views::main_app_markup_ok(session)) {
      if (session) {
        std::fprintf(stderr, "shell: load_markup: %s\n",
                     ui::views::main_app_markup_error(session));
      }
      ui::views::main_app_markup_release(session);
      return false;
    }
  }

  ui::views::MainAppMarkupHosts hosts{};
  if (!ui::views::main_app_markup_fill_hosts(session, &hosts)) {
    std::fprintf(stderr, "shell: main_app.ui.xml missing required hosts\n");
    ui::views::main_app_markup_release(session);
    return false;
  }

  ui::views::MenuBar* menu_bar = hosts.menu_bar;
  ui::views::View* catalog_host = hosts.catalog_host;
  ui::views::View* map_tabs_host = hosts.map_tabs_host;
  ui::views::View* tool_bar_host = hosts.tool_bar_host;
  ui::views::View* inspector_host = hosts.inspector_host;
  ui::views::View* diagnostic_host = hosts.diagnostic_host;
  ui::views::View* status_host = hosts.status_host;
  ui::views::Splitter* catalog_map = hosts.catalog_map;
  ui::views::Splitter* main_split = hosts.main_split;
  ui::views::Splitter* work = hosts.work;

  host_->menu_bar_ = menu_bar;
  // rebuild_menus → MenuBar::refresh_preferred_size (non-zero width). Do not
  // pin {0, 32}: Yoga measure with width 0 collapses File/Edit/View/Layer
  // (CSS width:100% is rejected by the px parser).
  {
    BASE_TRACE_EVENT("BuildContents.RebuildMenus", "startup");
    host_->rebuild_menus();
  }

  // Dual Catalog: outer 2D|3D mirrors Map|3D; each page is a full CatalogView.
  {
    BASE_TRACE_EVENT("BuildContents.Catalog", "startup");
    ui::views::TabStrip* catalog_modes = make_catalog_mode_tabs();
    catalog_host->set_preferred_size({kCatalogWDip, 0});
    ui::views::view_mount_fill_child(catalog_host, catalog_modes);
    host_->wire_catalog();
  }

  {
    BASE_TRACE_EVENT("BuildContents.MapTabs", "startup");
    ui::views::TabStrip* map_tabs = ui::views::tab_strip_new();
    ui::views::DrawHost* map_edit = ui::views::draw_host_new();
    ui::views::DrawHost* gis_scene = ui::views::draw_host_new();
    host_->map_edit_ = map_edit;
    host_->map_data_ = nullptr;
    host_->map_scene_ = gis_scene;
    host_->map_edit_->set_role(ui::views::DrawHost::Role::kMapEdit);
    host_->map_scene_->set_role(ui::views::DrawHost::Role::kScene3d);
    ui::views::tab_strip_adopt_tab(map_tabs, "Map", map_edit);
    ui::views::tab_strip_adopt_tab(map_tabs, "3D", gis_scene);
    map_tabs->set_header_placement(ui::views::TabStrip::HeaderPlacement::kTop);
    map_tabs->set_preferred_size({0, 0});
    map_tabs->set_change_fn(&ShellLayoutComposer::map_tab_change_trampoline,
                            host_);
    host_->map_tabs_ = map_tabs;
    map_tabs_host->set_preferred_size({0, 0});
    ui::views::view_mount_fill_child(map_tabs_host, map_tabs);
  }

  catalog_map->set_preferred_size({0, 0});
  host_->catalog_map_ = catalog_map;

  {
    BASE_TRACE_EVENT("BuildContents.AmboxInspector", "startup");
    {
      BASE_TRACE_EVENT("BuildContents.AmboxCtor", "startup");
      ui::views::AmboxView* tool_bar = ui::views::ambox_view_new();
      tool_bar->set_orientation(ui::views::AmboxView::Orientation::kHorizontal);
      // 48 DIP: glyph+label row must clear clip at 125–200% DPI.
      // Width 0 lets Yoga shrink-wrap packed chips on the map stack.
      // .tool_bar CSS height caps the band; do not copy a stretched host height.
      tool_bar->set_preferred_size({0, 48});
      host_->ambox_ = tool_bar;
      host_->ambox_->set_command_handler_fn(&ambox_command_trampoline, host_);
      tool_bar_host->set_preferred_size({0, 48});
      ui::views::view_mount_fill_child(tool_bar_host, tool_bar);

      ui::views::TabStrip* side = ui::views::tab_strip_new();
      // Idle Feature copy; 320 matches show_shell so attach Init is the shown
      // width.
      side->set_preferred_size({kInspectorWDip, 0});
      ui::views::AmboxView* ambox_page = ui::views::ambox_view_new();
      ambox_page->set_preferred_size({kInspectorWDip, 0});
      ambox_page->set_command_handler_fn(&ambox_command_trampoline, host_);
      host_->side_ambox_ = ambox_page;
      side->set_header_placement(ui::views::TabStrip::HeaderPlacement::kTop);
      ui::views::tab_strip_adopt_tab(side, "Tools", ambox_page);
      host_->feature_info_tab_ =
          ui::views::tab_strip_adopt_tab(side, "Feature",
                                         make_inspector_placeholder());
      ui::views::tab_strip_adopt_tab(side, "Attrs", make_inspector_placeholder());
      host_->measure_tab_ =
          ui::views::tab_strip_adopt_tab(side, "Measure",
                                         make_inspector_placeholder());
      host_->selection_tab_ =
          ui::views::tab_strip_adopt_tab(side, "Selection",
                                         make_inspector_placeholder());
      host_->layer_props_tab_ =
          ui::views::tab_strip_adopt_tab(side, "Layer",
                                         make_inspector_placeholder());
      host_->legend_tab_ =
          ui::views::tab_strip_adopt_tab(side, "Legend",
                                         make_inspector_placeholder());
      host_->spatial_analysis_tab_ =
          ui::views::tab_strip_adopt_tab(side, "Analysis",
                                         make_inspector_placeholder());
      host_->processing_tab_ =
          ui::views::tab_strip_adopt_tab(side, "Processing",
                                         make_inspector_placeholder());
      host_->playback_tab_ =
          ui::views::tab_strip_adopt_tab(side, "Play",
                                         make_inspector_placeholder());
      host_->report_tab_ =
          ui::views::tab_strip_adopt_tab(side, "Report",
                                         make_inspector_placeholder());
      host_->atmosphere_tab_ =
          ui::views::tab_strip_adopt_tab(side, "Atmosphere",
                                         make_inspector_placeholder());
      host_->plugins_tab_ =
          ui::views::tab_strip_adopt_tab(side, "Plugins",
                                         make_inspector_placeholder());
      side->set_active(0);  // Tools. Identify switches to Feature.
      host_->inspector_tabs_ = side;
      inspector_host->set_preferred_size({kInspectorWDip, 0});
      ui::views::view_mount_fill_child(inspector_host, wrap_inspector(side));
    }
    // After side_ambox_ is live — AmboxView ctor seeds Select/Edit; map bar
    // keeps them, right dock is cleared to plugin groups only (visual_review
    // #3).
    {
      BASE_TRACE_EVENT("BuildContents.PopulateAmbox", "startup");
      host_->populate_ambox();
    }
  }

  work->set_preferred_size({0, 0});
  main_split->set_preferred_size({0, 0});
  // Flex map column absorbs growth; inspector stays preferred dock width.
  // .tool_bar height/max-height are 48 DIP (scaled). Do not drop the cap:
  // the shared Ambox markup is a 240 DIP dock and will reopen the black band.
  if (hosts.map_column) {
    hosts.map_column->set_preferred_size({0, 0});
  }

  {
    BASE_TRACE_EVENT("BuildContents.DiagnosticTools", "startup");
    ui::views::DiagnosticToolsPanel* diagnostic_tools =
        ui::views::diagnostic_tools_panel_new();
    host_->diagnostic_tools_ = diagnostic_tools;
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
              : 260;
      host_->diagnostic_tools_->set_preferred_size({0, kDiagH});
      diagnostic_host->set_preferred_size({0, kDiagH});
    }
    ui::views::view_mount_fill_child(diagnostic_host, diagnostic_tools);
    host_->wire_debug_console();
    host_->attach_plugin_shell_ui();
  }
  if (host_->inspector_host_) {
    // Plugins is the default dock page. Catalog fill is deferred until after
    // the shell is up (see schedule_deferred_plugin_panel).
    host_->inspector_host_->note_tab_activated(host_->plugins_tab_);
    host_->inspector_host_->show_inspector_group(
        InspectorGroupBar::Group::kExtend, /*materialize=*/false);
  }

  {
    BASE_TRACE_EVENT("BuildContents.StatusFrame", "startup");
    {
      BASE_TRACE_EVENT("BuildContents.StatusBar", "startup");
      ui::views::StatusBar* status = ui::views::status_bar_new();
      host_->status_bar_ = status;
      status_host->set_preferred_size({0, 32});
      ui::views::view_mount_fill_child(status_host, status);
      if (host_->status_bar_) {
        host_->status_bar_->set_message("Ready");
        host_->status_bar_->set_scale_text("1:1000");
        host_->status_bar_->set_crs_text("CRS");
        host_->status_bar_->set_coord_text("XY");
      }
    }

    ui::views::main_app_markup_set_root_preferred_size(session, 0, 0);

    {
      BASE_TRACE_EVENT("BuildContents.SetContents", "startup");
      ui::views::FrameView* frame = ui::views::make_frame_view_raw();
      frame->set_title("SmartGIS Views");
      frame->set_can_maximize(true);
      frame->adopt_client(ui::views::main_app_markup_take_root(session));
      host_->widget_.adopt_contents_view(frame);
    }
    // After attach, preferred sizes are already DPI-scaled. Re-pin with
    // dip_to_px — raw DIP constants here crushed the Feature dock under HiDPI
    // (visual_review: "Click the map to ide…").
    {
      BASE_TRACE_EVENT("BuildContents.Reseed", "startup");
      const float scale = host_->widget_.device_scale_factor();
      const int inspector_w = ui::views::dip_to_px(kInspectorWDip, scale);
      // Pin preferred sizes first, then one reseed cascade — a second
      // main_split->reseed after diagnostic re-pin duplicated Yoga layout
      // (~seconds on Debug cold BuildContents.StatusFrame).
      inspector_host->set_preferred_size({inspector_w, 0});
      if (host_->inspector_tabs_) {
        host_->inspector_tabs_->set_preferred_size({inspector_w, 0});
      }
      if (host_->inspector_host_) {
        host_->inspector_host_->set_group_bar_extent(
            inspector_w, ui::views::dip_to_px(26, scale));
      }
      if (host_->diagnostic_tools_) {
        const int kDiagH =
            host_->diagnostic_tools_->preferred_size().height > 0
                ? host_->diagnostic_tools_->preferred_size().height
                : ui::views::dip_to_px(260, scale);
        diagnostic_host->set_preferred_size({0, kDiagH});
        host_->diagnostic_tools_->set_preferred_size({0, kDiagH});
      }
      // Reseed after preferred sizes settle so catalog is not locked at
      // kMinPanePx from a create-time zero/tiny host.
      catalog_map->reseed();
      work->reseed();
      main_split->reseed();
    }
  }
  ui::views::main_app_markup_release(session);
  return true;
}

void ShellLayoutComposer::build_imperative() {
  ui::views::View* root = ui::views::view_new();
  ui::views::BoxLayout* root_box = nullptr;

  ui::views::MenuBar* menu = ui::views::menu_bar_new();
  host_->menu_bar_ = menu;
  host_->rebuild_menus();

  ui::views::TabStrip* catalog_modes = make_catalog_mode_tabs();
  host_->wire_catalog();

  ui::views::TabStrip* map_tabs = ui::views::tab_strip_new();
  ui::views::DrawHost* map_edit = ui::views::draw_host_new();
  ui::views::DrawHost* gis_scene = ui::views::draw_host_new();
  host_->map_edit_ = map_edit;
  host_->map_data_ = nullptr;
  host_->map_scene_ = gis_scene;
  host_->map_edit_->set_role(ui::views::DrawHost::Role::kMapEdit);
  host_->map_scene_->set_role(ui::views::DrawHost::Role::kScene3d);
  ui::views::tab_strip_adopt_tab(map_tabs, "Map", map_edit);
  ui::views::tab_strip_adopt_tab(map_tabs, "3D", gis_scene);
  map_tabs->set_header_placement(ui::views::TabStrip::HeaderPlacement::kTop);
  map_tabs->set_preferred_size({0, 0});
  map_tabs->set_change_fn(&ShellLayoutComposer::map_tab_change_trampoline,
                          host_);
  host_->map_tabs_ = map_tabs;

  ui::views::AmboxView* tool_bar = ui::views::ambox_view_new();
  tool_bar->set_orientation(ui::views::AmboxView::Orientation::kHorizontal);
  tool_bar->set_preferred_size({0, 48});
  host_->ambox_ = tool_bar;
  host_->ambox_->set_command_handler_fn(&ambox_command_trampoline, host_);

  ui::views::Splitter* catalog_map = ui::views::splitter_new(
      ui::views::Splitter::Orientation::kHorizontal);
  catalog_map->set_preferred_size({0, 0});
  ui::views::view_adopt_child(catalog_map, catalog_modes);
  ui::views::view_adopt_child(catalog_map, map_tabs);
  host_->catalog_map_ = catalog_map;

  ui::views::View* map_column = ui::views::view_new();
  ui::views::BoxLayout* map_column_box = ui::views::install_box_layout(
      map_column, ui::views::BoxLayout::Orientation::kVertical);
  if (map_column_box) {
    map_column_box->set_flex_for_view(catalog_map, 1);
  }
  map_column->set_preferred_size({0, 0});
  ui::views::view_adopt_child(map_column, tool_bar);
  ui::views::view_adopt_child(map_column, catalog_map);

  ui::views::TabStrip* side = ui::views::tab_strip_new();
  side->set_preferred_size({kInspectorWDip, 0});
  ui::views::AmboxView* ambox_page = ui::views::ambox_view_new();
  ambox_page->set_preferred_size({kInspectorWDip, 0});
  ambox_page->set_command_handler_fn(&ambox_command_trampoline, host_);
  host_->side_ambox_ = ambox_page;
  side->set_header_placement(ui::views::TabStrip::HeaderPlacement::kTop);
  ui::views::tab_strip_adopt_tab(side, "Tools", ambox_page);
  host_->feature_info_tab_ =
      ui::views::tab_strip_adopt_tab(side, "Feature", make_inspector_placeholder());
  ui::views::tab_strip_adopt_tab(side, "Attrs", make_inspector_placeholder());
  host_->measure_tab_ =
      ui::views::tab_strip_adopt_tab(side, "Measure", make_inspector_placeholder());
  host_->selection_tab_ =
      ui::views::tab_strip_adopt_tab(side, "Selection", make_inspector_placeholder());
  host_->layer_props_tab_ =
      ui::views::tab_strip_adopt_tab(side, "Layer", make_inspector_placeholder());
  host_->legend_tab_ =
      ui::views::tab_strip_adopt_tab(side, "Legend", make_inspector_placeholder());
  host_->populate_ambox();
  host_->spatial_analysis_tab_ =
      ui::views::tab_strip_adopt_tab(side, "Analysis", make_inspector_placeholder());
  host_->processing_tab_ =
      ui::views::tab_strip_adopt_tab(side, "Processing", make_inspector_placeholder());
  host_->playback_tab_ =
      ui::views::tab_strip_adopt_tab(side, "Play", make_inspector_placeholder());
  host_->report_tab_ =
      ui::views::tab_strip_adopt_tab(side, "Report", make_inspector_placeholder());
  host_->atmosphere_tab_ =
      ui::views::tab_strip_adopt_tab(side, "Atmosphere", make_inspector_placeholder());
  host_->plugins_tab_ =
      ui::views::tab_strip_adopt_tab(side, "Plugins", make_inspector_placeholder());
  side->set_active(0);
  host_->inspector_tabs_ = side;
  ui::views::View* inspector_column = wrap_inspector(side);

  ui::views::Splitter* work = ui::views::splitter_new(
      ui::views::Splitter::Orientation::kHorizontal);
  work->set_preferred_size({0, 0});
  ui::views::view_adopt_child(work, map_column);
  ui::views::view_adopt_child(work, inspector_column);

  ui::views::DiagnosticToolsPanel* diagnostic_tools =
      ui::views::diagnostic_tools_panel_new();
  host_->diagnostic_tools_ = diagnostic_tools;
  host_->diagnostic_tools_->set_visible_tools(true);
  host_->diagnostic_tools_->set_active_tab(0);
  host_->wire_debug_console();
  host_->attach_plugin_shell_ui();
  if (host_->inspector_host_) {
    host_->inspector_host_->note_tab_activated(host_->plugins_tab_);
    host_->inspector_host_->show_inspector_group(
        InspectorGroupBar::Group::kExtend, /*materialize=*/false);
  }

  ui::views::Splitter* main_split = ui::views::splitter_new(
      ui::views::Splitter::Orientation::kVertical);
  main_split->set_preferred_size({0, 0});
  ui::views::view_adopt_child(main_split, work);
  ui::views::view_adopt_child(main_split, diagnostic_tools);

  ui::views::StatusBar* status = ui::views::status_bar_new();
  host_->status_bar_ = status;
  if (host_->status_bar_) {
    host_->status_bar_->set_message("Ready");
    host_->status_bar_->set_scale_text("1:1000");
    host_->status_bar_->set_crs_text("CRS");
    host_->status_bar_->set_coord_text("XY");
  }

  root_box = ui::views::install_box_layout(
      root, ui::views::BoxLayout::Orientation::kVertical);
  if (root_box) {
    root_box->set_flex_for_view(main_split, 1);
  }
  ui::views::view_adopt_child(root, menu);
  ui::views::view_adopt_child(root, main_split);
  ui::views::view_adopt_child(root, status);

  ui::views::FrameView* frame = ui::views::make_frame_view_raw();
  frame->set_title("SmartGIS Views");
  frame->set_can_maximize(true);
  frame->adopt_client(root);
  host_->widget_.adopt_contents_view(frame);
}

ui::views::View* ShellLayoutComposer::wrap_inspector(
    ui::views::TabStrip* side) {
  auto* bar = new InspectorGroupBar();
  bar->set_preferred_size({kInspectorWDip, 26});
  bar->set_change([this](InspectorGroupBar::Group group) {
    if (host_->inspector_host_) {
      host_->inspector_host_->show_inspector_group(group);
    }
  });
  if (host_->inspector_host_) {
    host_->inspector_host_->bind_group_bar(bar);
  }
  side->set_change_fn(&ShellLayoutComposer::inspector_tab_change_trampoline,
                      host_);
  ui::views::View* column = ui::views::view_new();
  ui::views::BoxLayout* box = ui::views::install_box_layout(
      column, ui::views::BoxLayout::Orientation::kVertical);
  if (box) {
    box->set_flex_for_view(side, 1);
  }
  column->set_preferred_size({kInspectorWDip, 0});
  ui::views::view_adopt_child(column, bar);
  ui::views::view_adopt_child(column, side);
  return column;
}

}  // namespace app
