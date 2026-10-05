// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/ui/panels/processing_composer.h"
#include "app/views/shell/ui/browser_view.h"

#include "app/views/shell/browser/browser.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
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

#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/ui/panels/report_panel.h"
#include "content/public/catalog_layers.h"
#include "content/public/map_contents.h"
#include "content/public/plugin_host.h"
#include "gis/style/document/style_document.h"
#include "gis/style/style_types.h"
#include "plugin/runtime/browser/fake_report_browser.h"
#include "plugin/runtime/browser/webview2_report_browser.h"
#include "plugin/runtime/processing/builtin_ops.h"
#include "plugin/runtime/python/runtime.h"
#include "ui/gis/analysis/geoprocessing_history_panel.h"
#include "ui/gis/analysis/processing_panel.h"
#include "ui/gis/analysis/result_playback_panel.h"
#include "ui/gis/analysis/spatial_analysis_panel.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "base/process/switches.h"

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

// Processing / playback / report / spatial-analysis wiring and Python GIS bridge.

// Wires processing / playback / report / spatial / Python GIS chrome.
ProcessingComposer::ProcessingComposer(BrowserView* host) : host_(host) {}

void ProcessingComposer::wire_processing_panel() {
  if (!host_->processing_panel_) {
    return;
  }
  std::vector<ui::views::ProcessingPanel::Operator> ops;
  ops.reserve(plugin::builtin_op_catalog().size());
  for (const plugin::BuiltinOpDesc& d : plugin::builtin_op_catalog()) {
    ops.push_back({d.id, d.title});
  }
  host_->processing_panel_->set_operators(std::move(ops));
  host_->processing_panel_->set_run_handler(
      [this](const std::string& id) { host_->run_processing_operator(id); });
}


void ProcessingComposer::wire_result_playback_panel() {
  if (!host_->result_playback_panel_) {
    return;
  }
  host_->result_playback_panel_->set_frame_change([this](int index) {
    host_->browser_->analysis_playback().set_playing(false);
    host_->result_playback_panel_->set_playing(false);
    host_->sync_result_playback_timer();
    (void)host_->browser_->apply_analysis_frame(index);
    host_->invalidate_map_overlays();
  });
  host_->result_playback_panel_->set_play_change([this](bool on) {
    auto& session = host_->browser_->analysis_playback();
    session.set_playing(on);
    if (on && session.frame_count() <= 0) {
      session.set_playing(false);
      host_->result_playback_panel_->set_playing(false);
      return;
    }
    host_->sync_result_playback_timer();
  });
  host_->result_playback_panel_->set_loop_change([this](bool on) {
    host_->browser_->analysis_playback().set_looping(on);
  });
  host_->result_playback_panel_->set_looping(host_->browser_->analysis_playback().looping());
  host_->result_playback_panel_->set_status_text("(no session)");
}


void ProcessingComposer::wire_report_panel() {
  if (!host_->report_panel_) {
    return;
  }
  std::unique_ptr<plugin::ReportBrowser> backend;
  if (const char* be = base::switch_cstr("report-browser");
      be && (std::strcmp(be, "fake") == 0 || std::strcmp(be, "0") == 0)) {
    backend = std::make_unique<plugin::FakeReportBrowser>();
  } else {
    backend = std::make_unique<plugin::WebView2ReportBrowser>();
  }
  host_->report_panel_->set_browser(std::move(backend));
  // Allow sample + captures trees when present next to the exe.
  wchar_t module_path[MAX_PATH] = {};
  if (GetModuleFileNameW(nullptr, module_path, MAX_PATH) > 0) {
    std::filesystem::path exe(module_path);
    host_->report_panel_->add_allowed_root((exe.parent_path() / ".." / "data").string());
    host_->report_panel_->add_allowed_root(
        (exe.parent_path() / "captures").string());
    host_->report_panel_->add_allowed_root(exe.parent_path().string());
    // Repo sample: out/Debug -> ../../testing/data/plugin/report
    host_->report_panel_->add_allowed_root(
        (exe.parent_path() / ".." / ".." / "testing" / "data" / "plugin" /
         "report")
            .string());
  }
  // Repo testing sample (dev trees): testing/data/plugin/report
  {
    std::filesystem::path cur = std::filesystem::current_path();
    host_->report_panel_->add_allowed_root(
        (cur / "testing" / "data" / "plugin" / "report").string());
    host_->report_panel_->add_allowed_root(
        (cur / ".." / "testing" / "data" / "plugin" / "report").string());
    host_->report_panel_->add_allowed_root(
        (cur / ".." / ".." / "testing" / "data" / "plugin" / "report")
            .string());
  }
  attach_report_plugin_bridge();
}

