// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_SCENE_SCENE_H_
#define SDB_SCENE_SCENE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Logical GIS world. Spatial query lives here; GPU instances live in render.

namespace Smt_Geo {
class SmtGeometry;
}

class OGRLayer;

namespace Smt_GIS {
class SmtMap;
class SmtLayer;
}

namespace Smt_3DGeo {
class Smt3DGeometry;
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
  const Smt_GIS::SmtLayer* layer;
  OGRLayer* ogr_layer;
  const Smt_3DGeo::Smt3DGeometry* geom_3d;
  std::vector<const Smt_Geo::SmtGeometry*> geoms;

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
        geom_3d(nullptr) {}
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

  void attach_map(const Smt_GIS::SmtMap* map);
  Node* attach_vector_geoms(const char* name,
                            const Smt_Geo::SmtGeometry* const* geoms,
                            size_t count);
  Node* attach_3d_geometry(const Smt_3DGeo::Smt3DGeometry* geom,
                           const char* name);

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
