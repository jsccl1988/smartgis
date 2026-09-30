// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/gdi/paint/canvas/paint_canvas.h"

#include <math.h>

#include <algorithm>
#include <climits>
#include <cstring>
#include <span>
#include <vector>

#include "base/core/log.h"
#include "base/math/simd.h"
#include "base/memory/arena.h"
#include "gis/model/envelope.h"
#include "gis/model/feature/feature.h"
#include "legacy/gis/present/carto/style_api.h"
#include "legacy/core/macros/macros.h"
#include "legacy/core/types/types.h"
#include "legacy/render/rhi2d/impl/gdi/paint/canvas/carto_frame.h"
#include "legacy/render/rhi2d/impl/gdi/paint/canvas/device_geom.h"
#include "legacy/render/rhi2d/impl/gdi/paint/player/gdi_player.h"
#include "legacy/render/rhi2d/impl/gdi/paint/canvas/paint_encoder_tls.h"
#include "legacy/render/rhi2d/impl/gdi/res/resource.h"
#include "ogrsf_frmts.h"
#include "ximage.h"

using namespace gis;
using namespace base;
using namespace geo;

namespace render {
namespace detail {
namespace {

// Scratch POINT[] for GDI draw paths. Use heap (not tls_allocate):
// HybridOptimized TLS pools have crashed on the NThreadPool worker thread
// (corrupt PMR iterators).
POINT* alloc_gdi_points(int count) {
  if (count <= 0) {
    return nullptr;
  }
  return new (std::nothrow) POINT[static_cast<size_t>(count)];
}

void free_gdi_points(POINT* points, int count) {
  (void)count;
  delete[] points;
}

}  // namespace

void GdiPaintCanvas::set_encoder(GdiCommandEncoder* encoder) {
  // New encode pass must not inherit DC-style cache — otherwise recording
  // can skip the first set_pen/set_brush of a fresh buffer.
  if (encoder != detail::active_encoder()) {
    flush_style();
  }
  set_paint_encoder(encoder);
}

bool GdiPaintCanvas::is_recording() const {
  return paint_encoder_recording();
}

GdiPaintCanvas::GdiPaintCanvas(HINSTANCE h_inst)
    : h_inst_(h_inst),
      h_cur_dc_(NULL),
      h_font_(NULL),
      h_pen_(NULL),
      h_brush_(NULL),
      h_icon_(NULL),
      h_old_font_(NULL),
      h_old_pen_(NULL),
      h_old_brush_(NULL),
      cur_use_style_(false),
      lock_style_(false),
      anno_angle_(0.f),
      feature_type_(0),
      label_priority_(5),
      is_river_(false),
      road_class_(0),
      rc_(nullptr),
      carto2d_(nullptr),
      rd_pra_(nullptr) {
  sz_anno_[0] = '\0';
}

GdiPaintCanvas::~GdiPaintCanvas() {
  if (h_font_) {
    DeleteObject(h_font_);
    h_font_ = NULL;
  }
  if (h_pen_) {
    DeleteObject(h_pen_);
    h_pen_ = NULL;
  }
  if (h_brush_) {
    DeleteObject(h_brush_);
    h_brush_ = NULL;
  }
  if (h_icon_) {
    DeleteObject(h_icon_);
    h_icon_ = NULL;
  }
}

void GdiPaintCanvas::set_dc(HDC dc) {
  release_style_from_dc();
  h_cur_dc_ = dc;
  style_cache_valid_ = false;
}

void GdiPaintCanvas::release_style_from_dc() {
  if (!style_on_dc_ || !h_cur_dc_) {
    style_on_dc_ = false;
    return;
  }
  if (h_old_brush_) {
    ::SelectObject(h_cur_dc_, h_old_brush_);
  }
  if (h_old_pen_) {
    ::SelectObject(h_cur_dc_, h_old_pen_);
  }
  if (h_old_font_) {
    ::SelectObject(h_cur_dc_, h_old_font_);
  }
  style_on_dc_ = false;
}

HDC GdiPaintCanvas::dc() const { return h_cur_dc_; }

void GdiPaintCanvas::set_context(SmtRenderContex* rc) { rc_ = rc; }

void GdiPaintCanvas::set_carto(GdiCartoFrame* carto) { carto2d_ = carto; }

void GdiPaintCanvas::set_render_pra(const Smt2DRenderPra* pra) {
  rd_pra_ = pra;
}

char* GdiPaintCanvas::anno_buf() { return sz_anno_; }

float& GdiPaintCanvas::anno_angle() { return anno_angle_; }

int& GdiPaintCanvas::feature_type() { return feature_type_; }

int& GdiPaintCanvas::label_priority() { return label_priority_; }

bool& GdiPaintCanvas::is_river() { return is_river_; }

int& GdiPaintCanvas::road_class() { return road_class_; }

bool& GdiPaintCanvas::lock_style() { return lock_style_; }

int GdiPaintCanvas::lp_to_dp(float x, float y, long& X, long& Y) const {
  if (is_equal(rc_->windowport.m_fWWidth, 0, dEPSILON) &&
      is_equal(rc_->windowport.m_fWHeight, 0, dEPSILON) &&
      is_equal(rc_->viewport.m_fVWidth, 0, dEPSILON) &&
      is_equal(rc_->viewport.m_fVHeight, 0, dEPSILON)) {
    X = x;
    Y = y;

    return SMT_ERR_FAILURE;
  }

  X = LONG(rc_->viewport.m_fVOX + (x - rc_->windowport.m_fWOX) * rc_->fblc +
           0.5);
  Y = LONG(rc_->viewport.m_fVOY + (y - rc_->windowport.m_fWOY) * rc_->fblc +
           0.5);

  Y = rc_->viewport.m_fVHeight - Y;

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::dp_to_lp(LONG X, LONG Y, float& x, float& y) const {
  if (is_equal(rc_->windowport.m_fWWidth, 0, dEPSILON) &&
      is_equal(rc_->windowport.m_fWHeight, 0, dEPSILON) &&
      is_equal(rc_->viewport.m_fVWidth, 0, dEPSILON) &&
      is_equal(rc_->viewport.m_fVHeight, 0, dEPSILON)) {
    x = X;
    y = Y;

    return SMT_ERR_FAILURE;
  }

  Y = rc_->viewport.m_fVHeight - Y;

  x = (X - rc_->viewport.m_fVOX) / rc_->fblc + rc_->windowport.m_fWOX;
  y = (Y - rc_->viewport.m_fVOY) / rc_->fblc + rc_->windowport.m_fWOY;

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::lrect_to_drect(const fRect& frect, lRect& lrect) const {
  lp_to_dp(frect.lb.x, frect.lb.y, lrect.lb.x, lrect.lb.y);
  lp_to_dp(frect.rt.x, frect.rt.y, lrect.rt.x, lrect.rt.y);

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::drect_to_lrect(const lRect& lrect, fRect& frect) const {
  dp_to_lp(lrect.lb.x, lrect.lb.y, frect.lb.x, frect.lb.y);
  dp_to_lp(lrect.rt.x, lrect.rt.y, frect.rt.x, frect.rt.y);

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::prepare_for_drawing(const SmtStyle* style, int draw_mode) {
  const bool recording = is_recording();
  if (!h_cur_dc_ && !recording) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (h_cur_dc_) {
    ::SetROP2(h_cur_dc_, draw_mode);
  }

  cur_use_style_ = (NULL != style);

  if (!cur_use_style_) {
    style_cache_valid_ = false;
    return SMT_ERR_NONE;
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
    pen_px = road_class_ > 0
                 ? carto2d_road_width_px(rc_->fblc, road_class_)
                 : carto2d_stroke_px_kind(rc_->fblc, is_river_, false);
    stroke = road_class_ > 0
                 ? carto2d_road_fill_color(road_class_)
                 : (is_river_ ? carto2d_river_color() : carto2d_admin_stroke());
  }
  if (format & ST_BrushDesc) {
    SmtBrushDesc brush = style->get_brush_desc();
    brush_tp = static_cast<int>(brush.brushTp);
    brush_style = static_cast<int>(brush.lBrushStyle);
    fill = is_river_ ? carto2d_water_fill()
                     : carto2d_boost_fill(brush.lBrushColor);
  }

  const bool cache_hit =
      style_cache_valid_ && cache_draw_mode_ == draw_mode &&
      cache_format_ == format && cache_road_class_ == road_class_ &&
      cache_is_river_ == (is_river_ ? 1 : 0) && cache_pen_px_ == pen_px &&
      cache_pen_ == stroke && cache_fill_ == fill &&
      cache_brush_tp_ == brush_tp && cache_brush_style_ == brush_style &&
      (recording ||
       ((!(format & ST_PenDesc) || h_pen_) &&
        (!(format & ST_BrushDesc) || h_brush_)));

  if (cache_hit) {
    // Recording: same pen/brush already emitted — skip redundant set_pen /
    // set_brush so replay does not CreatePen/DeleteObject per feature.
    // Immediate DC: re-select only if style was flushed off the HDC.
    if (!recording && !style_on_dc_) {
      if (format & ST_PenDesc) {
        h_old_pen_ = (HPEN)::SelectObject(h_cur_dc_, h_pen_);
      }
      if (format & ST_BrushDesc) {
        h_old_brush_ = (HBRUSH)::SelectObject(h_cur_dc_, h_brush_);
      }
      style_on_dc_ = true;
    }
    return SMT_ERR_NONE;
  }

  release_style_from_dc();

  if (format & ST_PenDesc) {
    if (recording) {
      active_encoder()->set_pen(stroke, (std::max)(1, pen_px));
    } else {
      if (h_pen_) {
        DeleteObject(h_pen_);
        h_pen_ = NULL;
      }
      LOGBRUSH lb = {BS_SOLID, stroke, 0};
      h_pen_ = ExtCreatePen(
          PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND,
          (std::max)(1, pen_px), &lb, 0, nullptr);
      if (!h_pen_) {
        h_pen_ = CreatePen(PS_SOLID, (std::max)(1, pen_px), stroke);
      }
      h_old_pen_ = (HPEN)::SelectObject(h_cur_dc_, h_pen_);
    }
  }

  if (format & ST_BrushDesc) {
    SmtBrushDesc brush = style->get_brush_desc();
    if (recording) {
      if (brush.brushTp == SmtBrushDesc::BT_Hatch) {
        active_encoder()->set_brush(fill, BS_HATCHED, brush.lBrushStyle);
      } else {
        active_encoder()->set_brush(fill, BS_SOLID);
      }
    } else {
      if (h_brush_) {
        DeleteObject(h_brush_);
        h_brush_ = NULL;
      }
      if (brush.brushTp == SmtBrushDesc::BT_Hatch) {
        h_brush_ = CreateHatchBrush(brush.lBrushStyle, fill);
      } else {
        h_brush_ = CreateSolidBrush(fill);
      }
      h_old_brush_ = (HBRUSH)::SelectObject(h_cur_dc_, h_brush_);
    }
  }

  if (!recording && (format & ST_SymbolDesc)) {
    SmtSymbolDesc symbol = style->get_symbol_desc();

    if (h_icon_) {
      DeleteObject(h_icon_);
      h_icon_ = NULL;
    }

    h_icon_ = LoadIcon(h_inst_, MAKEINTRESOURCE(symbol.lSymbolID + IDI_ICON_A));
  }

  if (!recording && (format & ST_AnnoDesc)) {
    SmtAnnotationDesc anno = style->get_anno_desc();

    if (h_font_) {
      DeleteObject(h_font_);
      h_font_ = NULL;
    }

    h_font_ = CreateFont(anno.fHeight * rc_->fblc, anno.fWidth * rc_->fblc,
                         anno.lEscapement, anno.lOrientation, anno.lWeight,
                         anno.lItalic, anno.lUnderline, anno.lStrikeOut,
                         anno.lCharSet, anno.lOutPrecision, anno.lClipPrecision,
                         anno.lQuality, anno.lPitchAndFamily, anno.szFaceName);

    h_old_font_ = (HFONT)::SelectObject(h_cur_dc_, h_font_);
    SetBkMode(h_cur_dc_, TRANSPARENT);
    SetTextColor(h_cur_dc_, anno.lAnnoClr);
  }

  cache_draw_mode_ = draw_mode;
  cache_format_ = format;
  cache_road_class_ = road_class_;
  cache_is_river_ = is_river_ ? 1 : 0;
  cache_pen_px_ = pen_px;
  cache_pen_ = stroke;
  cache_fill_ = fill;
  cache_brush_tp_ = brush_tp;
  cache_brush_style_ = brush_style;
  style_cache_valid_ = !(format & (ST_SymbolDesc | ST_AnnoDesc));
  style_on_dc_ = !recording;

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::end_drawing() {
  // Keep cached pen/brush selected across consecutive same-style features.
  // Layer / DC teardown calls flush_style / set_dc.
  if (!style_cache_valid_) {
    release_style_from_dc();
  }
  return SMT_ERR_NONE;
}

void GdiPaintCanvas::flush_style() {
  release_style_from_dc();
  style_cache_valid_ = false;
}

int GdiPaintCanvas::draw_multi_line_string(
    const OGRMultiLineString* multi_linestring) {
  int n_lines = multi_linestring->getNumGeometries();

  int i = 0;
  while (i < n_lines) {
    draw_line_string((OGRLineString*)multi_linestring->getGeometryRef(i));
    i++;
  }

  return SMT_ERR_FAILURE;
}

int GdiPaintCanvas::draw_multi_point(const SmtStyle* style,
                                     const OGRMultiPoint* multi_point) {
  int n_points = multi_point->getNumGeometries();

  int i = 0;
  while (i < n_points) {
    draw_point(style, (OGRPoint*)multi_point->getGeometryRef(i));
    i++;
  }

  return SMT_ERR_FAILURE;
}

int GdiPaintCanvas::draw_multi_polygon(const OGRMultiPolygon* multi_polygon) {
  int n_polygons = multi_polygon->getNumGeometries();

  int i = 0;
  while (i < n_polygons) {
    draw_polygon((OGRPolygon*)multi_polygon->getGeometryRef(i));
    i++;
  }

  return SMT_ERR_FAILURE;
}

int GdiPaintCanvas::draw_point(const SmtStyle* style, const OGRPoint* point) {
  if (!point) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (!style) {
    long lX = 0;
    long lY = 0;
    lp_to_dp(point->getX(), point->getY(), lX, lY);
    return draw_device_point(static_cast<int>(lX), static_cast<int>(lY));
  }
  ulong format = style->get_style_type();
  (void)format;
  if (feature_type_ == SmtFeatureType::SmtFtAnno) {
    SmtAnnotationDesc anno = style->get_anno_desc();
    return draw_anno(sz_anno_, anno_angle_, abs(anno.fHeight), abs(anno.fWidth),
                     abs(anno.fSpace), point);
  } else if (feature_type_ == SmtFeatureType::SmtFtChildImage) {
    SmtSymbolDesc symbol = style->get_symbol_desc();
    return draw_symbol(h_icon_, symbol.fSymbolHeight, symbol.fSymbolWidth,
                       point);
  } else if (feature_type_ == SmtFeatureType::SmtFtDot) {
    long lX, lY;
    lp_to_dp(point->getX(), point->getY(), lX, lY);
    return draw_device_point(static_cast<int>(lX), static_cast<int>(lY));
  }

  return SMT_ERR_FAILURE;
}

int GdiPaintCanvas::draw_anno(const char* anno, float angle, float c_height,
                              float c_width, float c_space,
                              const OGRPoint* point) {
  if (anno == NULL || !point) return SMT_ERR_INVALID_PARAM;

  (void)c_width;
  (void)c_space;
  c_height *= rc_->fblc;

  long x = 0;
  long y = 0;
  lp_to_dp(point->getX(), point->getY(), x, y);
  x -= static_cast<long>(2 * c_height * sin(angle));
  y -= static_cast<long>(2 * c_height * cos(angle));
  const int px_h = carto2d_label_px(label_priority_, rc_->fblc);
  const MapCartoBox box = carto2d_label_box(
      static_cast<int>(x), static_cast<int>(y), anno, px_h, label_priority_);
  if (!carto2d_->try_keep_label(box)) {
    return SMT_ERR_NONE;
  }
  const int text_n = static_cast<int>(std::strlen(anno));
  if (is_recording()) {
    active_encoder()->text(static_cast<int>(x), static_cast<int>(y), anno, text_n, px_h,
                   carto2d_halo_px(label_priority_), anno_angle_);
  } else {
    detail::GdiPlayer(h_cur_dc_).draw_anno_text( x, y, anno, px_h, carto2d_halo_px(label_priority_),
                   anno_angle_);
  }

  if (rd_pra_->bShowPoint && h_cur_dc_) {
    int r = rd_pra_->lPointRaduis;
    long lX, lY;
    lp_to_dp(point->getX(), point->getY(), lX, lY);
    detail::GdiPlayer(h_cur_dc_).draw_cross( lX, lY, r);
  }

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_symbol(HICON icon, long height, long width,
                                const OGRPoint* point) {
  if (!point || (!h_cur_dc_ && !is_recording())) {
    return SMT_ERR_INVALID_PARAM;
  }
  // Icon draw needs an HDC; skip while record-only (rare on map vector path).
  if (is_recording() && !h_cur_dc_) {
    return SMT_ERR_NONE;
  }
  height *= rc_->fblc;
  width *= rc_->fblc;

  long lX, lY;
  lp_to_dp(point->getX(), point->getY(), lX, lY);
  //::DrawIcon(h_cur_dc_,pt.x-width,pt.y-height,icon);
  ::DrawIconEx(h_cur_dc_, lX - width / 2, lY + height / 2, icon, width, height,
               0, NULL, DI_NORMAL);

  if (rd_pra_->bShowPoint) {
    int r = rd_pra_->lPointRaduis;
    detail::GdiPlayer(h_cur_dc_).draw_cross( lX, lY, r);
  }

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_line_spline(const OGRLineString* spline) {
  int n_points = spline->getNumPoints();
  if (n_points < 2) return SMT_ERR_INVALID_PARAM;

  POINT* lp_point = NULL;

  lp_point = alloc_gdi_points(n_points);
  if (!lp_point) return SMT_ERR_FAILURE;

  for (int i = 0; i < n_points; i++) {
    lp_to_dp(spline->getX(i), spline->getY(i), lp_point[i].x, lp_point[i].y);
  }

  MoveToEx(h_cur_dc_, lp_point[0].x, lp_point[0].y, NULL);
  PolylineTo(h_cur_dc_, lp_point, n_points);

  free_gdi_points(lp_point, n_points);
  lp_point = nullptr;
  if (rd_pra_->bShowPoint) {
    int r = rd_pra_->lPointRaduis;
    long lX, lY;
    for (int i = 0; i < spline->getNumPoints(); i++) {
      lp_to_dp(spline->getX(i), spline->getY(i), lX, lY);
      Ellipse(h_cur_dc_, lX - r, lY - r, lX + r, lY + r);
    }
  }

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_line_string(const OGRLineString* linestring) {
  if (!linestring || !rc_ || (!h_cur_dc_ && !is_recording())) {
    return SMT_ERR_INVALID_PARAM;
  }
  const int n_points = linestring->getNumPoints();
  if (n_points < 2) {
    return SMT_ERR_INVALID_PARAM;
  }

  thread_local std::vector<float> xy;
  thread_local std::vector<POINT> projected;
  thread_local std::vector<POINT> thinned;

  xy.resize(static_cast<size_t>(n_points) * 2u);
  for (int i = 0; i < n_points; ++i) {
    xy[static_cast<size_t>(i) * 2u] = static_cast<float>(linestring->getX(i));
    xy[static_cast<size_t>(i) * 2u + 1u] =
        static_cast<float>(linestring->getY(i));
  }

  projected.resize(static_cast<size_t>(n_points));
  const LpToDp2 xform = make_lp_to_dp(*rc_);
  transform_xy_batch(
      xform,
      std::span<const float>(xy.data(), static_cast<size_t>(n_points) * 2u),
      std::span<long>(reinterpret_cast<long*>(projected.data()),
                      static_cast<size_t>(n_points) * 2u));

  thinned.clear();
  thinned.reserve(static_cast<size_t>(n_points));
  const int kept = thin_device_polyline(projected.data(), n_points, &thinned,
                                        overview_thin_chebyshev(rc_->fblc));
  if (kept < 2) {
    return SMT_ERR_NONE;
  }
  return draw_device_polyline(thinned.data(), kept);
}

int GdiPaintCanvas::draw_device_polyline(const POINT* pts, int n) {
  if (!pts || n < 2 || !rc_ || (!h_cur_dc_ && !is_recording())) {
    return SMT_ERR_INVALID_PARAM;
  }

  const bool recording = is_recording();

  if (rd_pra_ && rd_pra_->bShowPoint && !recording) {
    constexpr int kMaxDebugCrosses = 48;
    const int r = rd_pra_->lPointRaduis > 0 ? rd_pra_->lPointRaduis : 3;
    const int n_draw = (std::min)(n, kMaxDebugCrosses);
    for (int i = 0; i < n_draw; ++i) {
      detail::GdiPlayer(h_cur_dc_).draw_cross( pts[i].x, pts[i].y, r);
    }
  }

  // Roads: same dual-GDI-pen path for encode and immediate (GdiPlayer).
  bool drew = false;
  if (road_class_ > 0) {
    const int fill_w = carto2d_road_width_px(rc_->fblc, road_class_);
    const COLORREF fill_c = carto2d_road_fill_color(road_class_);
    const COLORREF case_c = carto2d_road_casing_color(road_class_);
    const int case_w = fill_w + 2;
    if (recording) {
      if (fill_w <= 1) {
        active_encoder()->set_pen(fill_c, 1);
        active_encoder()->polyline(pts, n);
      } else {
        active_encoder()->set_pen(case_c, case_w);
        active_encoder()->polyline(pts, n);
        active_encoder()->set_pen(fill_c, fill_w);
        active_encoder()->polyline(pts, n);
      }
      drew = true;
    } else {
      release_style_from_dc();
      style_cache_valid_ = false;
      detail::GdiPlayer(h_cur_dc_)
          .road_polyline(pts, n, case_c, case_w, fill_c, fill_w);
      drew = true;
    }
  }
  if (!drew) {
    if (recording) {
      active_encoder()->polyline(pts, n);
    } else {
      detail::GdiPlayer(h_cur_dc_).polyline(pts, n);
    }
  }

  if (carto2d_ && (is_river_ || road_class_ > 0) && sz_anno_[0]) {
    thread_local std::vector<int> label_xy;
    label_xy.clear();
    label_xy.reserve(static_cast<size_t>(n) * 2u);
    for (int i = 0; i < n; ++i) {
      label_xy.push_back(static_cast<int>(pts[i].x));
      label_xy.push_back(static_cast<int>(pts[i].y));
    }
    MapCartoLineLabel pose;
    if (carto2d_line_label_pose(label_xy.data(), n, &pose)) {
      const int px_h = carto2d_label_px(label_priority_, rc_->fblc);
      const MapCartoBox box = carto2d_label_box_rotated(
          pose.x, pose.y, sz_anno_, px_h, label_priority_, pose.angle_deg);
      if (carto2d_->try_keep_label(box)) {
        const int text_n = static_cast<int>(std::strlen(sz_anno_));
        if (recording) {
          active_encoder()->text(pose.x, pose.y, sz_anno_, text_n, px_h,
                         carto2d_halo_px(label_priority_), pose.angle_deg);
        } else {
          detail::GdiPlayer(h_cur_dc_).draw_anno_text( pose.x, pose.y, sz_anno_, px_h,
                         carto2d_halo_px(label_priority_), pose.angle_deg);
        }
      }
    }
  }

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_device_polylines(const POINT* pts,
                                          const int* poly_counts, int n_polys) {
  if (!pts || !poly_counts || n_polys < 1 || !rc_ ||
      (!h_cur_dc_ && !is_recording())) {
    return SMT_ERR_INVALID_PARAM;
  }
  // Roads and river labels need the single-polyline path (pen / label).
  if (road_class_ > 0) {
    size_t offset = 0;
    for (int i = 0; i < n_polys; ++i) {
      const int n = poly_counts[i];
      if (n >= 2) {
        draw_device_polyline(pts + offset, n);
      }
      offset += static_cast<size_t>((std::max)(0, n));
    }
    return SMT_ERR_NONE;
  }
  if (is_recording()) {
    if (!active_encoder()->poly_polyline(pts, poly_counts, n_polys)) {
      return SMT_ERR_FAILURE;
    }
  } else {
    detail::GdiPlayer(h_cur_dc_).poly_polyline(pts, poly_counts, n_polys);
  }
  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_linear_ring(const OGRLinearRing* linear_ring) {
  int n_points = linear_ring->getNumPoints();
  if (n_points < 2) return SMT_ERR_INVALID_PARAM;
  if (!h_cur_dc_ && !is_recording()) {
    return SMT_ERR_INVALID_PARAM;
  }

  POINT* lp_point = NULL;
  lp_point = alloc_gdi_points(n_points);
  if (!lp_point) return SMT_ERR_FAILURE;

  if (rd_pra_->bShowPoint && h_cur_dc_) {
    int r = rd_pra_->lPointRaduis;
    for (int i = 0; i < linear_ring->getNumPoints(); i++) {
      lp_to_dp(linear_ring->getX(i), linear_ring->getY(i), lp_point[i].x,
               lp_point[i].y);
      detail::GdiPlayer(h_cur_dc_).draw_cross( lp_point[i].x, lp_point[i].y, r);
    }
  } else {
    for (int i = 0; i < n_points; i++) {
      lp_to_dp(linear_ring->getX(i), linear_ring->getY(i), lp_point[i].x,
               lp_point[i].y);
    }
  }

  if (is_recording()) {
    active_encoder()->polyline(lp_point, n_points);
  } else {
    MoveToEx(h_cur_dc_, lp_point[0].x, lp_point[0].y, NULL);
    PolylineTo(h_cur_dc_, lp_point, n_points);
  }

  free_gdi_points(lp_point, n_points);
  lp_point = nullptr;

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_polygon(const OGRPolygon* polygon) {
  if (!polygon || !rc_ || (!h_cur_dc_ && !is_recording())) {
    return SMT_ERR_INVALID_PARAM;
  }
  const OGRLinearRing* exterior = polygon->getExteriorRing();
  if (!exterior) {
    return SMT_ERR_INVALID_PARAM;
  }
  const int n_exterior_pts = exterior->getNumPoints();
  if (n_exterior_pts < 2) {
    return SMT_ERR_INVALID_PARAM;
  }

  const int n_interior_rings = polygon->getNumInteriorRings();
  int n_all_pts = n_exterior_pts;
  for (int i = 0; i < n_interior_rings; ++i) {
    const OGRLinearRing* interior = polygon->getInteriorRing(i);
    if (!interior) {
      continue;
    }
    n_all_pts += interior->getNumPoints();
  }
  if (n_all_pts < 2) {
    return SMT_ERR_INVALID_PARAM;
  }

  // Reuse scratch across features on this worker thread (avoid new/delete).
  thread_local std::vector<float> xy;
  thread_local std::vector<POINT> projected;
  thread_local std::vector<POINT> thinned;
  thread_local std::vector<int> ring_counts;
  thread_local std::vector<int> ring_starts;

  xy.resize(static_cast<size_t>(n_all_pts) * 2u);
  ring_starts.clear();
  ring_starts.reserve(static_cast<size_t>(n_interior_rings) + 1u);

  int n_count = 0;
  auto append_ring = [&](const OGRLinearRing* ring) {
    const int n = ring->getNumPoints();
    ring_starts.push_back(n_count);
    for (int i = 0; i < n; ++i, ++n_count) {
      xy[static_cast<size_t>(n_count) * 2u] = static_cast<float>(ring->getX(i));
      xy[static_cast<size_t>(n_count) * 2u + 1u] =
          static_cast<float>(ring->getY(i));
    }
    return n;
  };

  append_ring(exterior);
  for (int i = 0; i < n_interior_rings; ++i) {
    const OGRLinearRing* interior = polygon->getInteriorRing(i);
    if (interior) {
      append_ring(interior);
    }
  }
  const int ring_n = static_cast<int>(ring_starts.size());
  if (ring_n < 1 || n_count < 2) {
    return SMT_ERR_INVALID_PARAM;
  }

  projected.resize(static_cast<size_t>(n_count));
  const LpToDp2 xform = make_lp_to_dp(*rc_);
  transform_xy_batch(
      xform,
      std::span<const float>(xy.data(), static_cast<size_t>(n_count) * 2u),
      std::span<long>(reinterpret_cast<long*>(projected.data()),
                      static_cast<size_t>(n_count) * 2u));

  thinned.clear();
  ring_counts.clear();
  thinned.reserve(static_cast<size_t>(n_count));
  ring_counts.reserve(static_cast<size_t>(ring_n));
  for (int r = 0; r < ring_n; ++r) {
    const int start = ring_starts[static_cast<size_t>(r)];
    const int end =
        (r + 1 < ring_n) ? ring_starts[static_cast<size_t>(r + 1)] : n_count;
    const size_t base = thinned.size();
    const int kept =
        thin_device_ring(projected.data() + start, end - start, &thinned,
                         overview_thin_chebyshev(rc_->fblc));
    if (kept < 3) {
      thinned.resize(base);
      if (r == 0) {
        return SMT_ERR_NONE;
      }
      continue;
    }
    ring_counts.push_back(kept);
  }
  if (ring_counts.empty()) {
    return SMT_ERR_NONE;
  }
  return draw_device_polygon(thinned.data(), ring_counts.data(),
                             static_cast<int>(ring_counts.size()));
}

int GdiPaintCanvas::draw_device_polygon(const POINT* pts,
                                        const int* ring_counts, int n_rings) {
  if (!pts || !ring_counts || n_rings < 1 ||
      (!h_cur_dc_ && !is_recording())) {
    return SMT_ERR_INVALID_PARAM;
  }

  if (is_recording()) {
    active_encoder()->poly_polygon(pts, ring_counts, n_rings);
    return SMT_ERR_NONE;
  }

  if (rd_pra_ && rd_pra_->bShowPoint) {
    // Debug overlay: cap crosses so dense china rings cannot dominate paint.
    constexpr int kMaxDebugCrosses = 48;
    const int r = rd_pra_->lPointRaduis > 0 ? rd_pra_->lPointRaduis : 3;
    int total = 0;
    for (int i = 0; i < n_rings; ++i) {
      total += ring_counts[i];
    }
    const int n_draw = (std::min)(total, kMaxDebugCrosses);
    for (int i = 0; i < n_draw; ++i) {
      detail::GdiPlayer(h_cur_dc_).draw_cross( pts[i].x, pts[i].y, r);
    }
  }

  ::PolyPolygon(h_cur_dc_, const_cast<POINT*>(pts),
                const_cast<int*>(ring_counts), n_rings);
  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_device_point(int x, int y) {
  if (!rc_ || !carto2d_ || (!h_cur_dc_ && !is_recording())) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (!carto2d_->try_keep_point(x, y)) {
    return SMT_ERR_NONE;
  }
  const int radius = carto2d_point_radius(rc_->fblc);
  if (is_recording()) {
    active_encoder()->ellipse(x - radius, y - radius, x + radius, y + radius);
  } else {
    detail::GdiPlayer(h_cur_dc_).draw_point_disc( x, y, radius);
  }
  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_device_anno(int x, int y, const char* text) {
  if (!rc_ || !carto2d_ || !text || !text[0] ||
      (!h_cur_dc_ && !is_recording())) {
    return SMT_ERR_INVALID_PARAM;
  }
  // Match draw_anno offset using carto px height (style fHeight*fblc ~= same).
  const int px_h = carto2d_label_px(label_priority_, rc_->fblc);
  const float c_height = static_cast<float>(px_h);
  long dx = x - static_cast<long>(2 * c_height * sin(anno_angle_));
  long dy = y - static_cast<long>(2 * c_height * cos(anno_angle_));
  const MapCartoBox box = carto2d_label_box(
      static_cast<int>(dx), static_cast<int>(dy), text, px_h, label_priority_);
  if (!carto2d_->try_keep_label(box)) {
    return SMT_ERR_NONE;
  }
  const int text_n = static_cast<int>(std::strlen(text));
  if (is_recording()) {
    active_encoder()->text(static_cast<int>(dx), static_cast<int>(dy), text, text_n,
                   px_h, carto2d_halo_px(label_priority_), anno_angle_);
  } else {
    detail::GdiPlayer(h_cur_dc_).draw_anno_text( dx, dy, text, px_h,
                   carto2d_halo_px(label_priority_), anno_angle_);
  }
  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_tin(const SmtTin* tin) {
  draw_tin_lines(tin);

  // if (rd_pra_->bShowPoint)
  {
    draw_tin_nodes(tin);
  }

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_tin_lines(const SmtTin* tin) {
  POINT lPt1, lPt2, lPt3;
  OGRPoint oPt1, oPt2, oPt3;

  Envelope envTri, envViewp;
  lRect lViewp;
  fRect fViewp;

  viewport_to_rect(lViewp, rc_->viewport);
  drect_to_lrect(lViewp, fViewp);
  rect_to_envelope(envViewp, fViewp);

  for (int i = 0; i < tin->get_triangle_count(); i++) {
    SmtTriangle tri = tin->get_triangle(i);

    if (!tri.bDelete) {
      oPt1 = tin->get_point(tri.a);
      oPt2 = tin->get_point(tri.b);
      oPt3 = tin->get_point(tri.c);

      envTri.merge(oPt1.getX(), oPt1.getY());
      envTri.merge(oPt2.getX(), oPt2.getY());
      envTri.merge(oPt3.getX(), oPt3.getY());

      if (envTri.intersects(envViewp)) {
        lp_to_dp(oPt1.getX(), oPt1.getY(), lPt1.x, lPt1.y);
        lp_to_dp(oPt2.getX(), oPt2.getY(), lPt2.x, lPt2.y);
        lp_to_dp(oPt3.getX(), oPt3.getY(), lPt3.x, lPt3.y);

        MoveToEx(h_cur_dc_, lPt1.x, lPt1.y, NULL);
        LineTo(h_cur_dc_, lPt2.x, lPt2.y);
        LineTo(h_cur_dc_, lPt3.x, lPt3.y);
        LineTo(h_cur_dc_, lPt1.x, lPt1.y);
      }
    }
  }

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_tin_nodes(const SmtTin* tin) {
  POINT lPt;
  OGRPoint oPt;
  int r = rd_pra_->lPointRaduis;

  lRect lViewp;
  fRect fViewp;

  viewport_to_rect(lViewp, rc_->viewport);
  drect_to_lrect(lViewp, fViewp);
  fViewp.normalize();

  for (int i = 0; i < tin->get_point_count(); i++) {
    oPt = tin->get_point(i);
    if (fViewp.contains(static_cast<float>(oPt.getX()),
                        static_cast<float>(oPt.getY()))) {
      lp_to_dp(oPt.getX(), oPt.getY(), lPt.x, lPt.y);
      Ellipse(h_cur_dc_, lPt.x - r, lPt.y - r, lPt.x + r, lPt.y + r);
    }
  }

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_grid(const SmtGrid* grid) {
  draw_grid_lines(grid);

  // if (rd_pra_->bShowPoint)
  {
    draw_grid_nodes(grid);
  }

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_grid_lines(const SmtGrid* grid) {
  int nM, nN;
  grid->get_size(nM, nN);

  POINT lPt;

  for (int j = 0; j < nN; j++) {
    RawPoint rawPt = grid->node(0, j);
    lp_to_dp(rawPt.x, rawPt.y, lPt.x, lPt.y);
    MoveToEx(h_cur_dc_, lPt.x, lPt.y, NULL);
    for (int i = 0; i < nM; i++) {
      RawPoint rawPt1 = grid->node(i, j);
      lp_to_dp(rawPt1.x, rawPt1.y, lPt.x, lPt.y);
      LineTo(h_cur_dc_, lPt.x, lPt.y);
    }
  }

  for (int i = 0; i < nM; i++) {
    RawPoint rawPt = grid->node(i, 0);
    lp_to_dp(rawPt.x, rawPt.y, lPt.x, lPt.y);
    MoveToEx(h_cur_dc_, lPt.x, lPt.y, NULL);

    for (int j = 0; j < nN; j++) {
      RawPoint rawPt1 = grid->node(i, j);
      lp_to_dp(rawPt1.x, rawPt1.y, lPt.x, lPt.y);
      LineTo(h_cur_dc_, lPt.x, lPt.y);
    }
  }

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_grid_nodes(const SmtGrid* grid) {
  int nM, nN;
  grid->get_size(nM, nN);

  int r = rd_pra_->lPointRaduis;
  POINT lPt;

  for (int j = 0; j < nN; j++) {
    for (int i = 0; i < nM; i++) {
      RawPoint rawPt = grid->node(i, j);
      lp_to_dp(rawPt.x, rawPt.y, lPt.x, lPt.y);
      Ellipse(h_cur_dc_, lPt.x - r, lPt.y - r, lPt.x + r, lPt.y + r);
    }
  }

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_fan(const OGRPolygon* fan) {
  return draw_polygon(fan);
#if 0
  // Legacy Pie path retained in source thread; not active.
#endif
}

int GdiPaintCanvas::draw_arc(const OGRLineString* arc) {
  return draw_line_string(arc);
#if 0
  // Legacy Arc path retained in source thread; not active.
#endif
}

int GdiPaintCanvas::draw_ellipse(float left, float top, float right,
                                 float bottom, bool b_dp) {
  if (!h_cur_dc_ && !is_recording()) {
    return SMT_ERR_INVALID_PARAM;
  }
  lRect lrect;

  fRect frect;
  frect.lb.x = left;
  frect.lb.y = bottom;
  frect.rt.x = right;
  frect.rt.y = top;

  if (!b_dp)
    lrect_to_drect(frect, lrect);
  else
    lrect = frect.cast_to<long>();

  if (is_recording()) {
    active_encoder()->ellipse(lrect.lb.x, lrect.lb.y, lrect.rt.x, lrect.rt.y);
  } else {
    Ellipse(h_cur_dc_, lrect.lb.x, lrect.lb.y, lrect.rt.x, lrect.rt.y);
  }

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_rect(const fRect& rect, bool b_dp) {
  if (!h_cur_dc_ && !is_recording()) {
    return SMT_ERR_INVALID_PARAM;
  }
  lRect tmpRectDP = rect.cast_to<long>();

  if (!b_dp) {
    fRect tmpRectLP = tmpRectDP.cast_to<float>();
    lrect_to_drect(tmpRectLP, tmpRectDP);
  }

  POINT pts[5] = {
      {tmpRectDP.lb.x, tmpRectDP.lb.y},
      {tmpRectDP.rt.x, tmpRectDP.lb.y},
      {tmpRectDP.rt.x, tmpRectDP.rt.y},
      {tmpRectDP.lb.x, tmpRectDP.rt.y},
      {tmpRectDP.lb.x, tmpRectDP.lb.y},
  };
  if (is_recording()) {
    active_encoder()->polyline(pts, 5);
  } else {
    MoveToEx(h_cur_dc_, tmpRectDP.lb.x, tmpRectDP.lb.y, NULL);
    LineTo(h_cur_dc_, tmpRectDP.rt.x, tmpRectDP.lb.y);
    LineTo(h_cur_dc_, tmpRectDP.rt.x, tmpRectDP.rt.y);
    LineTo(h_cur_dc_, tmpRectDP.lb.x, tmpRectDP.rt.y);
    LineTo(h_cur_dc_, tmpRectDP.lb.x, tmpRectDP.lb.y);
  }

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_line(fPoint* points, int count, bool b_dp) {
  int n_points = count;
  if (n_points < 2) return SMT_ERR_INVALID_PARAM;
  if (!h_cur_dc_ && !is_recording()) {
    return SMT_ERR_INVALID_PARAM;
  }

  POINT* lp_point = NULL;

  lp_point = alloc_gdi_points(n_points);
  if (!lp_point) return SMT_ERR_FAILURE;

  if (rd_pra_->bShowPoint && h_cur_dc_) {
    int r = rd_pra_->lPointRaduis;
    if (!b_dp) {
      for (int i = 0; i < n_points; i++) {
        lp_to_dp(points[i].x, points[i].y, lp_point[i].x, lp_point[i].y);
        detail::GdiPlayer(h_cur_dc_).draw_cross( lp_point[i].x, lp_point[i].y, r);
      }
    } else {
      for (int i = 0; i < n_points; i++) {
        lp_point[i].x = points[i].x;
        lp_point[i].y = points[i].y;
        detail::GdiPlayer(h_cur_dc_).draw_cross( lp_point[i].x, lp_point[i].y, r);
      }
    }
  } else {
    if (!b_dp) {
      for (int i = 0; i < n_points; i++) {
        lp_to_dp(points[i].x, points[i].y, lp_point[i].x, lp_point[i].y);
      }
    } else {
      for (int i = 0; i < n_points; i++) {
        lp_point[i].x = points[i].x;
        lp_point[i].y = points[i].y;
      }
    }
  }

  if (is_recording()) {
    active_encoder()->polyline(lp_point, n_points);
  } else {
    MoveToEx(h_cur_dc_, lp_point[0].x, lp_point[0].y, NULL);
    PolylineTo(h_cur_dc_, lp_point, n_points);
  }

  free_gdi_points(lp_point, n_points);
  lp_point = nullptr;

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_line(const fPoint& pt_a, const fPoint& pt_b,
                              bool b_dp) {
  if (!h_cur_dc_ && !is_recording()) {
    return SMT_ERR_INVALID_PARAM;
  }
  lPoint pt1(pt_a.x, pt_a.y), pt2(pt_b.x, pt_b.y);

  if (!b_dp) {
    lp_to_dp(pt_a.x, pt_a.y, pt1.x, pt1.y);
    lp_to_dp(pt_b.x, pt_b.y, pt2.x, pt2.y);
  }

  POINT pts[2] = {{pt1.x, pt1.y}, {pt2.x, pt2.y}};
  if (is_recording()) {
    active_encoder()->polyline(pts, 2);
  } else {
    MoveToEx(h_cur_dc_, pt1.x, pt1.y, NULL);
    LineTo(h_cur_dc_, pt2.x, pt2.y);
  }

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_text(const char* anno, float angle, float c_height,
                              float c_width, float c_space, const fPoint& point,
                              bool b_dp) {
  if (anno == NULL) return SMT_ERR_INVALID_PARAM;

  c_height *= rc_->fblc;
  c_width *= rc_->fblc;
  c_space *= rc_->fblc;

  unsigned char c1, c2;
  fPoint pt;
  long x, y;
  char bz[4];
  const char* ls1;
  ls1 = anno;

  if (!b_dp) {
    lp_to_dp(point.x, point.y, x, y);
    pt.x = x;
    pt.y = y;
  } else {
    pt.x = point.x;
    pt.y = point.y;
  }

  pt.x -= 2 * c_height * sin(angle);
  pt.y -= 2 * c_height * cos(angle);

  int n_str_length = (int)strlen(ls1);
  while (n_str_length > 0) {
    c1 = *ls1;
    c2 = *(ls1 + 1);
    if (c1 > 127 && c2 > 127) {
      strncpy(bz, ls1, 2);
      bz[2] = 0;
      ls1 = ls1 + 2;
      TextOut(h_cur_dc_, pt.x, pt.y, (LPCSTR)bz, 2);
      n_str_length -= 2;
      pt.x += (c_width * 2 + c_space) * cos(angle);
      pt.y += (c_width * 2 + c_space) * sin(angle);
    } else {
      strncpy(bz, ls1, 1);
      bz[1] = 0;
      ls1++;
      TextOut(h_cur_dc_, pt.x, pt.y, (LPCSTR)bz, 1);
      n_str_length -= 1;

      pt.x += (c_width + c_space / 2.) * cos(angle);
      pt.y += (c_width + c_space / 2.) * sin(angle);
    }
  }

  if (rd_pra_->bShowPoint) {
    int r = rd_pra_->lPointRaduis;
    long lX, lY;
    if (!b_dp) {
      lp_to_dp(point.x, point.y, lX, lY);
    } else {
      lX = point.x;
      lY = point.y;
    }

    detail::GdiPlayer(h_cur_dc_).draw_cross( lX, lY, r);
  }

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::draw_image(const char* image_buf, int image_buf_size,
                               const fRect& frect, long code_type) {
  lRect lrt;
  lrect_to_drect(frect, lrt);

  CxImage tmpImage;
  tmpImage.Decode((BYTE*)image_buf, image_buf_size, code_type);
  tmpImage.Draw(h_cur_dc_, lrt.lb.x, lrt.rt.y, lrt.width(), lrt.height());

  return SMT_ERR_NONE;
}

int GdiPaintCanvas::stretch_image(const char* image_buf, int image_buf_size,
                                  const fRect& frect, long code_type) {
  lRect lrt;
  lrect_to_drect(frect, lrt);

  CxImage tmpImage;
  tmpImage.Decode((BYTE*)image_buf, image_buf_size, code_type);
  tmpImage.Stretch(h_cur_dc_, lrt.lb.x, lrt.rt.y, lrt.width(), lrt.height());

  return SMT_ERR_NONE;
}

}  // namespace detail
}  // namespace render
