// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/ui/browser_view.h"

#include "app/views/shell/browser/browser.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/shell/browser/commands/app_commands.h"
#include "app/views/camera/map_host_extent.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/browser/commands/view_commands.h"
#include "plugin/product/dem/dem_commands.h"
#include "plugin/product/orthogrid/commands.h"
#include "plugin/runtime/host/registry.h"
#include "content/public/catalog_layers.h"
#include "content/public/map_contents.h"
#include "content/public/map_types.h"
#include "content/public/plugin_host.h"
#include "content/public/view_host.h"
#include "gis/vista/domain/atmosphere/field/field_channel.h"
#include "render/rhi/rhi.h"
#include "gis/model/edit/session/edit_session.h"
#include "gis/present/tile/provider/tile_map_layer.h"
#include "gis/present/tile/provider/tile_provider.h"
#include "tool/nav/camera_nav.h"
#include "tool/command/command.h"
#include "tool/draft/draft.h"
#include "tool/workspace/workspace.h"
#include "ui/views/dialogs/gis/add_basemap_dialog.h"
#include "ui/views/gis/shell/ambox_view.h"
#include "plugin/runtime/processing/builtin_ops.h"
#include "plugin/runtime/processing/ops_runner.h"
#include "ui/views/gis/panel/atmosphere_panel.h"
#include "ui/views/dialogs/gis/att_struct_dialog.h"
#include "ui/views/gis/inspect/attribute_table.h"
#include "ui/views/gis/catalog/catalog_view.h"
#include "ui/views/dialogs/gis/create_datasource_dialog.h"
#include "ui/views/dialogs/gis/create_layer_dialog.h"
#include "ui/views/dialogs/gis/create_map_dialog.h"
#include "ui/views/gis/inspect/feature_info.h"
#include "ui/views/dialogs/shell/file_picker.h"
#include "ui/views/dialogs/shell/input_text_dialog.h"
#include "ui/views/gis/catalog/layer_tree.h"
#include "ui/views/gis/panel/processing_panel.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/menu/context_menu.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/views/gis/shell/status_bar.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/kernel/view/view.h"

