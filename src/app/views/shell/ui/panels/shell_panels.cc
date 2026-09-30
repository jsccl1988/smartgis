// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/ui/browser_view.h"

#include "app/views/shell/browser/browser.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/shell/browser/commands/app_commands.h"
#include "content/browser/camera/map_host_extent.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/browser/commands/view_commands.h"
#include "content/browser/debug/debug_agent.h"
#include "content/public/catalog_layers.h"
#include "plugin/runtime/python/runtime.h"
#include "plugin/product/dem/commands.h"
#include "content/public/map_types.h"
#include "content/public/map_contents.h"
#include "content/public/plugin_host.h"
#include "content/public/view_host.h"
#include "gis/present/style/style_document.h"
#include "gis/present/style/style_types.h"
#include "gis/vista/domain/atmosphere/field/field_channel.h"
#include "render/rhi/rhi.h"
#include "gis/model/edit/session/edit_session.h"
#include "gis/present/tile/provider/tile_map_layer.h"
#include "ui/gis/debug/debug_console_panel.h"
#include "ui/gis/debug/diagnostic_tools_panel.h"
#include "ui/gis/analysis/processing_panel.h"
#include "ui/views/kernel/view/view.h"
#include "gis/present/tile/provider/tile_provider.h"
#include "tool/nav/camera_nav.h"
#include "tool/command/command.h"
#include "tool/draft/draft.h"
#include "tool/workspace/workspace.h"
#include "ui/gis/dialogs/add_basemap_dialog.h"
#include "ui/gis/shell/ambox_view.h"
#include "plugin/runtime/processing/builtin_ops.h"
#include "plugin/runtime/processing/ops_runner.h"
#include "ui/gis/shell/atmosphere_panel.h"
#include "content/browser/debug/debug_agent.h"
#include "ui/gis/dialogs/att_struct_dialog.h"
#include "ui/gis/inspect/attribute_table.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/dialogs/create_datasource_dialog.h"
#include "ui/gis/dialogs/create_layer_dialog.h"
#include "ui/gis/dialogs/create_map_dialog.h"
#include "ui/gis/inspect/feature_info.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/input_text_dialog.h"
#include "ui/gis/catalog/layer_tree.h"
#include "ui/gis/style/layer_properties_panel.h"
#include "ui/gis/style/legend_panel.h"
#include "ui/gis/analysis/geoprocessing_history_panel.h"
#include "ui/gis/inspect/measure_panel.h"
#include "ui/gis/analysis/processing_panel.h"
#include "ui/gis/inspect/selection_panel.h"
#include "ui/gis/analysis/spatial_analysis_panel.h"
#include "ui/gis/style/symbology_panel.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/menu/context_menu.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/kernel/shell/event.h"
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

const ui::views::View* find_view_by_paint_role(const ui::views::View* root,
                                               std::string_view name) {
  if (!root) {
    return nullptr;
  }
  if (root->paint_role() == name) {
    return root;
  }
  for (size_t i = 0; i < root->child_count(); ++i) {
    if (const ui::views::View* hit =
            find_view_by_paint_role(root->child_at(i), name)) {
      return hit;
    }
  }
  return nullptr;
}

void append_view_tree_lines(const ui::views::View* view, int depth,
                            std::ostringstream& oss) {
  if (!view) {
    return;
  }
  for (int i = 0; i < depth; ++i) {
    oss << ' ';
  }
  const std::string_view role = view->paint_role();
  const ui::views::Rect b = view->bounds();
  oss << (role.empty() ? "view" : role) << " [" << b.x << ',' << b.y << ' '
      << b.width << 'x' << b.height << "]\n";
  for (size_t i = 0; i < view->child_count(); ++i) {
    append_view_tree_lines(view->child_at(i), depth + 1, oss);
  }
}

std::wstring utf8_to_wide(const std::string& utf8) {
  if (utf8.empty()) {
    return {};
  }
  const int chars = MultiByteToWideChar(
      CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  if (chars <= 0) {
    return {};
  }
  std::wstring out(static_cast<size_t>(chars), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                      out.data(), chars);
  return out;
}

}  // namespace

// Atmosphere and Processing panel wiring.

