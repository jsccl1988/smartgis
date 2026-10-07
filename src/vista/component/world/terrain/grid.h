// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Nested-grid tile selection and Y-up edge skirts (CPU IR).
// Selection uses policy distances; skirts only extend an existing mesh.

#ifndef VISTA_COMPONENT_WORLD_TERRAIN_GRID_H_
#define VISTA_COMPONENT_WORLD_TERRAIN_GRID_H_

#include <cstddef>
#include <vector>

#include "vista/vista_export.h"

namespace vista {
// One nested-grid / CPU-CDLOD ring tile. ring 0 is innermost (finest edge).
struct NestedGridTile {
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  int max_edge = 0;
  int ring = 0;
  // CDLOD-style morph in [0,1]: 0 = full current LOD, 1 = blend toward coarser.
  // CPU IR only — TerrainPass does not upload this as a vertex channel.
  float morph_weight = 0.f;
};
// Fill nested squares: inner square as one tile, outer rings as four strips.
// Covers [minx,maxx]×[miny,maxy]. Rings centered on the AABB. Returns
// |out|->size(), or 0 if invalid.
VISTA_EXPORT size_t select_nested_grid_tiles(double minx, double miny,
                                             double maxx, double maxy,
                                             float camera_distance,
                                             std::vector<NestedGridTile>* out);
// Same rings, but centered on |focus_x,focus_y| (clamped into the AABB).
// Morph-friendly when the focus is the camera lon/lat.
VISTA_EXPORT size_t select_nested_grid_tiles(double minx, double miny,
                                             double maxx, double maxy,
                                             float camera_distance,
                                             double focus_x, double focus_y,
                                             std::vector<NestedGridTile>* out);
// Vertical skirts on leftover Y-up AABB edges (drop Y by |skirt_drop|).
// Hides T-junction cracks between adjacent nested-grid densities.
// Extends |xyz| / |indices| / optional |uvs|. Returns triangles added.
VISTA_EXPORT size_t append_terrain_edge_skirts(std::vector<float>* xyz,
                                               std::vector<uint32_t>* indices,
                                               std::vector<float>* uvs,
                                               float skirt_drop);
}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_TERRAIN_GRID_H_
