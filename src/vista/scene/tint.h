// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Cpu mesh tint from ResolvedPaint / point RGBA / DEM defaults.

#ifndef VISTA_SCENE_MESH_TINT_H_
#define VISTA_SCENE_MESH_TINT_H_

#include "gis/style/style_types.h"
#include "vista/scene/gpu_mesh.h"
#include "vista/scene/gpu_instance.h"

namespace vista {
namespace detail {

void apply_paint_scalars(const gis::style::ResolvedPaint& paint, GpuMesh* mesh);

bool average_point_rgba(const GpuInstance& inst, float* r, float* g, float* b);

void apply_default_point_tint(GpuMesh* mesh);

void apply_untextured_terrain_tint(GpuMesh* mesh);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_SCENE_MESH_TINT_H_
