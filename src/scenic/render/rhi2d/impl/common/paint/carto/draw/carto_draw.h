// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_CARTO_DRAW_H_
#define SCENIC_RHI2D_CARTO_DRAW_H_

#include <cstddef>

#include "plugin/product/world3d/grid/orthogrid/lattice/ortho_lattice.h"
#include "gis/feature/feature.h"
#include "scenic/detail/style.h"
#include "scenic/detail/viewport.h"
#include "scenic/detail/err.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/encode/command_encoder.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/frame/carto_frame.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/draw_device.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/draw_mesh.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/draw_ogr.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/draw_primitives.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/style/style.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/style/xform.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/frame/context.h"

class OGRLineString;
class OGRLinearRing;
class OGRMultiLineString;
class OGRMultiPoint;
class OGRMultiPolygon;
class OGRPoint;
class OGRPolygon;

using namespace base;

namespace scenic {

namespace detail {

// Thin leftover carto draw façade: owns DC/context links and composes
// style / xform / device / ogr / mesh / primitives collaborators.
class Rhi2dCartoDraw {
 public:
  explicit Rhi2dCartoDraw(HINSTANCE h_inst);
  ~Rhi2dCartoDraw();

  Rhi2dCartoDraw(const Rhi2dCartoDraw&) = delete;
  Rhi2dCartoDraw& operator=(const Rhi2dCartoDraw&) = delete;

  HINSTANCE h_inst() const { return h_inst_; }

  void set_dc(HDC dc);
  HDC dc() const;
  // Binds the thread-local record target (non-owning). Null clears.
  void set_encoder(Rhi2dCommandEncoder* encoder);
  bool is_recording() const;
  void set_context(RenderContext* rc);  // non-owning; required for LPToDP
  void set_carto(GdiCartoFrame* carto);   // non-owning
  void set_render_options(const RenderOptions2d* options);  // non-owning

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

  int prepare_for_drawing(const Style* style, int draw_mode = R2_COPYPEN);
  int end_drawing();
  // Drop cached pen/brush from the DC (call at end of map/layer paint).
  void flush_style();

  int draw_multi_line_string(const OGRMultiLineString* multi_linestring);
  int draw_line_spline(const OGRLineString* spline);
  int draw_multi_point(const Style* style, const OGRMultiPoint* multi_point);
  int draw_multi_polygon(const OGRMultiPolygon* multi_polygon);

  int draw_point(const Style* style, const OGRPoint* point);
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

  int draw_tin(const OGRTriangulatedSurface* tin);
  int draw_tin_lines(const OGRTriangulatedSurface* tin);
  int draw_tin_nodes(const OGRTriangulatedSurface* tin);

  int draw_grid(const plugin::detail::OrthoLattice* grid);
  int draw_grid_lines(const plugin::detail::OrthoLattice* grid);
  int draw_grid_nodes(const plugin::detail::OrthoLattice* grid);

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

  // Out-of-line in carto_draw.cc so collaborator TUs can detect partial-rebuild
  // ODR when style_/xform_ size changes (stale objs read xform floats as
  // rd_options_ �?AV in draw_device_polyline).
  static std::size_t rd_options_offset();

 private:
  friend class GdiDeviceDraw;
  friend class GdiMeshDraw;
  friend class GdiOgrDraw;
  friend class GdiPrimitivesDraw;

  HINSTANCE h_inst_;
  HDC h_cur_dc_;

  bool lock_style_;

  char sz_anno_[2000];
  float anno_angle_;
  int feature_type_;
  int label_priority_;
  bool is_river_;
  int road_class_;

  Rhi2dCartoDrawStyle style_;
  Rhi2dCartoDrawXform xform_;
  GdiDeviceDraw device_draw_;
  GdiOgrDraw ogr_draw_;
  GdiMeshDraw mesh_draw_;
  GdiPrimitivesDraw primitives_draw_;

  RenderContext* rc_;
  GdiCartoFrame* carto2d_;
  const RenderOptions2d* rd_options_;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_CARTO_DRAW_H_
