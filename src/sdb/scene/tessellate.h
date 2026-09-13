// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_SCENE_TESSELLATE_H_
#define SDB_SCENE_TESSELLATE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

// CPU tessellation of leftover GIS geometry into GPU-uploadable xyz +
// indices. No render/rhi types.

class OGRGeometry;
class OGRLayer;
class OGRLineString;
class OGRPolygon;

namespace geo {
class Grid;
class Tin;
}

namespace sdb {
class SmtRasterLayer;
class SmtTileLayer;
}

namespace sdb {
namespace scene {

// Triangle mesh produced from leftover GIS geometry (2D features or 3D surface).
struct TessMesh {
  std::vector<float> positions;
  std::vector<uint32_t> indices;
  bool has_image = false;
};

bool tessellate_geometry(const OGRGeometry* geom, TessMesh& out);
bool tessellate_geoms(const OGRGeometry* const* geoms, size_t count,
                      TessMesh& out);
bool tessellate_layer(OGRLayer* layer, TessMesh& out);
bool tessellate_3d_geometry(const OGRGeometry* geom, TessMesh& out);
bool tessellate_3d_surface(const geo::Tin* surf, TessMesh& out);

bool tessellate_arc(const OGRLineString* arc, TessMesh& out);
bool tessellate_fan(const OGRPolygon* fan, TessMesh& out);
bool tessellate_tin(const geo::Tin* tin, TessMesh& out);
bool tessellate_grid(const geo::Grid* grid, TessMesh& out);
bool tessellate_raster_layer(const sdb::SmtRasterLayer* layer,
                             TessMesh& out);
bool tessellate_tile_layer(const sdb::SmtTileLayer* layer, TessMesh& out);

}  // namespace scene
}  // namespace sdb

#endif  // SDB_SCENE_TESSELLATE_H_
