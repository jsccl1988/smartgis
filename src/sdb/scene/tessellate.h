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

// MapLibre-aligned line-cap (layout line-cap).
enum class LineCap {
  kButt = 0,
  kRound,
  kSquare,
};

// MapLibre-aligned line-join (layout line-join). Simplified CPU join.
enum class LineJoin {
  kMiter = 0,
  kBevel,
  kRound,
};

// Options for stroked-line ribbon tessellation.
//
// Unit conventions:
// - Polyline coordinates and `half_width` are in the same **world** units
//   (map CRS / layer space), not screen pixels.
// - `pixel_width` is a screen-space stroke width in logical pixels. When both
//   `pixel_width` and `world_units_per_pixel` are > 0, the effective half-width
//   is `0.5 * pixel_width * world_units_per_pixel` (overrides `half_width`).
// - `dasharray` lengths are in **world** units along the polyline (same space
//   as coordinates). MapLibre order: [dash, gap, dash, gap, ...]. Empty =
//   solid stroke. Callers that hold MapLibre pixel dashes must convert with
//   the same `world_units_per_pixel` before calling.
// - Cap/join geometry uses the resolved world half-width. Round fans use
//   `round_segments` wedges per semicircle / outer join.
struct LineTessOptions {
  // World-space half-width (full stroke width = 2 * half_width).
  // Default matches the legacy fixed ribbon used by tessellate_geometry.
  double half_width = 0.05;

  // Screen-space stroke width in pixels (MapLibre line-width). Zero = unused.
  double pixel_width = 0;

  // World units per screen pixel at the current view / resolution.
  // Pair with pixel_width; see line_half_width_world().
  double world_units_per_pixel = 0;

  LineCap cap = LineCap::kButt;
  LineJoin join = LineJoin::kMiter;

  // Miter length limit as a multiple of half_width (MapLibre line-miter-limit).
  // When exceeded, the join falls back to bevel.
  double miter_limit = 2.0;

  // World-unit dash pattern; see struct comment. Empty = solid.
  std::vector<double> dasharray;

  // Fan subdivision for round caps / joins (semicircle uses this many wedges).
  int round_segments = 8;
};

// half_width_world = 0.5 * pixel_width * world_units_per_pixel.
GIS_EXPORT double line_half_width_world(double pixel_width,
                                        double world_units_per_pixel);

// Resolve resolution from a world envelope width and viewport width in pixels:
// world_units_per_pixel = envelope_world_width / viewport_width_px, then
// half_width_world = 0.5 * pixel_width * that ratio.
GIS_EXPORT double line_half_width_from_envelope(double pixel_width,
                                                double envelope_world_width,
                                                double viewport_width_px);

// Effective half-width for options (pixel path when both pixel fields are set).
GIS_EXPORT double resolve_line_half_width(const LineTessOptions& options);

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

// Legacy line stroke: fixed world half-width 0.05, butt segments, no dash.
// Same ribbon semantics historically used inside tessellate_geometry.
GIS_EXPORT bool tessellate_line(const OGRLineString* line, TessMesh& out);

// Styled line stroke: world or pixel-derived half-width, cap/join/dash.
GIS_EXPORT bool tessellate_line(const OGRLineString* line,
                                const LineTessOptions& options,
                                TessMesh& out);

// Raw polyline (x,y[,z] interleaved). stride_floats is 2 or 3.
GIS_EXPORT bool tessellate_polyline(const float* xyz, size_t point_count,
                                    size_t stride_floats,
                                    const LineTessOptions& options,
                                    TessMesh& out);

// Axis-aligned box (12 triangles) used as a tileset / model placeholder.
GIS_EXPORT bool tessellate_aabb(double min_x, double min_y, double min_z,
                                double max_x, double max_y, double max_z,
                                TessMesh& out);

}  // namespace scene
}  // namespace sdb

#endif  // SDB_SCENE_TESSELLATE_H_
