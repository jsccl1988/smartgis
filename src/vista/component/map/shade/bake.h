// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Hillshade bake orchestration: zoom LOD, style params, dem bake cache,
// shade_dem_rgba, and the TileSlot the layout consumes. Shade math stays
// in dem_hillshade.

#ifndef VISTA_COMPONENT_MAP_SHADE_BAKE_H_
#define VISTA_COMPONENT_MAP_SHADE_BAKE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "vista/vista_export.h"
#include "vista/component/map/ir.h"

namespace gis {
namespace style {
struct StyleLayer;
}
}  // namespace gis

namespace vista {

// Baked DEM shade plus the slot LayoutInput::hillshade_tiles expects.
struct HillshadeBake {
  std::vector<uint8_t> rgba;
  int width = 0;
  int height = 0;
  TileSlot slot;
  bool ok = false;
};

// Last bake_hillshade_slot phase clocks (C4). Downsample is fused into shade.
struct HillshadeBakeSample {
  int64_t mem_ms = 0;
  int64_t disk_ms = 0;
  int64_t load_ms = 0;
  int64_t shade_ms = 0;
  int64_t store_ms = 0;
  int mem_hit = 0;
  int disk_hit = 0;
  int used_cuda = 0;
  int width = 0;
  int height = 0;
  int max_edge = 0;
};

VISTA_EXPORT HillshadeBakeSample hillshade_last_bake_sample();
VISTA_EXPORT void reset_hillshade_bake_sample();
VISTA_EXPORT void reset_hillshade_bake_cache();

// |dem_path| is the host lookup. Cache key is that path plus illumination
// and the zoom LOD edge. |texture_key| is the host id for load_raster.
VISTA_EXPORT HillshadeBake bake_hillshade_slot(const std::string& dem_path,
                                              double zoom,
                                              const gis::style::StyleLayer& layer,
                                              uint32_t texture_key);

}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_SHADE_BAKE_H_
