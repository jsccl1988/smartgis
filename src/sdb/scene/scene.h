// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_SCENE_SCENE_H_
#define SDB_SCENE_SCENE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "sdb/model/model.h"
#include "sdb/model/tileset.h"

// Logical GIS world. Spatial query lives here; GPU instances live in render.

class OGRGeometry;
class OGRLayer;

namespace geo {
class Grid;
class Tin;
}

namespace sdb {
class SmtMap;
class SmtLayer;
class SmtRasterLayer;
class SmtTileLayer;
}


namespace sdb {
namespace scene {

enum class NodeKind {
  kEmpty,
  kVectorLayer,
  kRasterLayer,
  kModel,
  kTileset,
  kTerrain,
  kPointCloud,
};

struct Node {
  uint64_t id;
  NodeKind kind;
  uint64_t generation;
  double min_x;
  double min_y;
  double min_z;
  double max_x;
  double max_y;
  double max_z;
  std::string name;
  const sdb::SmtLayer* layer;
  OGRLayer* ogr_layer;
  const OGRGeometry* geom_3d;
  std::vector<const OGRGeometry*> geoms;
  const geo::Tin* tin;
  const geo::Grid* grid;
  const sdb::model::ModelAsset* model;
  const sdb::model::Tileset* tileset;
  std::vector<std::string> visible_uris;

  Node()
      : id(0),
        kind(NodeKind::kEmpty),
        generation(0),
        min_x(0),
        min_y(0),
        min_z(0),
        max_x(0),
        max_y(0),
        max_z(0),
        layer(nullptr),
        ogr_layer(nullptr),
        geom_3d(nullptr),
        tin(nullptr),
        grid(nullptr),
        model(nullptr),
        tileset(nullptr) {}
};

class World {
 public:
  World();

  uint64_t generation() const { return generation_; }
  Node* add_node(NodeKind kind, const char* name, double min_x, double min_y,
                 double min_z, double max_x, double max_y, double max_z);
  bool remove_node(uint64_t id);
  Node* find(uint64_t id);
  size_t node_count() const { return nodes_.size(); }
  const Node* node_at(size_t index) const;

  void attach_map(const sdb::SmtMap* map);
  Node* attach_vector_geoms(const char* name, const OGRGeometry* const* geoms,
                            size_t count);
  Node* attach_3d_geometry(const OGRGeometry* geom, const char* name);
  Node* attach_tin(const geo::Tin* tin, const char* name);
  Node* attach_grid(const geo::Grid* grid, const char* name);
  Node* attach_raster_layer(const sdb::SmtRasterLayer* layer);
  Node* attach_tile_layer(const sdb::SmtTileLayer* layer);
  Node* attach_model(const sdb::model::ModelAsset* asset, const char* name);
  Node* attach_tileset(const sdb::model::Tileset* tileset, const char* name);
  Node* attach_terrain(const char* name, double min_x, double min_y,
                       double min_z, double max_x, double max_y, double max_z);
  Node* attach_pointcloud(const char* name, double min_x, double min_y,
                          double min_z, double max_x, double max_y,
                          double max_z);
  bool apply_tileset_selection(uint64_t id,
                               const std::vector<const sdb::model::Tile*>& visible);

  void query_aabb(double min_x, double min_y, double min_z, double max_x,
                  double max_y, double max_z,
                  std::vector<const Node*>& hits) const;

 private:
  uint64_t generation_;
  uint64_t next_id_;
  std::vector<Node> nodes_;
};

}  // namespace scene
}  // namespace sdb

#endif  // SDB_SCENE_SCENE_H_
