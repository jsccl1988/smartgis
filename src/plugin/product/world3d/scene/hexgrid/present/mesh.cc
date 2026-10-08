// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/hexgrid/present/mesh.h"

#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/public/gis_document.h"
#include "plugin/product/world3d/commands.h"
#include "plugin/product/world3d/scene/hexgrid/lattice/hex_lattice.h"
#include "plugin/runtime/host/present/gis_present.h"
#include "plugin/runtime/host/capability/scene3d_sink.h"
#include "tool/draft/draft.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace plugin {

// Local hex XYZ → small geo pad so Scene3D lon/lat/elev overlay pipeline works
// (peer mine/stormsurge). Showcase frames this box after create_hex_grid.
constexpr double kHexLabOriginLon = 116.40;
constexpr double kHexLabOriginLat = 39.90;
constexpr double kHexLabDegPerUnit = 0.025;

bool present_hex_grid_mesh(content::GisDocument* doc,
                           Scene3dSink* sink,
                           content::Scene3dPresenter* scene3d,
                           const HexGridCommit& commit) {
  if (!doc || !commit.nodes || commit.nodes->IsEmpty()) {
    return false;
  }
  const int nx = commit.nx;
  const int ny = commit.ny;
  const int nz = commit.nz;
  if (nx < 2 || ny < 2 || nz < 1) {
    return false;
  }
  const OGRMultiPoint& nodes = *commit.nodes;
  auto node_at = [&](int i, int j, int k, double* x, double* y, double* z) {
    return detail::hex_point(nodes, nx, ny, nz, i, j, k, x, y, z);
  };
  doc->remove_layer("orthogrid_extent");
  doc->remove_layer("orthogrid");
  (void)apply_style_resource(doc, "smartgis.world3d", "orthogrid.style.json");

  // Local AABB of all nodes (map2d extent + Scene3D framing pad).
  double gminx = 0;
  double gminy = 0;
  double gminz = 0;
  if (!node_at(0, 0, 0, &gminx, &gminy, &gminz)) {
    return false;
  }
  double gmaxx = gminx;
  double gmaxy = gminy;
  double gmaxz = gminz;
  for (int k = 0; k < nz; ++k) {
    for (int j = 0; j < ny; ++j) {
      for (int i = 0; i < nx; ++i) {
        double x = 0;
        double y = 0;
        double z = 0;
        if (!node_at(i, j, k, &x, &y, &z)) {
          return false;
        }
        gminx = std::min(gminx, x);
        gmaxx = std::max(gmaxx, x);
        gminy = std::min(gminy, y);
        gmaxy = std::max(gmaxy, y);
        gminz = std::min(gminz, z);
        gmaxz = std::max(gmaxz, z);
      }
    }
  }
  if (!(gmaxx > gminx) || !(gmaxy > gminy)) {
    return false;
  }

  // Local engineering XYZ is not a GIS envelope. Writing 0..1.5 into GisScene
  // made document.world_extent() fail extent_looks_like_china, so
  // push_shared_extent / Scene3dGpuPresent::world_extent fell back to the
  // full China box. FlyCube then rebuilt china_dem as a country "globe" and
  // the amber overlay TIN (lon/lat hex lab) collapsed to a speck.
  const double xy_span =
      (std::max)(gmaxx - gminx, (std::max)(gmaxy - gminy, 1.0e-6));
  const double z_span = (std::max)(gmaxz - gminz, 1.0e-6);
  const double lon0 = kHexLabOriginLon;
  const double lat0 = kHexLabOriginLat;
  // Normalize any local CRS (unit cube or quarry meters) into the hex lab pad.
  const double lab_deg = 1.60 * kHexLabDegPerUnit;
  const double lon_span = lab_deg;
  const double lat_span = ((gmaxy - gminy) / xy_span) * lab_deg;
  auto local_xy_to_lonlat = [&](double x, double y, double* lon, double* lat) {
    *lon = lon0 + (x - gminx) / xy_span * lab_deg;
    *lat = lat0 + (y - gminy) / xy_span * lab_deg;
  };

  if (!doc->create_layer("orthogrid_extent", "Polygon")) {
    return false;
  }
  {
    tool::Draft extent;
    extent.kind = tool::DraftKind::kPolygon;
    extent.points = {{0, 0}, {1000, 0}, {1000, 1000}, {0, 1000}};
    const content::FeatureId extent_id = doc->append_from_draft(
        extent, "draw.polygon",
        [lon0, lat0, lon_span, lat_span](int vx, int vy, double* map_x,
                                         double* map_y) {
          const double u = static_cast<double>(vx) / 1000.0;
          const double v = static_cast<double>(vy) / 1000.0;
          *map_x = lon0 + u * lon_span;
          *map_y = lat0 + v * lat_span;
        });
    if (extent_id.len != 0) {
      doc->update_feature_field(content::GisDocument::feature_token(extent_id),
                                "name", "");
    }
  }

  if (!doc->create_layer("orthogrid", "LineString")) {
    return false;
  }
  auto append_polyline =
      [&](const std::vector<std::pair<double, double>>& lonlat) {
        if (lonlat.size() < 2) {
          return false;
        }
        tool::Draft draft;
        draft.kind = tool::DraftKind::kLineString;
        draft.points.reserve(lonlat.size());
        for (size_t i = 0; i < lonlat.size(); ++i) {
          draft.points.push_back({static_cast<int32_t>(i), 0});
        }
        const content::FeatureId id = doc->append_from_draft(
            draft, "draw.linestring",
            [&lonlat](int view_x, int, double* map_x, double* map_y) {
              const size_t i = static_cast<size_t>(view_x);
              if (i >= lonlat.size() || !map_x || !map_y) {
                return;
              }
              *map_x = lonlat[i].first;
              *map_y = lonlat[i].second;
            });
        if (id.len != 0) {
          doc->update_feature_field(content::GisDocument::feature_token(id),
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
        double x = 0;
        double y = 0;
        double z = 0;
        if (!node_at(i, j, k, &x, &y, &z)) {
          return false;
        }
        double lon = 0;
        double lat = 0;
        local_xy_to_lonlat(x, y, &lon, &lat);
        row.emplace_back(lon, lat);
      }
      if (!append_polyline(row)) {
        return false;
      }
    }
    for (int i = 0; i < nx; ++i) {
      std::vector<std::pair<double, double>> col;
      col.reserve(static_cast<size_t>(ny));
      for (int j = 0; j < ny; ++j) {
        double x = 0;
        double y = 0;
        double z = 0;
        if (!node_at(i, j, k, &x, &y, &z)) {
          return false;
        }
        double lon = 0;
        double lat = 0;
        local_xy_to_lonlat(x, y, &lon, &lat);
        col.emplace_back(lon, lat);
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
        double ax = 0;
        double ay = 0;
        double az = 0;
        double bx = 0;
        double by = 0;
        double bz = 0;
        if (!node_at(i, j, 0, &ax, &ay, &az) ||
            !node_at(i, j, nz - 1, &bx, &by, &bz)) {
          return false;
        }
        double alon = 0;
        double alat = 0;
        double blon = 0;
        double blat = 0;
        local_xy_to_lonlat(ax, ay, &alon, &alat);
        local_xy_to_lonlat(bx, by, &blon, &blat);
        if (!append_polyline({{alon, alat}, {blon, blat}})) {
          return false;
        }
      }
    }
  }

  // Scene3D: closed orthogonal hex volume (outer cell faces + dark grid
  // ribbons). FlyCube has no line PSO — grid ink is textured dark quads.
  if (scene3d || sink) {
    // Keep quarry Z (meters). attach_tin fits hex albedo into the DEM orbit
    // box; inflating elev here only stretched Y into a rainbow needle when
    // geo_frame was China-scale.
    auto local_z_to_elev = [&](double z) -> double { return z; };
    (void)z_span;
    const double cell =
        xy_span / static_cast<double>((std::max)((std::max)(nx, ny), 2) - 1);
    // FE edge ribbons (FlyCube has no line PSO). Too thin vanishes after
    // studio fit; too fat (0.07*cell) reads as solid black bars.
    const double half_w = cell * 0.028;
    std::vector<float> tin_geo;
    std::vector<float> tin_uv;
    std::vector<unsigned> tin_idx;
    tin_geo.reserve(static_cast<size_t>(nx * ny * 24u));
    tin_uv.reserve(static_cast<size_t>(nx * ny * 16u));
    tin_idx.reserve(static_cast<size_t>(nx * ny * 24u));

    constexpr int kAtlas = 16;
    constexpr int kDarkTexel = 15;
    auto uv_of = [](int texel) -> std::pair<float, float> {
      const float u = (static_cast<float>(texel) + 0.5f) /
                      static_cast<float>(kAtlas);
      const float v = 0.5f / static_cast<float>(kAtlas);
      return {u, v};
    };
    auto zone_of = [&](int ci, int cj, int ck) -> int {
      const float u = static_cast<float>(ci) /
                      static_cast<float>((std::max)(nx - 2, 1));
      const float v = static_cast<float>(cj) /
                      static_cast<float>((std::max)(ny - 2, 1));
      const float w = static_cast<float>(ck) /
                      static_cast<float>((std::max)(nz - 2, 1));
      // Top cap: lime / yellow / pink / magenta / purple (reference block).
      if (nz > 1 && ck >= nz - 2) {
        if (v > 0.55f) {
          return (u < 0.45f) ? 0 : 1;
        }
        if (u > 0.50f) {
          return (v < 0.38f) ? 3 : 2;
        }
        if (u > 0.22f && v < 0.52f) {
          return 4;
        }
        return 7;
      }
      // Bottom cap: blue / green / orange patches.
      if (ck <= 0) {
        if (v < 0.38f) {
          return 5;
        }
        if (u > 0.52f) {
          return 6;
        }
        return 7;
      }
      // Side walls: vary along strike and depth.
      if (u < 0.32f) {
        return (w > 0.50f) ? 0 : 1;
      }
      if (u > 0.68f) {
        return (w > 0.50f) ? 2 : 3;
      }
      if (v > 0.62f) {
        return 4;
      }
      if (v < 0.32f) {
        return 5;
      }
      return (w > 0.50f) ? 2 : 6;
    };

    auto emit_pt = [&](double x, double y, double z, float u, float v) -> unsigned {
      double glon = 0;
      double glat = 0;
      local_xy_to_lonlat(x, y, &glon, &glat);
      const unsigned id = static_cast<unsigned>(tin_geo.size() / 3u);
      tin_geo.push_back(static_cast<float>(glon));
      tin_geo.push_back(static_cast<float>(glat));
      tin_geo.push_back(static_cast<float>(local_z_to_elev(z)));
      tin_uv.push_back(u);
      tin_uv.push_back(v);
      return id;
    };
    auto emit_quad = [&](double ax, double ay, double az, double bx, double by,
                         double bz, double cx, double cy, double cz, double dx,
                         double dy, double dz, int texel) {
      const auto uv = uv_of(texel);
      const unsigned a = emit_pt(ax, ay, az, uv.first, uv.second);
      const unsigned b = emit_pt(bx, by, bz, uv.first, uv.second);
      const unsigned c = emit_pt(cx, cy, cz, uv.first, uv.second);
      const unsigned d = emit_pt(dx, dy, dz, uv.first, uv.second);
      tin_idx.push_back(a);
      tin_idx.push_back(b);
      tin_idx.push_back(c);
      tin_idx.push_back(a);
      tin_idx.push_back(c);
      tin_idx.push_back(d);
    };
    auto ribbon = [&](double ax, double ay, double az, double bx, double by,
                      double bz) {
      const double dx = bx - ax;
      const double dy = by - ay;
      const double len = std::hypot(dx, dy);
      double px = half_w;
      double py = 0.0;
      if (len > 1.0e-9) {
        px = -dy / len * half_w;
        py = dx / len * half_w;
      }
      // Lift off the cell face so coplanar z-fight does not fatten the ink.
      const double mx = 0.5 * (ax + bx);
      const double my = 0.5 * (ay + by);
      const double mz = 0.5 * (az + bz);
      double ox = mx - 0.5 * (gminx + gmaxx);
      double oy = my - 0.5 * (gminy + gmaxy);
      double oz = mz - 0.5 * (gminz + gmaxz);
      const double on = std::sqrt(ox * ox + oy * oy + oz * oz);
      const double lift = cell * 0.003;
      if (on > 1.0e-12) {
        ox = ox / on * lift;
        oy = oy / on * lift;
        oz = oz / on * lift;
      } else {
        ox = 0.0;
        oy = 0.0;
        oz = lift;
      }
      emit_quad(ax + px + ox, ay + py + oy, az + oz, ax - px + ox, ay - py + oy,
                az + oz, bx - px + ox, by - py + oy, bz + oz, bx + px + ox,
                by + py + oy, bz + oz, kDarkTexel);
    };

    auto node3 = [&](int i, int j, int k, double* x, double* y, double* z) {
      return node_at(i, j, k, x, y, z);
    };

    // Outer cell faces (closed hex volume).
    for (int j = 0; j < ny - 1; ++j) {
      for (int i = 0; i < nx - 1; ++i) {
        double p[4][3] = {};
        if (!node3(i, j, 0, &p[0][0], &p[0][1], &p[0][2]) ||
            !node3(i + 1, j, 0, &p[1][0], &p[1][1], &p[1][2]) ||
            !node3(i + 1, j + 1, 0, &p[2][0], &p[2][1], &p[2][2]) ||
            !node3(i, j + 1, 0, &p[3][0], &p[3][1], &p[3][2])) {
          return false;
        }
        emit_quad(p[0][0], p[0][1], p[0][2], p[3][0], p[3][1], p[3][2],
                  p[2][0], p[2][1], p[2][2], p[1][0], p[1][1], p[1][2],
                  zone_of(i, j, 0));
        if (nz > 1) {
          if (!node3(i, j, nz - 1, &p[0][0], &p[0][1], &p[0][2]) ||
              !node3(i + 1, j, nz - 1, &p[1][0], &p[1][1], &p[1][2]) ||
              !node3(i + 1, j + 1, nz - 1, &p[2][0], &p[2][1], &p[2][2]) ||
              !node3(i, j + 1, nz - 1, &p[3][0], &p[3][1], &p[3][2])) {
            return false;
          }
          emit_quad(p[0][0], p[0][1], p[0][2], p[1][0], p[1][1], p[1][2],
                    p[2][0], p[2][1], p[2][2], p[3][0], p[3][1], p[3][2],
                    zone_of(i, j, nz - 2));
        }
      }
    }
    if (nz > 1) {
      for (int k = 0; k < nz - 1; ++k) {
        for (int j = 0; j < ny - 1; ++j) {
          double p[4][3] = {};
          if (!node3(0, j, k, &p[0][0], &p[0][1], &p[0][2]) ||
              !node3(0, j + 1, k, &p[1][0], &p[1][1], &p[1][2]) ||
              !node3(0, j + 1, k + 1, &p[2][0], &p[2][1], &p[2][2]) ||
              !node3(0, j, k + 1, &p[3][0], &p[3][1], &p[3][2])) {
            return false;
          }
          emit_quad(p[0][0], p[0][1], p[0][2], p[1][0], p[1][1], p[1][2],
                    p[2][0], p[2][1], p[2][2], p[3][0], p[3][1], p[3][2],
                    zone_of(0, j, k));
          if (!node3(nx - 1, j, k, &p[0][0], &p[0][1], &p[0][2]) ||
              !node3(nx - 1, j, k + 1, &p[1][0], &p[1][1], &p[1][2]) ||
              !node3(nx - 1, j + 1, k + 1, &p[2][0], &p[2][1], &p[2][2]) ||
              !node3(nx - 1, j + 1, k, &p[3][0], &p[3][1], &p[3][2])) {
            return false;
          }
          emit_quad(p[0][0], p[0][1], p[0][2], p[1][0], p[1][1], p[1][2],
                    p[2][0], p[2][1], p[2][2], p[3][0], p[3][1], p[3][2],
                    zone_of(nx - 2, j, k));
        }
        for (int i = 0; i < nx - 1; ++i) {
          double p[4][3] = {};
          if (!node3(i, 0, k, &p[0][0], &p[0][1], &p[0][2]) ||
              !node3(i, 0, k + 1, &p[1][0], &p[1][1], &p[1][2]) ||
              !node3(i + 1, 0, k + 1, &p[2][0], &p[2][1], &p[2][2]) ||
              !node3(i + 1, 0, k, &p[3][0], &p[3][1], &p[3][2])) {
            return false;
          }
          emit_quad(p[0][0], p[0][1], p[0][2], p[1][0], p[1][1], p[1][2],
                    p[2][0], p[2][1], p[2][2], p[3][0], p[3][1], p[3][2],
                    zone_of(i, 0, k));
          if (!node3(i, ny - 1, k, &p[0][0], &p[0][1], &p[0][2]) ||
              !node3(i + 1, ny - 1, k, &p[1][0], &p[1][1], &p[1][2]) ||
              !node3(i + 1, ny - 1, k + 1, &p[2][0], &p[2][1], &p[2][2]) ||
              !node3(i, ny - 1, k + 1, &p[3][0], &p[3][1], &p[3][2])) {
            return false;
          }
          emit_quad(p[0][0], p[0][1], p[0][2], p[1][0], p[1][1], p[1][2],
                    p[2][0], p[2][1], p[2][2], p[3][0], p[3][1], p[3][2],
                    zone_of(i, ny - 2, k));
        }
      }
    }

    // Orthogonal grid lines on every outer i/j/k edge.
    for (int k : {0, nz > 1 ? nz - 1 : 0}) {
      for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx - 1; ++i) {
          double ax, ay, az, bx, by, bz;
          if (!node3(i, j, k, &ax, &ay, &az) ||
              !node3(i + 1, j, k, &bx, &by, &bz)) {
            return false;
          }
          ribbon(ax, ay, az, bx, by, bz);
        }
      }
      for (int i = 0; i < nx; ++i) {
        for (int j = 0; j < ny - 1; ++j) {
          double ax, ay, az, bx, by, bz;
          if (!node3(i, j, k, &ax, &ay, &az) ||
              !node3(i, j + 1, k, &bx, &by, &bz)) {
            return false;
          }
          ribbon(ax, ay, az, bx, by, bz);
        }
      }
    }
    if (nz > 1) {
      for (int i : {0, nx - 1}) {
        for (int j = 0; j < ny; ++j) {
          for (int k = 0; k < nz - 1; ++k) {
            double ax, ay, az, bx, by, bz;
            if (!node3(i, j, k, &ax, &ay, &az) ||
                !node3(i, j, k + 1, &bx, &by, &bz)) {
              return false;
            }
            ribbon(ax, ay, az, bx, by, bz);
          }
        }
      }
      for (int j : {0, ny - 1}) {
        for (int i = 1; i < nx - 1; ++i) {
          for (int k = 0; k < nz - 1; ++k) {
            double ax, ay, az, bx, by, bz;
            if (!node3(i, j, k, &ax, &ay, &az) ||
                !node3(i, j, k + 1, &bx, &by, &bz)) {
              return false;
            }
            ribbon(ax, ay, az, bx, by, bz);
          }
        }
      }
    }

    if (!tin_idx.empty()) {
      std::vector<uint8_t> atlas(static_cast<size_t>(kAtlas) * kAtlas * 4u, 255);
      const uint8_t zones[8][3] = {
          {0xb4, 0xe6, 0x3c}, {0xe6, 0xdc, 0x3c}, {0xe6, 0x6e, 0x96},
          {0xd2, 0x28, 0x8c}, {0xa0, 0x46, 0xb4}, {0x3c, 0x6e, 0xd2},
          {0xd2, 0x78, 0x32}, {0x50, 0xaa, 0x46},
      };
      for (int z = 0; z < 8; ++z) {
        for (int row = 0; row < kAtlas; ++row) {
          const size_t p = (static_cast<size_t>(row) * kAtlas +
                            static_cast<size_t>(z)) *
                           4u;
          atlas[p] = zones[z][0];
          atlas[p + 1] = zones[z][1];
          atlas[p + 2] = zones[z][2];
          atlas[p + 3] = 0xff;
        }
      }
      for (int row = 0; row < kAtlas; ++row) {
        const size_t p = (static_cast<size_t>(row) * kAtlas +
                          static_cast<size_t>(kDarkTexel)) *
                         4u;
        atlas[p] = 0x52;
        atlas[p + 1] = 0x54;
        atlas[p + 2] = 0x5a;
        atlas[p + 3] = 0xff;
      }
      // Amber shell so Scene3dHdcPainter hex_like / score stick_or_stratum
      // recognize the volume (green albedo was misclassified as DEM pad).
      constexpr uint8_t kHexAlbedo[4] = {0xe0, 0xa0, 0x40, 0xf0};
      if (scene3d) {
        scene3d->set_overlay_tin_mesh(
            tin_geo.data(), static_cast<int>(tin_geo.size() / 3u), tin_idx.data(),
            static_cast<int>(tin_idx.size()), kHexAlbedo);
        scene3d->set_overlay_tin_drape(
            atlas.data(), static_cast<uint32_t>(kAtlas),
            static_cast<uint32_t>(kAtlas), tin_uv.data(),
            static_cast<int>(tin_uv.size()));
      } else if (sink) {
        sink->set_overlay_tin_mesh(
            tin_geo.data(), static_cast<int>(tin_geo.size() / 3u), tin_idx.data(),
            static_cast<int>(tin_idx.size()), kHexAlbedo);
        sink->set_overlay_tin_drape(
            atlas.data(), static_cast<uint32_t>(kAtlas),
            static_cast<uint32_t>(kAtlas), tin_uv.data(),
            static_cast<int>(tin_uv.size()));
      }
    } else {
      if (scene3d) {
        scene3d->clear_overlay_tin_mesh();
      } else if (sink) {
        sink->clear_overlay_tin_mesh();
      }
    }
    if (scene3d) {
      scene3d->clear_overlay_pointcloud();
    }
  }
  if (sink) {
    sink->invalidate();
  }

  return true;
}

}  // namespace plugin
