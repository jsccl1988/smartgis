// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/plugin/analysis_writer_flood.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/browser_ui_delegate.h"
#include "app/views/shell/browser/plugin/analysis_writer_common.h"
#include "app/views/shell/runtime/analysis/playback.h"
#include "content/browser/document/map_scene.h"
#include "gis/present/style/style_document.h"
#include "plugin/product/flood/commands.h"
#include "tool/draft/draft.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace app {
namespace detail {
namespace {

constexpr const char* kFloodStyleJson = R"json({
  "version": 8,
  "name": "flood_mask",
  "layers": [
    {"id":"bg","type":"background",
     "paint":{"background-color":"#f5f0e6","background-opacity":1}},
    {"id":"terrain","type":"fill","source-layer":"flood_terrain",
     "paint":{"fill-color":"#d5d8dc","fill-opacity":0.55}},
    {"id":"terrain_edge","type":"line","source-layer":"flood_terrain",
     "paint":{"line-color":"#566573","line-width":2.0}},
    {"id":"flood_cells","type":"fill","source-layer":"flood_mask",
     "paint":{"fill-color":"#2980b9","fill-opacity":0.85}},
    {"id":"flood_edge","type":"line","source-layer":"flood_mask",
     "paint":{"line-color":"#1a5276","line-width":2.5}}
  ]
})json";


}  // namespace

bool paint_flood_mask_layer(content::MapScene* doc,
                            BrowserUiDelegate* ui,
                            const unsigned char* mask,
                            int width,
                            int height,
                            const double* geotransform,
                            double water_level,
                            bool rebuild_terrain,
                            bool add_water_standin) {
  if (!doc || !mask || !geotransform || width <= 0 || height <= 0) {
    return false;
  }
  const double gt0 = geotransform[0];
  const double gt1 = geotransform[1];
  const double gt2 = geotransform[2];
  const double gt3 = geotransform[3];
  const double gt4 = geotransform[4];
  const double gt5 = geotransform[5];

  if (rebuild_terrain) {
    doc->remove_layer("flood_terrain");
    if (!doc->create_layer("flood_terrain", "Polygon")) {
      return false;
    }
    const double x0 = gt0;
    const double y0 = gt3;
    const double x1 = gt0 + gt1 * width + gt2 * height;
    const double y1 = gt3 + gt4 * width + gt5 * height;
    const std::vector<std::pair<double, double>> terrain = {
        {x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}, {x0, y0},
    };
    if (!append_map_polygon(doc, terrain, "0")) {
      return false;
    }
  }

  doc->remove_layer("flood_mask");
  doc->remove_layer("Flood water 3D");
  if (!doc->create_layer("flood_mask", "Polygon")) {
    return false;
  }

  constexpr int kMaxDim = 96;
  const int step_x = std::max(1, (width + kMaxDim - 1) / kMaxDim);
  const int step_y = std::max(1, (height + kMaxDim - 1) / kMaxDim);

  // Presence scan (every cell) — step sampling alone can miss sparse wet masks.
  bool any_wet = false;
  for (int r = 0; r < height && !any_wet; ++r) {
    for (int c = 0; c < width; ++c) {
      const size_t idx =
          static_cast<size_t>(r) * static_cast<size_t>(width) +
          static_cast<size_t>(c);
      if (mask[idx]) {
        any_wet = true;
        break;
      }
    }
  }
  if (!any_wet) {
    return false;
  }

  int wet = 0;
  double wet_min_x = 0;
  double wet_min_y = 0;
  double wet_max_x = 0;
  double wet_max_y = 0;
  double sum_x = 0;
  double sum_y = 0;
  for (int r = 0; r < height; r += step_y) {
    for (int c = 0; c < width; c += step_x) {
      const size_t idx =
          static_cast<size_t>(r) * static_cast<size_t>(width) +
          static_cast<size_t>(c);
      if (!mask[idx]) {
        continue;
      }
      const double px0 = gt0 + gt1 * c + gt2 * r;
      const double py0 = gt3 + gt4 * c + gt5 * r;
      const double px1 = gt0 + gt1 * (c + step_x) + gt2 * (r + step_y);
      const double py1 = gt3 + gt4 * (c + step_x) + gt5 * (r + step_y);
      const double min_x = std::min(px0, px1);
      const double max_x = std::max(px0, px1);
      const double min_y = std::min(py0, py1);
      const double max_y = std::max(py0, py1);
      if (wet == 0) {
        wet_min_x = min_x;
        wet_max_x = max_x;
        wet_min_y = min_y;
        wet_max_y = max_y;
      } else {
        wet_min_x = std::min(wet_min_x, min_x);
        wet_max_x = std::max(wet_max_x, max_x);
        wet_min_y = std::min(wet_min_y, min_y);
        wet_max_y = std::max(wet_max_y, max_y);
      }
      sum_x += 0.5 * (min_x + max_x);
      sum_y += 0.5 * (min_y + max_y);
      ++wet;
    }
  }
  // Step grid missed every wet cell: fall back to full-raster envelope.
  if (wet == 0) {
    const double x0 = gt0;
    const double y0 = gt3;
    const double x1 = gt0 + gt1 * width + gt2 * height;
    const double y1 = gt3 + gt4 * width + gt5 * height;
    wet_min_x = std::min(x0, x1);
    wet_max_x = std::max(x0, x1);
    wet_min_y = std::min(y0, y1);
    wet_max_y = std::max(y0, y1);
    sum_x = 0.5 * (wet_min_x + wet_max_x);
    sum_y = 0.5 * (wet_min_y + wet_max_y);
    wet = 1;
  }
  const std::vector<std::pair<double, double>> lake = {
      {wet_min_x, wet_min_y},
      {wet_max_x, wet_min_y},
      {wet_max_x, wet_max_y},
      {wet_min_x, wet_max_y},
      {wet_min_x, wet_min_y},
  };
  if (!append_map_polygon(doc, lake, "1")) {
    return false;
  }

  const double cx = sum_x / static_cast<double>(wet);
  const double cy = sum_y / static_cast<double>(wet);
  const double half = std::max(
      0.01, 0.15 * std::max(std::abs(gt1) * width, std::abs(gt5) * height));
  (void)water_level;
  if (add_water_standin) {
    add_standin_mesh(doc, ui, "Flood water 3D", cx, -cy, half);
  }
  return refresh_ui_after_layer(ui);
}

