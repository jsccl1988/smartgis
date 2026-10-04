// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/paint/carto/style/style.h"

#include <algorithm>

#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_api.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/frame/carto_frame.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/encode/encoder_tls.h"
#include "scenic/render/rhi2d/impl/gdi/res/resource.h"

using namespace base;

namespace scenic {
namespace detail {

Rhi2dCartoDrawStyle::Rhi2dCartoDrawStyle() = default;

Rhi2dCartoDrawStyle::~Rhi2dCartoDrawStyle() { destroy_objects(); }

void Rhi2dCartoDrawStyle::destroy_objects() {
  if (h_font_) {
    DeleteObject(h_font_);
    h_font_ = nullptr;
  }
  if (h_pen_) {
    DeleteObject(h_pen_);
    h_pen_ = nullptr;
  }
  if (h_brush_) {
    DeleteObject(h_brush_);
    h_brush_ = nullptr;
  }
  if (h_icon_) {
    DeleteObject(h_icon_);
    h_icon_ = nullptr;
  }
}

void Rhi2dCartoDrawStyle::invalidate_cache() { style_cache_valid_ = false; }

void Rhi2dCartoDrawStyle::release_from_dc(HDC dc) {
  if (!style_on_dc_ || !dc) {
    style_on_dc_ = false;
    return;
  }
  if (h_old_brush_) {
    ::SelectObject(dc, h_old_brush_);
  }
  if (h_old_pen_) {
    ::SelectObject(dc, h_old_pen_);
  }
  if (h_old_font_) {
    ::SelectObject(dc, h_old_font_);
  }
  style_on_dc_ = false;
}

int Rhi2dCartoDrawStyle::prepare_for_drawing(HDC dc, bool recording,
                                        HINSTANCE h_inst, RenderContext* rc,
                                        bool is_river, int road_class,
                                        const Style* style, int draw_mode) {
  if (!dc && !recording) {
    return kErrInvalidParam;
  }
  if (dc) {
    ::SetROP2(dc, draw_mode);
  }

  cur_use_style_ = (nullptr != style);

  if (!cur_use_style_) {
    style_cache_valid_ = false;
    return kErrNone;
  }

  const ulong format = style->get_style_type();
  // Symbols / anno recreate every time (rare on china area/line hot path).
  if (format & (ST_SymbolDesc | ST_AnnoDesc)) {
    style_cache_valid_ = false;
  }

  int pen_px = 0;
  COLORREF stroke = 0;
  COLORREF fill = 0;
  int brush_tp = -1;
  int brush_style = 0;
  if (format & ST_PenDesc) {
    pen_px = road_class > 0 ? carto2d_road_width_px(rc->fblc, road_class)
                            : carto2d_stroke_px_kind(rc->fblc, is_river, false);
    stroke = road_class > 0
                 ? carto2d_road_fill_color(road_class)
                 : (is_river ? carto2d_river_color() : carto2d_admin_stroke());
  }
  if (format & ST_BrushDesc) {
    BrushDesc brush = style->get_brush_desc();
    brush_tp = static_cast<int>(brush.brushTp);
    brush_style = static_cast<int>(brush.lBrushStyle);
    fill = is_river ? carto2d_water_fill()
                    : carto2d_boost_fill(brush.lBrushColor);
  }

  const bool cache_hit =
      style_cache_valid_ && cache_draw_mode_ == draw_mode &&
      cache_format_ == format && cache_road_class_ == road_class &&
      cache_is_river_ == (is_river ? 1 : 0) && cache_pen_px_ == pen_px &&
      cache_pen_ == stroke && cache_fill_ == fill &&
      cache_brush_tp_ == brush_tp && cache_brush_style_ == brush_style &&
      (recording ||
       ((!(format & ST_PenDesc) || h_pen_) &&
        (!(format & ST_BrushDesc) || h_brush_)));

  if (cache_hit) {
    // Recording: same pen/brush already emitted �?skip redundant set_pen /
    // set_brush so execute does not CreatePen/DeleteObject per feature.
    // Immediate DC: re-select only if style was flushed off the HDC.
    if (!recording && !style_on_dc_) {
      if (format & ST_PenDesc) {
        h_old_pen_ = (HPEN)::SelectObject(dc, h_pen_);
      }
      if (format & ST_BrushDesc) {
        h_old_brush_ = (HBRUSH)::SelectObject(dc, h_brush_);
      }
      style_on_dc_ = true;
    }
    return kErrNone;
  }

  release_from_dc(dc);

  if (format & ST_PenDesc) {
    if (recording) {
      active_encoder()->set_pen(stroke, (std::max)(1, pen_px));
    } else {
      if (h_pen_) {
        DeleteObject(h_pen_);
        h_pen_ = nullptr;
      }
      LOGBRUSH lb = {BS_SOLID, stroke, 0};
      h_pen_ = ExtCreatePen(
          PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND,
          (std::max)(1, pen_px), &lb, 0, nullptr);
      if (!h_pen_) {
        h_pen_ = CreatePen(PS_SOLID, (std::max)(1, pen_px), stroke);
      }
      h_old_pen_ = (HPEN)::SelectObject(dc, h_pen_);
    }
  }

  if (format & ST_BrushDesc) {
    BrushDesc brush = style->get_brush_desc();
    if (recording) {
      if (brush.brushTp == BrushDesc::BT_Hatch) {
        active_encoder()->set_brush(fill, BS_HATCHED, brush.lBrushStyle);
      } else {
        active_encoder()->set_brush(fill, BS_SOLID);
      }
    } else {
      if (h_brush_) {
        DeleteObject(h_brush_);
        h_brush_ = nullptr;
      }
      if (brush.brushTp == BrushDesc::BT_Hatch) {
        h_brush_ = CreateHatchBrush(brush.lBrushStyle, fill);
      } else {
        h_brush_ = CreateSolidBrush(fill);
      }
      h_old_brush_ = (HBRUSH)::SelectObject(dc, h_brush_);
    }
  }

  if (!recording && (format & ST_SymbolDesc)) {
    SymbolDesc symbol = style->get_symbol_desc();

    if (h_icon_) {
      DeleteObject(h_icon_);
      h_icon_ = nullptr;
    }

    h_icon_ = LoadIcon(h_inst, MAKEINTRESOURCE(symbol.lSymbolID + IDI_ICON_A));
  }

  if (!recording && (format & ST_AnnoDesc)) {
    AnnotationDesc anno = style->get_anno_desc();

    if (h_font_) {
      DeleteObject(h_font_);
      h_font_ = nullptr;
    }

    h_font_ = CreateFont(anno.fHeight * rc->fblc, anno.fWidth * rc->fblc,
                         anno.lEscapement, anno.lOrientation, anno.lWeight,
                         anno.lItalic, anno.lUnderline, anno.lStrikeOut,
                         anno.lCharSet, anno.lOutPrecision, anno.lClipPrecision,
                         anno.lQuality, anno.lPitchAndFamily, anno.szFaceName);

    h_old_font_ = (HFONT)::SelectObject(dc, h_font_);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, anno.lAnnoClr);
  }

  cache_draw_mode_ = draw_mode;
  cache_format_ = format;
  cache_road_class_ = road_class;
  cache_is_river_ = is_river ? 1 : 0;
  cache_pen_px_ = pen_px;
  cache_pen_ = stroke;
  cache_fill_ = fill;
  cache_brush_tp_ = brush_tp;
  cache_brush_style_ = brush_style;
  style_cache_valid_ = !(format & (ST_SymbolDesc | ST_AnnoDesc));
  style_on_dc_ = !recording;

  return kErrNone;
}

int Rhi2dCartoDrawStyle::end_drawing(HDC dc) {
  // Keep cached pen/brush selected across consecutive same-style features.
  // Layer / DC teardown calls flush / set_dc.
  if (!style_cache_valid_) {
    release_from_dc(dc);
  }
  return kErrNone;
}

void Rhi2dCartoDrawStyle::flush(HDC dc) {
  release_from_dc(dc);
  style_cache_valid_ = false;
}

}  // namespace detail
}  // namespace scenic
