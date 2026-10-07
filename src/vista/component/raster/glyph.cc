// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/raster/glyph.h"

namespace vista {
namespace raster {

void raster_glyph(HDC hdc, const GlyphRaster& glyph, COLORREF* last_halo,
                  COLORREF* last_ink, bool* have_halo, bool* have_ink) {
  if (!last_halo || !last_ink || !have_halo || !have_ink) {
    return;
  }
  const uint32_t cp = glyph.codepoint;
  if (cp == 0 || cp > 0x10ffff) {
    return;
  }
  wchar_t utf16[2] = {};
  int utf16_n = 0;
  if (cp <= 0xffff) {
    utf16[0] = static_cast<wchar_t>(cp);
    utf16_n = 1;
  } else {
    const uint32_t u = cp - 0x10000;
    utf16[0] = static_cast<wchar_t>(0xd800 + (u >> 10));
    utf16[1] = static_cast<wchar_t>(0xdc00 + (u & 0x3ff));
    utf16_n = 2;
  }

  const COLORREF ink = glyph.ink;
  // Skip halo TextOut when layout emitted no halo — was always paying 4×.
  if (glyph.halo_width_px > 0.f) {
    const COLORREF halo = glyph.halo;
    if (!*have_halo || *last_halo != halo) {
      SetTextColor(hdc, halo);
      *last_halo = halo;
      *have_halo = true;
      *have_ink = false;
    }
    // Cardinal halo keeps CJK readable; diagonals are another 4 TextOutW.
    const int halo_d[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (const auto& d : halo_d) {
      TextOutW(hdc, glyph.x + d[0], glyph.y + d[1], utf16, utf16_n);
    }
  }
  if (!*have_ink || *last_ink != ink) {
    SetTextColor(hdc, ink);
    *last_ink = ink;
    *have_ink = true;
    *have_halo = false;
  }
  TextOutW(hdc, glyph.x, glyph.y, utf16, utf16_n);
}

}  // namespace raster
}  // namespace vista
