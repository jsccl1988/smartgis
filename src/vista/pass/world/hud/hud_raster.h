// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_WORLD_HUD_HUD_RASTER_H_
#define VISTA_PASS_WORLD_HUD_HUD_RASTER_H_

#include <cstdint>
#include <vector>

#include "vista/pass/world/hud/hud_ir.h"
#include "vista/vista_export.h"

namespace vista {

// Rasters HudFrame into premultiplied-ish BGRA8 (alpha 0 = transparent).
// CPU glyph bake only; product composite is HudPass GPU upload.
VISTA_EXPORT bool raster_hud_frame_bgra(const HudFrame& frame,
                                        std::vector<uint8_t>* out_bgra);

}  // namespace vista

#endif  // VISTA_PASS_WORLD_HUD_HUD_RASTER_H_
