// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/plugin/analysis_writer_orthogrid.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/browser_ui_delegate.h"
#include "app/views/shell/browser/plugin/analysis_writer_common.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "gis/present/style/style_document.h"
#include "plugin/product/orthogrid/commands.h"
#include "plugin/product/orthogrid/detail/orthogonality.h"
#include "plugin/product/orthogrid3d/commands.h"
#include "tool/draft/draft.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace app {
namespace detail {
namespace {

constexpr const char* kOrthogridStyleJson = R"json({
  "version": 8,
  "name": "orthogrid_heat",
  "layers": [
    {"id":"heat_raster","type":"fill","source-layer":"orthogrid_heat_raster",
     "paint":{
       "fill-color":["interpolate",["linear"],["get","heat"],
         0,"#1a9850",5,"#91cf60",15,"#fee08b",30,"#fc8d59",45,"#d73027"],
       "fill-opacity":0.42}},
    {"id":"heat_cells","type":"fill","source-layer":"orthogrid_heat_cells",
     "paint":{
       "fill-color":["interpolate",["linear"],["get","heat"],
         0,"#006837",5,"#31a354",15,"#fec44f",30,"#e6550d",45,"#a50f15"],
       "fill-opacity":0.62}},
    {"id":"mesh","type":"line","source-layer":"orthogrid",
     "paint":{"line-color":"#1b1b1b","line-width":1.6}}
  ]
})json";

}  // namespace

