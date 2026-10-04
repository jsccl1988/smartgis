// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_RASTER_TILE_MOSAIC_H_
#define GPU_RASTER_TILE_MOSAIC_H_

// Viewport XYZ mosaic: tile coords, world-rect blit, and 0/0/0 stretch.
// Composite calls this. Decode stays below it.

#include "gis/carto/tile/xyz_math.h"
#include "gpu/frame_sink.h"

#include <cstdint>
#include <string>
#include <vector>

namespace gpu {
namespace detail {

bool extent_is_valid(const content::Extent2& e);

gis::tile::Viewport viewport_from_request(const DrawRequest& req);

// Valid extent uses tiles_for_viewport. Degenerate extent is the single
// tile z/x/y = 0/0/0.
std::vector<gis::tile::TileCoord> visible_tile_coords(const DrawRequest& req);

// Stretch one decoded tile across the output size. Used when the extent
// is degenerate and the only tile is 0/0/0.
bool decode_tile_bgra(const std::string& bytes, uint32_t dst_w, uint32_t dst_h,
                      std::vector<uint8_t>* dst);

// Decode |body| into |layer|. Degenerate extent stretches the tile. A valid
// extent blits the tile's Web Mercator world rect.
bool mosaic_tile_bytes(std::vector<uint8_t>* layer, uint32_t w, uint32_t h,
                       const DrawRequest& req, const gis::tile::TileCoord& coord,
                       const std::string& body);

}  // namespace detail
}  // namespace gpu

#endif  // GPU_RASTER_TILE_MOSAIC_H_
