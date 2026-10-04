// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CPU tessellation for vector GpuInstance and lit normal interleave.

#ifndef EFFECT_SCENE_DETAIL_TESSELLATE_H_
#define EFFECT_SCENE_DETAIL_TESSELLATE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vista/world/terrain/mesh/tessellate.h"
#include "vista/scene/scene.h"

class OGRGeometry;

namespace vista {
namespace detail {

// Expand xyz-only positions to interleaved POSITION+NORMAL (6 floats/vert)
// for the lit program. Accumulates area-weighted face normals, then
// normalizes; zero-length falls back to +Y so the lit VS never sees 0.
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

bool tessellate_vector_instance(const GpuInstance& inst,
                                double world_units_per_pixel,
                                vista::TessMesh& out);

}  // namespace detail
}  // namespace vista

#endif  // EFFECT_SCENE_DETAIL_TESSELLATE_H_
