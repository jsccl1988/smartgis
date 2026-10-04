// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/world/dem_seed.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "base/time/elapsed_timer.h"
#include "vista/terrain/dem/dem_bake_cache.h"

namespace vista {

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
  // Fallback when callers pass 0/1: match national-frame LOD density for
  // china_dem 1536×960 (not the old soft 96-edge bake).
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

  std::vector<uint8_t> rgba;
  int tw = 0;
  int th = 0;
  // Hypsometric bake matches land-only mesh UVs. china_rs drape often leaves a
  // flat cyan sticker when imagery resolution and mesh LOD diverge.
  const bool have_tex = dem.bake_hypsometric_rgba(edge, &rgba, &tw, &th);
  if (have_tex && tw > 0 && th > 0) {
    world->set_terrain_texture(node->id, rgba.data(), rgba.size(),
                               static_cast<uint32_t>(tw),
                               static_cast<uint32_t>(th));
  }
  return world->find(node->id);
}

Node* seed_dem_raster_lod_into_world(World* world, const DemRaster& dem,
                                     const char* name,
                                     float camera_distance) {
  const int edge = DemRaster::lod_max_edge(camera_distance);
  return seed_dem_raster_into_world(world, dem, name, edge);
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
  // Far → 1 tile; mid → 2×2; near → 4×4. Zoom-in replaces coarse with finer.
  // Small framed extents (coastal showcase pads, city AOI) stay one tile —
  // multi-tile left/right hypsometric seams and chewed land-mask edges.
  const double span =
      (std::max)(maxx - minx, maxy - miny);
  int grid = 1;
  if (span > 2.0) {
    if (camera_distance < 1.2f) {
      grid = 4;
    } else if (camera_distance < 2.4f) {
      grid = 2;
    }
  }
  // Regional windows: skip land mask so the mesh keeps a rectangular skirt
  // instead of a chewed coastline silhouette against the clear color.
  const bool apply_land_mask = span > 2.0;
  int edge = DemRaster::lod_max_edge(camera_distance);
  // Cap per-tile edge so grid*edge^2 stays under the vertex budget.
  // Default budget fits one ~512×320 china_dem tile (≈163k verts) with headroom.
  const int budget =
      max_total_vertices > 64 ? max_total_vertices : 196608;
  const int max_edge_per_tile =
      (std::max)(8, static_cast<int>(
                        std::sqrt(static_cast<double>(budget) /
                                  static_cast<double>(grid * grid))));
  edge = (std::min)(edge, max_edge_per_tile);

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
      float mn_x = xyz[0];
      float mn_y = xyz[1];
      float mn_z = xyz[2];
      float mx_x = mn_x;
      float mx_y = mn_y;
      float mx_z = mn_z;
      for (size_t i = 0; i + 2 < xyz.size(); i += 3) {
        mn_x = (std::min)(mn_x, xyz[i]);
        mn_y = (std::min)(mn_y, xyz[i + 1]);
        mn_z = (std::min)(mn_z, xyz[i + 2]);
        mx_x = (std::max)(mx_x, xyz[i]);
        mx_y = (std::max)(mx_y, xyz[i + 1]);
        mx_z = (std::max)(mx_z, xyz[i + 2]);
      }
      char name_buf[64];
      std::snprintf(name_buf, sizeof(name_buf), "%s_%d_%d", prefix, tx, ty);
      Node* node = world->attach_terrain(name_buf, static_cast<double>(mn_x),
                                         static_cast<double>(mn_y),
                                         static_cast<double>(mn_z),
                                         static_cast<double>(mx_x),
                                         static_cast<double>(mx_y),
                                         static_cast<double>(mx_z));
      if (!node) {
        continue;
      }
      world->set_terrain_mesh(node->id, xyz.data(), xyz.size(), idx.data(),
                              idx.size());
      if (!uvs.empty()) {
        world->set_terrain_uvs(node->id, uvs.data(), uvs.size());
      }
      std::vector<uint8_t> rgba;
      int tw = 0;
      int th = 0;
      if (dem.bake_hypsometric_rgba(edge, &rgba, &tw, &th) && tw > 0 &&
          th > 0) {
        world->set_terrain_texture(node->id, rgba.data(), rgba.size(),
                                   static_cast<uint32_t>(tw),
                                   static_cast<uint32_t>(th));
      }
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
  DemRaster dem;
  const std::string path = find_sample_dem_path();
  if (path.empty() || !dem.load_gdal_raster(path.c_str()) || dem.empty()) {
    // Real-data policy: no synthetic China DEM stand-in.
    return nullptr;
  }
  // Real china_dem* already encodes land/ocean. Remasking with prefecture
  // rings can punch holes (mainland-contains caution). Match leftover
  // seed_stereo_underlay: skip mask_outside_rings when the loaded path is
  // china_dem. Other rasters may still mask with rings.
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