// Commit mesh lines + cell heat + axis-aligned raster heat underlay.
bool commit_orthogrid_mesh(content::MapScene* doc,
                           BrowserUiDelegate* ui,
                           const plugin::OrthogridMeshCommit& mesh) {
  if (!doc || !mesh.xs || !mesh.ys || mesh.nx < 2 || mesh.ny < 2) {
    return false;
  }
  const int nx = mesh.nx;
  const int ny = mesh.ny;
  const double* xs = mesh.xs;
  const double* ys = mesh.ys;

  doc->clear();
  auto style = std::make_shared<gis::style::StyleDocument>();
  if (gis::style::parse_style_document(kOrthogridStyleJson, style.get())) {
    doc->set_style_document(std::move(style));
  } else {
    doc->clear_style_document();
  }

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
      doc->update_feature_field(content::MapScene::feature_token(extent_id),
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
            orthogrid::format_heat_delta(d, heat_buf, sizeof(heat_buf));
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
            orthogrid::format_heat_delta(delta, heat_buf, sizeof(heat_buf));
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
          doc->update_feature_field(content::MapScene::feature_token(id),
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
  return refresh_ui_after_layer(ui);
}

// Local hex XYZ → small geo pad so Scene3D lon/lat/elev overlay pipeline works
// (peer mine/stormsurge). Showcase frames this box after create_hex_grid.
constexpr double kHexLabOriginLon = 116.40;
constexpr double kHexLabOriginLat = 39.90;
constexpr double kHexLabDegPerUnit = 0.025;

bool commit_hex_grid_mesh(content::MapScene* doc,
                          BrowserUiDelegate* ui,
                          content::Scene3dPresenter* scene3d,
                          const plugin::HexGridCommit& commit) {
  if (!doc || !commit.grid || commit.grid->is_empty()) {
    return false;
  }
  const geo::HexGrid& grid = *commit.grid;
  const int nx = grid.nx();
  const int ny = grid.ny();
  const int nz = grid.nz();
  if (nx < 2 || ny < 2 || nz < 1) {
    return false;
  }

  doc->clear();
  auto style = std::make_shared<gis::style::StyleDocument>();
  if (gis::style::parse_style_document(kOrthogridStyleJson, style.get())) {
    doc->set_style_document(std::move(style));
  } else {
    doc->clear_style_document();
  }

  // Local AABB of all nodes (map2d extent + Scene3D framing pad).
  double gminx = grid.node(0, 0, 0).x;
  double gmaxx = gminx;
  double gminy = grid.node(0, 0, 0).y;
  double gmaxy = gminy;
  double gminz = grid.node(0, 0, 0).z;
  double gmaxz = gminz;
  for (int k = 0; k < nz; ++k) {
    for (int j = 0; j < ny; ++j) {
      for (int i = 0; i < nx; ++i) {
        const geo::Raw3DPoint p = grid.node(i, j, k);
        gminx = std::min(gminx, p.x);
        gmaxx = std::max(gmaxx, p.x);
        gminy = std::min(gminy, p.y);
        gmaxy = std::max(gmaxy, p.y);
        gminz = std::min(gminz, p.z);
        gmaxz = std::max(gmaxz, p.z);
      }
    }
  }
  if (!(gmaxx > gminx) || !(gmaxy > gminy)) {
    return false;
  }

  if (!doc->create_layer("orthogrid_extent", "Polygon")) {
    return false;
  }
  {
    tool::Draft extent;
    extent.kind = tool::DraftKind::kPolygon;
    extent.points = {{0, 0}, {1000, 0}, {1000, 1000}, {0, 1000}};
    const content::FeatureId extent_id = doc->append_from_draft(
        extent, "draw.polygon",
        [gminx, gmaxx, gminy, gmaxy](int vx, int vy, double* map_x,
                                     double* map_y) {
          const double u = static_cast<double>(vx) / 1000.0;
          const double v = static_cast<double>(vy) / 1000.0;
          *map_x = gminx + u * (gmaxx - gminx);
          *map_y = -(gminy + v * (gmaxy - gminy));
        });
    if (extent_id.len != 0) {
      doc->update_feature_field(content::MapScene::feature_token(extent_id),
                                "name", "");
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
          doc->update_feature_field(content::MapScene::feature_token(id),
                                    "type", "highway");
        }
        return id.len != 0;
      };

  // Top-down shell: bottom + top k-slices only (all k would smear XY), plus
  // vertical pillars so lateral skew between faces stays readable.
  auto append_k_slice = [&](int k) {
    for (int j = 0; j < ny; ++j) {
      std::vector<std::pair<double, double>> row;
      row.reserve(static_cast<size_t>(nx));
      for (int i = 0; i < nx; ++i) {
        const geo::Raw3DPoint p = grid.node(i, j, k);
        row.emplace_back(p.x, p.y);
      }
      if (!append_polyline(row)) {
        return false;
      }
    }
    for (int i = 0; i < nx; ++i) {
      std::vector<std::pair<double, double>> col;
      col.reserve(static_cast<size_t>(ny));
      for (int j = 0; j < ny; ++j) {
        const geo::Raw3DPoint p = grid.node(i, j, k);
        col.emplace_back(p.x, p.y);
      }
      if (!append_polyline(col)) {
        return false;
      }
    }
    return true;
  };
  if (!append_k_slice(0)) {
    return false;
  }
  if (nz > 1) {
    if (!append_k_slice(nz - 1)) {
      return false;
    }
    for (int j = 0; j < ny; ++j) {
      for (int i = 0; i < nx; ++i) {
        const geo::Raw3DPoint a = grid.node(i, j, 0);
        const geo::Raw3DPoint b = grid.node(i, j, nz - 1);
        if (!append_polyline({{a.x, a.y}, {b.x, b.y}})) {
          return false;
        }
      }
    }
  }

  // Scene3D: outer hex shell as TIN + all nodes as point cloud (real Z).
  if (scene3d) {
    const double xy_span =
        (std::max)(gmaxx - gminx, (std::max)(gmaxy - gminy, 1.0e-6));
    const double z_span = (std::max)(gmaxz - gminz, 1.0e-6);
    const double lon0 = kHexLabOriginLon;
    const double lat0 = kHexLabOriginLat;
    const double lon1 = lon0 + xy_span * kHexLabDegPerUnit;
    const double lat1 = lat0 + xy_span * kHexLabDegPerUnit;
    const double span_deg = (std::max)(lon1 - lon0, lat1 - lat0);
    // Match mine DEM-style vertical fit so Z reads in orbit.
    const float vert_exag =
        static_cast<float>((span_deg * 0.45) / z_span);
    auto to_geo = [&](const geo::Raw3DPoint& p, float* lon, float* lat,
                      float* elev) {
      *lon = static_cast<float>(lon0 + (p.x - gminx) * kHexLabDegPerUnit);
      *lat = static_cast<float>(lat0 + (p.y - gminy) * kHexLabDegPerUnit);
      *elev = static_cast<float>((p.z - gminz) * vert_exag);
    };

    const int nnodes = grid.node_count();
    std::vector<float> tin_geo(static_cast<size_t>(nnodes) * 3u);
    for (int k = 0; k < nz; ++k) {
      for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
          const int idx = grid.index_of(i, j, k);
          float lon = 0.f;
          float lat = 0.f;
          float elev = 0.f;
          to_geo(grid.node(i, j, k), &lon, &lat, &elev);
          tin_geo[static_cast<size_t>(idx) * 3u] = lon;
          tin_geo[static_cast<size_t>(idx) * 3u + 1u] = lat;
          tin_geo[static_cast<size_t>(idx) * 3u + 2u] = elev;
        }
      }
    }

    std::vector<unsigned> tin_idx;
    tin_idx.reserve(static_cast<size_t>(nx * ny * 4 + nx * nz * 4 + ny * nz * 4));
    auto push_quad = [&](int a, int b, int c, int d) {
      tin_idx.push_back(static_cast<unsigned>(a));
      tin_idx.push_back(static_cast<unsigned>(b));
      tin_idx.push_back(static_cast<unsigned>(c));
      tin_idx.push_back(static_cast<unsigned>(a));
      tin_idx.push_back(static_cast<unsigned>(c));
      tin_idx.push_back(static_cast<unsigned>(d));
    };
    // k=0 / k=nz-1 faces (i,j).
    for (int j = 0; j < ny - 1; ++j) {
      for (int i = 0; i < nx - 1; ++i) {
        push_quad(grid.index_of(i, j, 0), grid.index_of(i + 1, j, 0),
                  grid.index_of(i + 1, j + 1, 0), grid.index_of(i, j + 1, 0));
        if (nz > 1) {
          push_quad(grid.index_of(i, j, nz - 1),
                    grid.index_of(i + 1, j, nz - 1),
                    grid.index_of(i + 1, j + 1, nz - 1),
                    grid.index_of(i, j + 1, nz - 1));
        }
      }
    }
    // i=0 / i=nx-1 faces (j,k).
    if (nz > 1) {
      for (int k = 0; k < nz - 1; ++k) {
        for (int j = 0; j < ny - 1; ++j) {
          push_quad(grid.index_of(0, j, k), grid.index_of(0, j + 1, k),
                    grid.index_of(0, j + 1, k + 1),
                    grid.index_of(0, j, k + 1));
          push_quad(grid.index_of(nx - 1, j, k),
                    grid.index_of(nx - 1, j + 1, k),
                    grid.index_of(nx - 1, j + 1, k + 1),
                    grid.index_of(nx - 1, j, k + 1));
        }
      }
      // j=0 / j=ny-1 faces (i,k).
      for (int k = 0; k < nz - 1; ++k) {
        for (int i = 0; i < nx - 1; ++i) {
          push_quad(grid.index_of(i, 0, k), grid.index_of(i + 1, 0, k),
                    grid.index_of(i + 1, 0, k + 1),
                    grid.index_of(i, 0, k + 1));
          push_quad(grid.index_of(i, ny - 1, k),
                    grid.index_of(i + 1, ny - 1, k),
                    grid.index_of(i + 1, ny - 1, k + 1),
                    grid.index_of(i, ny - 1, k + 1));
        }
      }
    }

    if (!tin_idx.empty()) {
      // Steel-blue shell; alpha keeps DEM readable if present.
      constexpr uint8_t kHexAlbedo[4] = {0x2c, 0x7a, 0xb8, 0xd0};
      scene3d->set_overlay_tin_mesh(tin_geo.data(), nnodes, tin_idx.data(),
                                    static_cast<int>(tin_idx.size()),
                                    kHexAlbedo);
    } else {
      scene3d->clear_overlay_tin_mesh();
    }

    std::vector<float> cloud_xyz;
    std::vector<uint8_t> cloud_rgba;
    cloud_xyz.reserve(static_cast<size_t>(nnodes) * 3u);
    cloud_rgba.reserve(static_cast<size_t>(nnodes) * 4u);
    const float lift = static_cast<float>(span_deg * 0.02);
    for (int k = 0; k < nz; ++k) {
      for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
          float lon = 0.f;
          float lat = 0.f;
          float elev = 0.f;
          to_geo(grid.node(i, j, k), &lon, &lat, &elev);
          cloud_xyz.push_back(lon);
          cloud_xyz.push_back(lat);
          cloud_xyz.push_back(elev + lift);
          // Amber nodes contrast blue shell (peer mine stick beads).
          cloud_rgba.push_back(0xf1);
          cloud_rgba.push_back(0xc4);
          cloud_rgba.push_back(0x0f);
          cloud_rgba.push_back(255);
        }
      }
    }
    if (!cloud_xyz.empty()) {
      scene3d->set_overlay_pointcloud(
          cloud_xyz.data(), static_cast<int>(cloud_xyz.size() / 3),
          cloud_rgba.data());
    } else {
      scene3d->clear_overlay_pointcloud();
    }
  }

  return refresh_ui_after_layer(ui);
}

