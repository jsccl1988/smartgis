// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_MESH_DETAIL_MESH_APPEND_H_
#define VISTA_MESH_DETAIL_MESH_APPEND_H_

#include <cstdint>

#include "vista/mesh/detail/mesh_types.h"
#include "vista/mesh/tessellate.h"

namespace vista {
namespace detail {

void reset_mesh(TessMesh& out);

void append_xyz(TessMesh& mesh, float x, float y, float z);
void append_xyz(TessMesh& mesh, Vec2 p, double z);

uint32_t vert_count(const TessMesh& mesh);

void append_triangle(TessMesh& out, uint32_t a, uint32_t b, uint32_t c);

// Triangle-strip / extruded-segment layout: v0/v1 at start, v2/v3 at end.
void append_quad_indices(TessMesh& out, uint32_t base);

// Boundary-ring layout: v0→v1→v2→v3 around the face (CCW or CW).
void append_ring_quad_indices(TessMesh& out, uint32_t base);

// Axis-aligned XY quad at z=0.
void append_quad(double min_x, double min_y, double max_x, double max_y,
                 TessMesh& out);

bool rect_has_area(double min_x, double min_y, double max_x, double max_y);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_MESH_DETAIL_MESH_APPEND_H_
