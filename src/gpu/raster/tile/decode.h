// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_RASTER_TILE_DECODE_H_
#define GPU_RASTER_TILE_DECODE_H_

// Tile body decode: native 4-byte pixels and PNG via WIC.
// Callers use mosaic; they do not include this header.

#include <cstdint>
#include <string>
#include <vector>

namespace gpu {
namespace detail {

// Four-byte bodies stay a 1x1 native pixel. PNG goes through one WIC
// factory per thread. Other bodies fail.
bool decode_tile_native(const std::string& bytes, std::vector<uint8_t>* bgra,
                        uint32_t* out_w, uint32_t* out_h);

}  // namespace detail
}  // namespace gpu

#endif  // GPU_RASTER_TILE_DECODE_H_