void wire_orthogrid_analysis_writers(Browser* browser) {
  if (!browser) {
    return;
  }
  plugin::set_orthogrid_mesh_writer(
      [browser](const plugin::OrthogridMeshCommit& mesh) {
        const int frames =
            mesh.frame_count > 0 ? mesh.frame_count : 1;
        browser->analysis_playback().begin_orthogrid(frames);
        if (mesh.frame_count > 0 && mesh.frame_xs && mesh.frame_ys) {
          for (int i = 0; i < mesh.frame_count; ++i) {
            browser->analysis_playback().push_orthogrid_frame(
                mesh.nx, mesh.ny, mesh.frame_xs[i].data(),
                mesh.frame_ys[i].data());
          }
        } else if (mesh.xs && mesh.ys) {
          browser->analysis_playback().push_orthogrid_frame(mesh.nx, mesh.ny, mesh.xs,
                                                   mesh.ys);
        }
        return commit_orthogrid_mesh(&browser->session().document(), browser->ui(), mesh);
      });

  plugin::set_hex_grid_writer(
      [browser](const plugin::HexGridCommit& mesh) {
        browser->analysis_playback().begin_orthogrid3d(1);
        return commit_hex_grid_mesh(&browser->session().document(), browser->ui(),
                                    &browser->session().scene3d(), mesh);
      });

}

}  // namespace detail
}  // namespace app
