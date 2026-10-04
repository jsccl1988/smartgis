// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/gis/present/carto/smt_style_from_paint.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace gis {
namespace style {
namespace {

COLORREF argb_to_colorref(uint32_t argb) {
  const int r = static_cast<int>((argb >> 16) & 0xFF);
  const int g = static_cast<int>((argb >> 8) & 0xFF);
  const int b = static_cast<int>(argb & 0xFF);
  return RGB(r, g, b);
}

}  // namespace

base::SmtStyle to_smt_style(const ResolvedPaint& paint, const char* name) {
  base::SmtPenDesc pen;
  base::SmtBrushDesc brush;
  base::SmtAnnotationDesc anno;
  base::SmtSymbolDesc symbol;

  pen.lPenColor = argb_to_colorref(paint.line_color);
  pen.fPenWidth = paint.line_width * 0.001f;
  pen.lPenStyle = PS_SOLID;

  brush.lBrushColor = argb_to_colorref(paint.fill_color);
  if (paint.type == LayerType::kCircle) {
    brush.lBrushColor = argb_to_colorref(paint.circle_color);
  }
  brush.brushTp = base::SmtBrushDesc::BT_Solid;
  brush.lBrushStyle = 0;

  if (!paint.text_field.empty()) {
    anno.lAnnoClr = pen.lPenColor;
  }

  symbol.fSymbolWidth = 0.4f * paint.icon_size;
  symbol.fSymbolHeight = 0.4f * paint.icon_size;
  symbol.lSymbolID = 0;

  const char* style_name = name && name[0] ? name : paint.layer_id.c_str();
  return base::SmtStyle(style_name, pen, brush, anno, symbol);
}

}  // namespace style
}  // namespace gis
