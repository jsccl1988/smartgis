// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/world.h"

#include <algorithm>
#include <cstring>

#include "vista/component/world/pointcloud/chunk.h"
#include "vista/component/world/pointcloud/lod.h"

#include "gis/geo/ops/geometry_traits.h"
#include "ogrsf_frmts.h"
#include "gis/map/map.h"

namespace vista {
namespace {

bool aabb_overlap(double a_min_x, double a_min_y, double a_min_z,
                  double a_max_x, double a_max_y, double a_max_z,
                  double b_min_x, double b_min_y, double b_min_z,
                  double b_max_x, double b_max_y, double b_max_z) {
  return !(a_max_x < b_min_x || b_max_x < a_min_x || a_max_y < b_min_y ||
           b_max_y < a_min_y || a_max_z < b_min_z || b_max_z < a_min_z);
}

}  // namespace

World::World() : generation_(1), next_id_(1) {}

Node* World::add_node(NodeKind kind, const char* name, double min_x,
                      double min_y, double min_z, double max_x, double max_y,
                      double max_z) {
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
  node.map_layer = nullptr;
  node.ogr_layer = nullptr;
  node.geom_3d = nullptr;
  node.tin = nullptr;
  node.grid = nullptr;
  node.grid_nx = 0;
  node.grid_ny = 0;
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

void World::attach_map(const gis::Map* map) {
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
  gis::Map* walk = const_cast<gis::Map*>(map);
  const int count = walk->layer_count();
  for (int i = 0; i < count; ++i) {
    const gis::MapLayer* slot = map->map_layer(i);
    if (!slot) {
      continue;
    }
    if (OGRLayer* ogr = const_cast<OGRLayer*>(slot->ogr())) {
      OGREnvelope ogr_env;
      gis::Envelope env;
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
    gis::Envelope env;
    slot->get_envelope(&env);
    Node* node =
        add_node(NodeKind::kRasterLayer, slot->name(), env.MinX, env.MinY, 0,
                 env.MaxX, env.MaxY, 0);
    if (node) {
      node->map_layer = slot;
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
    gis::Envelope env;
    geo::fill_envelope(*geoms[i], &env);
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

Node* World::attach_tin(const OGRTriangulatedSurface* tin, const char* name) {
  if (!tin || tin->IsEmpty()) {
    return nullptr;
  }
  gis::Envelope env;
  geo::fill_envelope(*tin, &env);
  Node* node = add_node(NodeKind::kVectorLayer, name, env.MinX, env.MinY, 0,
                        env.MaxX, env.MaxY, 0);
  if (node) {
    node->tin = tin;
  }
  return node;
}

Node* World::attach_grid(const OGRMultiPoint* grid, int nx, int ny,
                         const char* name) {
  if (!grid || grid->IsEmpty() || nx < 1 || ny < 1) {
    return nullptr;
  }
  gis::Envelope env;
  geo::fill_envelope(*grid, &env);
  Node* node = add_node(NodeKind::kVectorLayer, name, env.MinX, env.MinY, 0,
                        env.MaxX, env.MaxY, 0);
  if (node) {
    node->grid = grid;
    node->grid_nx = nx;
    node->grid_ny = ny;
  }
  return node;
}

Node* World::attach_raster_layer(const gis::MapLayer* slot) {
  if (!slot || !slot->raster()) {
    return nullptr;
  }
  gis::Envelope env;
  slot->get_envelope(&env);
  Node* node = add_node(NodeKind::kRasterLayer, slot->name(), env.MinX, env.MinY,
                        0, env.MaxX, env.MaxY, 0);
  if (node) {
    node->map_layer = slot;
  }
  return node;
}

Node* World::attach_tile_layer(const gis::MapLayer* slot) {
  if (!slot || !slot->tile()) {
    return nullptr;
  }
  gis::Envelope env;
  slot->get_envelope(&env);
  Node* node = add_node(NodeKind::kRasterLayer, slot->name(), env.MinX, env.MinY,
                        0, env.MaxX, env.MaxY, 0);
  if (node) {
    node->map_layer = slot;
  }
  return node;
}

Node* World::attach_model(const vista::ModelAsset* asset,
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
  if (!vista::model_aabb(*asset, &min_x, &min_y, &min_z, &max_x, &max_y,
                              &max_z)) {
    return nullptr;
  }
  Node* node = add_node(NodeKind::kModel, name, min_x, min_y, min_z, max_x,
                        max_y, max_z);
  if (node) {
    node->model = asset;
  }
  return node;
}

Node* World::attach_tileset(const vista::Tileset* tileset,
                            const char* name) {
  if (!tileset) {
    return nullptr;
  }
  const vista::Tile& root = tileset->root;
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

bool World::set_terrain_mesh(uint64_t id, const float* positions,
                             size_t position_count, const uint32_t* indices,
                             size_t index_count) {
  Node* node = find(id);
  if (!node || node->kind != NodeKind::kTerrain) {
    return false;
  }
  if (!positions || !indices || position_count < 9 ||
      (position_count % 3) != 0 || index_count < 3 || (index_count % 3) != 0) {
    node->terrain_positions.clear();
    node->terrain_indices.clear();
    node->terrain_uvs.clear();
    ++generation_;
    node->generation = generation_;
    return false;
  }
  node->terrain_positions.assign(positions, positions + position_count);
  node->terrain_indices.assign(indices, indices + index_count);
  // Keep terrain_uvs when vert count still matches; clear on mismatch.
  const size_t verts = position_count / 3;
  if (node->terrain_uvs.size() != verts * 2u) {
    node->terrain_uvs.clear();
  }
  ++generation_;
  node->generation = generation_;
  return true;
}

bool World::set_terrain_uvs(uint64_t id, const float* uvs, size_t float_count) {
  Node* node = find(id);
  if (!node || node->kind != NodeKind::kTerrain) {
    return false;
  }
  const size_t verts = node->terrain_positions.size() / 3;
  if (!uvs || verts == 0 || float_count != verts * 2u) {
    node->terrain_uvs.clear();
    ++generation_;
    node->generation = generation_;
    return false;
  }
  node->terrain_uvs.assign(uvs, uvs + float_count);
  ++generation_;
  node->generation = generation_;
  return true;
}

bool World::set_terrain_texture(uint64_t id, const uint8_t* rgba,
                                size_t byte_count, uint32_t width,
                                uint32_t height) {
  Node* node = find(id);
  if (!node || node->kind != NodeKind::kTerrain) {
    return false;
  }
  const size_t need =
      static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
  if (!rgba || width == 0 || height == 0 || byte_count < need) {
    node->terrain_rgba.clear();
    node->terrain_tex_w = 0;
    node->terrain_tex_h = 0;
    ++generation_;
    node->generation = generation_;
    return false;
  }
  node->terrain_rgba.assign(rgba, rgba + need);
  node->terrain_tex_w = width;
  node->terrain_tex_h = height;
  ++generation_;
  node->generation = generation_;
  return true;
}

Node* World::attach_pointcloud(const char* name, double min_x, double min_y,
                               double min_z, double max_x, double max_y,
                               double max_z) {
  return add_node(NodeKind::kPointCloud, name, min_x, min_y, min_z, max_x,
                  max_y, max_z);
}

bool World::set_pointcloud_points(uint64_t id, const float* xyz,
                                  size_t point_count, const uint8_t* rgba,
                                  size_t rgba_bytes) {
  Node* node = find(id);
  if (!node || node->kind != NodeKind::kPointCloud) {
    return false;
  }
  if (!xyz || point_count == 0) {
    node->point_positions.clear();
    node->point_rgba.clear();
    node->point_chunks.clear();
    ++generation_;
    node->generation = generation_;
    return false;
  }

  PointCloud cloud;
  cloud.xyz.assign(xyz, xyz + point_count * 3);
  if (rgba && rgba_bytes >= point_count * 4) {
    cloud.rgba.assign(rgba, rgba + point_count * 4);
  }
  cloud.recompute_bounds();

  // P2: thin very large clouds before chunking (uniform stride via LOD API).
  constexpr size_t kLodBudget = 500000;
  if (cloud.point_count() > kLodBudget) {
    PointCloudLodOptions lod;
    lod.max_points = kLodBudget;
    lod.focus_radius = 0;
    std::vector<uint32_t> keep;
    if (select_point_cloud_lod(cloud, lod, &keep) && !keep.empty()) {
      PointCloud thinned;
      thinned.xyz.reserve(keep.size() * 3);
      if (cloud.has_color()) {
        thinned.rgba.reserve(keep.size() * 4);
      }
      for (uint32_t idx : keep) {
        thinned.xyz.push_back(cloud.xyz[idx * 3]);
        thinned.xyz.push_back(cloud.xyz[idx * 3 + 1]);
        thinned.xyz.push_back(cloud.xyz[idx * 3 + 2]);
        if (cloud.has_color()) {
          thinned.rgba.push_back(cloud.rgba[idx * 4]);
          thinned.rgba.push_back(cloud.rgba[idx * 4 + 1]);
          thinned.rgba.push_back(cloud.rgba[idx * 4 + 2]);
          thinned.rgba.push_back(cloud.rgba[idx * 4 + 3]);
        }
      }
      thinned.recompute_bounds();
      cloud = std::move(thinned);
    }
  }

  node->point_positions = std::move(cloud.xyz);
  node->point_rgba = std::move(cloud.rgba);
  node->min_x = cloud.min_x;
  node->min_y = cloud.min_y;
  node->min_z = cloud.min_z;
  node->max_x = cloud.max_x;
  node->max_y = cloud.max_y;
  node->max_z = cloud.max_z;

  // Rebuild cloud view for chunker (positions already moved — reconstruct).
  PointCloud for_chunks;
  for_chunks.xyz = node->point_positions;
  for_chunks.rgba = node->point_rgba;
  for_chunks.min_x = node->min_x;
  for_chunks.min_y = node->min_y;
  for_chunks.min_z = node->min_z;
  for_chunks.max_x = node->max_x;
  for_chunks.max_y = node->max_y;
  for_chunks.max_z = node->max_z;
  PointCloudChunkOptions chunk_opts;
  chunk_opts.grid_axis = 8;
  chunk_opts.max_points_per_chunk = 50000;
  if (!build_point_cloud_chunks(for_chunks, chunk_opts, &node->point_chunks)) {
    node->point_chunks.clear();
  }

  ++generation_;
  node->generation = generation_;
  return true;
}

bool World::apply_tileset_selection(
    uint64_t id, const std::vector<const vista::Tile*>& visible) {
  Node* node = find(id);
  if (!node || node->kind != NodeKind::kTileset) {
    return false;
  }
  std::vector<std::string> uris;
  uris.reserve(visible.size());
  for (const vista::Tile* tile : visible) {
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

bool World::stream_tileset(uint64_t id, const ViewState& view, double max_sse,
                           size_t max_tiles) {
  Node* node = find(id);
  if (!node || node->kind != NodeKind::kTileset || !node->tileset) {
    return false;
  }
  std::vector<const Tile*> visible;
  select_tiles_limited(*node->tileset, view, max_sse, max_tiles, visible);
  return apply_tileset_selection(id, visible);
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

}  // namespace vista
