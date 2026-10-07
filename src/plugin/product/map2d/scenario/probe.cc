// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/probe.h"

#include "plugin/runtime/host/capability/shell.h"

#include "content/public/map_contents.h"
#include "content/public/view_host.h"
#include "ui/views/map/viewport/draw_host.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstring>

namespace plugin {

bool viewport_has_presented_frame(ui::views::DrawHost* pane) {
  if (!pane ||
      pane->attach_mode() !=
          ui::views::DrawHost::AttachMode::kContentMapView) {
    return false;
  }
  content::MapContents* session = pane->map_contents();
  if (!session || pane->view_id() == 0) {
    return false;
  }
  content::MapWidgetHostView* view = session->HostView(pane->view_id());
  if (!view) {
    return false;
  }
  const content::SharedSurface surface = view->Latest();
  return surface.generation > 0 && surface.nt_handle != nullptr &&
         surface.width_px >= 8 && surface.height_px >= 8;
}

bool try_open_china_sample(HarnessShell& browser, bool* city_pack) {
  if (city_pack) {
    *city_pack = false;
  }
  content::MapScene* doc = browser.document();
  if (!doc) {
    return false;
  }
  // Only reuse an already-loaded China pack. edit_m0 clears seed data and
  // leaves a small round-trip layer — that must not short-circuit reload.
  if (doc->layer_count() > 0 && doc->feature_count() >= 3 &&
      doc->has_china_extent()) {
    if (city_pack && doc->layer_count() >= 3 && doc->feature_count() >= 200) {
      *city_pack = true;
    }
    return true;
  }
  wchar_t sample_w[MAX_PATH] = {};
  if (!browser.exe_dir_slash(sample_w, MAX_PATH)) {
    return false;
  }
  const wchar_t* candidates[] = {L"..\\data\\china_city.gpkg",
                                 L"..\\data\\china_city.geojson",
                                 L"..\\data\\china_plp.geojson",
                                 L"data\\china_city.gpkg",
                                 L"data\\china_city.geojson",
                                 L"data\\china_plp.geojson",
                                 L"china_city.gpkg",
                                 L"china_city.geojson",
                                 L"china_plp.geojson"};
  char sample_a[MAX_PATH] = {};
  for (const wchar_t* name : candidates) {
    wchar_t joined[MAX_PATH] = {};
    if (wcscpy_s(joined, sample_w) != 0 || wcscat_s(joined, name) != 0) {
      continue;
    }
    if (GetFileAttributesW(joined) == INVALID_FILE_ATTRIBUTES) {
      continue;
    }
    wchar_t full[MAX_PATH] = {};
    if (GetFullPathNameW(joined, MAX_PATH, full, nullptr) == 0) {
      continue;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, full, -1, sample_a, MAX_PATH, nullptr,
                            nullptr) <= 0) {
      continue;
    }
    if (doc->open_path(sample_a) && doc->last_open_was_ogr() &&
        doc->feature_count() >= 3) {
      if (city_pack) {
        *city_pack = (wcsstr(name, L"china_city") != nullptr);
      }
      return true;
    }
  }
  return false;
}

}  // namespace plugin
