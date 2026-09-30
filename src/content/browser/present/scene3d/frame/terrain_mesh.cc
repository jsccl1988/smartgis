// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/frame/terrain_mesh.h"

#include "content/browser/camera/map_host_extent.h"
#include "content/browser/document/map_scene.h"
#include "gis/vista/world/terrain/dem_raster.h"
#include "gis/vista/world/terrain/land_mask.h"

#include <algorithm>
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
  // DEM is the China raster. Non-China extents shrink it into a cyan sticker
  // on a huge ocean — always frame with a lon/lat box inside China.
  const Extent2 frame =
      extent_looks_like_china(extent) ? extent : kChinaLonLatExtent;
  const int next_lod = gis::DemRaster::lod_max_edge(orbit_distance);
  if (!xyz->empty() && !idx->empty() && *lod_edge == next_lod &&
      geo->matches_extent(frame)) {
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
  *geo = OrbitGeoFrame::from_extent(frame);
  // Do not cut with MapScene prefecture rings — that punched the mainland into
  // a tiny land-only island. china_dem already encodes land/ocean.
  (void)scene;
  gis::Node* node =
      gis::seed_china_dem_into_world(world, nullptr, 0, "views_dem", next_lod);
  if (node && node->has_terrain_mesh()) {
    *xyz = node->terrain_positions;
    idx->assign(node->terrain_indices.begin(), node->terrain_indices.end());
    std::vector<float> uvs = node->terrain_uvs;
    std::vector<uint8_t> rgba = node->terrain_rgba;
    const uint32_t tw = node->terrain_tex_w;
    const uint32_t th = node->terrain_tex_h;
    geo->capture_elev_center(*xyz);
    geo->normalize_xyz(xyz);
    // Orbit-normalized verts live near the origin; keep the World node AABB in
    // the same space so GpuScene frustum cull / debug match the draw mesh.
    if (xyz->size() >= 3) {
      float mn_x = (*xyz)[0];
      float mn_y = (*xyz)[1];
      float mn_z = (*xyz)[2];
      float mx_x = mn_x;
      float mx_y = mn_y;
      float mx_z = mn_z;
      for (size_t i = 0; i + 2 < xyz->size(); i += 3) {
        const float x = (*xyz)[i];
        const float y = (*xyz)[i + 1];
        const float z = (*xyz)[i + 2];
        mn_x = (std::min)(mn_x, x);
        mn_y = (std::min)(mn_y, y);
        mn_z = (std::min)(mn_z, z);
        mx_x = (std::max)(mx_x, x);
        mx_y = (std::max)(mx_y, y);
        mx_z = (std::max)(mx_z, z);
      }
      node->min_x = mn_x;
      node->min_y = mn_y;
      node->min_z = mn_z;
      node->max_x = mx_x;
      node->max_y = mx_y;
      node->max_z = mx_z;
    }
    std::vector<uint32_t> mesh_idx(idx->begin(), idx->end());
    world->set_terrain_mesh(node->id, xyz->data(), xyz->size(), mesh_idx.data(),
                            mesh_idx.size());
    if (!uvs.empty() && uvs.size() == (xyz->size() / 3) * 2) {
      world->set_terrain_uvs(node->id, uvs.data(), uvs.size());
    }
    if (!rgba.empty() && tw > 0 && th > 0) {
      world->set_terrain_texture(node->id, rgba.data(), rgba.size(), tw, th);
    }
    *lod_edge = next_lod;
  }
}

}  // namespace content
