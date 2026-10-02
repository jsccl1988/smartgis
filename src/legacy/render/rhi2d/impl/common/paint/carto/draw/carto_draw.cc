// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/common/paint/carto/draw/carto_draw.h"

#include <cstddef>

#include "legacy/render/rhi2d/impl/common/paint/carto/encode/encoder_tls.h"

using namespace base;
using namespace geo;

namespace render {
namespace detail {

std::size_t Rhi2dCartoDraw::rd_options_offset() {
  return offsetof(Rhi2dCartoDraw, rd_options_);
}

void Rhi2dCartoDraw::set_encoder(Rhi2dCommandEncoder* encoder) {
  // New encode pass must not inherit DC-style cache — otherwise recording
  // can skip the first set_pen/set_brush of a fresh buffer.
  if (encoder != detail::active_encoder()) {
    flush_style();
  }
  set_paint_encoder(encoder);
}

bool Rhi2dCartoDraw::is_recording() const {
  return paint_encoder_recording();
}

Rhi2dCartoDraw::Rhi2dCartoDraw(HINSTANCE h_inst)
    : h_inst_(h_inst),
      h_cur_dc_(nullptr),
      lock_style_(false),
      anno_angle_(0.f),
      feature_type_(0),
      label_priority_(5),
      is_river_(false),
      road_class_(0),
      device_draw_(this),
      ogr_draw_(this),
      mesh_draw_(this),
      primitives_draw_(this),
      rc_(nullptr),
      carto2d_(nullptr),
      rd_options_(nullptr) {
  sz_anno_[0] = '\0';
}

Rhi2dCartoDraw::~Rhi2dCartoDraw() = default;

void Rhi2dCartoDraw::set_dc(HDC dc) {
  style_.release_from_dc(h_cur_dc_);
  h_cur_dc_ = dc;
  style_.invalidate_cache();
}

HDC Rhi2dCartoDraw::dc() const { return h_cur_dc_; }

void Rhi2dCartoDraw::set_context(SmtRenderContext* rc) {
  rc_ = rc;
  xform_.set_context(rc);
}

void Rhi2dCartoDraw::set_carto(GdiCartoFrame* carto) { carto2d_ = carto; }

void Rhi2dCartoDraw::set_render_options(const Smt2DRenderOptions* options) {
  rd_options_ = options;
}

char* Rhi2dCartoDraw::anno_buf() { return sz_anno_; }

float& Rhi2dCartoDraw::anno_angle() { return anno_angle_; }

int& Rhi2dCartoDraw::feature_type() { return feature_type_; }

int& Rhi2dCartoDraw::label_priority() { return label_priority_; }

bool& Rhi2dCartoDraw::is_river() { return is_river_; }

int& Rhi2dCartoDraw::road_class() { return road_class_; }

bool& Rhi2dCartoDraw::lock_style() { return lock_style_; }

int Rhi2dCartoDraw::lp_to_dp(float x, float y, long& X, long& Y) const {
  return xform_.lp_to_dp(x, y, X, Y);
}

int Rhi2dCartoDraw::dp_to_lp(LONG X, LONG Y, float& x, float& y) const {
  return xform_.dp_to_lp(X, Y, x, y);
}

int Rhi2dCartoDraw::lrect_to_drect(const fRect& frect, lRect& lrect) const {
  return xform_.lrect_to_drect(frect, lrect);
}

int Rhi2dCartoDraw::drect_to_lrect(const lRect& lrect, fRect& frect) const {
  return xform_.drect_to_lrect(lrect, frect);
}

int Rhi2dCartoDraw::prepare_for_drawing(const SmtStyle* style, int draw_mode) {
  return style_.prepare_for_drawing(h_cur_dc_, is_recording(), h_inst_, rc_,
                                    is_river_, road_class_, style, draw_mode);
}

int Rhi2dCartoDraw::end_drawing() { return style_.end_drawing(h_cur_dc_); }

void Rhi2dCartoDraw::flush_style() { style_.flush(h_cur_dc_); }

int Rhi2dCartoDraw::draw_multi_line_string(
    const OGRMultiLineString* multi_linestring) {
  return ogr_draw_.draw_multi_line_string(multi_linestring);
}

int Rhi2dCartoDraw::draw_line_spline(const OGRLineString* spline) {
  return ogr_draw_.draw_line_spline(spline);
}

int Rhi2dCartoDraw::draw_multi_point(const SmtStyle* style,
                                     const OGRMultiPoint* multi_point) {
  return ogr_draw_.draw_multi_point(style, multi_point);
}

int Rhi2dCartoDraw::draw_multi_polygon(const OGRMultiPolygon* multi_polygon) {
  return ogr_draw_.draw_multi_polygon(multi_polygon);
}

int Rhi2dCartoDraw::draw_point(const SmtStyle* style, const OGRPoint* point) {
  return ogr_draw_.draw_point(style, point);
}

int Rhi2dCartoDraw::draw_anno(const char* anno, float angle, float c_height,
                              float c_width, float c_space,
                              const OGRPoint* point) {
  return ogr_draw_.draw_anno(anno, angle, c_height, c_width, c_space, point);
}

int Rhi2dCartoDraw::draw_symbol(HICON icon, long height, long width,
                                const OGRPoint* point) {
  return ogr_draw_.draw_symbol(icon, height, width, point);
}

int Rhi2dCartoDraw::draw_line_string(const OGRLineString* linestring) {
  return ogr_draw_.draw_line_string(linestring);
}

int Rhi2dCartoDraw::draw_linear_ring(const OGRLinearRing* linear_ring) {
  return ogr_draw_.draw_linear_ring(linear_ring);
}

int Rhi2dCartoDraw::draw_polygon(const OGRPolygon* polygon) {
  return ogr_draw_.draw_polygon(polygon);
}

int Rhi2dCartoDraw::draw_device_polyline(const POINT* pts, int n) {
  return device_draw_.draw_device_polyline(pts, n);
}

int Rhi2dCartoDraw::draw_device_polylines(const POINT* pts,
                                          const int* poly_counts, int n_polys) {
  return device_draw_.draw_device_polylines(pts, poly_counts, n_polys);
}

int Rhi2dCartoDraw::draw_device_polygon(const POINT* pts,
                                        const int* ring_counts, int n_rings) {
  return device_draw_.draw_device_polygon(pts, ring_counts, n_rings);
}

int Rhi2dCartoDraw::draw_device_point(int x, int y) {
  return device_draw_.draw_device_point(x, y);
}

int Rhi2dCartoDraw::draw_device_anno(int x, int y, const char* text) {
  return device_draw_.draw_device_anno(x, y, text);
}

int Rhi2dCartoDraw::draw_tin(const SmtTin* tin) {
  return mesh_draw_.draw_tin(tin);
}

int Rhi2dCartoDraw::draw_tin_lines(const SmtTin* tin) {
  return mesh_draw_.draw_tin_lines(tin);
}

int Rhi2dCartoDraw::draw_tin_nodes(const SmtTin* tin) {
  return mesh_draw_.draw_tin_nodes(tin);
}

int Rhi2dCartoDraw::draw_grid(const SmtGrid* grid) {
  return mesh_draw_.draw_grid(grid);
}

int Rhi2dCartoDraw::draw_grid_lines(const SmtGrid* grid) {
  return mesh_draw_.draw_grid_lines(grid);
}

int Rhi2dCartoDraw::draw_grid_nodes(const SmtGrid* grid) {
  return mesh_draw_.draw_grid_nodes(grid);
}

int Rhi2dCartoDraw::draw_arc(const OGRLineString* arc) {
  return ogr_draw_.draw_arc(arc);
}

int Rhi2dCartoDraw::draw_fan(const OGRPolygon* fan) {
  return ogr_draw_.draw_fan(fan);
}

int Rhi2dCartoDraw::draw_ellipse(float left, float top, float right,
                                 float bottom, bool b_dp) {
  return primitives_draw_.draw_ellipse(left, top, right, bottom, b_dp);
}

int Rhi2dCartoDraw::draw_rect(const fRect& rect, bool b_dp) {
  return primitives_draw_.draw_rect(rect, b_dp);
}

int Rhi2dCartoDraw::draw_line(fPoint* points, int count, bool b_dp) {
  return primitives_draw_.draw_line(points, count, b_dp);
}

int Rhi2dCartoDraw::draw_line(const fPoint& pt_a, const fPoint& pt_b,
                              bool b_dp) {
  return primitives_draw_.draw_line(pt_a, pt_b, b_dp);
}

int Rhi2dCartoDraw::draw_text(const char* anno, float angle, float c_height,
                              float c_width, float c_space, const fPoint& point,
                              bool b_dp) {
  return primitives_draw_.draw_text(anno, angle, c_height, c_width, c_space,
                                    point, b_dp);
}

int Rhi2dCartoDraw::draw_image(const char* image_buf, int image_buf_size,
                               const fRect& frect, long code_type) {
  return primitives_draw_.draw_image(image_buf, image_buf_size, frect,
                                     code_type);
}

int Rhi2dCartoDraw::stretch_image(const char* image_buf, int image_buf_size,
                                  const fRect& frect, long code_type) {
  return primitives_draw_.stretch_image(image_buf, image_buf_size, frect,
                                        code_type);
}

}  // namespace detail
}  // namespace render
