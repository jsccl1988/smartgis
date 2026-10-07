// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// GN-DEP: //src/vista/component/raster:raster

#include "content/browser/present/map2d/software/map2d_gdi_text.h"

#include <cmath>
#include <cstdint>

#include "content/browser/present/map2d/software/map2d_frame_gdi.h"
#include "vista/component/raster/glyph.h"

namespace content {
namespace detail {

void paint_text_glyph(HDC hdc, DcStyle* style, HFONT default_font,
                      HFONT* dc_font, GdiPaintResourceCache* resources,
                      const vista::DrawItem& item, const ViewXform& xform,
                      COLORREF* last_halo, COLORREF* last_ink, bool* have_halo,
                      bool* have_ink) {
  if (!style || !dc_font || !last_halo || !last_ink || !have_halo ||
      !have_ink) {
    return;
  }
  const uint32_t cp = item.codepoint;
  if (cp == 0 || cp > 0x10ffff) {
    return;
  }

  POINT anchor = xform.map(item.anchor_x, item.anchor_y, true);
  if (!item.vertices.empty()) {
    anchor = xform.map(item.vertices.front().x, item.vertices.front().y,
                       item.pixel_space);
  }

  const int font_px =
      item.text_size_px > 0.5f ? static_cast<int>(std::lround(item.text_size_px))
                               : 13;
  const double deg = item.angle_rad * (180.0 / 3.14159265358979323846);
  const int esc =
      std::fabs(deg) > 0.5 ? static_cast<int>(std::lround(-deg * 10.0)) : 0;

  HFONT glyph_font = default_font;
  if (resources && (esc != 0 || font_px != 13)) {
    HFONT created = resources->font_for(font_px, esc);
    if (created) {
      glyph_font = created;
    }
  }
  if (glyph_font && glyph_font != *dc_font) {
    // Font select breaks sticky brush/pen tracking on this DC.
    SelectObject(hdc, glyph_font);
    style->invalidate();
    *dc_font = glyph_font;
  }

  vista::raster::GlyphRaster glyph;
  glyph.codepoint = cp;
  glyph.x = static_cast<int>(anchor.x);
  glyph.y = static_cast<int>(anchor.y);
  glyph.ink = rgba_to_colorref(item.rgba);
  glyph.halo =
      rgba_to_colorref(item.halo_rgba ? item.halo_rgba : 0xffffffffu);
  glyph.halo_width_px = item.halo_width_px;
  vista::raster::raster_glyph(hdc, glyph, last_halo, last_ink, have_halo,
                              have_ink);
}

}  // namespace detail
}  // namespace content
