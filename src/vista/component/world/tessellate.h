// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CPU tessellation for vector Instance and lit normal interleave.

#ifndef VISTA_COMPONENT_WORLD_TESSELLATE_H_
#define VISTA_COMPONENT_WORLD_TESSELLATE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vista/component/world/instance.h"
#include "vista/mesh/tessellate.h"

class OGRGeometry;

namespace vista {
namespace detail {

void interleave_positions_with_normals(const float* positions,
                                       size_t position_count,
                                       const uint32_t* indices,
                                       size_t index_count,
                                       std::vector<float>* out);

void append_circle_diamond(double x, double y, double z, double radius_world,
                           vista::TessMesh& out);

void append_mesh(vista::TessMesh& dst, const vista::TessMesh& src);

bool tessellate_geom_paint_aware(const OGRGeometry* geom,
                                 const gis::style::ResolvedPaint* paint,
                                 double world_units_per_pixel,
                                 vista::TessMesh& out);

bool tessellate_vector_instance(const Instance& inst,
                                double world_units_per_pixel,
                                vista::TessMesh& out);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_TESSELLATE_H_
