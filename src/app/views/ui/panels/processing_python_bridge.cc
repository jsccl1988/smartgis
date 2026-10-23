// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/panels/processing_composer.h"
#include "app/views/ui/browser_view.h"

#include "app/views/browser/browser.h"

#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/browser/plugin/shell/plugin_shell.h"
#include "base/process/switches.h"
#include "content/browser/session/browser_session.h"
#include "content/public/tool_session.h"
#include "content/public/types.h"
#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/host/bind/console.h"
#include "gis/style/document/style_document.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/primitives/collection/tab_strip.h"

namespace app {

// Python GIS console: fill CapabilityHost document/view lanes (no GisConsoleBridge).

void ProcessingComposer::bind_gis_python_bridge() {
  // Out-of-line BrowserView::browser() keeps browser_ offsetof in browser_view
  // .obj (not inlined into a skewed ProcessingComposer TU).
  Browser* browser = host_ ? host_->browser() : nullptr;
  if (!browser || !browser->plugins()) {
    return;
  }
  content::CapabilityHost host;
  host.document.write_active_geojson = [this](const std::string& path) {
    return host_->browser_ && host_->browser_->document() &&
           host_->browser_->session().write_document(path);
  };
  host.document.write_document = host.document.write_active_geojson;
  host.document.load_result_geojson = [this](const std::string& path) {
    if (!host_->browser_ || !host_->browser_->document()) {
      return false;
    }
    if (!host_->browser_->session().open_document(path)) {
      return false;
    }
    host_->sync_catalog_from_scene();
    host_->sync_inspectors_from_scene();
    host_->invalidate_map_overlays();
    return host_->browser_->session().document_feature_count() >= 1;
  };
  host.document.open_document = host.document.load_result_geojson;
  host.document.feature_count = [this]() {
    return host_->browser_ && host_->browser_->document()
               ? static_cast<int>(
                     host_->browser_->session().document_feature_count())
               : 0;
  };
  host.document.refresh_gis = [this]() {
    host_->sync_catalog_from_scene();
    host_->sync_inspectors_from_scene();
    host_->invalidate_map_overlays();
    if (host_->browser_) {
      host_->browser_->fit_map_extent();
    }
  };
  host.document.flush_processing = [this]() {
    if (host_->browser_ && host_->browser_->plugins()) {
      host_->browser_->plugins()->flush_processing_for_test();
    }
  };
  host.document.layers_json = [this]() {
    if (!host_->browser_ || !host_->browser_->document()) {
      return std::string("[]");
    }
    return content::layers_to_catalog_json(
        host_->browser_->session().document_layer_descs());
  };
  host.document.select_layer = [this](const std::string& id) {
    return host_->browser_ && host_->browser_->document() &&
           host_->browser_->session().select_layer(id);
  };
  host.document.set_layer_visible = [this](const std::string& id, bool on) {
    return host_->browser_ && host_->browser_->document() &&
           host_->browser_->session().set_layer_visible(id, on);
  };
  host.document.extent_json = [this]() {
    if (!host_->browser_ || !host_->browser_->document()) {
      return std::string("null");
    }
    double min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    if (!host_->browser_->session().document_compute_extent(&min_x, &min_y,
                                                            &max_x, &max_y)) {
      return std::string("null");
    }
    char buf[192];
    std::snprintf(buf, sizeof(buf),
                  "{\"min_x\":%.6f,\"min_y\":%.6f,\"max_x\":%.6f,\"max_y\":%.6f}",
                  min_x, min_y, max_x, max_y);
    return std::string(buf);
  };
  host.view.present_mode = [this]() {
    if (!host_->map_tabs_) {
      return std::string("map2d");
    }
    if (host_->map_tabs_->active() == 1) {
      return std::string("scene3d");
    }
    return std::string("map2d");
  };
  host.view.set_present_mode = [this](const std::string& mode) {
    if (!host_->map_tabs_) {
      return false;
    }
    int idx = 0;
    if (mode == "scene3d") {
      idx = 1;
    } else if (mode == "data") {
      idx = 0;
    } else if (mode != "map2d") {
      return false;
    }
    host_->switch_map_tab(idx);
    return true;
  };
  host.document.has_style_document = [this]() {
    return host_->browser_ && host_->browser_->document() &&
           host_->browser_->session().document_has_style();
  };
  host.document.apply_style_file = [this](const std::string& path) {
    return host_->browser_ && host_->browser_->document() &&
           host_->browser_->session().load_style_path(path);
  };
  host.document.clear_style = [this]() {
    if (host_->browser_ && host_->browser_->document()) {
      host_->browser_->session().clear_style_document();
    }
  };
  host.document.style_summary_json = [this]() {
    if (!host_->browser_ || !host_->browser_->document() ||
        !host_->browser_->session().document_style()) {
      return std::string("null");
    }
    const gis::style::StyleDocument* doc =
        host_->browser_->session().document_style();
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
  host.view.activate_tool_at = [this](uint32_t view_id,
                                      const std::string& tool_id) {
    if (!host_->browser_ || !host_->browser_->map_session()) {
      return false;
    }
    uint32_t vid = view_id;
    if (vid == 0) {
      if (ui::views::DrawHost* pane = host_->active_map()) {
        vid = pane->view_id();
      }
    }
    content::ToolSession* session = host_->active_tool_session();
    if (!session) {
      session = host_->browser_->edit_tool_session();
    }
    if (!session) {
      return false;
    }
    if (!session->execute(tool_id, vid)) {
      session->activate(tool_id);
    }
    return true;
  };
  content::set_console_host(std::move(host));
  // --self-test / --self-test-console must not ensure_builtins (traffic start
  // has called a null fn under cdb; product packs are not the suite).
  if (!base::switch_is_one("harness-run")) {
    host_->browser_->plugins()->ensure_python();
  }
}

}  // namespace app
