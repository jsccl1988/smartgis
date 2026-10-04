// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_VISTA_WORLD_TERRAIN_MESH_MESH_APPEND_H_
#define GIS_VISTA_WORLD_TERRAIN_MESH_MESH_APPEND_H_

#include <cstdint>

#include "vista/world/terrain/mesh/mesh_types.h"
#include "vista/world/terrain/mesh/tessellate.h"

namespace vista {
namespace detail {

void reset_mesh(TessMesh& out);

void append_xyz(TessMesh& mesh, float x, float y, float z);
void append_xyz(TessMesh& mesh, Vec2 p, double z);

uint32_t vert_count(const TessMesh& mesh);

void append_triangle(TessMesh& out, uint32_t a, uint32_t b, uint32_t c);
void append_quad_indices(TessMesh& out, uint32_t base);

// Axis-aligned XY quad at z=0.
void append_quad(double min_x, double min_y, double max_x, double max_y,
                 TessMesh& out);

bool rect_has_area(double min_x, double min_y, double max_x, double max_y);

}  // namespace detail
}  // namespace vista

#endif  // GIS_VISTA_WORLD_TERRAIN_MESH_MESH_APPEND_H_
