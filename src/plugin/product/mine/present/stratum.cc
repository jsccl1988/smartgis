// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/mine/present/stratum.h"

#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/public/gis_document.h"
#include "gis/analysis/geology/borehole.h"
#include "gis/analysis/geology/stratum_tin.h"
#include "plugin/runtime/host/present/gis_present.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace plugin {
namespace {

struct Rgb {
  uint8_t r = 128;
  uint8_t g = 128;
  uint8_t b = 128;
};

struct OverlayMesh {
  std::vector<float> xyz;
  std::vector<unsigned> idx;
  std::vector<float> uv;
};

Rgb lithology_rgb(const std::string& id) {
  if (id == "soil" || id == "clay") {
    return {214, 176, 96};
  }
  if (id == "snow") {
    return {244, 246, 250};
  }
  if (id == "water") {
    return {56, 138, 176};
  }
  if (id == "weathered") {
    return {72, 148, 92};
  }
  if (id == "silt") {
    return {42, 92, 148};
  }
  if (id == "marine") {
    return {28, 58, 110};
  }
  if (id == "chalk") {
    return {232, 226, 210};
  }
  if (id == "sand") {
    return {48, 138, 142};
  }
  if (id == "oxidized") {
    return {214, 168, 120};
  }
  if (id == "bedrock") {
    return {168, 88, 92};
  }
  if (id == "basement") {
    return {118, 62, 70};
  }
  uint32_t h = 2166136261u;
  for (unsigned char c : id) {
    h ^= c;
    h *= 16777619u;
  }
  return {static_cast<uint8_t>(90 + (h & 127)),
          static_cast<uint8_t>(70 + ((h >> 8) & 90)),
          static_cast<uint8_t>(50 + ((h >> 16) & 70))};
}

int lithology_pattern(const std::string& id) {
  if (id == "soil" || id == "clay" || id == "oxidized") {
    return 0;
  }
  if (id == "weathered" || id == "silt" || id == "marine") {
    return 1;
  }
  if (id == "sand" || id == "water") {
    return 2;
  }
  if (id == "snow" || id == "chalk") {
    return 4;
  }
  return 3;
}

void bake_stratum_atlas(const std::vector<std::string>& layers,
                        std::vector<uint8_t>* rgba, int w, int h) {
  rgba->assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u, 255);
  const int n = (std::max)(1, static_cast<int>(layers.size()));
  for (int li = 0; li < n; ++li) {
    const Rgb c = lithology_rgb(layers[static_cast<size_t>(li)]);
    const int pattern = lithology_pattern(layers[static_cast<size_t>(li)]);
    const int y0 = li * h / n;
    const int y1 = (li + 1) * h / n;
    for (int y = y0; y < y1; ++y) {
      for (int x = 0; x < w; ++x) {
        int rr = c.r;
        int gg = c.g;
        int bb = c.b;
        const int rel = y - y0;
        if (pattern == 0) {
          // Clay: thick bedding.
          if ((rel % 6) == 0) {
            rr = (std::max)(40, rr - 36);
            gg = (std::max)(24, gg - 28);
            bb = (std::max)(16, bb - 20);
          } else if ((x % 11) == 0) {
            rr = (std::min)(255, rr + 18);
          }
        } else if (pattern == 1) {
          // Silt: finer laminations.
          if ((rel % 3) == 0) {
            rr = (std::max)(40, rr - 22);
            gg = (std::max)(40, gg - 18);
          }
        } else if (pattern == 2) {
          // Sand / water: speckled grains or foam.
          const uint32_t grain =
              static_cast<uint32_t>(x * 73856093u) ^
              static_cast<uint32_t>(y * 19349663u);
          if ((grain % 5u) == 0u) {
            rr = (std::min)(255, rr + 28);
            gg = (std::min)(255, gg + 22);
            bb = (std::min)(255, bb + 8);
          } else if ((grain % 7u) == 0u) {
            rr = (std::max)(40, rr - 24);
            gg = (std::max)(40, gg - 20);
          }
        } else if (pattern == 4) {
          // Snow / chalk: soft highlight grain.
          if ((rel % 5) == 0) {
            rr = (std::max)(180, rr - 12);
            gg = (std::max)(180, gg - 10);
            bb = (std::max)(170, bb - 14);
          } else if ((x % 13) == 0) {
            rr = (std::min)(255, rr + 8);
            gg = (std::min)(255, gg + 8);
            bb = (std::min)(255, bb + 10);
          }
        } else {
          // Bedrock: diagonal joints.
          if (((x + y) % 8) == 0 || ((x - y + 64) % 11) == 0) {
            rr = (std::max)(28, rr - 32);
            gg = (std::max)(28, gg - 32);
            bb = (std::max)(28, bb - 28);
          }
        }
        const size_t p =
            (static_cast<size_t>(y) * static_cast<size_t>(w) +
             static_cast<size_t>(x)) *
            4u;
        (*rgba)[p] = static_cast<uint8_t>(rr);
        (*rgba)[p + 1] = static_cast<uint8_t>(gg);
        (*rgba)[p + 2] = static_cast<uint8_t>(bb);
        (*rgba)[p + 3] = 255;
      }
    }
  }
}

