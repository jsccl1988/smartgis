// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_WORLD_SCENE_H_
#define GIS_WORLD_SCENE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "gis/gis_export.h"
#include "gis/assets/model.h"
#include "gis/assets/tileset.h"

// Logical GIS world. Spatial query lives here; GPU instances live in render.

class OGRGeometry;
class OGRLayer;

namespace geo {
class Grid;
class Tin;
}  // namespace geo

namespace gis {

class SmtMap;
class SmtLayer;
class SmtRasterLayer;
class SmtTileLayer;

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
  const gis::SmtLayer* layer;
  OGRLayer* ogr_layer;
  const OGRGeometry* geom_3d;
  std::vector<const OGRGeometry*> geoms;
  const geo::Tin* tin;
  const geo::Grid* grid;
  const gis::ModelAsset* model;
  const gis::Tileset* tileset;
  std::vector<std::string> visible_uris;
  // Optional CPU terrain mesh (SP4). Layout matches DemHeightField::build_mesh:
  // leftover Y-up XYZ (X=-lon, elev, lat) + triangle indices. Empty = AABB-only.
  std::vector<float> terrain_positions;
  std::vector<uint32_t> terrain_indices;

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

  bool has_terrain_mesh() const {
    return terrain_positions.size() >= 9 &&
           (terrain_positions.size() % 3) == 0 &&
           terrain_indices.size() >= 3 && (terrain_indices.size() % 3) == 0;
  }
};

class GIS_EXPORT World {
 public:
  World();

  uint64_t generation() const { return generation_; }
  Node* add_node(NodeKind kind, const char* name, double min_x, double min_y,
                 double min_z, double max_x, double max_y, double max_z);
  bool remove_node(uint64_t id);
  Node* find(uint64_t id);
  size_t node_count() const { return nodes_.size(); }
  const Node* node_at(size_t index) const;

  void attach_map(const gis::SmtMap* map);
  Node* attach_vector_geoms(const char* name, const OGRGeometry* const* geoms,
                            size_t count);
  Node* attach_3d_geometry(const OGRGeometry* geom, const char* name);
  Node* attach_tin(const geo::Tin* tin, const char* name);
  Node* attach_grid(const geo::Grid* grid, const char* name);
  Node* attach_raster_layer(const gis::SmtRasterLayer* layer);
  Node* attach_tile_layer(const gis::SmtTileLayer* layer);
  Node* attach_model(const gis::ModelAsset* asset, const char* name);
  Node* attach_tileset(const gis::Tileset* tileset, const char* name);
  Node* attach_terrain(const char* name, double min_x, double min_y,
                       double min_z, double max_x, double max_y, double max_z);
  // Attach / replace CPU terrain triangles on a kTerrain node (SP4 mesh seam).
  // |positions| is interleaved XYZ (count floats); |indices| triangle list.
  bool set_terrain_mesh(uint64_t id, const float* positions,
                        size_t position_count, const uint32_t* indices,
                        size_t index_count);
  Node* attach_pointcloud(const char* name, double min_x, double min_y,
                          double min_z, double max_x, double max_y,
                          double max_z);
  bool apply_tileset_selection(
      uint64_t id, const std::vector<const gis::Tile*>& visible);

  void query_aabb(double min_x, double min_y, double min_z, double max_x,
                  double max_y, double max_z,
                  std::vector<const Node*>& hits) const;

 private:
  uint64_t generation_;
  uint64_t next_id_;
  std::vector<Node> nodes_;
};

}  // namespace gis

#endif  // GIS_WORLD_SCENE_H_
