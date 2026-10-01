// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_VISTA_WORLD_TERRAIN_MESH_FILL_TESS_H_
#define GIS_VISTA_WORLD_TERRAIN_MESH_FILL_TESS_H_

#include "gis/vista/world/terrain/mesh/tessellate.h"

class OGRGeometry;
class OGRLineString;
class OGRLinearRing;
class OGRPolygon;

namespace gis {
namespace detail {

void tessellate_point_xy(double x, double y, double z, TessMesh& out);

// Legacy per-segment ribbon (fixed half-width 0.05). Kept for
// tessellate_geometry.
void tessellate_segment(double ax, double ay, double az, double bx, double by,
                        double bz, TessMesh& out);
void tessellate_line_legacy(const OGRLineString* line, TessMesh& out);

void tessellate_ring_fan(const OGRLinearRing* ring, const FillTessOptions& opts,
                         TessMesh& out);
void tessellate_polygon(const OGRPolygon* poly, const FillTessOptions& opts,
                        TessMesh& out);

bool tessellate_geom_into(const OGRGeometry* geom, const FillTessOptions& opts,
                          TessMesh& out);

}  // namespace detail
}  // namespace gis

#endif  // GIS_VISTA_WORLD_TERRAIN_MESH_FILL_TESS_H_
