// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/dem_to_world.h"

#include <cstdint>
#include <vector>

namespace render {

gis::Node* seed_dem_height_field_into_world(gis::World* world,
                                            const DemHeightField& dem,
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
  gis::Node* node = world->attach_terrain(node_name, min_x, min_y, min_z,
                                          max_x, max_y, max_z);
  if (!node) {
    return nullptr;
  }
  const int edge = max_edge > 1 ? max_edge : 96;
  std::vector<float> xyz;
  std::vector<unsigned> idx;
  if (!dem.build_mesh(edge, &xyz, &idx, nullptr, nullptr) || xyz.empty() ||
      idx.empty()) {
    return node;
  }
  std::vector<uint32_t> indices(idx.begin(), idx.end());
  world->set_terrain_mesh(node->id, xyz.data(), xyz.size(), indices.data(),
                          indices.size());
  return world->find(node->id);
}

}  // namespace render
