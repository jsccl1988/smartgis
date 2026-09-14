// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_MAPLIBRE_ADAPTER_H_
#define GPU_MAPLIBRE_ADAPTER_H_

#include "gpu/render_backend.h"

namespace gpu {

// Track A 2D basemap: StyleDocument background + TileProvider raster,
// uploaded into PresentTarget. Does not include mln/mbgl headers.
bool paint_track_a_basemap(detail::PresentTarget* present,
                           const MapPaintRequest& req);

}  // namespace gpu

#endif  // GPU_MAPLIBRE_ADAPTER_H_
