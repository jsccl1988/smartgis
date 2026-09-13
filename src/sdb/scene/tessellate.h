// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_SCENE_TESSELLATE_H_
#define SDB_SCENE_TESSELLATE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "sdb/gis_export.h"

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

GIS_EXPORT bool tessellate_geometry(const OGRGeometry* geom, TessMesh& out);
GIS_EXPORT bool tessellate_geoms(const OGRGeometry* const* geoms, size_t count,
                                 TessMesh& out);
GIS_EXPORT bool tessellate_layer(OGRLayer* layer, TessMesh& out);
GIS_EXPORT bool tessellate_3d_geometry(const OGRGeometry* geom, TessMesh& out);
GIS_EXPORT bool tessellate_3d_surface(const geo::Tin* surf, TessMesh& out);

GIS_EXPORT bool tessellate_arc(const OGRLineString* arc, TessMesh& out);
GIS_EXPORT bool tessellate_fan(const OGRPolygon* fan, TessMesh& out);
GIS_EXPORT bool tessellate_tin(const geo::Tin* tin, TessMesh& out);
GIS_EXPORT bool tessellate_grid(const geo::Grid* grid, TessMesh& out);
GIS_EXPORT bool tessellate_raster_layer(const sdb::SmtRasterLayer* layer,
                                        TessMesh& out);
GIS_EXPORT bool tessellate_tile_layer(const sdb::SmtTileLayer* layer,
                                      TessMesh& out);

// Axis-aligned box (12 triangles) used as a tileset / model placeholder.
GIS_EXPORT bool tessellate_aabb(double min_x, double min_y, double min_z,
                                double max_x, double max_y, double max_z,
                                TessMesh& out);

}  // namespace scene
}  // namespace sdb

#endif  // SDB_SCENE_TESSELLATE_H_
