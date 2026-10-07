// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/stormsurge/present/mask.h"

#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/public/gis_document.h"
#include "plugin/runtime/host/present/gis_present.h"
#include "plugin/runtime/host/capability/scene3d_sink.h"

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

bool paint_surge_mask_cells(content::GisDocument* doc,
                            const unsigned char* mask,
                            int width,
                            int height,
                            const double* geotransform,
                            double water_level) {
  if (!doc || !mask || !geotransform || width <= 0 || height <= 0) {
    return false;
  }
  const double gt0 = geotransform[0];
  const double gt1 = geotransform[1];
  const double gt2 = geotransform[2];
  const double gt3 = geotransform[3];
  const double gt4 = geotransform[4];
  const double gt5 = geotransform[5];

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
  return wet > 0;
}

}  // namespace

bool present_stormsurge_mask(content::GisDocument* doc,
                             Scene3dSink* sink,
                             content::Scene3dPresenter* scene3d,
                             const unsigned char* mask,
                             int width,
                             int height,
                             const double* geotransform,
                             bool begin_session,
                             double water_level) {
  if (!doc || !mask || !geotransform || width <= 0 || height <= 0) {
    return false;
  }
  if (begin_session) {
    (void)apply_style_resource(doc, "smartgis.stormsurge",
                               "stormsurge.style.json");
    // Prefer sink clear when scene3d is null (native pack → shell bridges).
    if (scene3d) {
      scene3d->clear_overlay_tin_mesh();
    } else if (sink) {
      sink->clear_overlay_tin_mesh();
    }
    if (sink) {
      sink->invalidate();
    }
  }
  return paint_surge_mask_cells(doc, mask, width, height, geotransform,
                                water_level);
}

}  // namespace plugin
