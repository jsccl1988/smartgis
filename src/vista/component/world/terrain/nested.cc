// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/terrain/seed.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "vista/component/world/terrain/grid.h"
#include "vista/component/world/terrain/mesh.h"
#include "vista/component/world/terrain/policy.h"

namespace vista {
using detail::apply_nested_patch_continuity;
using detail::seed_dem_window_node;

size_t seed_dem_nested_grid_into_world(
    World* world, const DemRaster& dem, double view_minx, double view_miny,
    double view_maxx, double view_maxy, float camera_distance,
    int max_total_vertices, const char* name_prefix) {
  if (!(view_maxx > view_minx) || !(view_maxy > view_miny)) {
    return 0;
  }
  return seed_dem_nested_grid_into_world(
      world, dem, view_minx, view_miny, view_maxx, view_maxy, camera_distance,
      max_total_vertices, name_prefix, 0.5 * (view_minx + view_maxx),
      0.5 * (view_miny + view_maxy));
}
size_t seed_dem_nested_grid_into_world(
    World* world, const DemRaster& dem, double view_minx, double view_miny,
    double view_maxx, double view_maxy, float camera_distance,
    int max_total_vertices, const char* name_prefix, double focus_x,
    double focus_y) {
  if (!world || dem.empty()) {
    return 0;
  }
  double dem_minx = 0;
  double dem_miny = 0;
  double dem_maxx = 0;
  double dem_maxy = 0;
  dem.envelope(&dem_minx, &dem_miny, &dem_maxx, &dem_maxy);
  const double minx = (std::max)(view_minx, dem_minx);
  const double miny = (std::max)(view_miny, dem_miny);
  const double maxx = (std::min)(view_maxx, dem_maxx);
  const double maxy = (std::min)(view_maxy, dem_maxy);
  if (!(maxx > minx) || !(maxy > miny)) {
    return 0;
  }
  std::vector<NestedGridTile> tiles;
  if (select_nested_grid_tiles(minx, miny, maxx, maxy, camera_distance, focus_x,
                               focus_y, &tiles) == 0) {
    return 0;
  }
  const double span = (std::max)(maxx - minx, maxy - miny);
  const bool apply_land_mask = span > 2.0;
  const int budget =
      max_total_vertices > 64 ? max_total_vertices : 196608;
  const char* prefix =
      (name_prefix && name_prefix[0]) ? name_prefix : "nested";
  size_t attached = 0;
  size_t total_verts = 0;
  for (size_t i = 0; i < tiles.size(); ++i) {
    const NestedGridTile& tile = tiles[i];
    const double tminx = (std::max)(tile.minx, minx);
    const double tminy = (std::max)(tile.miny, miny);
    const double tmaxx = (std::min)(tile.maxx, maxx);
    const double tmaxy = (std::min)(tile.maxy, maxy);
    if (!(tmaxx > tminx) || !(tmaxy > tminy)) {
      continue;
    }
    int edge = tile.max_edge;
    const int remain = budget - static_cast<int>(total_verts);
    if (remain < 9) {
      break;
    }
    const int cap = (std::max)(
        8, static_cast<int>(std::sqrt(static_cast<double>(remain))));
    edge = (std::min)(edge, cap);
    char name_buf[80];
    std::snprintf(name_buf, sizeof(name_buf), "%s_r%d_%zu", prefix, tile.ring,
                  i);
    const int lod_key = terrain_lod_nested_tile_key(tile.ring, tile.max_edge);
    Node* node = seed_dem_window_node(world, dem, tminx, tminy, tmaxx, tmaxy,
                                      edge, apply_land_mask, lod_key,
                                      TerrainSource::kRaster, name_buf);
    if (!node || !node->has_terrain_mesh()) {
      continue;
    }
    apply_nested_patch_continuity(world, node, tile);
    node = world->find(node->id);
    if (!node || !node->has_terrain_mesh()) {
      continue;
    }
    const size_t verts = node->terrain.positions.size() / 3;
    if (total_verts + verts > static_cast<size_t>(budget) && attached > 0) {
      world->remove_node(node->id);
      break;
    }
    total_verts += verts;
    ++attached;
  }
  return attached;
}
}  // namespace vista
