// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/terrain/seed.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "base/time/elapsed_timer.h"
#include "ogrsf_frmts.h"
#include "vista/component/world/terrain/lod.h"
#include "vista/mesh/tessellate.h"
#include "vista/terrain/dem/dem_bake_cache.h"

namespace vista {
namespace {

void stamp_payload_meta(World* world, uint64_t node_id, int lod_key,
                        TerrainSource source) {
  if (!world) {
    return;
  }
  Node* node = world->find(node_id);
  if (!node || node->kind != NodeKind::kTerrain) {
    return;
  }
  node->terrain.lod_key = lod_key;
  node->terrain.source = source;
}

void apply_elevation_overlay_texture(World* world, uint64_t node_id,
                                     const DemRaster& dem, int edge,
                                     std::vector<uint8_t>* rgba, int tw,
                                     int th) {
  if (!world || !rgba || tw < 2 || th < 2) {
    return;
  }
  std::vector<uint8_t> overlay;
  int ow = 0;
  int oh = 0;
  if (!dem.bake_elevation_overlay_rgba(edge, /*surface=*/false, /*curves=*/true,
                                       &overlay, &ow, &oh) ||
      ow != tw || oh != th || overlay.size() != rgba->size()) {
    return;
  }
  for (size_t i = 0; i + 3 < overlay.size(); i += 4) {
    if (overlay[i + 3] == 0) {
      continue;
    }
    (*rgba)[i + 0] = overlay[i + 0];
    (*rgba)[i + 1] = overlay[i + 1];
    (*rgba)[i + 2] = overlay[i + 2];
    (*rgba)[i + 3] = 255;
  }
  world->set_terrain_texture(node_id, rgba->data(), rgba->size(),
                             static_cast<uint32_t>(tw),
                             static_cast<uint32_t>(th));
}

// Keep every |stride|-th triangle from a TessMesh (discrete TIN LOD).
bool thin_tess_mesh(const TessMesh& in, int stride, TessMesh* out) {
  if (!out || in.positions.empty()) {
    return false;
  }
  out->positions.clear();
  out->indices.clear();
  if (in.indices.size() < 3 || (in.indices.size() % 3) != 0) {
    return false;
  }
  const int s = stride > 1 ? stride : 1;
  const size_t tri_count = in.indices.size() / 3;
  out->positions.reserve(in.positions.size() / static_cast<size_t>(s));
  out->indices.reserve(in.indices.size() / static_cast<size_t>(s));
  for (size_t t = 0; t < tri_count; ++t) {
    if ((t % static_cast<size_t>(s)) != 0) {
      continue;
    }
    const uint32_t i0 = in.indices[t * 3 + 0];
    const uint32_t i1 = in.indices[t * 3 + 1];
    const uint32_t i2 = in.indices[t * 3 + 2];
    const size_t b0 = static_cast<size_t>(i0) * 3u;
    const size_t b1 = static_cast<size_t>(i1) * 3u;
    const size_t b2 = static_cast<size_t>(i2) * 3u;
    if (b0 + 2 >= in.positions.size() || b1 + 2 >= in.positions.size() ||
        b2 + 2 >= in.positions.size()) {
      continue;
    }
    const uint32_t base =
        static_cast<uint32_t>(out->positions.size() / 3);
    out->positions.push_back(in.positions[b0]);
    out->positions.push_back(in.positions[b0 + 1]);
    out->positions.push_back(in.positions[b0 + 2]);
    out->positions.push_back(in.positions[b1]);
    out->positions.push_back(in.positions[b1 + 1]);
    out->positions.push_back(in.positions[b1 + 2]);
    out->positions.push_back(in.positions[b2]);
    out->positions.push_back(in.positions[b2 + 1]);
    out->positions.push_back(in.positions[b2 + 2]);
    out->indices.push_back(base);
    out->indices.push_back(base + 1);
    out->indices.push_back(base + 2);
  }
  return out->indices.size() >= 3;
}

bool aabb_from_xyz(const std::vector<float>& xyz, double* min_x, double* min_y,
                   double* min_z, double* max_x, double* max_y, double* max_z) {
  if (xyz.size() < 3 || !min_x || !min_y || !min_z || !max_x || !max_y ||
      !max_z) {
    return false;
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
  *min_x = mn_x;
  *min_y = mn_y;
  *min_z = mn_z;
  *max_x = mx_x;
  *max_y = mx_y;
  *max_z = mx_z;
  return true;
}

Node* attach_payload_mesh(World* world, const char* name,
                          const std::vector<float>& xyz,
                          const std::vector<uint32_t>& idx, int lod_key,
                          TerrainSource source) {
  if (!world || xyz.size() < 9 || idx.size() < 3) {
    return nullptr;
  }
  double min_x = 0;
  double min_y = 0;
  double min_z = 0;
  double max_x = 0;
  double max_y = 0;
  double max_z = 0;
  if (!aabb_from_xyz(xyz, &min_x, &min_y, &min_z, &max_x, &max_y, &max_z)) {
    return nullptr;
  }
  const char* node_name = (name && name[0]) ? name : "terrain";
  Node* node =
      world->attach_terrain(node_name, min_x, min_y, min_z, max_x, max_y, max_z);
  if (!node) {
    return nullptr;
  }
  if (!world->set_terrain_mesh(node->id, xyz.data(), xyz.size(), idx.data(),
                               idx.size())) {
    return nullptr;
  }
  stamp_payload_meta(world, node->id, lod_key, source);
  return world->find(node->id);
}

}  // namespace

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

