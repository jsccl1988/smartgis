// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/detail/style_ogr.h"

#include "gis/datasource/ogr/ogr_feature_codec.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <cstring>

#include "ogrsf_frmts.h"

namespace scenic {
namespace detail {
namespace {

bool parse_html_rgb(const char* s, COLORREF* out) {
  if (!s || s[0] != '#' || !out) {
    return false;
  }
  unsigned r = 0;
  unsigned g = 0;
  unsigned b = 0;
  if (std::sscanf(s, "#%2x%2x%2x", &r, &g, &b) != 3) {
    return false;
  }
  *out = RGB(r, g, b);
  return true;
}

COLORREF field_color_rgb(OGRFeature* src, const char* name, COLORREF fallback) {
  if (!src || !name) {
    return fallback;
  }
  const int i = src->GetFieldIndex(name);
  if (i < 0) {
    return fallback;
  }
  COLORREF c = fallback;
  if (parse_html_rgb(src->GetFieldAsString(i), &c)) {
    return c;
  }
  return fallback;
}

}  // namespace

void copy_style_to_ogr(const Style* src, OGRFeature* dst) {
  if (!src || !dst) {
    return;
  }
  const int oi = dst->GetFieldIndex("style");
  if (oi < 0) {
    return;
  }
  dst->SetField(oi, static_cast<int>(sizeof(Style)),
                reinterpret_cast<const void*>(src));
}

Style* copy_style_from_ogr(OGRFeature* src) {
  if (!src) {
    return nullptr;
  }
  const int oi = src->GetFieldIndex("style");
  if (oi < 0) {
    return nullptr;
  }
  int nbytes = 0;
  GByte* data = src->GetFieldAsBinary(oi, &nbytes);
  if (!data || nbytes < static_cast<int>(sizeof(Style))) {
    return nullptr;
  }
  auto* style = new Style();
  std::memcpy(style, data, sizeof(Style));
  return style;
}

void fill_default_draw_style(OGRFeature* src, Style* dst, float fblc) {
  if (!dst) {
    return;
  }
  const char* kind = nullptr;
  if (src) {
    const int ki = src->GetFieldIndex("kind");
    if (ki >= 0) {
      kind = src->GetFieldAsString(ki);
    }
  }
  const bool river = kind && (std::strcmp(kind, "river") == 0 ||
                              std::strcmp(kind, "water") == 0);
  PenDesc pen;
  pen.lPenStyle = PS_SOLID;
  if (river) {
    pen.lPenColor = field_color_rgb(src, "stroke", RGB(100, 160, 208));
    pen.fPenWidth = fblc > 0.01f ? (0.9f / fblc) : 0.14f;
  } else {
    pen.lPenColor = field_color_rgb(src, "stroke", RGB(196, 190, 176));
    pen.fPenWidth = fblc > 0.01f ? (1.15f / fblc) : 0.2f;
  }
  BrushDesc brush;
  COLORREF fill = RGB(245, 243, 233);
  const int fi = src ? src->GetFieldIndex("fill") : -1;
  if (fi < 0 || !parse_html_rgb(src->GetFieldAsString(fi), &fill)) {
    fill = RGB(245, 243, 233);
  }
  brush.lBrushColor = river ? RGB(163, 204, 255) : fill;
  dst->set_pen_desc(pen);
  dst->set_brush_desc(brush);

  unsigned flags = kStPenDesc | kStBrushDesc;
  if (gis::datasource::infer_vector_schema(src) == gis::VectorSchema::kAnno) {
    AnnotationDesc anno;
    std::strcpy(anno.szFaceName, "Microsoft YaHei");
    anno.lCharSet = DEFAULT_CHARSET;
    anno.lWeight = FW_NORMAL;
    anno.lAnnoClr = field_color_rgb(src, "stroke", RGB(24, 24, 24));
    if (fblc > 0.05f) {
      const float px = anno.fHeight * fblc;
      if (px < 12.f) {
        anno.fHeight = 14.f / fblc;
        anno.fWidth = 0.f;
      } else if (px > 28.f) {
        anno.fHeight = 16.f / fblc;
        anno.fWidth = 0.f;
      }
    }
    dst->set_anno_desc(anno);
    flags |= kStAnnoDesc;
  }
  dst->set_style_type(flags);
}

}  // namespace detail
}  // namespace scenic
