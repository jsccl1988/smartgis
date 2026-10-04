// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Per-NodeKind CPU tessellation for one GpuInstance.

#ifndef VISTA_SCENE_MESH_KIND_TESS_H_
#define VISTA_SCENE_MESH_KIND_TESS_H_

#include "vista/assets/tileset/tileset.h"
#include "vista/scene/gpu_instance.h"
#include "vista/mesh/tessellate.h"

namespace vista {
namespace detail {

bool uses_point_cloud_chunks(const GpuInstance& inst);

bool tessellate_instance(const GpuInstance& inst, double world_units_per_pixel,
                         vista::TilesetContentCache* cache,
                         vista::TessMesh* out);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_SCENE_MESH_KIND_TESS_H_