void band_uv(int layer, int n_layers, float su, float sv, float* u, float* v) {
  const float n = static_cast<float>((std::max)(1, n_layers));
  const float v0 = static_cast<float>(layer) / n;
  const float vh = 0.92f / n;
  float fu = su - std::floor(su);
  float fv = sv - std::floor(sv);
  if (fu < 0.f) {
    fu += 1.f;
  }
  if (fv < 0.f) {
    fv += 1.f;
  }
  *u = fu;
  *v = v0 + 0.04f / n + fv * vh;
}

void append_vert(OverlayMesh* mesh, float lon, float lat, float elev, float u,
                 float v) {
  mesh->xyz.push_back(lon);
  mesh->xyz.push_back(lat);
  mesh->xyz.push_back(elev);
  mesh->uv.push_back(u);
  mesh->uv.push_back(v);
}

void append_tri(OverlayMesh* mesh, unsigned a, unsigned b, unsigned c) {
  mesh->idx.push_back(a);
  mesh->idx.push_back(b);
  mesh->idx.push_back(c);
}

// Four unique verts so each wall/cyl face can hold its own UV (no share).
void append_quad(OverlayMesh* mesh, float lon0, float lat0, float z0, float u0,
                 float v0, float lon1, float lat1, float z1, float u1, float v1,
                 float lon2, float lat2, float z2, float u2, float v2,
                 float lon3, float lat3, float z3, float u3, float v3) {
  const unsigned base = static_cast<unsigned>(mesh->xyz.size() / 3);
  append_vert(mesh, lon0, lat0, z0, u0, v0);
  append_vert(mesh, lon1, lat1, z1, u1, v1);
  append_vert(mesh, lon2, lat2, z2, u2, v2);
  append_vert(mesh, lon3, lat3, z3, u3, v3);
  append_tri(mesh, base, base + 1, base + 2);
  append_tri(mesh, base, base + 2, base + 3);
}