void BrowserView::wire_atmosphere_panel() {
  if (!atmosphere_panel_) {
    return;
  }
  atmosphere_panel_->set_time_range(0.0, 3600.0);
  atmosphere_panel_->set_time_sec(browser_->scene3d()->atmosphere_session().time_sec());
  atmosphere_panel_->set_ocean_checked(false);
  atmosphere_panel_->set_cloud_checked(false);
  atmosphere_panel_->set_sky_checked(false);
  atmosphere_panel_->set_fog_checked(false);
  atmosphere_panel_->set_wind_checked(false);

  atmosphere_panel_->set_time_change([this](double t) {
    browser_->scene3d()->atmosphere_session().set_time_sec(t);
    invalidate_map_overlays();
  });
  auto seed_if_empty = [this]() {
    const auto* env = browser_->scene3d()->atmosphere_session().environment();
    if (!env || env->field_store().layer_count() == 0) {
      browser_->scene3d()->atmosphere_session().seed_procedural();
    }
  };
  atmosphere_panel_->set_ocean_change([this, seed_if_empty](bool on) {
    if (on) {
      seed_if_empty();
    }
    browser_->scene3d()->atmosphere_session().set_ocean_enabled(on);
    invalidate_map_overlays();
  });
  atmosphere_panel_->set_cloud_change([this, seed_if_empty](bool on) {
    if (on) {
      seed_if_empty();
    }
    browser_->scene3d()->atmosphere_session().set_cloud_enabled(on);
    invalidate_map_overlays();
  });
  atmosphere_panel_->set_sky_change([this, seed_if_empty](bool on) {
    if (on) {
      seed_if_empty();
    }
    browser_->scene3d()->atmosphere_session().set_sky_enabled(on);
    invalidate_map_overlays();
  });
  atmosphere_panel_->set_fog_change([this, seed_if_empty](bool on) {
    if (on) {
      seed_if_empty();
    }
    browser_->scene3d()->atmosphere_session().set_fog_enabled(on);
    invalidate_map_overlays();
  });
  atmosphere_panel_->set_wind_change([this](bool on) {
    browser_->scene3d()->atmosphere_session().set_wind_overlay_enabled(on);
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

void BrowserView::wire_measure_panel() {
  if (!measure_panel_) {
    return;
  }
  measure_panel_->set_unit_text("m");
  measure_panel_->set_mode_change([this](ui::views::MeasurePanel::Mode mode) {
    measure_armed_ = true;
    const char* name = "length";
    const char* tool = "edit.append.linestring";
    if (mode == ui::views::MeasurePanel::Mode::kArea) {
      name = "area";
      tool = "edit.append.polygon";
    } else if (mode == ui::views::MeasurePanel::Mode::kAzimuth) {
      name = "azimuth";
      tool = "edit.append.linestring";
    }
    browser_->run_tool_command(tool);
    show_inspector_tab_index(measure_tab_);
    set_status_message(std::string("Measure mode: ") + name +
                       " (digitize; not stored)");
  });
}

void BrowserView::wire_selection_panel() {
  if (!selection_panel_) {
    return;
  }
  selection_panel_->set_command([this](const std::string& id) {
    if (id == "selection.clear") {
      browser_->run_tool_command("selection.clear");
      sync_selection_panel_from_scene();
      return;
    }
    if (id == "selection.invert") {
      if (invert_selection()) {
        sync_inspectors_from_scene();
        invalidate_map_overlays();
        set_status_message("Selection inverted (next feature)");
      } else {
        set_status_message("Selection invert: no features");
      }
      return;
    }
    if (id == "selection.zoom_to") {
      browser_->run_tool_command("view.zoom_selection");
      return;
    }
    if (id == "selection.export_selected") {
      std::string path;
      if (export_selection_geojson(&path)) {
        set_status_message(std::string("Exported selection: ") + path);
      } else {
        set_status_message("Selection export failed (nothing selected)");
      }
      return;
    }
    set_status_message(std::string("Selection: ") + id);
  });
  sync_selection_panel_from_scene();
}

void BrowserView::wire_layer_properties_panel() {
  if (!layer_properties_panel_ || !layer_properties_panel_->symbology()) {
    return;
  }
  auto* symbology = layer_properties_panel_->symbology();
  symbology->set_paint({{"line-color", "#3388ff"},
                        {"line-width", "1.5"},
                        {"fill-color", "#88aacc"},
                        {"fill-opacity", "0.6"}});
  symbology->set_apply_handler(
      [this](const ui::views::SymbologyPanel::PaintKv& paint) {
        auto* doc_scene = browser_->document();
        std::string layer_name;
        if (layer_properties_panel_ && layer_properties_panel_->symbology()) {
          layer_name = layer_properties_panel_->symbology()->layer_token();
        }
        if (layer_name.empty() && !doc_scene->layers().empty()) {
          layer_name = doc_scene->active_layer_id();
          for (const auto& layer : doc_scene->layers()) {
            if (layer.id == layer_name || layer.name == layer_name) {
              layer_name = layer.name;
              break;
            }
          }
          if (layer_name.empty()) {
            layer_name = doc_scene->layers().front().name;
          }
        }
        auto doc = std::make_shared<gis::style::StyleDocument>();
        if (const gis::style::StyleDocument* existing =
                doc_scene->style_document()) {
          *doc = *existing;
        } else {
          doc->version = 8;
          doc->name = "symbology-panel";
        }
        gis::style::StyleLayer* target = nullptr;
        for (auto& layer : doc->layers) {
          if (layer.source_layer == layer_name || layer.id == layer_name) {
            target = &layer;
            break;
          }
        }
        if (!target) {
          gis::style::StyleLayer created;
          created.id = layer_name.empty() ? "panel-layer" : layer_name;
          created.source_layer = layer_name;
          created.type = gis::style::LayerType::kLine;
          for (const auto& kv : paint) {
            if (kv.first.find("fill") != std::string::npos) {
              created.type = gis::style::LayerType::kFill;
              break;
            }
          }
          doc->layers.push_back(std::move(created));
          target = &doc->layers.back();
        }
        target->paint.clear();
        for (const auto& kv : paint) {
          target->paint[kv.first] = kv.second;
        }
        doc_scene->set_style_document(std::move(doc));
        sync_legend_panel_from_scene();
        invalidate_map_overlays();
        set_status_message(std::string("Symbology applied: ") + layer_name);
      });
  sync_layer_properties_from_scene();
}

void BrowserView::wire_legend_panel() {
  if (!legend_panel_) {
    return;
  }
  legend_panel_->set_toggle([this](const std::string& id, bool visible) {
    if (browser_->document()->set_layer_visible(id, visible)) {
      sync_catalog_from_scene();
      invalidate_map_overlays();
      set_status_message(std::string("Layer ") + id +
                         (visible ? " visible" : " hidden"));
    } else {
      set_status_message(std::string("Legend toggle failed: ") + id);
    }
  });
  sync_legend_panel_from_scene();
}

void BrowserView::wire_spatial_analysis_panel() {
  if (!spatial_analysis_panel_) {
    return;
  }
  std::vector<ui::views::SpatialAnalysisPanel::Operator> ops;
  ops.reserve(plugin::builtin_op_catalog().size());
  for (const plugin::BuiltinOpDesc& d : plugin::builtin_op_catalog()) {
    ops.push_back({d.id, d.title, "native"});
  }
  spatial_analysis_panel_->set_operators(std::move(ops));
  spatial_analysis_panel_->set_params(
      {{"distance", "0.05", "buffer/simplify"},
       {"input", "(scene)", "path"},
       {"output", "(temp)", "path"}});
  spatial_analysis_panel_->set_run_handler(
      [this](const std::string& id,
             const std::vector<ui::views::SpatialAnalysisPanel::Param>&
                 params) {
        std::string distance = "0.05";
        for (const auto& p : params) {
          if (p.name == "distance" && !p.value.empty()) {
            distance = p.value;
          }
        }
        spatial_analysis_panel_->set_progress(0.1, "running");
        run_processing_operator(id, distance);
        spatial_analysis_panel_->set_progress(1.0, "done");
      });
  spatial_analysis_panel_->set_cancel_handler([this]() {
    spatial_analysis_panel_->set_progress(0.0, "cancelled");
    set_status_message("Analysis cancelled");
  });
  if (auto* history = spatial_analysis_panel_->history()) {
    history->set_rerun_handler([this](const std::string& op_id) {
      run_processing_operator(op_id);
    });
    history->set_clear_handler(
        [this]() { set_status_message("Analysis history cleared"); });
  }
}

void BrowserView::run_processing_operator(const std::string& processing_id) {
  run_processing_operator(processing_id, "0.05");
}

void BrowserView::run_processing_operator(const std::string& processing_id,
                                            const std::string& distance) {
  if (!browser_->plugins() || !browser_->plugins()->host() ||
      processing_id.empty()) {
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
    args += ",\"distance\":";
    args += distance.empty() ? "0.05" : distance;
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
  if (!browser_->document()->open_path(out.string()) ||
      browser_->document()->feature_count() < 1) {
    set_status_message(std::string("Processing write-back failed: ") +
                       processing_id);
    return;
  }
  sync_catalog_from_scene();
  sync_inspectors_from_scene();
  invalidate_map_overlays();
  browser_->fit_map_extent();
  const int n = static_cast<int>(browser_->document()->feature_count());
  const std::string text = std::string("Processing ok: ") + processing_id +
                           " features=" + std::to_string(n);
  set_status_message(text);
  if (spatial_analysis_panel_ && spatial_analysis_panel_->history()) {
    spatial_analysis_panel_->history()->append_entry({"now", processing_id, text});
  }
}

void BrowserView::bind_gis_python_bridge() {
  if (!browser_ || !browser_->plugins()) {
    return;
  }
  plugin::GisConsoleBridge bridge;
  bridge.write_active_geojson = [this](const std::string& path) {
    return browser_ && browser_->document() &&
           browser_->document()->write_path(path);
  };
  bridge.write_path = bridge.write_active_geojson;
  bridge.load_result_geojson = [this](const std::string& path) {
    if (!browser_ || !browser_->document()) {
      return false;
    }
    if (!browser_->document()->open_path(path)) {
      return false;
    }
    sync_catalog_from_scene();
    sync_inspectors_from_scene();
    invalidate_map_overlays();
    return browser_->document()->feature_count() >= 1;
  };
  bridge.open_path = bridge.load_result_geojson;
  bridge.feature_count = [this]() {
    return browser_ && browser_->document()
               ? static_cast<int>(browser_->document()->feature_count())
               : 0;
  };
  bridge.refresh_map = [this]() {
    sync_catalog_from_scene();
    sync_inspectors_from_scene();
    invalidate_map_overlays();
    if (browser_) {
      browser_->fit_map_extent();
    }
  };
  bridge.flush_processing = [this]() {
    if (browser_ && browser_->plugins()) {
      browser_->plugins()->flush_processing_for_test();
    }
  };
  bridge.layers_json = [this]() {
    if (!browser_ || !browser_->document()) {
      return std::string("[]");
    }
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const content::LayerDesc& d : browser_->document()->layer_descs()) {
      if (!first) {
        oss << ",";
      }
      first = false;
      oss << "{\"id\":\"" << content::json_escape_string(d.id) << "\",\"name\":\""
          << content::json_escape_string(d.name)
          << "\",\"visible\":" << (d.visible ? "true" : "false")
          << ",\"active\":" << (d.active ? "true" : "false") << "}";
    }
    oss << "]";
    return oss.str();
  };
  bridge.select_layer = [this](const std::string& id) {
    return browser_ && browser_->document() &&
           browser_->document()->select_layer(id);
  };
  bridge.set_layer_visible = [this](const std::string& id, bool on) {
    return browser_ && browser_->document() &&
           browser_->document()->set_layer_visible(id, on);
  };
  bridge.extent_json = [this]() {
    if (!browser_ || !browser_->document()) {
      return std::string("null");
    }
    double min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    if (!browser_->document()->compute_extent(&min_x, &min_y, &max_x, &max_y)) {
      return std::string("null");
    }
    char buf[192];
    std::snprintf(buf, sizeof(buf),
                  "{\"min_x\":%.6f,\"min_y\":%.6f,\"max_x\":%.6f,\"max_y\":%.6f}",
                  min_x, min_y, max_x, max_y);
    return std::string(buf);
  };
  bridge.present_mode = [this]() {
    if (!map_tabs_) {
      return std::string("map2d");
    }
    const int a = map_tabs_->active();
    if (a == 1) {
      return std::string("data");
    }
    if (a == 2) {
      return std::string("scene3d");
    }
    return std::string("map2d");
  };
  bridge.set_present_mode = [this](const std::string& mode) {
    if (!map_tabs_) {
      return false;
    }
    int idx = 0;
    if (mode == "data") {
      idx = 1;
    } else if (mode == "scene3d") {
      idx = 2;
    } else if (mode != "map2d") {
      return false;
    }
    // Full tab switch (lazy FlyCube attach + China orbit), not set_active alone.
    switch_map_tab(idx);
    return true;
  };
  bridge.has_style_document = [this]() {
    return browser_ && browser_->document() &&
           browser_->document()->has_style_document();
  };
  bridge.load_style_path = [this](const std::string& path) {
    return browser_ && browser_->document() &&
           browser_->document()->load_style_path(path);
  };
  bridge.clear_style = [this]() {
    if (browser_ && browser_->document()) {
      browser_->document()->clear_style_document();
    }
  };
  bridge.style_summary_json = [this]() {
    if (!browser_ || !browser_->document() ||
        !browser_->document()->style_document()) {
      return std::string("null");
    }
    const gis::style::StyleDocument* doc =
        browser_->document()->style_document();
    std::ostringstream oss;
    oss << "{\"name\":\"" << content::json_escape_string(doc->name)
        << "\",\"version\":" << doc->version << ",\"layers\":[";
    for (size_t i = 0; i < doc->layers.size(); ++i) {
      if (i) {
        oss << ",";
      }
      oss << "\"" << content::json_escape_string(doc->layers[i].id) << "\"";
    }
    oss << "]}";
    return oss.str();
  };
  bridge.activate_tool = [this](uint32_t view_id, const std::string& tool_id) {
    if (!browser_ || !browser_->map_session()) {
      return false;
    }
    uint32_t vid = view_id;
    if (vid == 0) {
      if (ui::views::MapViewport* pane = active_map()) {
        vid = pane->view_id();
      }
    }
    browser_->map_session()->ActivateTool(vid, tool_id.c_str());
    return true;
  };
  plugin::set_gis_console_bridge(std::move(bridge));
  browser_->plugins()->ensure_python();
}

void BrowserView::bind_debug_agent_host() {
  if (!browser_) {
    return;
  }
  bind_gis_python_bridge();
  content::DebugAgentHost host;
  host.refresh_map = [this] {
    if (browser_) {
      browser_->on_view_command("view.refresh", -1, false, 0, 0);
    }
  };
  host.extent_string = [this] {
    if (!browser_) {
      return std::string("no browser");
    }
    double min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    if (!browser_->document()->compute_extent(&min_x, &min_y, &max_x,
                                              &max_y)) {
      return std::string("empty extent");
    }
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%.6f,%.6f,%.6f,%.6f", min_x, min_y, max_x,
                  max_y);
    return std::string(buf);
  };
  host.layer_names = [this] {
    std::vector<std::string> names;
    if (!browser_ || !browser_->document()) {
      return names;
    }
    names.push_back("features=" +
                    std::to_string(browser_->document()->feature_count()));
    return names;
  };
  host.ui_find = [this](const std::string& name) {
    ui::views::View* root = widget_.contents_view();
    if (!root) {
      return std::string("not wired");
    }
    const ui::views::View* hit =
        find_view_by_paint_role(root, std::string_view(name));
    if (!hit) {
      return std::string("not wired");
    }
    const ui::views::Rect b = hit->bounds();
    std::ostringstream oss;
    oss << "{\"role\":\"" << hit->paint_role() << "\",\"x\":" << b.x
        << ",\"y\":" << b.y << ",\"w\":" << b.width << ",\"h\":" << b.height
        << "}";
    return oss.str();
  };
  host.ui_click = [this](int x, int y, int button) {
    ui::views::MouseEvent down;
    down.type = ui::views::MouseEvent::Type::kDown;
    down.x = x;
    down.y = y;
    down.button = button > 0 ? button : 1;
    widget_.send_mouse(down);
    ui::views::MouseEvent up = down;
    up.type = ui::views::MouseEvent::Type::kUp;
    widget_.send_mouse(up);
    return std::string("clicked");
  };
  host.ui_type = [this](const std::string& utf8) {
    const std::wstring wide = utf8_to_wide(utf8);
    for (wchar_t ch : wide) {
      ui::views::CharEvent ev;
      ev.ch = ch;
      widget_.send_char(ev);
    }
    return std::string("typed ") + std::to_string(wide.size()) + " chars";
  };
  host.ui_dump_tree = [this]() {
    ui::views::View* root = widget_.contents_view();
    if (!root) {
      return std::string("not wired");
    }
    std::ostringstream oss;
    append_view_tree_lines(root, 0, oss);
    return oss.str();
  };
  host.ui_overlay_stats = [this]() {
    (void)this;
    return std::string("unavailable");
  };
  host.ui_capture_shell = [this](const std::string& path_utf8) {
    HWND hwnd = widget_.hwnd();
    if (!hwnd || !IsWindow(hwnd)) {
      return std::string("error: no hwnd");
    }
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
      return std::string("error: empty client");
    }
    std::filesystem::path out_path = path_utf8.empty()
                                         ? (std::filesystem::path("out") /
                                            "ui_forensics" / "capture.bmp")
                                         : std::filesystem::path(path_utf8);
    std::error_code ec;
    if (out_path.has_parent_path()) {
      std::filesystem::create_directories(out_path.parent_path(), ec);
    }
    HDC hdc_win = GetDC(hwnd);
    HDC mem = CreateCompatibleDC(hdc_win);
    HBITMAP bmp = CreateCompatibleBitmap(hdc_win, w, h);
    HGDIOBJ old = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, w, h, hdc_win, 0, 0, SRCCOPY);

    BITMAPFILEHEADER bfh = {};
    BITMAPINFOHEADER bih = {};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = w;
    bih.biHeight = -h;
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    const DWORD img_bytes = static_cast<DWORD>(w) * static_cast<DWORD>(h) * 4u;
    bfh.bfType = 0x4D42;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + img_bytes;

    std::vector<std::uint8_t> pixels(img_bytes);
    BITMAPINFO bi = {};
    bi.bmiHeader = bih;
    GetDIBits(mem, bmp, 0, static_cast<UINT>(h), pixels.data(), &bi,
              DIB_RGB_COLORS);

    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(hwnd, hdc_win);

    std::ofstream file(out_path, std::ios::binary);
    if (!file) {
      return std::string("error: open failed");
    }
    file.write(reinterpret_cast<const char*>(&bfh), sizeof(bfh));
    file.write(reinterpret_cast<const char*>(&bih), sizeof(bih));
    file.write(reinterpret_cast<const char*>(pixels.data()),
               static_cast<std::streamsize>(pixels.size()));
    return out_path.string();
  };
  host.py_eval = [this](const std::string& code) {
    if (!browser_ || !browser_->plugins()) {
      return std::string("error: no plugin shell");
    }
    return browser_->plugins()->eval_python(code);
  };
  content::debug_agent().set_host(std::move(host));
}

