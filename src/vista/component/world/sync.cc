// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/sync.h"

namespace vista {
namespace detail {

void copy_world_instances(const vista::World& world,
                          std::vector<Instance>* out) {
  if (!out) {
    return;
  }
  std::vector<Instance> next;
  const size_t n = world.node_count();
  next.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    const vista::Node* node = world.node_at(i);
    if (!node) {
      continue;
    }
    Instance inst;
    inst.node_id = node->id;
    inst.kind = node->kind;
    inst.min_x = node->min_x;
    inst.min_y = node->min_y;
    inst.min_z = node->min_z;
    inst.max_x = node->max_x;
    inst.max_y = node->max_y;
    inst.max_z = node->max_z;
    inst.layer = node->map_layer;
    inst.ogr_layer = node->ogr_layer;
    inst.geom_3d = node->geom_3d;
    inst.geoms = node->geoms;
    inst.tin = node->tin;
    inst.grid = node->grid;
    inst.grid_nx = node->grid_nx;
    inst.grid_ny = node->grid_ny;
    inst.model = node->model;
    inst.tileset = node->tileset;
    inst.visible_uris = node->visible_uris;
    inst.terrain_positions = node->terrain_positions;
    inst.terrain_indices = node->terrain_indices;
    inst.terrain_uvs = node->terrain_uvs;
    inst.terrain_rgba = node->terrain_rgba;
    inst.terrain_tex_w = node->terrain_tex_w;
    inst.terrain_tex_h = node->terrain_tex_h;
    inst.point_positions = node->point_positions;
    inst.point_rgba = node->point_rgba;
    inst.point_chunks = node->point_chunks;
    inst.has_paint = false;
    next.push_back(std::move(inst));
  }
  out->swap(next);
}

}  // namespace detail
}  // namespace vista
