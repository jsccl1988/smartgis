// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/frame/terrain_mesh.h"

#include "content/browser/document/map_scene.h"
#include "gis/vista/world/terrain/dem_raster.h"
#include "gis/vista/world/terrain/land_mask.h"

#include <cstdint>
#include <vector>

namespace content {

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
  const int next_lod = gis::DemRaster::lod_max_edge(orbit_distance);
  if (!xyz->empty() && !idx->empty() && *lod_edge == next_lod &&
      geo->matches_extent(extent)) {
    return;
  }
  while (world->node_count() > 0) {
    const gis::Node* n = world->node_at(0);
    if (!n || !world->remove_node(n->id)) {
      break;
    }
  }
  xyz->clear();
  idx->clear();
  *geo = OrbitGeoFrame::from_extent(extent);
  std::vector<gis::LonLatRing> rings;
  if (scene) {
    scene->export_land_rings(&rings);
  }
  gis::Node* node = gis::seed_china_dem_into_world(
      world, rings.empty() ? nullptr : rings.data(), rings.size(), "views_dem",
      next_lod);
  if (node && node->has_terrain_mesh()) {
    *xyz = node->terrain_positions;
    idx->assign(node->terrain_indices.begin(), node->terrain_indices.end());
    geo->capture_elev_center(*xyz);
    geo->normalize_xyz(xyz);
    std::vector<uint32_t> mesh_idx(idx->begin(), idx->end());
    world->set_terrain_mesh(node->id, xyz->data(), xyz->size(), mesh_idx.data(),
                            mesh_idx.size());
    *lod_edge = next_lod;
  }
}

}  // namespace content