bool build_mine_volume_mesh(const gis::detail::BoreholeSet& holes,
                            OverlayMesh* mesh,
                            std::vector<uint8_t>* atlas, int tex_w, int tex_h) {
  struct HoleCol {
    std::string id;
    double lon = 0;
    double lat = 0;
    std::map<std::string, double> z;
  };
  std::map<std::string, HoleCol> by_id;
  for (const auto& c : holes.contacts) {
    HoleCol& h = by_id[c.hole_id];
    if (h.id.empty()) {
      h.id = c.hole_id;
      h.lon = c.x;
      h.lat = c.y;
    }
    h.z[c.stratum_id] = c.z;
  }
  if (by_id.size() < 3) {
    return false;
  }
  std::vector<HoleCol> cols;
  cols.reserve(by_id.size());
  for (auto& [id, col] : by_id) {
    (void)id;
    cols.push_back(std::move(col));
  }
  const int n_holes = static_cast<int>(cols.size());

  double lon_min = cols[0].lon;
  double lat_min = cols[0].lat;
  double lon_max = lon_min;
  double lat_max = lat_min;
  for (int i = 0; i < n_holes; ++i) {
    lon_min = (std::min)(lon_min, cols[static_cast<size_t>(i)].lon);
    lat_min = (std::min)(lat_min, cols[static_cast<size_t>(i)].lat);
    lon_max = (std::max)(lon_max, cols[static_cast<size_t>(i)].lon);
    lat_max = (std::max)(lat_max, cols[static_cast<size_t>(i)].lat);
  }
  const double cx = 0.5 * (lon_min + lon_max);
  const double cy = 0.5 * (lat_min + lat_max);
  const double pad = (std::max)(lon_max - lon_min, lat_max - lat_min) * 0.12;
  const double span =
      (std::max)(lon_max - lon_min, lat_max - lat_min) + pad * 2.0;
  const double lon0 = cx - span * 0.5;
  const double lat0 = cy - span * 0.5;
  const double lon1 = cx + span * 0.5;
  const double lat1 = cy + span * 0.5;
  const float lon_span = static_cast<float>((std::max)(lon1 - lon0, 1.0e-5));
  const float lat_span = static_cast<float>((std::max)(lat1 - lat0, 1.0e-5));

  auto idw_z = [&](double lon, double lat, const char* sid, double fallback) {
    double sw = 0.0;
    double sz = 0.0;
    int hit = 0;
    for (int i = 0; i < n_holes; ++i) {
      const HoleCol& col = cols[static_cast<size_t>(i)];
      const auto it = col.z.find(sid);
      if (it == col.z.end()) {
        continue;
      }
      const double dx = col.lon - lon;
      const double dy = col.lat - lat;
      const double w = 1.0 / (dx * dx + dy * dy + 1.0e-12);
      sw += w;
      sz += w * it->second;
      ++hit;
    }
    if (hit == 0 || sw <= 0.0) {
      return fallback;
    }
    return sz / sw;
  };

  constexpr int kGrid = 36;
  const int n_cell = kGrid - 1;
  auto hill = [](float u, float v) {
    return 0.58f * std::sin(u * 6.3f) * std::sin(v * 5.2f + 0.35f) +
           0.28f * std::sin(u * 11.4f + 1.1f) * std::sin(v * 9.6f) +
           0.18f * std::sin((u + v) * 8.1f + 0.7f);
  };

  std::vector<std::string> layers = {
      "soil",  "weathered", "silt",     "marine", "chalk",
      "sand",  "oxidized",  "bedrock",  "basement", "snow", "water"};
  constexpr int kWallLayers = 9;
  constexpr int kSnowLayer = 9;
  constexpr int kWaterLayer = 10;
  const int n_atlas = static_cast<int>(layers.size());
  bake_stratum_atlas(layers, atlas, tex_w, tex_h);

  constexpr float kLift = 40.f;
  constexpr float kHillM = 22.f;
  const int n_surf = kWallLayers + 1;
  std::vector<std::vector<float>> z_grid(
      static_cast<size_t>(n_surf),
      std::vector<float>(static_cast<size_t>(kGrid) * static_cast<size_t>(kGrid),
                         0.f));
  std::vector<uint8_t> top_kind(
      static_cast<size_t>(kGrid) * static_cast<size_t>(kGrid), 0);
  auto at = [&](int i, int j) {
    return static_cast<size_t>(j) * static_cast<size_t>(kGrid) +
           static_cast<size_t>(i);
  };
  auto lon_at = [&](int i) {
    return static_cast<float>(lon0 + (static_cast<double>(i) /
                                      static_cast<double>(n_cell)) *
                                         (lon1 - lon0));
  };
  auto lat_at = [&](int j) {
    return static_cast<float>(lat0 + (static_cast<double>(j) /
                                      static_cast<double>(n_cell)) *
                                         (lat1 - lat0));
  };

  float z_top_min = 1.0e9f;
  float z_top_max = -1.0e9f;
  for (int j = 0; j < kGrid; ++j) {
    for (int i = 0; i < kGrid; ++i) {
      const float u = static_cast<float>(i) / static_cast<float>(n_cell);
      const float v = static_cast<float>(j) / static_cast<float>(n_cell);
      const double lon = static_cast<double>(lon_at(i));
      const double lat = static_cast<double>(lat_at(j));
      const float clay = static_cast<float>(idw_z(lon, lat, "clay", 55.0));
      const float silt = static_cast<float>(idw_z(lon, lat, "silt", 34.0));
      const float sand = static_cast<float>(idw_z(lon, lat, "sand", 17.0));
      const float wave = hill(u, v);
      const float clay_h = clay + wave * kHillM;
      const float bed = (std::max)(10.f, clay - sand) * 0.38f;
      const size_t p = at(i, j);
      z_grid[0][p] = clay_h;
      z_grid[1][p] = clay * 0.62f + silt * 0.38f + wave * kHillM * 0.35f;
      z_grid[2][p] = silt + wave * kHillM * 0.18f;
      z_grid[3][p] = silt * 0.55f + sand * 0.45f;
      z_grid[4][p] = silt * 0.28f + sand * 0.72f;
      z_grid[5][p] = sand;
      z_grid[6][p] = sand - bed * 0.35f;
      z_grid[7][p] = sand - bed;
      z_grid[8][p] = sand - bed * 1.55f;
      z_grid[9][p] = sand - bed * 2.15f;
      for (int s = 1; s < n_surf; ++s) {
        if (z_grid[static_cast<size_t>(s)][p] >
            z_grid[static_cast<size_t>(s - 1)][p] - 0.8f) {
          z_grid[static_cast<size_t>(s)][p] =
              z_grid[static_cast<size_t>(s - 1)][p] - 0.8f;
        }
      }
      z_top_min = (std::min)(z_top_min, z_grid[0][p]);
      z_top_max = (std::max)(z_top_max, z_grid[0][p]);
    }
  }
  const float waterline = z_top_min + (z_top_max - z_top_min) * 0.42f;
  for (int j = 0; j < kGrid; ++j) {
    for (int i = 0; i < kGrid; ++i) {
      const float u = static_cast<float>(i) / static_cast<float>(n_cell);
      const float v = static_cast<float>(j) / static_cast<float>(n_cell);
      const size_t p = at(i, j);
      const bool basin = (u > 0.46f && v > 0.38f);
      if (basin && z_grid[0][p] < waterline + 4.f) {
        z_grid[0][p] = waterline;
        top_kind[p] = 2;
      } else if (z_grid[0][p] >
                 z_top_min + (z_top_max - z_top_min) * 0.78f) {
        top_kind[p] = 1;
      }
    }
  }

  auto elev = [&](int surf, int i, int j) {
    return z_grid[static_cast<size_t>(surf)][at(i, j)] + kLift;
  };
  auto top_layer = [&](int i, int j) {
    const uint8_t k = top_kind[at(i, j)];
    if (k == 2) {
      return kWaterLayer;
    }
    if (k == 1) {
      return kSnowLayer;
    }
    return 0;
  };

  // Top cap: soil / snow / water.
  for (int j = 0; j < n_cell; ++j) {
    for (int i = 0; i < n_cell; ++i) {
      const int i1 = i + 1;
      const int j1 = j + 1;
      const int li = top_layer(i, j);
      float u0 = 0.f, v0 = 0.f, u1 = 0.f, v1 = 0.f, u2 = 0.f, v2 = 0.f,
            u3 = 0.f, v3 = 0.f;
      const float su0 = static_cast<float>(i) / static_cast<float>(n_cell) * 2.4f;
      const float sv0 = static_cast<float>(j) / static_cast<float>(n_cell) * 2.4f;
      const float su1 = static_cast<float>(i1) / static_cast<float>(n_cell) * 2.4f;
      const float sv1 = static_cast<float>(j1) / static_cast<float>(n_cell) * 2.4f;
      band_uv(li, n_atlas, su0, sv0, &u0, &v0);
      band_uv(li, n_atlas, su1, sv0, &u1, &v1);
      band_uv(li, n_atlas, su1, sv1, &u2, &v2);
      band_uv(li, n_atlas, su0, sv1, &u3, &v3);
      append_quad(mesh, lon_at(i), lat_at(j), elev(0, i, j), u0, v0, lon_at(i1),
                  lat_at(j), elev(0, i1, j), u1, v1, lon_at(i1), lat_at(j1),
                  elev(0, i1, j1), u2, v2, lon_at(i), lat_at(j1),
                  elev(0, i, j1), u3, v3);
    }
  }
  // Floor.
  {
    const int fs = n_surf - 1;
    const int li = kWallLayers - 1;
    for (int j = 0; j < n_cell; ++j) {
      for (int i = 0; i < n_cell; ++i) {
        const int i1 = i + 1;
        const int j1 = j + 1;
        float u0 = 0.f, v0 = 0.f, u1 = 0.f, v1 = 0.f, u2 = 0.f, v2 = 0.f,
              u3 = 0.f, v3 = 0.f;
        band_uv(li, n_atlas, 0.1f, 0.1f, &u0, &v0);
        band_uv(li, n_atlas, 0.9f, 0.1f, &u1, &v1);
        band_uv(li, n_atlas, 0.9f, 0.9f, &u2, &v2);
        band_uv(li, n_atlas, 0.1f, 0.9f, &u3, &v3);
        append_quad(mesh, lon_at(i), lat_at(j), elev(fs, i, j), u0, v0,
                    lon_at(i), lat_at(j1), elev(fs, i, j1), u3, v3,
                    lon_at(i1), lat_at(j1), elev(fs, i1, j1), u2, v2,
                    lon_at(i1), lat_at(j), elev(fs, i1, j), u1, v1);
      }
    }
  }

  auto wall_strip = [&](int i0, int j0, int i1, int j1, float along0,
                        float along1) {
    for (int li = 0; li < kWallLayers; ++li) {
      float ut0 = 0.f, vt0 = 0.f, ut1 = 0.f, vt1 = 0.f;
      float ub0 = 0.f, vb0 = 0.f, ub1 = 0.f, vb1 = 0.f;
      band_uv(li, n_atlas, along0, 0.06f, &ut0, &vt0);
      band_uv(li, n_atlas, along1, 0.06f, &ut1, &vt1);
      band_uv(li, n_atlas, along1, 0.94f, &ub1, &vb1);
      band_uv(li, n_atlas, along0, 0.94f, &ub0, &vb0);
      append_quad(mesh, lon_at(i0), lat_at(j0), elev(li, i0, j0), ut0, vt0,
                  lon_at(i1), lat_at(j1), elev(li, i1, j1), ut1, vt1,
                  lon_at(i1), lat_at(j1), elev(li + 1, i1, j1), ub1, vb1,
                  lon_at(i0), lat_at(j0), elev(li + 1, i0, j0), ub0, vb0);
    }
  };
  for (int i = 0; i < n_cell; ++i) {
    const float a0 = static_cast<float>(i) / static_cast<float>(n_cell) * 4.f;
    const float a1 = static_cast<float>(i + 1) / static_cast<float>(n_cell) * 4.f;
    wall_strip(i, 0, i + 1, 0, a0, a1);
    wall_strip(i + 1, n_cell, i, n_cell, a0, a1);
  }
  for (int j = 0; j < n_cell; ++j) {
    const float a0 = static_cast<float>(j) / static_cast<float>(n_cell) * 4.f;
    const float a1 = static_cast<float>(j + 1) / static_cast<float>(n_cell) * 4.f;
    wall_strip(0, j + 1, 0, j, a0, a1);
    wall_strip(n_cell, j, n_cell, j + 1, a0, a1);
  }

  constexpr int kSides = 8;
  constexpr float kRadius = 0.00055f;
  constexpr float kPi = 3.14159265f;
  for (int h = 0; h < n_holes; ++h) {
    const float lon = static_cast<float>(cols[static_cast<size_t>(h)].lon);
    const float lat = static_cast<float>(cols[static_cast<size_t>(h)].lat);
    if (lon < lon0 || lon > lon1 || lat < lat0 || lat > lat1) {
      continue;
    }
    const float u = (lon - static_cast<float>(lon0)) / lon_span;
    const float v = (lat - static_cast<float>(lat0)) / lat_span;
    const int gi = (std::max)(
        0, (std::min)(n_cell, static_cast<int>(u * static_cast<float>(n_cell))));
    const int gj = (std::max)(
        0, (std::min)(n_cell, static_cast<int>(v * static_cast<float>(n_cell))));
    for (int li = 0; li < kWallLayers; ++li) {
      const float z_top = elev(li, gi, gj) + 0.8f;
      const float z_bot = elev(li + 1, gi, gj);
      for (int s = 0; s < kSides; ++s) {
        const float a0 = static_cast<float>(s) / static_cast<float>(kSides) *
                         2.f * kPi;
        const float a1 = static_cast<float>(s + 1) /
                         static_cast<float>(kSides) * 2.f * kPi;
        const float x0 = lon + std::cos(a0) * kRadius;
        const float y0 = lat + std::sin(a0) * kRadius * 0.78f;
        const float x1 = lon + std::cos(a1) * kRadius;
        const float y1 = lat + std::sin(a1) * kRadius * 0.78f;
        float u0 = 0.f, v0 = 0.f, u1 = 0.f, v1 = 0.f, u2 = 0.f, v2 = 0.f,
              u3 = 0.f, v3 = 0.f;
        const float su0 = static_cast<float>(s) / static_cast<float>(kSides);
        const float su1 =
            static_cast<float>(s + 1) / static_cast<float>(kSides);
        band_uv(li, n_atlas, su0, 0.08f, &u0, &v0);
        band_uv(li, n_atlas, su1, 0.08f, &u1, &v1);
        band_uv(li, n_atlas, su1, 0.92f, &u2, &v2);
        band_uv(li, n_atlas, su0, 0.92f, &u3, &v3);
        append_quad(mesh, x0, y0, z_top, u0, v0, x1, y1, z_top, u1, v1, x1, y1,
                    z_bot, u2, v2, x0, y0, z_bot, u3, v3);
      }
    }
  }

  return mesh->idx.size() >= 3 && mesh->xyz.size() >= 9 &&
         mesh->uv.size() == (mesh->xyz.size() / 3) * 2;
}

}  // namespace

