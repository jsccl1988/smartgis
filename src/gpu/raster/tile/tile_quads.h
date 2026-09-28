// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_RASTER_TILE_QUADS_H_
#define GPU_RASTER_TILE_QUADS_H_

// Tile raster: Style walk records solid and image quads. Does not present.
// raster_tile_quads is the only entry display includes from this directory.

#include "gpu/compositor/compositor_frame.h"
#include "gpu/frame_sink.h"

namespace gpu {
namespace detail {

// Records background, then rasters, into |pass| (back to front).
// False when |surface| is null or its size is 0.
bool raster_tile_quads(OutputSurface* surface, const DrawRequest& req,
                       RenderPass* pass);

}  // namespace detail
}  // namespace gpu

#endif  // GPU_RASTER_TILE_QUADS_H_
