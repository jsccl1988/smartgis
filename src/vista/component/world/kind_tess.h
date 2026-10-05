// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Per-NodeKind CPU tessellation for one Instance.

#ifndef VISTA_COMPONENT_WORLD_KIND_TESS_H_
#define VISTA_COMPONENT_WORLD_KIND_TESS_H_

#include "vista/assets/tileset/tileset.h"
#include "vista/component/world/instance.h"
#include "vista/mesh/tessellate.h"

namespace vista {
namespace detail {

bool uses_point_cloud_chunks(const Instance& inst);

bool tessellate_instance(const Instance& inst, double world_units_per_pixel,
                         vista::TilesetContentCache* cache,
                         vista::TessMesh* out);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_KIND_TESS_H_
