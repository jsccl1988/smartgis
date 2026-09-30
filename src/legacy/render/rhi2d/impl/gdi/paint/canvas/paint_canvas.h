// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_GDI_PAINT_CANVAS_H_
#define SMT_LEGACY_RENDER_GDI_PAINT_CANVAS_H_

#include "gis/kernel/geo/mesh/geometry.h"
#include "gis/model/feature/feature.h"
#include "legacy/gis/present/carto/style.h"
#include "legacy/gis/present/carto/style_bas_struct.h"
#include "legacy/core/macros/macros.h"
#include "legacy/render/rhi2d/impl/gdi/paint/encode/command_encoder.h"
#include "legacy/render/rhi2d/impl/gdi/paint/canvas/carto_frame.h"
#include "legacy/render/rhi2d/impl/gdi/paint/canvas/paint_context.h"

class OGRLineString;
class OGRLinearRing;
class OGRMultiLineString;
class OGRMultiPoint;
class OGRMultiPolygon;
class OGRPoint;
class OGRPolygon;

using namespace base;
using namespace geo;

namespace render {

namespace detail {

// Owns GDI style objects and implements leftover carto draw primitives.
class GdiPaintCanvas {
 public:
  explicit GdiPaintCanvas(HINSTANCE h_inst);
  ~GdiPaintCanvas();

  GdiPaintCanvas(const GdiPaintCanvas&) = delete;
  GdiPaintCanvas& operator=(const GdiPaintCanvas&) = delete;

  HINSTANCE h_inst() const { return h_inst_; }

  void set_dc(HDC dc);
  HDC dc() const;
  // Binds the thread-local record target (non-owning). Null clears.
  void set_encoder(GdiCommandEncoder* encoder);
  bool is_recording() const;
  void set_context(SmtRenderContex* rc);  // non-owning; required for LPToDP
  void set_carto(GdiCartoFrame* carto);   // non-owning
  void set_render_pra(const Smt2DRenderPra* pra);  // non-owning

  // Feature annotate / carto fields used by map painter
  char* anno_buf();
  float& anno_angle();
  int& feature_type();
  int& label_priority();
  bool& is_river();
  int& road_class();
  bool& lock_style();

  int lp_to_dp(float x, float y, long& X, long& Y) const;
  int dp_to_lp(LONG X, LONG Y, float& x, float& y) const;
  int lrect_to_drect(const fRect& frect, lRect& lrect) const;
  int drect_to_lrect(const lRect& lrect, fRect& frect) const;

  int prepare_for_drawing(const SmtStyle* style, int draw_mode = R2_COPYPEN);
  int end_drawing();
  // Drop cached pen/brush from the DC (call at end of map/layer paint).
  void flush_style();

  int draw_multi_line_string(const OGRMultiLineString* multi_linestring);
  int draw_line_spline(const OGRLineString* spline);
  int draw_multi_point(const SmtStyle* style, const OGRMultiPoint* multi_point);
  int draw_multi_polygon(const OGRMultiPolygon* multi_polygon);

  int draw_point(const SmtStyle* style, const OGRPoint* point);
  int draw_anno(const char* anno, float angle, float c_height, float c_width,
                float c_space, const OGRPoint* point);
  int draw_symbol(HICON icon, long height, long width, const OGRPoint* point);

  int draw_line_string(const OGRLineString* linestring);
  int draw_linear_ring(const OGRLinearRing* linear_ring);
  int draw_polygon(const OGRPolygon* polygon);

  // Device-space playback after parallel CPU prep (no LP→DP).
  int draw_device_polyline(const POINT* pts, int n);
  // Multiple open polylines (same pen). Roads / line labels stay on
  // draw_device_polyline.
  int draw_device_polylines(const POINT* pts, const int* poly_counts,
                            int n_polys);
  int draw_device_polygon(const POINT* pts, const int* ring_counts,
                          int n_rings);
  int draw_device_point(int x, int y);
  int draw_device_anno(int x, int y, const char* text);

  int draw_tin(const SmtTin* tin);
  int draw_tin_lines(const SmtTin* tin);
  int draw_tin_nodes(const SmtTin* tin);

  int draw_grid(const SmtGrid* grid);
  int draw_grid_lines(const SmtGrid* grid);
  int draw_grid_nodes(const SmtGrid* grid);

  int draw_arc(const OGRLineString* arc);
  int draw_fan(const OGRPolygon* fan);

  int draw_ellipse(float left, float top, float right, float bottom,
                   bool b_dp = false);
  int draw_rect(const fRect& rect, bool b_dp = false);
  int draw_line(fPoint* points, int count, bool b_dp = false);
  int draw_line(const fPoint& pt_a, const fPoint& pt_b, bool b_dp = false);
  int draw_text(const char* anno, float angle, float c_height, float c_width,
                float c_space, const fPoint& point, bool b_dp = false);
  int draw_image(const char* image_buf, int image_buf_size, const fRect& frect,
                 long code_type);
  int stretch_image(const char* image_buf, int image_buf_size,
                    const fRect& frect, long code_type);

 private:
  HINSTANCE h_inst_;
  HDC h_cur_dc_;

  HFONT h_font_;
  HPEN h_pen_;
  HBRUSH h_brush_;
  HICON h_icon_;

  HFONT h_old_font_;
  HPEN h_old_pen_;
  HBRUSH h_old_brush_;
  bool cur_use_style_;
  bool lock_style_;

  char sz_anno_[2000];
  float anno_angle_;
  int feature_type_;
  int label_priority_;
  bool is_river_;
  int road_class_;

  // Skip ExtCreatePen / CreateBrush when consecutive features share
  // stroke/fill. When cache hits, keep pen/brush selected on the DC across
  // features.
  bool style_cache_valid_ = false;
  bool style_on_dc_ = false;
  int cache_draw_mode_ = 0;
  ulong cache_format_ = 0;
  int cache_road_class_ = -1;
  int cache_is_river_ = 0;
  int cache_pen_px_ = 0;
  COLORREF cache_pen_ = 0;
  COLORREF cache_fill_ = 0;
  int cache_brush_tp_ = -1;
  int cache_brush_style_ = 0;

  void release_style_from_dc();

  SmtRenderContex* rc_;
  GdiCartoFrame* carto2d_;
  const Smt2DRenderPra* rd_pra_;
};

}  // namespace detail
}  // namespace render

#endif  // SMT_LEGACY_RENDER_GDI_PAINT_CANVAS_H_
