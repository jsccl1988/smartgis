// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_MESH_LINE_LINE_TESS_H_
#define VISTA_MESH_LINE_LINE_TESS_H_

#include <vector>

#include "vista/mesh/detail/mesh_types.h"
#include "vista/mesh/tessellate.h"

class OGRLineString;

namespace vista {
namespace detail {

bool append_styled_polyline(const std::vector<PolyPt>& pts,
                            const LineTessOptions& options, TessMesh& out);

// Envelope smaller than ~0.5 px (stroke-aware) — skip tess before OGR walks.
bool line_skips_tessellation(const OGRLineString* line,
                             const LineTessOptions& options);

void line_to_points_decimated(const OGRLineString* line,
                              const LineTessOptions& options,
                              std::vector<PolyPt>& pts);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_MESH_LINE_LINE_TESS_H_
