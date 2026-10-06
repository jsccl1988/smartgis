// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// IR-side LOD for DEM terrain (CPU).
// Raster: lod_max_edge + uniform view tiles + nested-grid (CDLOD-style rings
// with per-tile max_edge from patch-camera distance, CPU morph weights, and
// vertical edge skirts for T-junctions).
// TIN: keep-every-Nth stride, or spatial thin by centroid-to-camera distance.
// Surface: heightfield sample density.
// Gap: no GPU geometry clipmap / hardware tess (RHI has no hull pipeline).

#ifndef VISTA_COMPONENT_WORLD_TERRAIN_LOD_H_
#define VISTA_COMPONENT_WORLD_TERRAIN_LOD_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vista/vista_export.h"

namespace vista {

// How a TerrainPayload was produced (seed stamps this; GPU path ignores it).
enum class TerrainSource : uint8_t {
  kUnknown = 0,
  kRaster = 1,
  kTin = 2,
  kSurface = 3,
};

// Max DEM mesh edge length for |camera_distance| (forwards DemRaster).
VISTA_EXPORT int terrain_lod_max_edge(float camera_distance);

// Cache / generation key: lod_max_edge * 10 + grid_hint (1 far, 2 near).
// Same semantics as dem_seed_cache_key.
VISTA_EXPORT int terrain_lod_cache_key(float camera_distance);

// TIN triangle keep stride: 1 = full mesh; 2/4/8 = keep every Nth triangle.
// Closer camera → denser (smaller stride). Discrete buckets only (fallback
// when seed has no camera XYZ).
VISTA_EXPORT int terrain_lod_tin_stride(float camera_distance);

// Spatial TIN: map |centroid_distance| / |mesh_span| onto the same buckets as
// terrain_lod_tin_stride, using |camera_distance| as the near-floor (same
// +radial*4 convention as terrain_lod_patch_distance).
VISTA_EXPORT int terrain_lod_tin_stride_at(float camera_distance,
                                           float centroid_distance,
                                           float mesh_span);

// Surface-interp sample edge for DemHeightField / DemRaster rebuilds.
// Aliases terrain_lod_max_edge (same discrete buckets this wave).
VISTA_EXPORT int terrain_lod_surface_edge(float camera_distance);

// Cache key for TIN LOD: stride * 1000 + surface_edge (avoids raster key clash).
VISTA_EXPORT int terrain_lod_tin_cache_key(float camera_distance);

// Spatial TIN cache key (1e6 band; does not clash with nested 2e6).
VISTA_EXPORT int terrain_lod_tin_cache_key(float camera_distance, float camera_x,
                                           float camera_y, float camera_z);

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

// Concentric ring count for nested-grid selection (1..4). Closer → more rings.
VISTA_EXPORT int terrain_lod_nested_rings(float camera_distance);

// max_edge for |ring| (0 = inner). Coarse ring-halving hint only.
// Seed uses terrain_lod_max_edge(terrain_lod_patch_distance(...)).
VISTA_EXPORT int terrain_lod_nested_edge(float camera_distance, int ring);

// Orbit-style distance for one patch: camera_distance plus radial offset
// from the view-AABB center (0 at center, ~1 at the box edge). Outer
// patches map into coarser DemRaster::lod_max_edge buckets.
VISTA_EXPORT float terrain_lod_patch_distance(float camera_distance,
                                              double view_minx, double view_miny,
                                              double view_maxx, double view_maxy,
                                              double tile_minx, double tile_miny,
                                              double tile_maxx, double tile_maxy);

// Cache key for a nested-grid ring hint (offset 2e6 so it cannot clash with TIN).
VISTA_EXPORT int terrain_lod_nested_cache_key(float camera_distance, int ring);

// Cache key stamped on a nested tile (ring + actual max_edge).
VISTA_EXPORT int terrain_lod_nested_tile_key(int ring, int max_edge);

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

// CPU CDLOD morph in [0,1] from patch distance + ring (outer rings morph more).
VISTA_EXPORT float terrain_lod_morph_weight(float patch_distance, int ring);

// Vertical skirts on leftover Y-up AABB edges (drop Y by |skirt_drop|).
// Hides T-junction cracks between adjacent nested-grid densities.
// Extends |xyz| / |indices| / optional |uvs|. Returns triangles added.
VISTA_EXPORT size_t append_terrain_edge_skirts(std::vector<float>* xyz,
                                               std::vector<uint32_t>* indices,
                                               std::vector<float>* uvs,
                                               float skirt_drop);

}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_TERRAIN_LOD_H_
