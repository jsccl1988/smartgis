// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/scene/scene.h"

#include "3dgeometry.h"
#include "geometry.h"
#include "layer.h"
#include "map.h"

#include "ogrsf_frmts.h"

namespace sdb {
namespace scene {
namespace {

bool aabb_overlap(double a_min_x, double a_min_y, double a_min_z, double a_max_x,
                  double a_max_y, double a_max_z, double b_min_x, double b_min_y,
                  double b_min_z, double b_max_x, double b_max_y,
                  double b_max_z) {
  return !(a_max_x < b_min_x || b_max_x < a_min_x || a_max_y < b_min_y ||
           b_max_y < a_min_y || a_max_z < b_min_z || b_max_z < a_min_z);
}

}  // namespace

World::World() : generation_(1), next_id_(1) {}

Node* World::add_node(NodeKind kind, const char* name, double min_x, double min_y,
                      double min_z, double max_x, double max_y, double max_z) {
  Node node;
  node.id = next_id_++;
  node.kind = kind;
  node.min_x = min_x;
  node.min_y = min_y;
  node.min_z = min_z;
  node.max_x = max_x;
  node.max_y = max_y;
  node.max_z = max_z;
  if (name) {
    node.name = name;
  }
  node.layer = nullptr;
  node.geom_3d = nullptr;
  ++generation_;
  node.generation = generation_;
  nodes_.push_back(node);
  return &nodes_.back();
}

bool World::remove_node(uint64_t id) {
  for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
    if (it->id == id) {
      nodes_.erase(it);
      ++generation_;
      return true;
    }
  }
  return false;
}

Node* World::find(uint64_t id) {
  for (Node& node : nodes_) {
    if (node.id == id) {
      return &node;
    }
  }
  return nullptr;
}

const Node* World::node_at(size_t index) const {
  if (index >= nodes_.size()) {
    return nullptr;
  }
  return &nodes_[index];
}

void World::attach_map(const Smt_GIS::SmtMap* map) {
  if (!map) {
    return;
  }
  std::vector<Node> kept;
  for (const Node& node : nodes_) {
    if (node.kind != NodeKind::kVectorLayer &&
        node.kind != NodeKind::kRasterLayer) {
      kept.push_back(node);
    }
  }
  nodes_.swap(kept);
  Smt_GIS::SmtMap* walk = const_cast<Smt_GIS::SmtMap*>(map);
  const int count = walk->GetLayerCount();
  for (int i = 0; i < count; ++i) {
    if (OGRLayer* ogr = const_cast<OGRLayer*>(map->GetOgrLayer(i))) {
      OGREnvelope ogr_env;
      Smt_Base::Envelope env;
      if (ogr->GetExtent(&ogr_env, TRUE) == OGRERR_NONE) {
        env.MinX = ogr_env.MinX;
        env.MinY = ogr_env.MinY;
        env.MaxX = ogr_env.MaxX;
        env.MaxY = ogr_env.MaxY;
      }
      Node* node = add_node(NodeKind::kVectorLayer, ogr->GetName(), env.MinX,
                            env.MinY, 0, env.MaxX, env.MaxY, 0);
      if (node) {
        node->ogr_layer = ogr;
      }
      continue;
    }
    const Smt_GIS::SmtLayer* layer = map->GetLeftoverLayer(i);
    if (!layer) {
      continue;
    }
    Smt_Base::Envelope env;
    layer->GetEnvelope(env);
    NodeKind kind = NodeKind::kRasterLayer;
    Node* node = add_node(kind, layer->GetLayerName(), env.MinX, env.MinY, 0,
                          env.MaxX, env.MaxY, 0);
    if (node) {
      node->layer = layer;
    }
  }
}

Node* World::attach_vector_geoms(const char* name,
                                 const Smt_Geo::SmtGeometry* const* geoms,
                                 size_t count) {
  if (!geoms || count == 0) {
    return nullptr;
  }
  double min_x = 0;
  double min_y = 0;
  double max_x = 0;
  double max_y = 0;
  bool have_env = false;
  for (size_t i = 0; i < count; ++i) {
    if (!geoms[i]) {
      continue;
    }
    Smt_Base::Envelope env;
    geoms[i]->GetEnvelope(&env);
    if (!have_env) {
      min_x = env.MinX;
      min_y = env.MinY;
      max_x = env.MaxX;
      max_y = env.MaxY;
      have_env = true;
    } else {
      if (env.MinX < min_x) min_x = env.MinX;
      if (env.MinY < min_y) min_y = env.MinY;
      if (env.MaxX > max_x) max_x = env.MaxX;
      if (env.MaxY > max_y) max_y = env.MaxY;
    }
  }
  Node* node =
      add_node(NodeKind::kVectorLayer, name, min_x, min_y, 0, max_x, max_y, 0);
  if (node) {
    node->geoms.assign(geoms, geoms + count);
  }
  return node;
}

Node* World::attach_3d_geometry(const Smt_3DGeo::Smt3DGeometry* geom,
                                const char* name) {
  if (!geom) {
    return nullptr;
  }
  Smt_3DMath::Aabb box;
  geom->GetAabb(&box);
  Node* node =
      add_node(NodeKind::kModel, name, box.vcMin.x, box.vcMin.y, box.vcMin.z,
               box.vcMax.x, box.vcMax.y, box.vcMax.z);
  if (node) {
    node->geom_3d = geom;
  }
  return node;
}

void World::query_aabb(double min_x, double min_y, double min_z, double max_x,
                       double max_y, double max_z,
                       std::vector<const Node*>& hits) const {
  hits.clear();
  for (const Node& node : nodes_) {
    if (aabb_overlap(min_x, min_y, min_z, max_x, max_y, max_z, node.min_x,
                     node.min_y, node.min_z, node.max_x, node.max_y,
                     node.max_z)) {
      hits.push_back(&node);
    }
  }
}

}  // namespace scene
}  // namespace sdb
