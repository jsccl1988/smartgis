// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Solid scanline fill into a bound DibSurface. The content FillBatch decides
// which triangles to submit.

#ifndef VISTA_COMPONENT_RASTER_FILL_H_
#define VISTA_COMPONENT_RASTER_FILL_H_

#include "vista/component/raster/dib.h"

namespace vista {
namespace raster {

// Overdraw union — no WINDING cancel holes, no tri edges. Subpixel coverage
// on the left and right edges.
void fill_tri_solid(DibSurface* dib, POINT a, POINT b, POINT c, uint32_t bgra);

}  // namespace raster
}  // namespace vista

#endif  // VISTA_COMPONENT_RASTER_FILL_H_
