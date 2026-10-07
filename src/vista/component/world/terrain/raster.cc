// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/terrain/seed.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "base/time/elapsed_timer.h"
#include "vista/component/world/terrain/mesh.h"
#include "vista/component/world/terrain/policy.h"
#include "vista/terrain/dem/dem_bake_cache.h"

namespace vista {
using detail::aabb_from_xyz;
using detail::apply_terrain_albedo_texture;
using detail::stamp_payload_meta;

Node* seed_dem_raster_into_world(World* world, const DemRaster& dem,
                                 const char* name, int max_edge) {
  if (!world || dem.empty()) {
    return nullptr;
  }
  double min_x = 0;
  double min_y = 0;
  double max_x = 0;
  double max_y = 0;
  dem.envelope(&min_x, &min_y, &max_x, &max_y);
  const double min_z =
      static_cast<double>(dem.min_meters() * dem.vertical_exaggeration());
  const double max_z =
      static_cast<double>(dem.max_meters() * dem.vertical_exaggeration());
  const char* node_name = (name && name[0]) ? name : "dem";
  Node* node =
      world->attach_terrain(node_name, min_x, min_y, min_z, max_x, max_y, max_z);
  if (!node) {
    return nullptr;
  }
  const int edge = max_edge > 1 ? max_edge : 512;
  std::vector<float> xyz;
  std::vector<uint32_t> idx;
  std::vector<float> uvs;
  {
    base::ElapsedTimer tess_timer;
    double dminx = 0;
    double dminy = 0;
    double dmaxx = 0;
    double dmaxy = 0;
    dem.envelope(&dminx, &dminy, &dmaxx, &dmaxy);
    const char* sp =
        dem.source_path().empty() ? nullptr : dem.source_path().c_str();
    bool have = sp && dem_mesh_cache_try_get(sp, edge, dminx, dminy, dmaxx,
                                            dmaxy, /*windowed=*/false,
                                            /*apply_land_mask=*/true, &xyz,
                                            &idx, &uvs);
    if (!have) {
      have = dem.build_mesh(edge, &xyz, &idx, &uvs) && !xyz.empty() &&
             !idx.empty();
      if (have && sp) {
        dem_mesh_cache_put(sp, edge, dminx, dminy, dmaxx, dmaxy,
                           /*windowed=*/false, /*apply_land_mask=*/true, xyz,
                           idx, uvs);
      }
    }
    note_dem_phase_tess(
        static_cast<int64_t>(tess_timer.elapsed_milliseconds() + 0.5));
    if (!have) {
      return node;
    }
  }
  world->set_terrain_mesh(node->id, xyz.data(), xyz.size(), idx.data(),
                          idx.size());
  if (!uvs.empty()) {
    world->set_terrain_uvs(node->id, uvs.data(), uvs.size());
  }
  apply_terrain_albedo_texture(world, node->id, dem, edge);
  stamp_payload_meta(world, node->id, edge * 10 + 1, TerrainSource::kRaster);
  return world->find(node->id);
}
Node* seed_dem_raster_lod_into_world(World* world, const DemRaster& dem,
                                     const char* name,
                                     float camera_distance) {
  const int edge = terrain_lod_max_edge(camera_distance);
  Node* node = seed_dem_raster_into_world(world, dem, name, edge);
  if (node) {
    stamp_payload_meta(world, node->id, terrain_lod_cache_key(camera_distance),
                       TerrainSource::kRaster);
  }
  return node;
}
size_t seed_dem_view_tiles_into_world(World* world, const DemRaster& dem,
                                      double view_minx, double view_miny,
                                      double view_maxx, double view_maxy,
                                      float camera_distance,
                                      int max_total_vertices,
                                      const char* name_prefix) {
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
  const double span = (std::max)(maxx - minx, maxy - miny);
  int grid = 1;
  if (span > 2.0) {
    if (camera_distance < 1.2f) {
      grid = 4;
    } else if (camera_distance < 2.4f) {
      grid = 2;
    }
  }
  const bool apply_land_mask = span > 2.0;
  int edge = terrain_lod_max_edge(camera_distance);
  const int budget =
      max_total_vertices > 64 ? max_total_vertices : 196608;
  const int max_edge_per_tile =
      (std::max)(8, static_cast<int>(
                        std::sqrt(static_cast<double>(budget) /
                                  static_cast<double>(grid * grid))));
  edge = (std::min)(edge, max_edge_per_tile);
  const int lod_key = terrain_lod_cache_key(camera_distance);

  const char* prefix =
      (name_prefix && name_prefix[0]) ? name_prefix : "dem_tile";
  const double tile_w = (maxx - minx) / static_cast<double>(grid);
  const double tile_h = (maxy - miny) / static_cast<double>(grid);
  size_t attached = 0;
  size_t total_verts = 0;
  for (int ty = 0; ty < grid; ++ty) {
    for (int tx = 0; tx < grid; ++tx) {
      const double tminx = minx + tx * tile_w;
      const double tmaxx = minx + (tx + 1) * tile_w;
      const double tminy = miny + ty * tile_h;
      const double tmaxy = miny + (ty + 1) * tile_h;
      std::vector<float> xyz;
      std::vector<uint32_t> idx;
      std::vector<float> uvs;
      {
        base::ElapsedTimer tess_timer;
        const char* sp =
            dem.source_path().empty() ? nullptr : dem.source_path().c_str();
        bool have =
            sp && dem_mesh_cache_try_get(sp, edge, tminx, tminy, tmaxx, tmaxy,
                                         /*windowed=*/true, apply_land_mask,
                                         &xyz, &idx, &uvs);
        if (!have) {
          have = dem.build_mesh_window(tminx, tminy, tmaxx, tmaxy, edge, &xyz,
                                       &idx, &uvs, apply_land_mask) &&
                 !xyz.empty() && !idx.empty();
          if (have && sp) {
            dem_mesh_cache_put(sp, edge, tminx, tminy, tmaxx, tmaxy,
                               /*windowed=*/true, apply_land_mask, xyz, idx,
                               uvs);
          }
        }
        note_dem_phase_tess(
            static_cast<int64_t>(tess_timer.elapsed_milliseconds() + 0.5));
        if (!have) {
          continue;
        }
      }
      const size_t verts = xyz.size() / 3;
      if (total_verts + verts > static_cast<size_t>(budget) && attached > 0) {
        break;
      }
      double mn_x = 0;
      double mn_y = 0;
      double mn_z = 0;
      double mx_x = 0;
      double mx_y = 0;
      double mx_z = 0;
      if (!aabb_from_xyz(xyz, &mn_x, &mn_y, &mn_z, &mx_x, &mx_y, &mx_z)) {
        continue;
      }
      char name_buf[64];
      std::snprintf(name_buf, sizeof(name_buf), "%s_%d_%d", prefix, tx, ty);
      Node* node =
          world->attach_terrain(name_buf, mn_x, mn_y, mn_z, mx_x, mx_y, mx_z);
      if (!node) {
        continue;
      }
      world->set_terrain_mesh(node->id, xyz.data(), xyz.size(), idx.data(),
                              idx.size());
      if (!uvs.empty()) {
        world->set_terrain_uvs(node->id, uvs.data(), uvs.size());
      }
      apply_terrain_albedo_texture(world, node->id, dem, edge);
      stamp_payload_meta(world, node->id, lod_key, TerrainSource::kRaster);
      total_verts += verts;
      ++attached;
    }
  }
  return attached;
}
Node* seed_china_dem_into_world(World* world, const LonLatRing* rings,
                                size_t ring_count, const char* name,
                                int max_edge) {
  if (!world) {
    return nullptr;
  }
  // Hide first dem_bake mkdir / root resolve under seed cold path.
  dem_bake_cache_warmup();
  DemRaster dem;
  const std::string path = find_sample_dem_path();
  if (path.empty() || !dem.load_gdal_raster(path.c_str()) || dem.empty()) {
    return nullptr;
  }
  const bool skip_cutline = path.find("china_dem") != std::string::npos;
  if (!skip_cutline && rings && ring_count > 0) {
    std::vector<LonLatRing> clip(rings, rings + ring_count);
    dem.mask_outside_rings(clip);
  }
  return seed_dem_raster_into_world(world, dem, name, max_edge);
}
}  // namespace vista

namespace render {

vista::Node* seed_dem_height_field_into_world(vista::World* world,
                                              const DemHeightField& dem,
                                              const char* name, int max_edge) {
  return vista::seed_dem_raster_into_world(world, dem.dem_raster(), name,
                                           max_edge);
}

}  // namespace render
