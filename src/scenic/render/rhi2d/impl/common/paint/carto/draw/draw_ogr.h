// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_GDI_DRAW_OGR_H_
#define SCENIC_GDI_DRAW_OGR_H_

#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_pod.h"

class OGRLineString;
class OGRLinearRing;
class OGRMultiLineString;
class OGRMultiPoint;
class OGRMultiPolygon;
class OGRPoint;
class OGRPolygon;

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace scenic {
namespace detail {

class Rhi2dCartoDraw;

// OGR geometry draws for Rhi2dCartoDraw (project �?device play).
class GdiOgrDraw {
 public:
  explicit GdiOgrDraw(Rhi2dCartoDraw* carto_draw) : c_(carto_draw) {}

  int draw_multi_line_string(const OGRMultiLineString* multi_linestring);
  int draw_line_spline(const OGRLineString* spline);
  int draw_multi_point(const base::Style* style,
                       const OGRMultiPoint* multi_point);
  int draw_multi_polygon(const OGRMultiPolygon* multi_polygon);
  int draw_point(const base::Style* style, const OGRPoint* point);
  int draw_anno(const char* anno, float angle, float c_height, float c_width,
                float c_space, const OGRPoint* point);
  int draw_symbol(HICON icon, long height, long width, const OGRPoint* point);
  int draw_line_string(const OGRLineString* linestring);
  int draw_linear_ring(const OGRLinearRing* linear_ring);
  int draw_polygon(const OGRPolygon* polygon);
  int draw_arc(const OGRLineString* arc);
  int draw_fan(const OGRPolygon* fan);

 private:
  Rhi2dCartoDraw* c_;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_GDI_DRAW_OGR_H_