// Downsampled wet-cell polygons; all frames buffered on |session|.
bool commit_flood_mask(content::MapScene* doc,
                       BrowserUiDelegate* ui,
                       AnalysisPlayback* session,
                       const unsigned char* mask,
                       int width,
                       int height,
                       const double* geotransform,
                       int frame_index,
                       int frame_count,
                       double water_level) {
  if (!doc || !mask || !geotransform || width <= 0 || height <= 0 || !session) {
    return false;
  }
  if (frame_index == 0 || session->product() != AnalysisProduct::kFlood) {
    session->begin_flood(width, height, geotransform, water_level, frame_count);
    doc->clear();
    if (!apply_style_json(doc, kFloodStyleJson)) {
      return false;
    }
  }
  session->push_flood_mask(mask, width, height, water_level);

  // Paint only the latest pushed frame (intermediate + final).
  return paint_flood_mask_layer(doc, ui, mask, width, height, geotransform,
                                water_level, frame_index == 0,
                                /*add_water_standin=*/true);
}

void wire_flood_analysis_writers(Browser* browser) {
  if (!browser) {
    return;
  }
  plugin::set_flood_mask_writer(
      [browser](const unsigned char* mask, int width, int height,
             const double* geotransform, int frame_index, int frame_count,
             double water_level) {
        return commit_flood_mask(&browser->session().document(), browser->ui(),
                                 &browser->analysis_playback(), mask, width, height,
                                 geotransform, frame_index, frame_count,
                                 water_level);
      });

}

}  // namespace detail
}  // namespace app
