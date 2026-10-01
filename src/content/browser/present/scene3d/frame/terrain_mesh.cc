// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/frame/terrain_mesh.h"

#include "content/browser/camera/map_host_extent.h"
#include "content/browser/document/map_scene.h"
#include "gis/vista/world/terrain/dem/dem_raster.h"
#include "gis/vista/world/terrain/process/land_mask.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace content {
namespace {

// Seed DEM mesh for the active lon/lat frame.
// Regional windows (plugin world3d AOI, coast pads) must crop the raster to
// |frame|. Seeding full-China DEM into a tight OrbitGeoFrame overscales the
// mesh (±12 orbit units vs camera span 3.2) and paints black silhouettes.
gis::Node* seed_view_or_china(gis::World* world, const Extent2& frame,
                              float orbit_distance, int next_lod) {
  gis::DemRaster dem;
  const std::string path = gis::find_sample_dem_path();
  if (path.empty() || !dem.load_gdal_raster(path.c_str())) {
    dem.fill_synthetic_china();
  }
  const double span_lon = frame.xmax - frame.xmin;
  const double span_lat = frame.ymax - frame.ymin;
  const bool regional =
      span_lon < 50.0 || span_lat < 30.0;  // full China is ~62° × 36°
  // Close orbit → multi-tile; any regional frame → crop even when far.
  if (orbit_distance < 2.4f || regional) {
    const size_t tiles = gis::seed_dem_view_tiles_into_world(
        world, dem, frame.xmin, frame.ymin, frame.xmax, frame.ymax,
        orbit_distance, 65536, "views_dem");
    if (tiles > 0) {
      for (size_t i = 0; i < world->node_count(); ++i) {
        gis::Node* n = world->find(world->node_at(i)->id);
        if (n && n->kind == gis::NodeKind::kTerrain && n->has_terrain_mesh()) {
          return n;
        }
      }
    }
  }
  return gis::seed_china_dem_into_world(world, nullptr, 0, "views_dem",
                                        next_lod);
}

}  // namespace

void rebuild_terrain_mesh(gis::World* world,
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
  // DEM is the China raster. Non-China extents shrink it into a cyan sticker
  // on a huge ocean — always frame with a lon/lat box inside China.
  const Extent2 frame =
      extent_looks_like_china(extent) ? extent : kChinaLonLatExtent;
  const int next_lod = gis::DemRaster::lod_max_edge(orbit_distance);
  const int grid_key = orbit_distance < 2.4f ? 2 : 1;
  const int cache_key = next_lod * 10 + grid_key;
  if (!xyz->empty() && !idx->empty() && *lod_edge == cache_key &&
      geo->matches_extent(frame)) {
    return;
  }
  while (true) {
    bool removed = false;
    for (size_t i = 0; i < world->node_count(); ++i) {
      const gis::Node* n = world->node_at(i);
      if (n && n->kind == gis::NodeKind::kTerrain) {
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
  xyz->clear();
  idx->clear();
  *geo = OrbitGeoFrame::from_extent(frame);
  (void)scene;
  gis::Node* node = seed_view_or_china(world, frame, orbit_distance, next_lod);
  if (!node || !node->has_terrain_mesh()) {
    return;
  }

  // Prefer concatenating all terrain tiles into the paint buffer.
  xyz->clear();
  idx->clear();
  std::vector<uint64_t> ids;
  for (size_t i = 0; i < world->node_count(); ++i) {
    const gis::Node* n = world->node_at(i);
    if (n && n->kind == gis::NodeKind::kTerrain && n->has_terrain_mesh()) {
      ids.push_back(n->id);
    }
  }
  if (ids.empty()) {
    return;
  }
  // Elev center from first tile; normalize each tile independently then merge.
  {
    gis::Node* first = world->find(ids[0]);
    if (first) {
      geo->capture_elev_center(first->terrain_positions);
    }
  }
  for (uint64_t id : ids) {
    gis::Node* n = world->find(id);
    if (!n || !n->has_terrain_mesh()) {
      continue;
    }
    std::vector<float> local = n->terrain_positions;
    std::vector<uint32_t> local_idx = n->terrain_indices;
    std::vector<float> uvs = n->terrain_uvs;
    std::vector<uint8_t> rgba = n->terrain_rgba;
    const uint32_t tw = n->terrain_tex_w;
    const uint32_t th = n->terrain_tex_h;
    geo->normalize_xyz(&local);
    if (local.size() >= 3) {
      float mn_x = local[0], mn_y = local[1], mn_z = local[2];
      float mx_x = mn_x, mx_y = mn_y, mx_z = mn_z;
      for (size_t i = 0; i + 2 < local.size(); i += 3) {
        mn_x = (std::min)(mn_x, local[i]);
        mn_y = (std::min)(mn_y, local[i + 1]);
        mn_z = (std::min)(mn_z, local[i + 2]);
        mx_x = (std::max)(mx_x, local[i]);
        mx_y = (std::max)(mx_y, local[i + 1]);
        mx_z = (std::max)(mx_z, local[i + 2]);
      }
      n->min_x = mn_x;
      n->min_y = mn_y;
      n->min_z = mn_z;
      n->max_x = mx_x;
      n->max_y = mx_y;
      n->max_z = mx_z;
    }
    world->set_terrain_mesh(n->id, local.data(), local.size(), local_idx.data(),
                            local_idx.size());
    if (!uvs.empty() && uvs.size() == (local.size() / 3) * 2) {
      world->set_terrain_uvs(n->id, uvs.data(), uvs.size());
    }
    if (!rgba.empty() && tw > 0 && th > 0) {
      world->set_terrain_texture(n->id, rgba.data(), rgba.size(), tw, th);
    }
    const size_t base = xyz->size() / 3;
    xyz->insert(xyz->end(), local.begin(), local.end());
    for (uint32_t tri : local_idx) {
      idx->push_back(static_cast<unsigned>(base + tri));
    }
  }
  if (!xyz->empty() && !idx->empty()) {
    *lod_edge = cache_key;
  }
}

}  // namespace content