namespace app {

namespace {

std::string m2_json_path(std::string path) {
  for (char& c : path) {
    if (c == '\\') {
      c = '/';
    }
  }
  return path;
}

bool write_m2_clip_geojson(const std::filesystem::path& path, double min_x,
                           double min_y, double max_x, double max_y) {
  const double cx = 0.5 * (min_x + max_x);
  const double cy = 0.5 * (min_y + max_y);
  const double hx = 0.25 * (max_x - min_x);
  const double hy = 0.25 * (max_y - min_y);
  const double x0 = cx - hx;
  const double y0 = cy - hy;
  const double x1 = cx + hx;
  const double y1 = cy + hy;
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  out << "{\"type\":\"FeatureCollection\",\"features\":[{"
         "\"type\":\"Feature\",\"properties\":{},\"geometry\":{"
         "\"type\":\"Polygon\",\"coordinates\":[[["
      << x0 << "," << y0 << "],[" << x1 << "," << y0 << "],[" << x1 << ","
      << y1 << "],[" << x0 << "," << y1 << "],[" << x0 << "," << y0
      << "]]]}}]}";
  return static_cast<bool>(out);
}

}  // namespace

// Atmosphere and Processing panel wiring.

void BrowserView::wire_atmosphere_panel() {
  if (!atmosphere_panel_) {
    return;
  }
  atmosphere_panel_->set_time_range(0.0, 3600.0);
  atmosphere_panel_->set_time_sec(browser_->scene3d()->time_sec());
  atmosphere_panel_->set_ocean_checked(false);
  atmosphere_panel_->set_cloud_checked(false);
  atmosphere_panel_->set_sky_checked(false);
  atmosphere_panel_->set_fog_checked(false);
  atmosphere_panel_->set_wind_checked(false);

  atmosphere_panel_->set_time_change([this](double t) {
    browser_->scene3d()->set_time_sec(t);
    invalidate_map_overlays();
  });
  auto seed_if_empty = [this]() {
    const auto* env = browser_->scene3d()->atmosphere();
    if (!env || env->field_store().layer_count() == 0) {
      browser_->scene3d()->seed_atmosphere_procedural();
    }
  };
  atmosphere_panel_->set_ocean_change([this, seed_if_empty](bool on) {
    if (on) {
      seed_if_empty();
    }
    browser_->scene3d()->set_ocean_enabled(on);
    invalidate_map_overlays();
  });
  atmosphere_panel_->set_cloud_change([this, seed_if_empty](bool on) {
    if (on) {
      seed_if_empty();
    }
    browser_->scene3d()->set_cloud_enabled(on);
    invalidate_map_overlays();
  });
  atmosphere_panel_->set_sky_change([this, seed_if_empty](bool on) {
    if (on) {
      seed_if_empty();
    }
    browser_->scene3d()->set_sky_enabled(on);
    invalidate_map_overlays();
  });
  atmosphere_panel_->set_fog_change([this, seed_if_empty](bool on) {
    if (on) {
      seed_if_empty();
    }
    browser_->scene3d()->set_fog_enabled(on);
    invalidate_map_overlays();
  });
  atmosphere_panel_->set_wind_change([this](bool on) {
    browser_->scene3d()->set_wind_overlay_enabled(on);
    invalidate_map_overlays();
  });
}

void BrowserView::wire_processing_panel() {
  if (!processing_panel_) {
    return;
  }
  std::vector<ui::views::ProcessingPanel::Operator> ops;
  ops.reserve(plugin::builtin_op_catalog().size());
  for (const plugin::BuiltinOpDesc& d : plugin::builtin_op_catalog()) {
    ops.push_back({d.id, d.title});
  }
  processing_panel_->set_operators(std::move(ops));
  processing_panel_->set_run_handler(
      [this](const std::string& id) { run_processing_operator(id); });
}

void BrowserView::run_processing_operator(const std::string& processing_id) {
  if (!browser_->plugins() || !browser_->plugins()->host() || processing_id.empty()) {
    set_status_message("Processing: no host");
    return;
  }
  if (browser_->document()->feature_count() == 0) {
    set_status_message("Processing: no features");
    return;
  }
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path();
  const fs::path in = dir / "smartgis_views_proc_in.geojson";
  const fs::path out = dir / "smartgis_views_proc_out.geojson";
  const fs::path clip = dir / "smartgis_views_proc_clip.geojson";
  if (!browser_->document()->write_path(in.string())) {
    set_status_message("Processing: write_path failed");
    return;
  }
  std::string args = std::string("{\"input\":\"") + m2_json_path(in.string()) +
                     "\",\"output\":\"" + m2_json_path(out.string()) + "\"";
  if (processing_id == "native.buffer" ||
      processing_id == "native.simplify") {
    args += ",\"distance\":0.05";
  }
  const bool needs_clip =
      processing_id == "native.clip" ||
      processing_id == "native.intersection" ||
      processing_id == "native.union" ||
      processing_id == "native.difference" ||
      processing_id == "native.symmetric_difference";
  if (needs_clip) {
    double min_x = 0, min_y = 0, max_x = 1, max_y = 1;
    if (!browser_->document()->compute_extent(&min_x, &min_y, &max_x, &max_y)) {
      set_status_message("Processing: no extent for clip");
      return;
    }
    // MapScene stores Y flipped for screen; GeoJSON write_path unflips.
    // Clip fixture uses geographic lon/lat matching written GeoJSON.
    if (!write_m2_clip_geojson(clip, min_x, -max_y, max_x, -min_y)) {
      set_status_message("Processing: clip fixture failed");
      return;
    }
    args += ",\"clip\":\"" + m2_json_path(clip.string()) + "\"";
  }
  args += "}";

  if (!browser_->plugins()->run_processing(processing_id, args)) {
    set_status_message(std::string("Processing failed: ") + processing_id);
    return;
  }
  browser_->plugins()->flush_processing_for_test();
  if (!browser_->document()->open_path(out.string()) || browser_->document()->feature_count() < 1) {
    set_status_message(std::string("Processing write-back failed: ") +
                       processing_id);
    return;
  }
  sync_catalog_from_scene();
  sync_inspectors_from_scene();
  invalidate_map_overlays();
  browser_->fit_map_extent();
  set_status_message(std::string("Processing ok: ") + processing_id);
}

}  // namespace app
