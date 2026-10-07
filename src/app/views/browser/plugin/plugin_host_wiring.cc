// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/view_host.h"
#include "app/views/browser/browser.h"

#include <string>
#include <string_view>
#include <utility>

#include "app/views/browser/china_product_defaults.h"
#include "app/views/browser/plugin/map2d_bridges.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/browser/plugin/present.h"
#include "app/views/browser/plugin/scene3d_bridges.h"
#include "content/browser/document/map_scene.h"
#include "content/public/event_bus.h"
#include "content/public/plugin_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {

void Browser::install_plugin_host_bridges() {
  if (!plugins_ || !plugins_->host()) {
    return;
  }
  content::PluginHost* host = plugins_->host();
  content::EventBus* events = nullptr;
  if (session_->edit_host()) {
    events = session_->edit_host()->events();
  }
  gis_document_ = std::make_unique<content::MapSceneGisDocument>(
      &session_->document(), events);
  host->set_gis_document(gis_document_.get());

  detail::Scene3dHostContext ctx;
  ctx.document = &session_->document();
  ctx.scene3d = &session_->scene3d();
  ctx.orbit = &session_->orbit_frame();
  ctx.host = host;
  ctx.present_scene3d = [this]() { detail::present_plugin_scene3d(ui_.get()); };
  ctx.apply_china_product = [this]() {
    apply_china_scene3d_product_defaults(*this);
  };
  ctx.apply_china_atmo = [this]() { apply_china_scene3d_atmosphere(*this); };
  ctx.push_shared_extent = [this]() { push_shared_extent(); };
  ctx.select_map_tab = [this](int index) { select_map_tab(index); };
  detail::install_scene3d_host_bridges(ctx);

  detail::Map2dHostContext map2d_ctx;
  map2d_ctx.document = &session_->document();
  map2d_ctx.map2d = map2d();
  map2d_ctx.view_frame = view_frame();
  map2d_ctx.host = host;
  map2d_ctx.present_map2d = [this]() {
    detail::present_plugin_map2d(ui_.get(), map2d(),
                                 [this]() { fit_map_extent(); });
  };
  map2d_ctx.apply_china_product = [this](int w, int h) {
    apply_china_map2d_product_defaults(*this, w, h);
  };
  map2d_ctx.push_shared_extent = [this]() { push_shared_extent(); };
  map2d_ctx.select_map_tab = [this](int index) { select_map_tab(index); };
  map2d_ctx.view_size = [this](int* w, int* h) {
    if (w) {
      *w = 1280;
    }
    if (h) {
      *h = 720;
    }
    if (HWND horizon = hwnd()) {
      if (IsWindow(horizon)) {
        RECT rc = {};
        GetClientRect(horizon, &rc);
        if (w && rc.right > 32) {
          *w = rc.right;
        }
        if (h && rc.bottom > 32) {
          *h = rc.bottom;
        }
      }
    }
  };
  detail::install_map2d_host_bridges(map2d_ctx);

  if (content::PluginHost::Playback* pb = host->playback()) {
    plugin_playback_.bind_host_playback(
        [pb](int index, int count, bool rebuild) {
          if (rebuild) {
            pb->clear();
            for (int i = 0; i < count; ++i) {
              pb->push_frame("{\"index\":" + std::to_string(i) + "}");
            }
          }
          if (count > 0) {
            pb->set_index(static_cast<size_t>(index < 0 ? 0 : index));
          }
        });
  }

  if (events) {
    layers_sub_ = events->subscribe<content::LayersChanged>(
        [this](const content::LayersChanged&) {
          sync_catalog_from_scene();
          refresh_inspectors();
        });
  }
}

void Browser::wire_plugin_present_dataset() {
  if (!plugins_ || !plugins_->host()) {
    return;
  }
  plugins_->host()->set_present_dataset_bridge(
      [this](std::string_view plugin_id, std::string_view path, int face,
             int surface) {
        // Nested present_dataset (orthogrid commit → tab/fit/invalidate, or
        // for_each_processing walking the same vtable) must not re-enter.
        static thread_local int depth = 0;
        if (depth > 0) {
          return true;
        }
        ++depth;
        struct DepthGuard {
          ~DepthGuard() { --depth; }
        } guard;

        const bool scene3d = face == 1;
        if (!scene3d && !path.empty()) {
          const size_t dot = path.find_last_of('.');
          if (dot != std::string_view::npos) {
            const std::string ext(path.substr(dot));
            if (_stricmp(ext.c_str(), ".geojson") == 0 ||
                _stricmp(ext.c_str(), ".gpkg") == 0 ||
                _stricmp(ext.c_str(), ".shp") == 0 ||
                _stricmp(ext.c_str(), ".kml") == 0 ||
                _stricmp(ext.c_str(), ".gml") == 0) {
              (void)session_->document().open_path(std::string(path));
              sync_catalog_from_scene();
            }
          }
        }
        if (plugins_ && plugins_->host() && plugins_->host()->playback()) {
          const int n = static_cast<int>(
              plugins_->host()->playback()->frame_count());
          if (n > 0 && !plugin_id.empty()) {
            // Resolve *.present_frame lazily in present_plugin_frame. Looking
            // it up here via for_each_processing re-entered present_dataset
            // (0xC00000FD) on --plugin-showcase=orthogrid.
            plugin_playback_.adopt_host_frames(std::string(plugin_id),
                                               std::string(), n);
          }
        }
        return detail::present_plugin_dataset(
            this, map2d(), [this]() { fit_map_extent(); }, path, face, surface);
      });
}

bool Browser::apply_plugin_frame(int index) {
  return detail::present_plugin_frame(plugins_.get(), &plugin_playback_, index);
}

int Browser::export_plugin_frames(const std::string& dir_leaf) {
  return detail::export_plugin_frames(plugins_.get(), &plugin_playback_,
                                      map2d(), dir_leaf);
}

}  // namespace app
