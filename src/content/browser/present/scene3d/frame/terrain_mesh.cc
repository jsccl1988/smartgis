// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/frame/terrain_mesh.h"

#include "content/browser/document/map_scene.h"
#include "vista/terrain/dem/dem_bake_cache.h"
#include "vista/terrain/dem/dem_raster.h"
#include "vista/world/dem_seed.h"
#include "vista/terrain/process/land_mask.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace content {
namespace {

void clear_terrain_nodes(vista::World* world) {
  while (true) {
    bool removed = false;
    for (size_t i = 0; i < world->node_count(); ++i) {
      const vista::Node* n = world->node_at(i);
      if (n && n->kind == vista::NodeKind::kTerrain) {
        if (world->remove_node(n->id)) {
          removed = true;
          break;
        }
      }
    }
    if (!removed) {
      break;
    }
  }
}

bool attach_view_seed(vista::World* world, const vista::DemViewSeed& seed,
                      const char* name, std::vector<float>* xyz,
                      std::vector<unsigned>* idx) {
  vista::Node* node =
      world->attach_terrain(name, static_cast<double>(seed.min_x),
                            static_cast<double>(seed.min_y),
                            static_cast<double>(seed.min_z),
                            static_cast<double>(seed.max_x),
                            static_cast<double>(seed.max_y),
                            static_cast<double>(seed.max_z));
  if (!node) {
    return false;
  }
  if (!world->set_terrain_mesh(node->id, seed.xyz.data(), seed.xyz.size(),
                               seed.indices.data(), seed.indices.size())) {
    return false;
  }
  if (!seed.uvs.empty() &&
      seed.uvs.size() == (seed.xyz.size() / 3) * 2) {
    world->set_terrain_uvs(node->id, seed.uvs.data(), seed.uvs.size());
  }
  if (!seed.rgba.empty() && seed.tex_w > 0 && seed.tex_h > 0) {
    world->set_terrain_texture(node->id, seed.rgba.data(), seed.rgba.size(),
                               seed.tex_w, seed.tex_h);
  }
  node->min_x = seed.min_x;
  node->min_y = seed.min_y;
  node->min_z = seed.min_z;
  node->max_x = seed.max_x;
  node->max_y = seed.max_y;
  node->max_z = seed.max_z;
  xyz->assign(seed.xyz.begin(), seed.xyz.end());
  idx->assign(seed.indices.begin(), seed.indices.end());
  return !xyz->empty() && !idx->empty();
}

// Seed DEM mesh for the active lon/lat frame.
// Regional windows (plugin world3d AOI, coast pads) must crop the raster to
// |frame|. Seeding full-China DEM into a tight OrbitGeoFrame overscales the
// mesh (±12 orbit units vs camera span 3.2) and paints black silhouettes.
vista::Node* seed_view_or_china(vista::World* world, const Extent2& frame,
                              float orbit_distance, int next_lod) {
  vista::DemRaster dem;
  const std::string path = vista::find_sample_dem_path();
  if (path.empty() || !dem.load_gdal_raster(path.c_str()) || dem.empty()) {
    // Real-data policy: no synthetic China DEM stand-in.
    return nullptr;
  }
  const double span_lon = frame.xmax - frame.xmin;
  const double span_lat = frame.ymax - frame.ymin;
  const bool regional =
      span_lon < 50.0 || span_lat < 30.0;  // full China is ~62° × 36°
  // Close orbit → multi-tile; any regional frame → crop even when far.
  if (orbit_distance < 2.4f || regional) {
    // Budget matches DemRaster default: one dense china_dem national tile
    // (~512×320) plus headroom for 2×2 regional crops.
    const size_t tiles = vista::seed_dem_view_tiles_into_world(
        world, dem, frame.xmin, frame.ymin, frame.xmax, frame.ymax,
        orbit_distance, 196608, "views_dem");
    if (tiles > 0) {
      for (size_t i = 0; i < world->node_count(); ++i) {
        vista::Node* n = world->find(world->node_at(i)->id);
        if (n && n->kind == vista::NodeKind::kTerrain && n->has_terrain_mesh()) {
          return n;
        }
      }
    }
  }
  return vista::seed_china_dem_into_world(world, nullptr, 0, "views_dem",
                                        next_lod);
}

}  // namespace