void ProcessingComposer::attach_report_plugin_bridge() {
  if (!host_->report_panel_ || !host_->browser_) {
    return;
  }
  if (!host_->browser_->plugins() || !host_->browser_->plugins()->host()) {
    return;
  }
  content::PluginHost* host = host_->browser_->plugins()->host();
  host->set_report_bridge(
      [this](std::string_view dir) {
        if (host_->report_tab_ >= 0) {
          host_->show_inspector_tab_index(host_->report_tab_);
        }
        return host_->report_panel_ && host_->report_panel_->open_report(dir);
      },
      [this](std::string_view json) {
        return host_->report_panel_ && host_->report_panel_->post_json(json);
      },
      [this]() {
        if (host_->report_panel_) {
          host_->report_panel_->close_report();
        }
      });
}


void ProcessingComposer::sync_result_playback_timer() {
  HWND h = host_->hwnd();
  if (!h) {
    return;
  }
  constexpr UINT_PTR kPlayback = 0x504C424Bu;  // 'PLBK'
  SetPropW(h, L"PlaybackBrowser", reinterpret_cast<HANDLE>(host_));
  KillTimer(h, kPlayback);
  auto& session = host_->browser_->analysis_playback();
  if (!session.playing() || session.frame_count() <= 0) {
    return;
  }
  const double fps = session.fps() > 0.0 ? session.fps() : 12.0;
  const UINT ms = static_cast<UINT>(
      std::max(16.0, 1000.0 / fps));
  SetTimer(h, kPlayback, ms, [](HWND hwnd, UINT, UINT_PTR, DWORD) {
    auto* self = reinterpret_cast<BrowserView*>(
        GetPropW(hwnd, L"PlaybackBrowser"));
    if (!self || !self->browser_ || !self->result_playback_panel_) {
      return;
    }
    auto& session = self->browser_->analysis_playback();
    if (!session.playing() || session.frame_count() <= 0) {
      self->sync_result_playback_timer();
      return;
    }
    int next = session.frame_index() + 1;
    if (next >= session.frame_count()) {
      if (session.looping()) {
        next = 0;
      } else {
        session.set_playing(false);
        self->result_playback_panel_->set_playing(false);
        self->sync_result_playback_timer();
        return;
      }
    }
    if (self->browser_->apply_analysis_frame(next)) {
      self->result_playback_panel_->set_frame_index(next);
      self->invalidate_map_overlays();
    }
  });
}


