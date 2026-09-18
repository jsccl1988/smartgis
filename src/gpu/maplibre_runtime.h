// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_MAPLIBRE_RUNTIME_H_
#define GPU_MAPLIBRE_RUNTIME_H_

#include <cstdint>
#include <string>
#include <vector>

// Private GPU TU. Do not include from content/ or app/.
namespace gpu {
namespace detail {

struct MaplibreStill {
  uint32_t width_px = 0;
  uint32_t height_px = 0;
  std::vector<uint8_t> bgra;
};

bool maplibre_runtime_compiled_impl();
bool maplibre_map_linked_impl();

// Attempts a still-image render through the MapLibre Native pin facade.
// Background (+ opacity) only — no tile fetch. Returns false when the
// pin/lib is missing; caller uses the adapter (full Track A richness).
bool try_maplibre_still_image(const char* style_json,
                              uint32_t width_px,
                              uint32_t height_px,
                              MaplibreStill* out);

}  // namespace detail
}  // namespace gpu

#endif  // GPU_MAPLIBRE_RUNTIME_H_