void BrowserView::wire_debug_console() {
  if (!diagnostic_tools_) {
    return;
  }
  diagnostic_tools_->set_console_submit([this](const std::string& line) {
    bind_debug_agent_host();
    if (!content::debug_agent().is_running()) {
      content::debug_agent().start();
    }
    const std::string out = content::debug_agent().exec_line(line);
    if (diagnostic_tools_ && diagnostic_tools_->console_pane() &&
        !out.empty()) {
      diagnostic_tools_->console_pane()->append_line(out);
    }
  });
}

void BrowserView::toggle_debug_console() {
  if (!diagnostic_tools_) {
    return;
  }
  const bool next = !diagnostic_tools_->is_tools_visible();
  if (next) {
    bind_debug_agent_host();
    content::debug_agent().start();
  }
  diagnostic_tools_->set_visible_tools(next);
  set_status_message(next ? "Diagnostic Tools on" : "Diagnostic Tools off");
  if (ui::views::View* root = contents_view()) {
    root->schedule_paint();
  }
}

bool BrowserView::try_consume_measure_draft(const tool::Draft& draft) {
  if (!measure_armed_ || !measure_panel_ || !browser_ ||
      !browser_->view_frame()) {
    return false;
  }
  if (draft.points.size() < 2) {
    return false;
  }
  std::vector<std::pair<double, double>> map_pts;
  map_pts.reserve(draft.points.size());
  for (const auto& pt : draft.points) {
    double mx = 0;
    double my = 0;
    browser_->view_frame()->view_to_map(pt.x_px, pt.y_px, &mx, &my);
    map_pts.emplace_back(mx, my);
  }

  const bool china = browser_->document()->has_china_extent();
  auto segment_m = [china](double x0, double y0, double x1, double y1) {
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    if (!china) {
      return std::sqrt(dx * dx + dy * dy);
    }
    const double lat = 0.5 * (-y0 + -y1);
    const double m_per_deg_lat = 111320.0;
    const double m_per_deg_lon =
        111320.0 * std::cos(lat * 3.14159265358979323846 / 180.0);
    const double east = dx * m_per_deg_lon;
    const double north = (-dy) * m_per_deg_lat;
    return std::sqrt(east * east + north * north);
  };

  const auto mode = measure_panel_->mode();
  std::vector<ui::views::MeasurePanel::ResultRow> rows;
  char buf[64];

  if (mode == ui::views::MeasurePanel::Mode::kLength ||
      mode == ui::views::MeasurePanel::Mode::kAzimuth) {
    double total = 0;
    for (size_t i = 1; i < map_pts.size(); ++i) {
      total += segment_m(map_pts[i - 1].first, map_pts[i - 1].second,
                         map_pts[i].first, map_pts[i].second);
    }
    std::snprintf(buf, sizeof(buf), "%.3f", total);
    rows.push_back({"length_m", buf});
    if (mode == ui::views::MeasurePanel::Mode::kAzimuth &&
        map_pts.size() >= 2) {
      const auto& a = map_pts.front();
      const auto& b = map_pts.back();
      const double dx = b.first - a.first;
      const double dy = -(b.second - a.second);
      double deg = std::atan2(dx, dy) * 180.0 / 3.14159265358979323846;
      if (deg < 0) {
        deg += 360.0;
      }
      std::snprintf(buf, sizeof(buf), "%.2f", deg);
      rows.push_back({"azimuth_deg", buf});
    }
    measure_panel_->set_unit_text("m");
  } else {
    double area = 0;
    for (size_t i = 0; i < map_pts.size(); ++i) {
      const auto& p0 = map_pts[i];
      const auto& p1 = map_pts[(i + 1) % map_pts.size()];
      area += p0.first * (-p1.second) - p1.first * (-p0.second);
    }
    area = std::fabs(area) * 0.5;
    if (china) {
      const double lat = -map_pts.front().second;
      const double m_per_deg_lat = 111320.0;
      const double m_per_deg_lon =
          111320.0 * std::cos(lat * 3.14159265358979323846 / 180.0);
      area *= m_per_deg_lon * m_per_deg_lat;
    }
    std::snprintf(buf, sizeof(buf), "%.3f", area);
    rows.push_back({"area_m2", buf});
    measure_panel_->set_unit_text("m²");
  }

  measure_panel_->set_results(std::move(rows));
  show_inspector_tab_index(measure_tab_);
  set_status_message("Measure updated");
  return true;
}

