// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/orthogrid/present/mesh.h"

#include "content/public/gis_document.h"
#include "plugin/product/world3d/commands.h"
#include "plugin/runtime/host/present/gis_present.h"
#include "tool/draft/draft.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace plugin {
namespace {

// StyleDocument interpolate(["get","heat"], ...) wants a decimal |90-theta|.
const char* format_heat_delta(float delta_deg, char* buf, size_t cap) {
  if (!buf || cap < 4) {
    return "";
  }
  float v = delta_deg;
  if (!(v >= 0.f)) {
    v = 0.f;
  }
  if (v > 90.f) {
    v = 90.f;
  }
  const int n = std::snprintf(buf, cap, "%.4f", static_cast<double>(v));
  if (n <= 0 || static_cast<size_t>(n) >= cap) {
    buf[0] = '\0';
    return "";
  }
  return buf;
}

}  // namespace

// Commit mesh lines + cell heat + axis-aligned raster heat underlay.
bool present_orthogrid_mesh(content::GisDocument* doc,
                            const OrthogridMeshCommit& mesh) {
  if (!doc || !mesh.xs || !mesh.ys || mesh.nx < 2 || mesh.ny < 2) {
    return false;
  }
  const int nx = mesh.nx;
  const int ny = mesh.ny;
  const double* xs = mesh.xs;
  const double* ys = mesh.ys;

  doc->remove_layer("orthogrid_extent");
  doc->remove_layer("orthogrid_heat_raster");
  doc->remove_layer("orthogrid_heat_cells");
  doc->remove_layer("orthogrid");
  (void)apply_style_resource(doc, "smartgis.world3d", "orthogrid.style.json");

  if (!doc->create_layer("orthogrid_extent", "Polygon")) {
    return false;
  }
  {
    tool::Draft extent;
    extent.kind = tool::DraftKind::kPolygon;
    extent.points = {{0, 0}, {1000, 0}, {1000, 1000}, {0, 1000}};
    const content::FeatureId extent_id = doc->append_from_draft(
        extent, "draw.polygon",
        [](int vx, int vy, double* map_x, double* map_y) {
          *map_x = static_cast<double>(vx) / 1000.0;
          *map_y = -static_cast<double>(vy) / 1000.0;
        });
    if (extent_id.len != 0) {
      // Wash polygon must not paint layer-id labels on showcase BMPs.
      doc->update_feature_field(content::GisDocument::feature_token(extent_id),
                                "name", "");
    }
  }

  // Raster underlay (axis-aligned samples) for A/C comparison. Dense rasters
  // still abort map2d export_bmp — keep a hard skip over the budget (do not
  // stride into thousands of fill polys). Cell-heat is the primary ramp.
  constexpr int kMaxRasterCells = 32 * 32;
  if (mesh.raster_orth && mesh.raster_w > 1 && mesh.raster_h > 1 &&
      mesh.raster_max_x > mesh.raster_min_x &&
      mesh.raster_max_y > mesh.raster_min_y &&
      mesh.raster_w * mesh.raster_h <= kMaxRasterCells) {
    if (!doc->create_layer("orthogrid_heat_raster", "Polygon")) {
      return false;
    }
    const double dx =
        (mesh.raster_max_x - mesh.raster_min_x) /
        static_cast<double>(mesh.raster_w);
    const double dy =
        (mesh.raster_max_y - mesh.raster_min_y) /
        static_cast<double>(mesh.raster_h);
    for (int j = 0; j < mesh.raster_h; ++j) {
      for (int i = 0; i < mesh.raster_w; ++i) {
        const float d =
            mesh.raster_orth[static_cast<size_t>(j * mesh.raster_w + i)];
        const double x0 = mesh.raster_min_x + static_cast<double>(i) * dx;
        const double y0 = mesh.raster_min_y + static_cast<double>(j) * dy;
        const std::vector<std::pair<double, double>> ring = {
            {x0, y0},
            {x0 + dx, y0},
            {x0 + dx, y0 + dy},
            {x0, y0 + dy},
        };
        char heat_buf[32];
        const char* heat =
            format_heat_delta(d, heat_buf, sizeof(heat_buf));
        if (!append_map_polygon(doc, ring, heat)) {
          return false;
        }
      }
    }
  }

  // Cell-heat polygons: coastal 33x33 (~1024 quads) aborts export at 1:1.
  // Stride down to a small budget so the heat ramp stays visible under the
  // wireframe without blowing map2d frame/export.
  constexpr int kMaxCellHeat = 16 * 16;
  if (mesh.cell_orth && nx > 1 && ny > 1) {
    const int cell_w = nx - 1;
    const int cell_h = ny - 1;
    const int cell_n = cell_w * cell_h;
    const int step =
        (cell_n > kMaxCellHeat)
            ? std::max(1, static_cast<int>(std::ceil(
                              std::sqrt(static_cast<double>(cell_n) /
                                        static_cast<double>(kMaxCellHeat)))))
            : 1;
    if (!doc->create_layer("orthogrid_heat_cells", "Polygon")) {
      return false;
    }
    for (int j = 0; j < cell_h; j += step) {
      for (int i = 0; i < cell_w; i += step) {
        const int i1 = std::min(i + step, cell_w);
        const int j1 = std::min(j + step, cell_h);
        const size_t a = static_cast<size_t>(j * nx + i);
        const size_t b = static_cast<size_t>(j * nx + i1);
        const size_t c = static_cast<size_t>(j1 * nx + i1);
        const size_t d = static_cast<size_t>(j1 * nx + i);
        const float delta =
            mesh.cell_orth[static_cast<size_t>(j * cell_w + i)];
        const std::vector<std::pair<double, double>> ring = {
            {xs[a], ys[a]},
            {xs[b], ys[b]},
            {xs[c], ys[c]},
            {xs[d], ys[d]},
        };
        char heat_buf[32];
        const char* heat =
            format_heat_delta(delta, heat_buf, sizeof(heat_buf));
        if (!append_map_polygon(doc, ring, heat)) {
          return false;
        }
      }
    }
  }

  if (!doc->create_layer("orthogrid", "LineString")) {
    return false;
  }
  auto append_polyline =
      [&](const std::vector<std::pair<double, double>>& xy) {
        if (xy.size() < 2) {
          return false;
        }
        tool::Draft draft;
        draft.kind = tool::DraftKind::kLineString;
        draft.points.reserve(xy.size());
        for (size_t i = 0; i < xy.size(); ++i) {
          draft.points.push_back({static_cast<int32_t>(i), 0});
        }
        const content::FeatureId id = doc->append_from_draft(
            draft, "draw.linestring",
            [&xy](int view_x, int, double* map_x, double* map_y) {
              const size_t i = static_cast<size_t>(view_x);
              if (i >= xy.size() || !map_x || !map_y) {
                return;
              }
              *map_x = xy[i].first;
              *map_y = -xy[i].second;
            });
        if (id.len != 0) {
          doc->update_feature_field(content::GisDocument::feature_token(id),
                                    "type", "highway");
        }
        return id.len != 0;
      };
  for (int j = 0; j < ny; ++j) {
    std::vector<std::pair<double, double>> row;
    row.reserve(static_cast<size_t>(nx));
    for (int i = 0; i < nx; ++i) {
      const size_t at = static_cast<size_t>(j * nx + i);
      row.emplace_back(xs[at], ys[at]);
    }
    if (!append_polyline(row)) {
      return false;
    }
  }
  for (int i = 0; i < nx; ++i) {
    std::vector<std::pair<double, double>> col;
    col.reserve(static_cast<size_t>(ny));
    for (int j = 0; j < ny; ++j) {
      const size_t at = static_cast<size_t>(j * nx + i);
      col.emplace_back(xs[at], ys[at]);
    }
    if (!append_polyline(col)) {
      return false;
    }
  }
  return true;
}

}  // namespace plugin
