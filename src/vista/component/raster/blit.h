// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// RGBA quad blit into an HDC. kMultiply writes the bound DIB; kOver is
// AlphaBlend. Content chooses the quad and the blend.

#ifndef VISTA_COMPONENT_RASTER_BLIT_H_
#define VISTA_COMPONENT_RASTER_BLIT_H_

#include <cstdint>
#include <vector>

#include "vista/component/map/draw.h"
#include "vista/component/raster/dib.h"

namespace vista {
namespace raster {

// Stretch tightly packed RGBA8 into the axis-aligned bbox of |pts|.
// Clips to the HDC bitmap, then bilinear-samples coverage (keeps alpha).
// kMultiply bakes luma into the coverage then multiplies the snapped land.
// kOver is AlphaBlend (SRC_OVER).
bool blit_rgba_quad(HDC hdc, const std::vector<POINT>& pts,
                    const std::vector<uint8_t>& rgba, int tw, int th,
                    float opacity, DrawBlend blend);

// Same as above with a borrowed tightly packed RGBA8 pointer (no vector).
bool blit_rgba_quad(HDC hdc, const std::vector<POINT>& pts, const uint8_t* rgba,
                    int tw, int th, float opacity, DrawBlend blend);

}  // namespace raster
}  // namespace vista

#endif  // VISTA_COMPONENT_RASTER_BLIT_H_