  std::vector<uint8_t> rgba;
  int tw = 0;
  int th = 0;
  const bool have_tex = dem.bake_hypsometric_rgba(edge, &rgba, &tw, &th);
  if (have_tex && tw > 0 && th > 0) {
    world->set_terrain_texture(node->id, rgba.data(), rgba.size(),
                               static_cast<uint32_t>(tw),
                               static_cast<uint32_t>(th));
    apply_elevation_overlay_texture(world, node->id, dem, edge, &rgba, tw, th);
  }
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
      std::vector<uint8_t> rgba;
      int tw = 0;
      int th = 0;
      if (dem.bake_hypsometric_rgba(edge, &rgba, &tw, &th) && tw > 0 &&
          th > 0) {
        world->set_terrain_texture(node->id, rgba.data(), rgba.size(),
                                   static_cast<uint32_t>(tw),
                                   static_cast<uint32_t>(th));
        apply_elevation_overlay_texture(world, node->id, dem, edge, &rgba, tw,
                                        th);
      }
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

Node* seed_dem_surface_into_world(World* world,
                                  const render::DemHeightField& dem,
                                  const char* name, int max_edge) {
  if (!world || dem.empty()) {
    return nullptr;
  }
  const int edge = max_edge > 1 ? max_edge : 96;
  std::vector<float> xyz;
  std::vector<unsigned> idx_u;
  std::vector<float> rgb;
  std::vector<float> nrm;
  if (!dem.build_mesh(edge, &xyz, &idx_u, &rgb, &nrm) || xyz.size() < 9 ||
      idx_u.size() < 3) {
    return nullptr;
  }
  std::vector<uint32_t> idx(idx_u.begin(), idx_u.end());
  Node* node = attach_payload_mesh(world, name, xyz, idx, edge * 10 + 1,
                                   TerrainSource::kSurface);
  return node;
}

Node* seed_dem_surface_lod_into_world(World* world,
                                      const render::DemHeightField& dem,
                                      const char* name,
                                      float camera_distance) {
  const int edge = terrain_lod_surface_edge(camera_distance);
  Node* node = seed_dem_surface_into_world(world, dem, name, edge);
  if (node) {
    stamp_payload_meta(world, node->id, terrain_lod_cache_key(camera_distance),
                       TerrainSource::kSurface);
  }
  return node;
}

Node* seed_tin_into_world(World* world, const OGRTriangulatedSurface* tin,
                          const char* name) {
  return seed_tin_lod_into_world(world, tin, name, /*camera_distance=*/0.f);
}

Node* seed_tin_lod_into_world(World* world, const OGRTriangulatedSurface* tin,
                              const char* name, float camera_distance) {
  if (!world || !tin || tin->IsEmpty()) {
    return nullptr;
  }
  TessMesh full;
  // Keep Z so TIN elevations survive into TerrainPayload.
  if (!tessellate_3d_surface(tin, full) || full.indices.size() < 3) {
    return nullptr;
  }
  const int stride = terrain_lod_tin_stride(camera_distance);
  TessMesh thinned;
  if (stride <= 1) {
    thinned = std::move(full);
  } else if (!thin_tess_mesh(full, stride, &thinned)) {
    return nullptr;
  }
  const int lod_key = terrain_lod_tin_cache_key(camera_distance);
  return attach_payload_mesh(world, name, thinned.positions, thinned.indices,
                             lod_key, TerrainSource::kTin);
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
