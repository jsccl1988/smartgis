// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_GDI_DRAW_PRIMITIVES_H_
#define SCENIC_GDI_DRAW_PRIMITIVES_H_

#include "scenic/detail/geom.h"

using namespace base;

namespace scenic {
namespace detail {

class Rhi2dCartoDraw;

// Immediate/record primitive draws (ellipse/rect/line/text/image).
class GdiPrimitivesDraw {
 public:
  explicit GdiPrimitivesDraw(Rhi2dCartoDraw* carto_draw) : c_(carto_draw) {}

  int draw_ellipse(float left, float top, float right, float bottom, bool b_dp);
  int draw_rect(const fRect& rect, bool b_dp);
  int draw_line(fPoint* points, int count, bool b_dp);
  int draw_line(const fPoint& pt_a, const fPoint& pt_b, bool b_dp);
  int draw_text(const char* anno, float angle, float c_height, float c_width,
                float c_space, const fPoint& point, bool b_dp);
  int draw_image(const char* image_buf, int image_buf_size, const fRect& frect,
                 long code_type);
  int stretch_image(const char* image_buf, int image_buf_size,
                    const fRect& frect, long code_type);

 private:
  Rhi2dCartoDraw* c_;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_GDI_DRAW_PRIMITIVES_H_
