// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// GDI TextOut glyph stamp. The content painter selects the font and anchor.

#ifndef VISTA_COMPONENT_RASTER_GLYPH_H_
#define VISTA_COMPONENT_RASTER_GLYPH_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>

namespace vista {
namespace raster {

// One codepoint already placed in device pixels. |font| is the caller's
// current selection; this stamp only sets text color and issues TextOutW.
struct GlyphRaster {
  uint32_t codepoint = 0;
  int x = 0;
  int y = 0;
  COLORREF ink = 0;
  COLORREF halo = 0;
  float halo_width_px = 0.f;
};

void raster_glyph(HDC hdc, const GlyphRaster& glyph, COLORREF* last_halo,
                  COLORREF* last_ink, bool* have_halo, bool* have_ink);

}  // namespace raster
}  // namespace vista

#endif  // VISTA_COMPONENT_RASTER_GLYPH_H_