// Borehole sticks (map LineString footprint) + stratum TIN (map2d polygons) +
// scene3d overlay: layered prism volume, textured sides, borehole cylinders.
bool present_mine_stratum(content::GisDocument* doc,
                          content::PluginHost::Scene3dSink* sink,
                          content::Scene3dPresenter* scene3d,
                         const gis::detail::StratumTin& tin,
                         const gis::detail::BoreholeSet& holes,
                         std::string* err) {
  if (!doc) {
    if (err) {
      *err = "no_doc";
    }
    return false;
  }
  if (!holes.ok || holes.contacts.empty()) {
    if (err) {
      *err = holes.error.empty() ? "no_holes" : holes.error;
    }
    return false;
  }

  doc->remove_layer("mine_stratum");
  doc->remove_layer("mine_boreholes");
  (void)apply_style_resource(doc, "smartgis.mine", "mine.style.json");

  if (tin.ok && !tin.xyz.empty() && tin.indices.size() >= 3) {
    const int point_count = static_cast<int>(tin.xyz.size() / 3);
    const int triangle_count = static_cast<int>(tin.indices.size() / 3);
    std::vector<int> tris(tin.indices.begin(), tin.indices.end());
    if (!doc->add_triangle_mesh("mine_stratum", tin.xyz.data(), point_count,
                                tris.data(), triangle_count)) {
      if (err) {
        *err = "tin_layer_failed";
      }
      return false;
    }
  }

  std::map<std::string, std::vector<const gis::detail::BoreholeContact*>> by_hole;
  for (const auto& c : holes.contacts) {
    by_hole[c.hole_id].push_back(&c);
  }
  if (!doc->create_layer("mine_boreholes", "LineString")) {
    if (err) {
      *err = "stick_layer_failed";
    }
    return false;
  }
  int sticks = 0;
  for (auto& [hole_id, contacts] : by_hole) {
    (void)hole_id;
    if (contacts.empty()) {
      continue;
    }
    std::sort(contacts.begin(), contacts.end(),
              [](const gis::detail::BoreholeContact* a,
                 const gis::detail::BoreholeContact* b) {
                return a->z > b->z;
              });
    const double x = contacts.front()->x;
    const double y = contacts.front()->y;
    const double depth = contacts.front()->z - contacts.back()->z;
    const double stick_len = std::max(0.002, std::abs(depth) * 0.0002);
    std::vector<std::pair<double, double>> pts = {
        {x, y},
        {x, y - stick_len},
    };
    if (append_map_polyline(doc, pts, nullptr)) {
      ++sticks;
    }
  }
  if (sticks == 0) {
    if (err) {
      *err = "no_sticks";
    }
    return false;
  }

  auto commit_overlay = [&](const float* xyz, int point_count,
                            const unsigned* indices, int index_count,
                            const uint8_t* albedo) {
    if (scene3d) {
      scene3d->set_overlay_tin_mesh(xyz, point_count, indices, index_count,
                                    albedo);
      return;
    }
    if (sink) {
      sink->set_overlay_tin_mesh(xyz, point_count, indices, index_count, albedo);
    }
  };
  auto commit_drape = [&](const uint8_t* rgba, uint32_t width, uint32_t height,
                          const float* uv, int uv_n) {
    if (scene3d) {
      scene3d->set_overlay_tin_drape(rgba, width, height, uv, uv_n);
      return;
    }
    if (sink) {
      sink->set_overlay_tin_drape(rgba, width, height, uv, uv_n);
    }
  };
  auto clear_overlay = [&]() {
    if (scene3d) {
      scene3d->clear_overlay_tin_mesh();
    } else if (sink) {
      sink->clear_overlay_tin_mesh();
    }
  };

  if (scene3d || sink) {
    OverlayMesh mesh;
    std::vector<uint8_t> atlas;
    constexpr int kTex = 128;
    if (build_mine_volume_mesh(holes, &mesh, &atlas, kTex, kTex)) {
      const int point_count = static_cast<int>(mesh.xyz.size() / 3);
      constexpr uint8_t kClayAlbedo[4] = {176, 92, 62, 230};
      commit_overlay(mesh.xyz.data(), point_count, mesh.idx.data(),
                     static_cast<int>(mesh.idx.size()), kClayAlbedo);
      commit_drape(atlas.data(), kTex, kTex, mesh.uv.data(),
                   static_cast<int>(mesh.uv.size()));
    } else if (tin.ok && !tin.xyz.empty() && tin.indices.size() >= 3) {
      const int point_count = static_cast<int>(tin.xyz.size() / 3);
      std::vector<float> tin_geo(static_cast<size_t>(point_count) * 3u);
      constexpr float kOverlayLiftM = 40.f;
      for (int i = 0; i < point_count; ++i) {
        tin_geo[static_cast<size_t>(i) * 3u] =
            static_cast<float>(tin.xyz[static_cast<size_t>(i) * 3u]);
        tin_geo[static_cast<size_t>(i) * 3u + 1u] =
            static_cast<float>(tin.xyz[static_cast<size_t>(i) * 3u + 1u]);
        tin_geo[static_cast<size_t>(i) * 3u + 2u] =
            static_cast<float>(tin.xyz[static_cast<size_t>(i) * 3u + 2u]) +
            kOverlayLiftM;
      }
      std::vector<unsigned> tin_idx(tin.indices.begin(), tin.indices.end());
      constexpr uint8_t kStratumAlbedo[4] = {176, 92, 62, 230};
      commit_overlay(tin_geo.data(), point_count, tin_idx.data(),
                     static_cast<int>(tin_idx.size()), kStratumAlbedo);
    } else {
      clear_overlay();
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
