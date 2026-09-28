// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_RASTER_DIRECT_H_
#define GPU_RASTER_DIRECT_H_

// Direct raster: GDI demo bitmap and the Scene3d DEM underlay.
// Records quads. Does not present.

#include "content/public/map_types.h"
#include "gpu/compositor/frame/frame.h"

namespace gpu {
namespace detail {

// Records the direct bitmap into |pass|. One replacing BGRA quad when GDI
// draws; otherwise one solid clear quad.
void raster_direct_quads(RenderPass* pass, content::ViewKind kind,
                         uint32_t width_px, uint32_t height_px);

}  // namespace detail
}  // namespace gpu

#endif  // GPU_RASTER_DIRECT_H_
