// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/flood/present/present.h"

#include "content/public/gis_document.h"
#include "plugin/runtime/host/present/gis_present.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

namespace plugin {
namespace {

void format_heat01(double t01, char* buf, size_t cap) {
  if (!buf || cap < 4) {
    return;
  }
  const double v = std::clamp(t01, 0.0, 1.0) * 100.0;
  std::snprintf(buf, cap, "%.1f", v);
}

}  // namespace

bool present_flood_style(content::GisDocument* doc) {
  return apply_style_resource(doc, "smartgis.flood", "flood.style.json");
}

bool present_flood_mask(content::GisDocument* doc,
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
    // Full DEM pad as dry land (cream heat) so inundation contrast is never
    // blank wash (wet-majority tiles used to skip every terrain polygon).
    {
      const double px0 = gt0;
      const double py0 = gt3;
      const double px1 = gt0 + gt1 * width + gt2 * height;
      const double py1 = gt3 + gt4 * width + gt5 * height;
      const double min_x = std::min(px0, px1);
      const double max_x = std::max(px0, px1);
      const double min_y = std::min(py0, py1);
      const double max_y = std::max(py0, py1);
      const std::vector<std::pair<double, double>> pad = {
          {min_x, min_y}, {max_x, min_y}, {max_x, max_y},
          {min_x, max_y}, {min_x, min_y},
      };
      if (!append_map_polygon(doc, pad, "40.0")) {
        return false;
      }
    }
    constexpr int kTerrainTiles = 12;
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
        // Keep dry-majority tiles only (pad already covers the wet core).
        if (cells > 0 && dry * 2 < cells) {
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
        const double row_t = static_cast<double>(tr) /
                             static_cast<double>((std::max)(1, height - 1));
        const double col_t = static_cast<double>(tc) /
                             static_cast<double>((std::max)(1, width - 1));
        const double heat_t = 0.18 + 0.70 * row_t + 0.12 * col_t;
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

  // Finer wet mosaic so inundation reads as a basin, not one toy blob.
  constexpr int kMaxDim = 96;
  const int step_x = std::max(1, (width + kMaxDim - 1) / kMaxDim);
  const int step_y = std::max(1, (height + kMaxDim - 1) / kMaxDim);

  bool any_wet = false;
  for (int r = 0; r < height && !any_wet; ++r) {
    for (int c = 0; c < width; ++c) {
      const size_t idx = static_cast<size_t>(r) * static_cast<size_t>(width) +
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
      const double frac = static_cast<double>(wet_cells) /
                          static_cast<double>((std::max)(1, cell_n));
      // Keep wet-majority cells only so the shoreline is readable.
      if (wet_cells == 0 || frac < 0.35) {
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
      const double level_t = std::clamp((water_level - 10.0) / 80.0, 0.0, 1.0);
      char heat_buf[32] = {};
      format_heat01(0.20 * level_t + 0.80 * frac, heat_buf, sizeof(heat_buf));
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
  return true;
}

}  // namespace plugin
