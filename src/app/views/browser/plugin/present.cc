// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/plugin/present.h"

#include "app/views/browser/browser.h"
#include "app/views/browser/ui_delegate.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/browser/plugin/playback.h"
#include "app/views/util/exe_sidecar_path.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/public/gis_document.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/present/gis_present.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {
namespace detail {

bool add_standin_mesh(content::MapScene* doc, const char* name, double lon,
                      double lat, double half_deg) {
  content::MapSceneGisDocument gis(doc);
  return plugin::add_standin_mesh(&gis, name, lon, lat, half_deg);
}

void present_plugin_map2d(BrowserUiDelegate* ui, content::Map2dPresenter* map2d,
                          const std::function<void()>& fit_extent) {
  if (!ui) {
    return;
  }
  ui->select_map_tab(0);
  if (fit_extent) {
    fit_extent();
  }
  if (map2d) {
    map2d->invalidate_frame_cache();
  }
  ui->invalidate_native_map();
}

void present_plugin_scene3d(BrowserUiDelegate* ui) {
  if (!ui) {
    return;
  }
  ui->select_map_tab(1);
  ui->invalidate_native_scene();
}

bool present_plugin_dataset(Browser* browser, content::Map2dPresenter* map2d,
                            const std::function<void()>& fit_extent,
                            std::string_view path, int face, int surface) {
  if (!browser) {
    return false;
  }
  if (surface != 0) {
    return browser->plugin_preview().present(browser, face, path);
  }
  if (face != 0) {
    present_plugin_scene3d(browser->ui());
  } else {
    present_plugin_map2d(browser->ui(), map2d, fit_extent);
  }
  return true;
}

std::string present_frame_processing_id(content::PluginHost* host,
                                        std::string_view plugin_id) {
  std::string found;
  if (!host || plugin_id.empty()) {
    return found;
  }
  host->for_each_processing(
      [&](std::string_view pid, std::string_view id, std::string_view) {
        if (pid != plugin_id || !id.ends_with(".present_frame")) {
          return;
        }
        found.assign(id.data(), id.size());
      });
  return found;
}

bool present_plugin_frame(PluginShell* plugins,
                          PluginPlayback* session,
                          int index) {
  if (!session || !session->set_frame_index(index)) {
    return false;
  }
  if (!plugins || !plugins->host()) {
    return false;
  }
  std::string id = session->present_frame_id();
  if (id.empty()) {
    id = present_frame_processing_id(plugins->host(), session->plugin_id());
  }
  if (id.empty()) {
    return false;
  }
  char args[64] = {};
  std::snprintf(args, sizeof(args), "{\"index\":%d}", session->frame_index());
  return plugins->run_processing(id, args);
}

int export_plugin_frames(PluginShell* plugins, PluginPlayback* session,
                         content::Map2dPresenter* map2d,
                         const std::string& dir_leaf) {
  if (!session || !map2d || dir_leaf.empty() || session->frame_count() <= 0) {
    return 0;
  }
  std::wstring leaf_w(dir_leaf.begin(), dir_leaf.end());
  const int nframes = session->frame_count();
  int wrote = 0;
  for (int i = 0; i < nframes; ++i) {
    if (!present_plugin_frame(plugins, session, i)) {
      continue;
    }
    wchar_t frame_leaf[MAX_PATH] = {};
    _snwprintf_s(frame_leaf, _TRUNCATE, L"%s\\frame_%04d.bmp", leaf_w.c_str(),
                 i);
    wchar_t bmp_w[MAX_PATH] = {};
    if (!exe_capture_path(bmp_w, MAX_PATH, frame_leaf)) {
      continue;
    }
    char bmp_a[MAX_PATH] = {};
    if (WideCharToMultiByte(CP_ACP, 0, bmp_w, -1, bmp_a, MAX_PATH, nullptr,
                            nullptr) <= 0) {
      continue;
    }
    if (map2d->export_bmp(bmp_a, 640, 480)) {
      ++wrote;
    }
  }

  std::wstring json_leaf = leaf_w + L"\\playback.json";
  wchar_t json_w[MAX_PATH] = {};
  if (exe_capture_path(json_w, MAX_PATH, json_leaf.c_str())) {
    char json_a[MAX_PATH] = {};
    if (WideCharToMultiByte(CP_ACP, 0, json_w, -1, json_a, MAX_PATH, nullptr,
                            nullptr) > 0) {
      std::ofstream out(json_a, std::ios::binary);
      if (out) {
        out << session->playback_json();
      }
    }
  }
  return wrote;
}

}  // namespace detail
}  // namespace app
