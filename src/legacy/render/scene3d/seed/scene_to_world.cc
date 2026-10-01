// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/seed/scene_to_world.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "gis/vista/world/terrain/dem/dem_frame.h"
#include "legacy/gis/vista/coord.h"

namespace render {
namespace {

gis::World* g_scene_world_mirror = nullptr;

void leftover_aabb_to_gis_local(const Aabb& aabb, double* min_x, double* min_y,
                                double* min_z, double* max_x, double* max_y,
                                double* max_z) {
  // Inline of vista/coord leftover_aabb_to_gis — avoids GIS_EXPORT dllimport
  // when gis.dll export table is mid-migration.
  const double lon0 = gis::dem_x_to_lon(aabb.vcMin.x);
  const double lon1 = gis::dem_x_to_lon(aabb.vcMax.x);
  if (min_x) {
    *min_x = (std::min)(lon0, lon1);
  }
  if (max_x) {
    *max_x = (std::max)(lon0, lon1);
  }
  if (min_y) {
    *min_y = (std::min)(static_cast<double>(aabb.vcMin.z),
                        static_cast<double>(aabb.vcMax.z));
  }
  if (max_y) {
    *max_y = (std::max)(static_cast<double>(aabb.vcMin.z),
                        static_cast<double>(aabb.vcMax.z));
  }
  if (min_z) {
    *min_z = (std::min)(static_cast<double>(aabb.vcMin.y),
                        static_cast<double>(aabb.vcMax.y));
  }
  if (max_z) {
    *max_z = (std::max)(static_cast<double>(aabb.vcMin.y),
                        static_cast<double>(aabb.vcMax.y));
  }
}

void remove_empty_mirror_nodes(gis::World* world) {
  if (!world) {
    return;
  }
  // Collect ids first — remove_node invalidates indices.
  std::vector<uint64_t> drop;
  for (size_t i = 0; i < world->node_count(); ++i) {
    const gis::Node* n = world->node_at(i);
    if (n && n->kind == gis::NodeKind::kEmpty) {
      drop.push_back(n->id);
    }
  }
  for (uint64_t id : drop) {
    world->remove_node(id);
  }
}

}  // namespace

size_t seed_smt_scene_aabbs_into_world(gis::World* world,
                                       const SmtScene* scene) {
  if (!world || !scene) {
    return 0;
  }
  remove_empty_mirror_nodes(world);
  size_t added = 0;
  vSmt3DObjectPtrs objs;
  // const_cast: Get3DObjectPtrs is non-const on leftover ABI.
  const_cast<SmtScene*>(scene)->Get3DObjectPtrs(objs);
  for (size_t i = 0; i < objs.size(); ++i) {
    Smt3DObject* obj = objs[i];
    if (!obj) {
      continue;
    }
    const Aabb& aabb = obj->GetAabb();
    if (!aabb.is_init()) {
      continue;
    }
    double min_x = 0;
    double min_y = 0;
    double min_z = 0;
    double max_x = 0;
    double max_y = 0;
    double max_z = 0;
    leftover_aabb_to_gis_local(aabb, &min_x, &min_y, &min_z, &max_x, &max_y,
                               &max_z);
    char name[64];
    std::snprintf(name, sizeof(name), "scene_obj_%zu", i);
    if (attach_gis_aabb(world, name, min_x, min_y, min_z, max_x, max_y,
                        max_z)) {
      ++added;
    }
  }
  return added;
}

void set_smt_scene_world_mirror(gis::World* world) {
  g_scene_world_mirror = world;
}

gis::World* smt_scene_world_mirror() { return g_scene_world_mirror; }

}  // namespace render