void rebuild_terrain_mesh(vista::World* world,
                          const MapScene* scene,
                          const Extent2& extent,
                          float orbit_distance,
                          std::vector<float>* xyz,
                          std::vector<unsigned>* idx,
                          OrbitGeoFrame* geo,
                          int* lod_edge) {
  if (!world || !xyz || !idx || !geo || !lod_edge) {
    return;
  }
  // China-box fallback, LOD edge, and cache key live next to the gis seeds.
  // A set sample_dem_path_override must not force the China box.
  double box_minx = extent.xmin;
  double box_miny = extent.ymin;
  double box_maxx = extent.xmax;
  double box_maxy = extent.ymax;
  vista::dem_seed_lonlat_box(extent.xmin, extent.ymin, extent.xmax, extent.ymax,
                           &box_minx, &box_miny, &box_maxx, &box_maxy);
  Extent2 frame;
  frame.xmin = box_minx;
  frame.ymin = box_miny;
  frame.xmax = box_maxx;
  frame.ymax = box_maxy;
  const int cache_key = vista::dem_seed_cache_key(orbit_distance);
  const int next_lod = vista::DemRaster::lod_max_edge(orbit_distance);
  if (!xyz->empty() && !idx->empty() && *lod_edge == cache_key &&
      geo->matches_extent(frame)) {
    return;
  }

  const std::string dem_path = vista::find_sample_dem_path();
  if (!dem_path.empty()) {
    vista::DemViewSeed seed;
    if (vista::dem_view_seed_cache_try_get(dem_path.c_str(), cache_key,
                                         frame.xmin, frame.ymin, frame.xmax,
                                         frame.ymax, &seed)) {
      clear_terrain_nodes(world);
      xyz->clear();
      idx->clear();
      *geo = OrbitGeoFrame::from_extent(frame);
      geo->cy = seed.elev_cy;
      if (attach_view_seed(world, seed, "views_dem", xyz, idx)) {
        *lod_edge = cache_key;
        // Attribute cold DEM phases as hits (seed blob covers load/tess/hypso).
        vista::note_dem_phase_load(0, /*cache_hit=*/true);
        vista::note_dem_phase_tess(0);
        vista::note_dem_phase_hypso(0, /*cache_hit=*/true);
        return;
      }
    }
  }

  clear_terrain_nodes(world);
  xyz->clear();
  idx->clear();
  *geo = OrbitGeoFrame::from_extent(frame);
  (void)scene;
  vista::Node* node = seed_view_or_china(world, frame, orbit_distance, next_lod);
  if (!node || !node->has_terrain_mesh()) {
    return;
  }

  // Prefer concatenating all terrain tiles into the paint buffer.
  xyz->clear();
  idx->clear();
  std::vector<uint64_t> ids;
  for (size_t i = 0; i < world->node_count(); ++i) {
    const vista::Node* n = world->node_at(i);
    if (n && n->kind == vista::NodeKind::kTerrain && n->has_terrain_mesh()) {
      ids.push_back(n->id);
    }
  }
  if (ids.empty()) {
    return;
  }
  // Elev center from first tile; normalize each tile in place (no deep copies).
  {
    vista::Node* first = world->find(ids[0]);
    if (first) {
      geo->capture_elev_center(first->terrain_positions);
    }
  }
  size_t total_verts = 0;
  size_t total_idx = 0;
  for (uint64_t id : ids) {
    vista::Node* n = world->find(id);
    if (!n || !n->has_terrain_mesh()) {
      continue;
    }
    total_verts += n->terrain_positions.size() / 3;
    total_idx += n->terrain_indices.size();
  }
  xyz->reserve(total_verts * 3);
  idx->reserve(total_idx);
  for (uint64_t id : ids) {
    vista::Node* n = world->find(id);
    if (!n || !n->has_terrain_mesh()) {
      continue;
    }
    geo->normalize_xyz(&n->terrain_positions);
    if (n->terrain_positions.size() >= 3) {
      float mn_x = n->terrain_positions[0];
      float mn_y = n->terrain_positions[1];
      float mn_z = n->terrain_positions[2];
      float mx_x = mn_x;
      float mx_y = mn_y;
      float mx_z = mn_z;
      for (size_t i = 0; i + 2 < n->terrain_positions.size(); i += 3) {
        mn_x = (std::min)(mn_x, n->terrain_positions[i]);
        mn_y = (std::min)(mn_y, n->terrain_positions[i + 1]);
        mn_z = (std::min)(mn_z, n->terrain_positions[i + 2]);
        mx_x = (std::max)(mx_x, n->terrain_positions[i]);
        mx_y = (std::max)(mx_y, n->terrain_positions[i + 1]);
        mx_z = (std::max)(mx_z, n->terrain_positions[i + 2]);
      }
      n->min_x = mn_x;
      n->min_y = mn_y;
      n->min_z = mn_z;
      n->max_x = mx_x;
      n->max_y = mx_y;
      n->max_z = mx_z;
    }
    const size_t base = xyz->size() / 3;
    xyz->insert(xyz->end(), n->terrain_positions.begin(),
                n->terrain_positions.end());
    for (uint32_t tri : n->terrain_indices) {
      idx->push_back(static_cast<unsigned>(base + tri));
    }
  }
  if (!xyz->empty() && !idx->empty()) {
    *lod_edge = cache_key;
    if (!dem_path.empty() && ids.size() == 1) {
      vista::Node* n = world->find(ids[0]);
      if (n && n->has_terrain_mesh()) {
        vista::DemViewSeed seed;
        seed.elev_cy = geo->cy;
        seed.min_x = static_cast<float>(n->min_x);
        seed.min_y = static_cast<float>(n->min_y);
        seed.min_z = static_cast<float>(n->min_z);
        seed.max_x = static_cast<float>(n->max_x);
        seed.max_y = static_cast<float>(n->max_y);
        seed.max_z = static_cast<float>(n->max_z);
        seed.tex_w = n->terrain_tex_w;
        seed.tex_h = n->terrain_tex_h;
        seed.xyz = n->terrain_positions;
        seed.indices = n->terrain_indices;
        seed.uvs = n->terrain_uvs;
        seed.rgba = n->terrain_rgba;
        vista::dem_view_seed_cache_put(dem_path.c_str(), cache_key, frame.xmin,
                                     frame.ymin, frame.xmax, frame.ymax, seed);
      }
    }
  }
}

}  // namespace content
