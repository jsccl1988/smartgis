// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_MAPLIBRE_ADAPTER_H_
#define GPU_MAPLIBRE_ADAPTER_H_

#include "gpu/render_backend.h"

namespace gpu {

// Track A 2D basemap: StyleDocument background (+ opacity) and ordered
// raster layers (+ raster-opacity). Raster `layer.source` binds Style
// `sources` via SourceRegistry / TileProvider; request tile_url_templates
// remain a fallback when source id is empty. Visible XYZ tiles come from
// MapPaintRequest.extent (+ zoom) via sdb::tile::tiles_for_viewport;
// degenerate extent keeps the legacy single tile 0/0/0. TileFetchFn injects
// offline.
bool paint_track_a_basemap(detail::PresentTarget* present,
                           const MapPaintRequest& req);

}  // namespace gpu

#endif  // GPU_MAPLIBRE_ADAPTER_H_
