// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/scene/scene.h"

#include "algorithm/geo/geometry.h"
#include "sdb/layer/layer.h"
#include "sdb/map/map.h"

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
  node.ogr_layer = nullptr;
  node.geom_3d = nullptr;
  node.tin = nullptr;
  node.grid = nullptr;
  node.model = nullptr;
  node.tileset = nullptr;
  node.visible_uris.clear();
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

void World::attach_map(const sdb::SmtMap* map) {
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
  sdb::SmtMap* walk = const_cast<sdb::SmtMap*>(map);
  const int count = walk->GetLayerCount();
  for (int i = 0; i < count; ++i) {
    if (OGRLayer* ogr = const_cast<OGRLayer*>(map->GetOgrLayer(i))) {
      OGREnvelope ogr_env;
      base::Envelope env;
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
    const sdb::SmtLayer* layer = map->GetLeftoverLayer(i);
    if (!layer) {
      continue;
    }
    base::Envelope env;
    layer->get_envelope(env);
    NodeKind kind = NodeKind::kRasterLayer;
    Node* node = add_node(kind, layer->GetLayerName(), env.MinX, env.MinY, 0,
                          env.MaxX, env.MaxY, 0);
    if (node) {
      node->layer = layer;
    }
  }
}

Node* World::attach_vector_geoms(const char* name,
                                 const OGRGeometry* const* geoms,
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
    base::Envelope env;
    geo::copy_envelope(*geoms[i], &env);
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

Node* World::attach_3d_geometry(const OGRGeometry* geom, const char* name) {
  if (!geom) {
    return nullptr;
  }
  OGREnvelope env;
  geom->getEnvelope(&env);
  Node* node = add_node(NodeKind::kModel, name, env.MinX, env.MinY, 0, env.MaxX,
                        env.MaxY, 0);
  if (node) {
    node->geom_3d = geom;
  }
  return node;
}

Node* World::attach_tin(const geo::Tin* tin, const char* name) {
  if (!tin || tin->is_empty()) {
    return nullptr;
  }
  base::Envelope env;
  tin->get_envelope(&env);
  Node* node =
      add_node(NodeKind::kVectorLayer, name, env.MinX, env.MinY, 0, env.MaxX,
               env.MaxY, 0);
  if (node) {
    node->tin = tin;
  }
  return node;
}

Node* World::attach_grid(const geo::Grid* grid, const char* name) {
  if (!grid || grid->is_empty()) {
    return nullptr;
  }
  base::Envelope env;
  grid->get_envelope(&env);
  Node* node =
      add_node(NodeKind::kVectorLayer, name, env.MinX, env.MinY, 0, env.MaxX,
               env.MaxY, 0);
  if (node) {
    node->grid = grid;
  }
  return node;
}

Node* World::attach_raster_layer(const sdb::SmtRasterLayer* layer) {
  if (!layer) {
    return nullptr;
  }
  base::fRect rect;
  base::Envelope env;
  if (layer->GetRasterRect(rect) == SMT_ERR_NONE && rect.width() > 0 &&
      rect.height() > 0) {
    env.MinX = rect.lb.x;
    env.MinY = rect.lb.y;
    env.MaxX = rect.rt.x;
    env.MaxY = rect.rt.y;
  } else {
    layer->get_envelope(env);
  }
  Node* node = add_node(NodeKind::kRasterLayer, layer->GetLayerName(), env.MinX,
                        env.MinY, 0, env.MaxX, env.MaxY, 0);
  if (node) {
    node->layer = layer;
  }
  return node;
}

Node* World::attach_tile_layer(const sdb::SmtTileLayer* layer) {
  if (!layer) {
    return nullptr;
  }
  base::Envelope env;
  layer->get_envelope(env);
  const int n = layer->GetTileCount();
  for (int i = 0; i < n; ++i) {
    const base::SmtTile* tile = layer->GetTile(i);
    if (!tile) {
      continue;
    }
    env.merge(tile->rtTileRect.lb.x, tile->rtTileRect.lb.y);
    env.merge(tile->rtTileRect.rt.x, tile->rtTileRect.rt.y);
  }
  Node* node = add_node(NodeKind::kRasterLayer, layer->GetLayerName(), env.MinX,
                        env.MinY, 0, env.MaxX, env.MaxY, 0);
  if (node) {
    node->layer = layer;
  }
  return node;
}

Node* World::attach_model(const sdb::model::ModelAsset* asset,
                          const char* name) {
  if (!asset) {
    return nullptr;
  }
  double min_x = 0;
  double min_y = 0;
  double min_z = 0;
  double max_x = 0;
  double max_y = 0;
  double max_z = 0;
  if (!sdb::model::model_aabb(*asset, &min_x, &min_y, &min_z, &max_x, &max_y,
                              &max_z)) {
    return nullptr;
  }
  Node* node =
      add_node(NodeKind::kModel, name, min_x, min_y, min_z, max_x, max_y, max_z);
  if (node) {
    node->model = asset;
  }
  return node;
}

Node* World::attach_tileset(const sdb::model::Tileset* tileset,
                            const char* name) {
  if (!tileset) {
    return nullptr;
  }
  const sdb::model::Tile& root = tileset->root;
  Node* node = add_node(NodeKind::kTileset, name, root.min_x, root.min_y,
                        root.min_z, root.max_x, root.max_y, root.max_z);
  if (node) {
    node->tileset = tileset;
  }
  return node;
}

Node* World::attach_terrain(const char* name, double min_x, double min_y,
                            double min_z, double max_x, double max_y,
                            double max_z) {
  return add_node(NodeKind::kTerrain, name, min_x, min_y, min_z, max_x, max_y,
                  max_z);
}

Node* World::attach_pointcloud(const char* name, double min_x, double min_y,
                               double min_z, double max_x, double max_y,
                               double max_z) {
  return add_node(NodeKind::kPointCloud, name, min_x, min_y, min_z, max_x,
                  max_y, max_z);
}

bool World::apply_tileset_selection(
    uint64_t id, const std::vector<const sdb::model::Tile*>& visible) {
  Node* node = find(id);
  if (!node || node->kind != NodeKind::kTileset) {
    return false;
  }
  std::vector<std::string> uris;
  uris.reserve(visible.size());
  for (const sdb::model::Tile* tile : visible) {
    if (tile) {
      uris.push_back(tile->content_uri);
    }
  }
  if (uris == node->visible_uris) {
    return false;
  }
  node->visible_uris.swap(uris);
  ++generation_;
  node->generation = generation_;
  return true;
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
