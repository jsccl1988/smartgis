// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/frame.h"

#include <algorithm>
#include <string>

#include "app/views/browser/browser.h"
#include "app/views/browser/china_product_defaults.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/il.runtime/backend/bmp.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/public/plugin_host.h"

namespace app {
namespace detail {

bool resolve_export_frame(Browser& browser, const std::string& frame) {
  content::ViewFrame* vf = browser.view_frame();
  if (!vf) {
    return false;
  }
  if (frame == "china_product") {
    ensure_china_maplibre_carto(browser);
    frame_china_map2d(browser, kCaptureW, kCaptureH);
  } else if (frame == "unit_square") {
    constexpr content::Extent2 kUnit{0.0, 0.0, 1.0, 1.0};
    vf->apply_world_extent(kUnit, kCaptureW, kCaptureH);
  } else if (frame == "document_extent") {
    double minx = 0.0;
    double miny = 0.0;
    double maxx = 0.0;
    double maxy = 0.0;
    if (browser.document() &&
        browser.document()->compute_extent(&minx, &miny, &maxx, &maxy) &&
        maxx > minx && maxy > miny) {
      // compute_extent returns map space (y = -lat). apply_world_extent expects
      // lon/lat Extent2 and converts to map internally — convert here once.
      const double lat_min = -maxy;
      const double lat_max = -miny;
      const double pad_x = std::max(0.05, (maxx - minx) * 0.15);
      const double pad_y = std::max(0.05, (lat_max - lat_min) * 0.15);
      const content::Extent2 live{minx - pad_x, lat_min - pad_y, maxx + pad_x,
                                  lat_max + pad_y};
      vf->apply_world_extent(live, kCaptureW, kCaptureH);
    } else {
      constexpr content::Extent2 kUnit{0.0, 0.0, 1.0, 1.0};
      vf->apply_world_extent(kUnit, kCaptureW, kCaptureH);
    }
  } else {
    double min_lon = 0.0;
    double min_lat = 0.0;
    double max_lon = 0.0;
    double max_lat = 0.0;
    PluginShell* shell = browser.plugins();
    content::PluginHost* host = shell ? shell->host() : nullptr;
    if (!host || !host->lookup_export_frame(frame, &min_lon, &min_lat, &max_lon,
                                            &max_lat)) {
      return false;
    }
    const content::Extent2 extent{min_lon, min_lat, max_lon, max_lat};
    vf->apply_world_extent(extent, kCaptureW, kCaptureH);
  }
  if (content::Map2dPresenter* map2d = browser.map2d()) {
    map2d->invalidate_frame_cache();
  }
  return true;
}

}  // namespace detail
}  // namespace app