void ProcessingComposer::wire_spatial_analysis_panel() {
  if (!host_->spatial_analysis_panel_) {
    return;
  }
  std::vector<ui::views::SpatialAnalysisPanel::Operator> ops;
  ops.reserve(plugin::builtin_op_catalog().size());
  for (const plugin::BuiltinOpDesc& d : plugin::builtin_op_catalog()) {
    ops.push_back({d.id, d.title, "native"});
  }
  host_->spatial_analysis_panel_->set_operators(std::move(ops));
  host_->spatial_analysis_panel_->set_params(
      {{"distance", "0.05", "buffer/simplify"},
       {"input", "(scene)", "path"},
       {"output", "(temp)", "path"}});
  host_->spatial_analysis_panel_->set_run_handler(
      [this](const std::string& id,
             const std::vector<ui::views::SpatialAnalysisPanel::Param>&
                 params) {
        std::string distance = "0.05";
        for (const auto& p : params) {
          if (p.name == "distance" && !p.value.empty()) {
            distance = p.value;
          }
        }
        host_->spatial_analysis_panel_->set_progress(0.1, "running");
        host_->run_processing_operator(id, distance);
        host_->spatial_analysis_panel_->set_progress(1.0, "done");
      });
  host_->spatial_analysis_panel_->set_cancel_handler([this]() {
    host_->spatial_analysis_panel_->set_progress(0.0, "cancelled");
    host_->set_status_message("Analysis cancelled");
  });
  if (auto* history = host_->spatial_analysis_panel_->history()) {
    history->set_rerun_handler([this](const std::string& op_id) {
      host_->run_processing_operator(op_id);
    });
    history->set_clear_handler(
        [this]() { host_->set_status_message("Analysis history cleared"); });
  }
}


void ProcessingComposer::run_processing_operator(const std::string& processing_id) {
  host_->run_processing_operator(processing_id, "0.05");
}


void ProcessingComposer::run_processing_operator(const std::string& processing_id,
                                            const std::string& distance) {
  if (!host_->browser_->plugins() || !host_->browser_->plugins()->host() ||
      processing_id.empty()) {
    host_->set_status_message("Processing: no host");
    return;
  }
  if (host_->browser_->document()->feature_count() == 0) {
    host_->set_status_message("Processing: no features");
    return;
  }
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path();
  const fs::path in = dir / "smartgis_views_proc_in.geojson";
  const fs::path out = dir / "smartgis_views_proc_out.geojson";
  const fs::path clip = dir / "smartgis_views_proc_clip.geojson";
  if (!host_->browser_->document()->write_path(in.string())) {
    host_->set_status_message("Processing: write_path failed");
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
    if (!host_->browser_->document()->compute_extent(&min_x, &min_y, &max_x, &max_y)) {
      host_->set_status_message("Processing: no extent for clip");
      return;
    }
    // MapScene stores Y flipped for screen; GeoJSON write_path unflips.
    // Clip fixture uses geographic lon/lat matching written GeoJSON.
    if (!write_m2_clip_geojson(clip, min_x, -max_y, max_x, -min_y)) {
      host_->set_status_message("Processing: clip fixture failed");
      return;
    }
    args += ",\"clip\":\"" + m2_json_path(clip.string()) + "\"";
  }
  args += "}";

  if (!host_->browser_->plugins()->run_processing(processing_id, args)) {
    host_->set_status_message(std::string("Processing failed: ") + processing_id);
    return;
  }
  host_->browser_->plugins()->flush_processing_for_test();
  if (!host_->browser_->document()->open_path(out.string()) ||
      host_->browser_->document()->feature_count() < 1) {
    host_->set_status_message(std::string("Processing write-back failed: ") +
                       processing_id);
    return;
  }
  host_->sync_catalog_from_scene();
  host_->sync_inspectors_from_scene();
  host_->invalidate_map_overlays();
  host_->browser_->fit_map_extent();
  const int n = static_cast<int>(host_->browser_->document()->feature_count());
  const std::string text = std::string("Processing ok: ") + processing_id +
                           " features=" + std::to_string(n);
  host_->set_status_message(text);
  if (host_->spatial_analysis_panel_ && host_->spatial_analysis_panel_->history()) {
    host_->spatial_analysis_panel_->history()->append_entry({"now", processing_id, text});
  }
}


