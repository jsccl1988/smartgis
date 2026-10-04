// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CPU mirror of one World node. Upload and record stay on WorldPass.

#ifndef VISTA_WORLD_INSTANCE_H_
#define VISTA_WORLD_INSTANCE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "gis/style/style_types.h"
#include "vista/world/world.h"

class OGRGeometry;
class OGRLayer;
class OGRTriangulatedSurface;
class OGRMultiPoint;

namespace vista {

// One World node mirrored for GPU upload (layer / 3D geom pointers stay
// non-owning). Optional ResolvedPaint is applied after sync_from.
struct Instance {
  uint64_t node_id;
  vista::NodeKind kind;
  double min_x;
  double min_y;
  double min_z;
  double max_x;
  double max_y;
  double max_z;
  const gis::MapLayer* layer = nullptr;
  OGRLayer* ogr_layer = nullptr;
  const OGRGeometry* geom_3d = nullptr;
  std::vector<const OGRGeometry*> geoms;
  const OGRTriangulatedSurface* tin = nullptr;
  const OGRMultiPoint* grid = nullptr;
  int grid_nx = 0;
  int grid_ny = 0;
  const vista::ModelAsset* model = nullptr;
  const vista::Tileset* tileset = nullptr;
  std::vector<std::string> visible_uris;
  // Copied from vista::Node on sync_from (terrain mesh upload seam).
  std::vector<float> terrain_positions;
  std::vector<uint32_t> terrain_indices;
  std::vector<float> terrain_uvs;
  std::vector<uint8_t> terrain_rgba;
  uint32_t terrain_tex_w = 0;
  uint32_t terrain_tex_h = 0;
  std::vector<float> point_positions;
  std::vector<uint8_t> point_rgba;
  std::vector<vista::PointCloudChunk> point_chunks;
  bool has_paint = false;
  gis::style::ResolvedPaint paint;
};

}  // namespace vista

#endif  // VISTA_WORLD_INSTANCE_H_
