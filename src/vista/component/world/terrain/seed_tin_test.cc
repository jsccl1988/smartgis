// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/terrain/seed.h"

#include <cstdio>

#include "gis/geo/ops/indexed_tin.h"
#include "vista/component/world/terrain/lod.h"
#include "vista/component/world/world.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

// In-process OGRTriangulatedSurface: cells×cells quads as two triangles each.
bool fill_tin_grid(OGRTriangulatedSurface* tin, int cells) {
  if (!tin || cells < 1) {
    return false;
  }
  for (int y = 0; y < cells; ++y) {
    for (int x = 0; x < cells; ++x) {
      const double z00 = static_cast<double>(x + y);
      const double z10 = static_cast<double>(x + 1 + y);
      const double z11 = static_cast<double>(x + 1 + y + 1);
      const double z01 = static_cast<double>(x + y + 1);
      OGRPoint p00(x, y, z00);
      OGRPoint p10(x + 1, y, z10);
      OGRPoint p11(x + 1, y + 1, z11);
      OGRPoint p01(x, y + 1, z01);
      if (!geo::add_patch(tin, p00, p10, p11) ||
          !geo::add_patch(tin, p00, p11, p01)) {
        return false;
      }
    }
  }
  return tin->getNumGeometries() == cells * cells * 2;
}

}  // namespace

int main() {
  OGRTriangulatedSurface tin;
  expect(fill_tin_grid(&tin, 4), "real TIN fixture 4x4");
  expect(tin.getNumGeometries() == 32, "32 OGR triangles");

  {
    vista::World world;
    vista::Node* node = vista::seed_tin_into_world(&world, &tin, "tin_full");
    expect(node != nullptr, "seed_tin_into_world");
    expect(node && node->kind == vista::NodeKind::kTerrain, "kTerrain kind");
    expect(node && node->has_terrain_mesh(), "TerrainPayload mesh");
    expect(node && node->terrain.source == vista::TerrainSource::kTin,
           "source kTin");
    expect(node && node->terrain.indices.size() == 96u, "full 32 tris");
    expect(node && node->terrain.lod_key == vista::terrain_lod_tin_cache_key(0.f),
           "full lod_key");
  }

  vista::World tin_near;
  vista::World tin_far;
  vista::Node* n_near =
      vista::seed_tin_lod_into_world(&tin_near, &tin, "tin_n", 0.5f);
  vista::Node* n_far =
      vista::seed_tin_lod_into_world(&tin_far, &tin, "tin_f", 5.0f);
  expect(n_near && n_near->has_terrain_mesh(), "near seed mesh");
  expect(n_far && n_far->has_terrain_mesh(), "far seed mesh");
  expect(n_near && n_near->kind == vista::NodeKind::kTerrain, "near kTerrain");
  expect(n_far && n_far->kind == vista::NodeKind::kTerrain, "far kTerrain");
  expect(n_near && n_near->terrain.source == vista::TerrainSource::kTin,
         "near source");
  expect(n_far && n_far->terrain.source == vista::TerrainSource::kTin,
         "far source");
  expect(n_near && n_near->terrain.lod_key ==
                       vista::terrain_lod_tin_cache_key(0.5f),
         "near tin key");
  expect(n_far && n_far->terrain.lod_key == vista::terrain_lod_tin_cache_key(5.0f),
         "far tin key");
  expect(n_near && n_near->terrain.indices.size() == 96u, "near keeps all tris");
  expect(n_far && n_near &&
             n_far->terrain.indices.size() < n_near->terrain.indices.size(),
         "closer camera denser triangles");
  expect(vista::terrain_lod_tin_stride(0.5f) <
             vista::terrain_lod_tin_stride(5.0f),
         "near stride smaller");

  {
    OGRTriangulatedSurface spatial;
    expect(fill_tin_grid(&spatial, 8), "spatial TIN 8x8");
    vista::World world;
    vista::Node* node = vista::seed_tin_lod_into_world(
        &world, &spatial, "tin_xyz", 0.5f, 0.f, 0.f, 0.f);
    expect(node && node->has_terrain_mesh(), "spatial seed mesh");
    expect(node && node->terrain.lod_key ==
                       vista::terrain_lod_tin_cache_key(0.5f, 0.f, 0.f, 0.f),
           "spatial lod_key");
    expect(vista::terrain_lod_tin_stride_at(0.5f, 0.2f, 12.f) <
               vista::terrain_lod_tin_stride_at(0.5f, 11.f, 12.f),
           "centroid near denser stride than far");
    size_t near_tris = 0;
    size_t far_tris = 0;
    if (node) {
      const auto& p = node->terrain.positions;
      const auto& ix = node->terrain.indices;
      for (size_t t = 0; t + 2 < ix.size(); t += 3) {
        const size_t a = static_cast<size_t>(ix[t]) * 3u;
        const size_t b = static_cast<size_t>(ix[t + 1]) * 3u;
        const size_t c = static_cast<size_t>(ix[t + 2]) * 3u;
        if (a + 2 >= p.size() || b + 2 >= p.size() || c + 2 >= p.size()) {
          continue;
        }
        const float mx = (p[a] + p[b] + p[c]) / 3.f;
        const float my = (p[a + 1] + p[b + 1] + p[c + 1]) / 3.f;
        if (mx < 2.5f && my < 2.5f) {
          ++near_tris;
        } else if (mx > 5.5f && my > 5.5f) {
          ++far_tris;
        }
      }
    }
    expect(near_tris > far_tris, "same TIN near patch denser than far");
    vista::ViewState view;
    view.eye_x = 0;
    view.eye_y = 0;
    view.eye_z = 0;
    view.sse_denominator = 1;
    vista::World world_vs;
    vista::Node* vs = vista::seed_tin_lod_into_world(&world_vs, &spatial,
                                                     "tin_vs", 0.5f, view);
    expect(vs && vs->has_terrain_mesh() &&
               vs->terrain.indices.size() == node->terrain.indices.size(),
           "ViewState overload matches xyz");
  }

  if (g_fails) {
    std::fprintf(stderr, "seed_tin_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "seed_tin_test: ok\n");
  return 0;
}