void BrowserView::sync_selection_panel_from_scene() {
  if (!selection_panel_ || !browser_) {
    return;
  }
  const MapScene::Feature* sel = browser_->document()->selected_feature();
  selection_panel_->set_count(sel ? 1 : 0);
  std::vector<ui::views::SelectionPanel::LayerSummary> layers;
  for (const auto& layer : browser_->document()->layers()) {
    int n = 0;
    for (const auto& f : layer.features) {
      if (sel && f.id.len == sel->id.len &&
          std::memcmp(f.id.bytes, sel->id.bytes, sizeof(f.id.bytes)) == 0) {
        ++n;
      }
    }
    if (n > 0 || layer.visible) {
      layers.push_back({layer.id, layer.name, n});
    }
  }
  selection_panel_->set_layers(std::move(layers));
}

void BrowserView::sync_legend_panel_from_scene() {
  if (!legend_panel_ || !browser_) {
    return;
  }
  std::vector<ui::views::LegendPanel::Entry> entries;
  const double scale =
      browser_->view_frame() ? browser_->view_frame()->scale() : 8.0;
  for (const auto& layer : browser_->document()->layers()) {
    ui::views::LegendPanel::Entry e;
    e.id = layer.id;
    e.label = layer.name;
    e.visible = layer.visible;
    e.swatch = "#888888";
    if (!layer.features.empty()) {
      COLORREF fill = RGB(136, 136, 136);
      COLORREF stroke = fill;
      int width = 1;
      if (browser_->document()->style_colors_for_feature(
              layer, layer.features.front(), scale, &fill, &stroke, &width)) {
        char hex[16];
        std::snprintf(hex, sizeof(hex), "#%02X%02X%02X", GetRValue(stroke),
                      GetGValue(stroke), GetBValue(stroke));
        e.swatch = hex;
      }
    }
    entries.push_back(std::move(e));
  }
  legend_panel_->set_entries(std::move(entries));
}

