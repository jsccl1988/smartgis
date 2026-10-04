// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_VISTA_WORLD_TERRAIN_MESH_LINE_TESS_H_
#define GIS_VISTA_WORLD_TERRAIN_MESH_LINE_TESS_H_

#include <vector>

#include "vista/world/terrain/mesh/mesh_types.h"
#include "vista/world/terrain/mesh/tessellate.h"

class OGRLineString;

namespace vista {
namespace detail {

bool append_styled_polyline(const std::vector<PolyPt>& pts,
                            const LineTessOptions& options, TessMesh& out);

void line_to_points_decimated(const OGRLineString* line,
                              const LineTessOptions& options,
                              std::vector<PolyPt>& pts);

}  // namespace detail
}  // namespace vista

#endif  // GIS_VISTA_WORLD_TERRAIN_MESH_LINE_TESS_H_