void ProcessingComposer::bind_gis_python_bridge() {
  if (!host_->browser_ || !host_->browser_->plugins()) {
    return;
  }
  plugin::GisConsoleBridge bridge;
  bridge.write_active_geojson = [this](const std::string& path) {
    return host_->browser_ && host_->browser_->document() &&
           host_->browser_->document()->write_path(path);
  };
  bridge.write_path = bridge.write_active_geojson;
  bridge.load_result_geojson = [this](const std::string& path) {
    if (!host_->browser_ || !host_->browser_->document()) {
      return false;
    }
    if (!host_->browser_->document()->open_path(path)) {
      return false;
    }
    host_->sync_catalog_from_scene();
    host_->sync_inspectors_from_scene();
    host_->invalidate_map_overlays();
    return host_->browser_->document()->feature_count() >= 1;
  };
  bridge.open_path = bridge.load_result_geojson;
  bridge.feature_count = [this]() {
    return host_->browser_ && host_->browser_->document()
               ? static_cast<int>(host_->browser_->document()->feature_count())
               : 0;
  };
  bridge.refresh_map = [this]() {
    host_->sync_catalog_from_scene();
    host_->sync_inspectors_from_scene();
    host_->invalidate_map_overlays();
    if (host_->browser_) {
      host_->browser_->fit_map_extent();
    }
  };
  bridge.flush_processing = [this]() {
    if (host_->browser_ && host_->browser_->plugins()) {
      host_->browser_->plugins()->flush_processing_for_test();
    }
  };
  bridge.layers_json = [this]() {
    if (!host_->browser_ || !host_->browser_->document()) {
      return std::string("[]");
    }
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const content::LayerDesc& d : host_->browser_->document()->layer_descs()) {
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
    return host_->browser_ && host_->browser_->document() &&
           host_->browser_->document()->select_layer(id);
  };
  bridge.set_layer_visible = [this](const std::string& id, bool on) {
    return host_->browser_ && host_->browser_->document() &&
           host_->browser_->document()->set_layer_visible(id, on);
  };
  bridge.extent_json = [this]() {
    if (!host_->browser_ || !host_->browser_->document()) {
      return std::string("null");
    }
    double min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    if (!host_->browser_->document()->compute_extent(&min_x, &min_y, &max_x, &max_y)) {
      return std::string("null");
    }
    char buf[192];
    std::snprintf(buf, sizeof(buf),
                  "{\"min_x\":%.6f,\"min_y\":%.6f,\"max_x\":%.6f,\"max_y\":%.6f}",
                  min_x, min_y, max_x, max_y);
    return std::string(buf);
  };
  bridge.present_mode = [this]() {
    if (!host_->map_tabs_) {
      return std::string("map2d");
    }
    const int a = host_->map_tabs_->active();
    if (a == 1) {
      return std::string("data");
    }
    if (a == 2) {
      return std::string("scene3d");
    }
    return std::string("map2d");
  };
  bridge.set_present_mode = [this](const std::string& mode) {
    if (!host_->map_tabs_) {
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
    host_->switch_map_tab(idx);
    return true;
  };
  bridge.has_style_document = [this]() {
    return host_->browser_ && host_->browser_->document() &&
           host_->browser_->document()->has_style_document();
  };
  bridge.load_style_path = [this](const std::string& path) {
    return host_->browser_ && host_->browser_->document() &&
           host_->browser_->document()->load_style_path(path);
  };
  bridge.clear_style = [this]() {
    if (host_->browser_ && host_->browser_->document()) {
      host_->browser_->document()->clear_style_document();
    }
  };
  bridge.style_summary_json = [this]() {
    if (!host_->browser_ || !host_->browser_->document() ||
        !host_->browser_->document()->style_document()) {
      return std::string("null");
    }
    const gis::style::StyleDocument* doc =
        host_->browser_->document()->style_document();
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
    if (!host_->browser_ || !host_->browser_->map_session()) {
      return false;
    }
    uint32_t vid = view_id;
    if (vid == 0) {
      if (ui::views::MapViewport* pane = host_->active_map()) {
        vid = pane->view_id();
      }
    }
    host_->browser_->map_session()->ActivateTool(vid, tool_id.c_str());
    return true;
  };
  plugin::set_gis_console_bridge(std::move(bridge));
  host_->browser_->plugins()->ensure_python();
}


}  // namespace app
