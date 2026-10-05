// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/software/map2d_gdi_text.h"

#include <cmath>
#include <cstdint>

#include "content/browser/present/map2d/software/map2d_frame_gdi.h"

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

  POINT anchor = xform.map(item.anchor_x, item.anchor_y, true);
  if (!item.vertices.empty()) {
    anchor = xform.map(item.vertices.front().x, item.vertices.front().y,
                       item.pixel_space);
  }
  const int ax = static_cast<int>(anchor.x);
  const int ay = static_cast<int>(anchor.y);

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

  const COLORREF ink = rgba_to_colorref(item.rgba);
  // Skip halo TextOut when layout emitted no halo — was always paying 4×.
  if (item.halo_width_px > 0.f) {
    const COLORREF halo =
        rgba_to_colorref(item.halo_rgba ? item.halo_rgba : 0xffffffffu);
    if (!*have_halo || *last_halo != halo) {
      SetTextColor(hdc, halo);
      *last_halo = halo;
      *have_halo = true;
      *have_ink = false;
    }
    // Cardinal halo keeps CJK readable; diagonals are another 4 TextOutW.
    const int halo_d[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (const auto& d : halo_d) {
      TextOutW(hdc, ax + d[0], ay + d[1], utf16, utf16_n);
    }
  }
  if (!*have_ink || *last_ink != ink) {
    SetTextColor(hdc, ink);
    *last_ink = ink;
    *have_ink = true;
    *have_halo = false;
  }
  TextOutW(hdc, ax, ay, utf16, utf16_n);
}

}  // namespace detail
}  // namespace content
