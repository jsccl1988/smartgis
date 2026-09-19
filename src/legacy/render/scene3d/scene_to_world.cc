// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/scene_to_world.h"

#include "gis/world/dem_frame.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace render {
namespace {

gis::World* g_scene_world_mirror = nullptr;

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

void leftover_yup_to_gis(double x0, double elev0, double lat0, double x1,
                         double elev1, double lat1, double* min_x,
                         double* min_y, double* min_z, double* max_x,
                         double* max_y, double* max_z) {
  // Mesh / AABB X is -lon; recover geographic lon for World envelope.
  const double lon0 = gis::dem_x_to_lon(static_cast<float>(x0));
  const double lon1 = gis::dem_x_to_lon(static_cast<float>(x1));
  if (min_x) {
    *min_x = (std::min)(lon0, lon1);
  }
  if (max_x) {
    *max_x = (std::max)(lon0, lon1);
  }
  if (min_y) {
    *min_y = (std::min)(lat0, lat1);
  }
  if (max_y) {
    *max_y = (std::max)(lat0, lat1);
  }
  if (min_z) {
    *min_z = (std::min)(elev0, elev1);
  }
  if (max_z) {
    *max_z = (std::max)(elev0, elev1);
  }
}

void leftover_aabb_to_gis(const Aabb& aabb, double* min_x, double* min_y,
                          double* min_z, double* max_x, double* max_y,
                          double* max_z) {
  leftover_yup_to_gis(static_cast<double>(aabb.vcMin.x),
                      static_cast<double>(aabb.vcMin.y),
                      static_cast<double>(aabb.vcMin.z),
                      static_cast<double>(aabb.vcMax.x),
                      static_cast<double>(aabb.vcMax.y),
                      static_cast<double>(aabb.vcMax.z), min_x, min_y, min_z,
                      max_x, max_y, max_z);
}

gis::Node* attach_gis_aabb(gis::World* world, const char* name, double min_x,
                           double min_y, double min_z, double max_x,
                           double max_y, double max_z) {
  if (!world) {
    return nullptr;
  }
  return world->add_node(gis::NodeKind::kEmpty, name, min_x, min_y, min_z,
                         max_x, max_y, max_z);
}

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
    leftover_aabb_to_gis(aabb, &min_x, &min_y, &min_z, &max_x, &max_y, &max_z);
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

gis::World* smt_scene_world_mirror() {
  return g_scene_world_mirror;
}

}  // namespace render