void BrowserView::sync_layer_properties_from_scene() {
  if (!layer_properties_panel_ || !browser_) {
    return;
  }
  std::string source = "(document)";
  std::string layer_token;
  std::string geom = "unknown";
  for (const auto& layer : browser_->document()->layers()) {
    if (layer.id == browser_->document()->active_layer_id() ||
        layer_token.empty()) {
      layer_token = layer.id.empty() ? layer.name : layer.id;
      if (!layer.features.empty()) {
        switch (layer.features.front().kind) {
          case MapScene::GeomKind::kPoint:
            geom = "point";
            break;
          case MapScene::GeomKind::kLine:
            geom = "line";
            break;
          case MapScene::GeomKind::kPolygon:
            geom = "fill";
            break;
          case MapScene::GeomKind::kText:
            geom = "symbol";
            break;
        }
      }
      if (layer.id == browser_->document()->active_layer_id()) {
        break;
      }
    }
  }
  layer_properties_panel_->set_source_text(source);
  if (auto* symbology = layer_properties_panel_->symbology()) {
    symbology->set_layer(layer_token, geom);
  }
}

bool BrowserView::invert_selection() {
  if (!browser_) {
    return false;
  }
  const MapScene::Feature* sel = browser_->document()->selected_feature();
  bool take_next = (sel == nullptr);
  const MapScene::Feature* first = nullptr;
  for (const auto& layer : browser_->document()->layers()) {
    for (const auto& f : layer.features) {
      if (!first) {
        first = &f;
      }
      if (take_next) {
        return browser_->document()->select_feature(f.id);
      }
      if (sel && f.id.len == sel->id.len &&
          std::memcmp(f.id.bytes, sel->id.bytes, sizeof(f.id.bytes)) == 0) {
        take_next = true;
      }
    }
  }
  if (first) {
    return browser_->document()->select_feature(first->id);
  }
  return false;
}

