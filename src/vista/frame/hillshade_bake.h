// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Hillshade bake orchestration: zoom LOD, style params, process cache,
// shade_dem_rgba, and the TileSlot the layout consumes. Shade math stays
// in dem_hillshade.

#ifndef GIS_VISTA_FRAME_HILLSHADE_BAKE_H_
#define GIS_VISTA_FRAME_HILLSHADE_BAKE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "vista/vista_export.h"
#include "vista/frame/frame.h"

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

// |dem_path| is the host lookup. Cache key is that path plus illumination
// and the zoom LOD edge. |texture_key| is the host id for load_raster.
VISTA_EXPORT HillshadeBake bake_hillshade_slot(const std::string& dem_path,
                                              double zoom,
                                              const gis::style::StyleLayer& layer,
                                              uint32_t texture_key);

}  // namespace vista

#endif  // GIS_VISTA_FRAME_HILLSHADE_BAKE_H_
