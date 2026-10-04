// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/plugin/analysis_writer_flood.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/browser_ui_delegate.h"
#include "app/views/shell/browser/plugin/analysis_writer_common.h"
#include "app/views/shell/runtime/analysis/playback.h"
#include "content/browser/document/map_scene.h"
#include "gis/carto/style/style_document.h"
#include "plugin/product/flood/commands.h"
#include "tool/draft/draft.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace app {
namespace detail {
namespace {

// Mid-complexity inundation face: green/tan dry land mosaic + depth-graded
// flood cells (not a single monochrome blue blob on empty grey tiles).
constexpr const char* kFloodStyleJson = R"json({
  "version": 8,
  "name": "flood_mask",
  "layers": [
    {"id":"bg","type":"background",
     "paint":{"background-color":"#aad3df","background-opacity":1}},
    {"id":"terrain","type":"fill","source-layer":"flood_terrain",
     "paint":{
       "fill-color":["interpolate",["linear"],["get","heat"],
         0,"#1e8449",35,"#7dcea0",65,"#d5a35b",100,"#a04000"],
       "fill-opacity":0.92}},
    {"id":"terrain_edge","type":"line","source-layer":"flood_terrain",
     "paint":{"line-color":"#1b4332","line-width":1.2,"line-opacity":0.55}},
    {"id":"flood_cells","type":"fill","source-layer":"flood_mask",
     "paint":{
       "fill-color":["interpolate",["linear"],["get","heat"],
         0,"#85c1e9",40,"#2e86c1",70,"#1a5276",100,"#0b3c5d"],
       "fill-opacity":0.88}},
    {"id":"flood_edge","type":"line","source-layer":"flood_mask",
     "paint":{"line-color":"#0e4d7a","line-width":1.8}}
  ]
})json";

void format_heat01(double t01, char* buf, size_t cap) {
  if (!buf || cap < 4) {
    return;
  }
  const double v = std::clamp(t01, 0.0, 1.0) * 100.0;
  std::snprintf(buf, cap, "%.1f", v);
}

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
    // Dense land mosaic with elevation-like heat (row/col proxy) so dry land
    // reads green→tan against the graded flood mask.
    constexpr int kTerrainTiles = 16;
    const int tw = (std::max)(1, width / kTerrainTiles);
    const int th = (std::max)(1, height / kTerrainTiles);
    for (int tr = 0; tr < height; tr += th) {
      for (int tc = 0; tc < width; tc += tw) {
        const int tr1 = (std::min)(tr + th, height);
        const int tc1 = (std::min)(tc + tw, width);
        int dry = 0;
        int cells = 0;
        for (int rr = tr; rr < tr1; ++rr) {
          for (int cc = tc; cc < tc1; ++cc) {
            ++cells;
            const size_t idx =
                static_cast<size_t>(rr) * static_cast<size_t>(width) +
                static_cast<size_t>(cc);
            if (!mask[idx]) {
              ++dry;
            }
          }
        }
        // Skip fully wet tiles so flood mosaic + wash margin stay readable.
        if (cells > 0 && dry * 4 < cells) {
          continue;
        }
        const double px0 = gt0 + gt1 * tc + gt2 * tr;
        const double py0 = gt3 + gt4 * tc + gt5 * tr;
        const double px1 = gt0 + gt1 * tc1 + gt2 * tr1;
        const double py1 = gt3 + gt4 * tc1 + gt5 * tr1;
        const double min_x = std::min(px0, px1);
        const double max_x = std::max(px0, px1);
        const double min_y = std::min(py0, py1);
        const double max_y = std::max(py0, py1);
        const std::vector<std::pair<double, double>> terrain = {
            {min_x, min_y}, {max_x, min_y}, {max_x, max_y},
            {min_x, max_y}, {min_x, min_y},
        };
        // Low heat = green hillside; high heat = tan ridge (checker + lat).
        const double row_t = static_cast<double>(tr) /
                             static_cast<double>((std::max)(1, height - 1));
        const double col_t = static_cast<double>(tc) /
                             static_cast<double>((std::max)(1, width - 1));
        const int checker =
            (static_cast<int>(tc / tw) + static_cast<int>(tr / th)) & 1;
        const double heat_t =
            0.15 + 0.55 * row_t + 0.25 * static_cast<double>(checker) +
            0.05 * col_t;
        char heat_buf[32] = {};
        format_heat01(heat_t, heat_buf, sizeof(heat_buf));
        if (!append_map_polygon(doc, terrain, heat_buf)) {
          return false;
        }
      }
    }
  }

  doc->remove_layer("flood_mask");
  doc->remove_layer("Flood water 3D");
  if (!doc->create_layer("flood_mask", "Polygon")) {
    return false;
  }

  constexpr int kMaxDim = 48;
  const int step_x = std::max(1, (width + kMaxDim - 1) / kMaxDim);
  const int step_y = std::max(1, (height + kMaxDim - 1) / kMaxDim);

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
  for (int r = 0; r < height; r += step_y) {
    for (int c = 0; c < width; c += step_x) {
      const int r1 = (std::min)(r + step_y, height);
      const int c1 = (std::min)(c + step_x, width);
      int wet_cells = 0;
      int cell_n = 0;
      for (int rr = r; rr < r1; ++rr) {
        for (int cc = c; cc < c1; ++cc) {
          ++cell_n;
          const size_t idx =
              static_cast<size_t>(rr) * static_cast<size_t>(width) +
              static_cast<size_t>(cc);
          if (mask[idx]) {
            ++wet_cells;
          }
        }
      }
      if (wet_cells == 0) {
        continue;
      }
      const double px0 = gt0 + gt1 * c + gt2 * r;
      const double py0 = gt3 + gt4 * c + gt5 * r;
      const double px1 = gt0 + gt1 * c1 + gt2 * r1;
      const double py1 = gt3 + gt4 * c1 + gt5 * r1;
      const double min_x = std::min(px0, px1);
      const double max_x = std::max(px0, px1);
      const double min_y = std::min(py0, py1);
      const double max_y = std::max(py0, py1);
      const std::vector<std::pair<double, double>> cell = {
          {min_x, min_y}, {max_x, min_y}, {max_x, max_y},
          {min_x, max_y}, {min_x, min_y},
      };
      // Depth proxy: wet fraction in block + water_level bias.
      const double frac =
          static_cast<double>(wet_cells) /
          static_cast<double>((std::max)(1, cell_n));
      const double level_t =
          std::clamp((water_level - 10.0) / 80.0, 0.0, 1.0);
      char heat_buf[32] = {};
      format_heat01(0.25 * level_t + 0.75 * frac, heat_buf, sizeof(heat_buf));
      if (!append_map_polygon(doc, cell, heat_buf)) {
        return false;
      }
      ++wet;
    }
  }
  if (wet == 0) {
    return false;
  }

  (void)add_water_standin;
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

  return paint_flood_mask_layer(doc, ui, mask, width, height, geotransform,
                                water_level, frame_index == 0,
                                /*add_water_standin=*/false);
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
                                 &browser->analysis_playback(), mask, width,
                                 height, geotransform, frame_index, frame_count,
                                 water_level);
      });
}

}  // namespace detail
}  // namespace app