bool BrowserView::export_selection_geojson(std::string* out_path) {
  if (!out_path || !browser_) {
    return false;
  }
  const MapScene::Feature* sel = browser_->document()->selected_feature();
  if (!sel || sel->points.empty()) {
    return false;
  }
  namespace fs = std::filesystem;
  const fs::path path =
      fs::temp_directory_path() / "smartgis_selection_export.geojson";
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  const char* gtype = "LineString";
  if (sel->kind == MapScene::GeomKind::kPoint || sel->points.size() == 1) {
    gtype = "Point";
  } else if (sel->kind == MapScene::GeomKind::kPolygon) {
    gtype = "Polygon";
  }
  out << "{\"type\":\"FeatureCollection\",\"features\":[{"
         "\"type\":\"Feature\",\"properties\":{\"id\":\""
      << MapScene::feature_token(sel->id)
      << "\"},\"geometry\":{\"type\":\"" << gtype << "\",\"coordinates\":";
  if (std::strcmp(gtype, "Point") == 0) {
    out << "[" << sel->points.front().x << "," << -sel->points.front().y
        << "]";
  } else if (std::strcmp(gtype, "Polygon") == 0) {
    out << "[[";
    for (size_t i = 0; i < sel->points.size(); ++i) {
      if (i) {
        out << ",";
      }
      out << "[" << sel->points[i].x << "," << -sel->points[i].y << "]";
    }
    if (!sel->points.empty()) {
      out << ",[" << sel->points.front().x << "," << -sel->points.front().y
          << "]";
    }
    out << "]]";
  } else {
    out << "[";
    for (size_t i = 0; i < sel->points.size(); ++i) {
      if (i) {
        out << ",";
      }
      out << "[" << sel->points[i].x << "," << -sel->points[i].y << "]";
    }
    out << "]";
  }
  out << "}}]}";
  if (!out) {
    return false;
  }
  *out_path = path.string();
  return true;
}

}  // namespace app
