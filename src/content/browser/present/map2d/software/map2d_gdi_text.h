// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_GDI_TEXT_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_GDI_TEXT_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/browser/present/map2d/software/map2d_gdi_style.h"
#include "vista/component/map/ir.h"

namespace content {
namespace detail {

void paint_text_glyph(HDC hdc, DcStyle* style, HFONT default_font,
                      HFONT* dc_font, GdiPaintResourceCache* resources,
                      const vista::DrawItem& item, const ViewXform& xform,
                      COLORREF* last_halo, COLORREF* last_ink, bool* have_halo,
                      bool* have_ink);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_GDI_TEXT_H_
