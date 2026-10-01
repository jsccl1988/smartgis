// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/common/paint/carto/draw/carto_draw.h"

#include <math.h>

#include <cstring>
#include <span>
#include <vector>

#include "base/math/simd.h"
#include "legacy/core/types/types.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/draw/device_geom.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/draw/points.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/encode/encoder_tls.h"
#include "legacy/render/rhi2d/impl/common/paint/backend/paint_backend.h"
#include "ximage.h"

using namespace base;
using namespace geo;

namespace render {
namespace detail {

int GdiPrimitivesDraw::draw_ellipse(float left, float top, float right,
                                 float bottom, bool b_dp) {
  if (!c_->h_cur_dc_ && !c_->is_recording()) {
    return SMT_ERR_INVALID_PARAM;
  }
  lRect lrect;

  fRect frect;
  frect.lb.x = left;
  frect.lb.y = bottom;
  frect.rt.x = right;
  frect.rt.y = top;

  if (!b_dp)
    c_->lrect_to_drect(frect, lrect);
  else
    lrect = frect.cast_to<long>();

  if (c_->is_recording()) {
    active_encoder()->ellipse(lrect.lb.x, lrect.lb.y, lrect.rt.x, lrect.rt.y);
  } else {
    Ellipse(c_->h_cur_dc_, lrect.lb.x, lrect.lb.y, lrect.rt.x, lrect.rt.y);
  }

  return SMT_ERR_NONE;
}

int GdiPrimitivesDraw::draw_rect(const fRect& rect, bool b_dp) {
  if (!c_->h_cur_dc_ && !c_->is_recording()) {
    return SMT_ERR_INVALID_PARAM;
  }
  lRect tmpRectDP = rect.cast_to<long>();

  if (!b_dp) {
    fRect tmpRectLP = tmpRectDP.cast_to<float>();
    c_->lrect_to_drect(tmpRectLP, tmpRectDP);
  }

  POINT pts[5] = {
      {tmpRectDP.lb.x, tmpRectDP.lb.y},
      {tmpRectDP.rt.x, tmpRectDP.lb.y},
      {tmpRectDP.rt.x, tmpRectDP.rt.y},
      {tmpRectDP.lb.x, tmpRectDP.rt.y},
      {tmpRectDP.lb.x, tmpRectDP.lb.y},
  };
  if (c_->is_recording()) {
    active_encoder()->polyline(pts, 5);
  } else {
    MoveToEx(c_->h_cur_dc_, tmpRectDP.lb.x, tmpRectDP.lb.y, nullptr);
    LineTo(c_->h_cur_dc_, tmpRectDP.rt.x, tmpRectDP.lb.y);
    LineTo(c_->h_cur_dc_, tmpRectDP.rt.x, tmpRectDP.rt.y);
    LineTo(c_->h_cur_dc_, tmpRectDP.lb.x, tmpRectDP.rt.y);
    LineTo(c_->h_cur_dc_, tmpRectDP.lb.x, tmpRectDP.lb.y);
  }

  return SMT_ERR_NONE;
}

int GdiPrimitivesDraw::draw_line(fPoint* points, int count, bool b_dp) {
  int n_points = count;
  if (n_points < 2) return SMT_ERR_INVALID_PARAM;
  if (!c_->h_cur_dc_ && !c_->is_recording()) {
    return SMT_ERR_INVALID_PARAM;
  }

  ScopedGdiPoints pts(n_points);
  if (!pts) return SMT_ERR_FAILURE;

  if (!b_dp) {
    thread_local std::vector<float> xy;
    xy.resize(static_cast<size_t>(n_points) * 2u);
    for (int i = 0; i < n_points; ++i) {
      xy[static_cast<size_t>(i) * 2u] = points[i].x;
      xy[static_cast<size_t>(i) * 2u + 1u] = points[i].y;
    }
    const LpToDp2 xform = make_lp_to_dp(*c_->rc_);
    transform_xy_batch(
        xform, std::span<const float>(xy.data(), xy.size()),
        std::span<long>(reinterpret_cast<long*>(pts.data),
                        static_cast<size_t>(n_points) * 2u));
  } else {
    for (int i = 0; i < n_points; ++i) {
      pts.data[i].x = static_cast<long>(points[i].x);
      pts.data[i].y = static_cast<long>(points[i].y);
    }
  }

  if (c_->rd_options_->bShowPoint && c_->h_cur_dc_) {
    const int r = c_->rd_options_->lPointRaduis;
    for (int i = 0; i < n_points; ++i) {
      detail::ScopedPaintBackend(c_->h_cur_dc_)
          ->draw_cross(pts.data[i].x, pts.data[i].y, r);
    }
  }

  if (c_->is_recording()) {
    active_encoder()->polyline(pts.data, n_points);
  } else {
    MoveToEx(c_->h_cur_dc_, pts.data[0].x, pts.data[0].y, nullptr);
    PolylineTo(c_->h_cur_dc_, pts.data, n_points);
  }

  return SMT_ERR_NONE;
}

int GdiPrimitivesDraw::draw_line(const fPoint& pt_a, const fPoint& pt_b,
                              bool b_dp) {
  if (!c_->h_cur_dc_ && !c_->is_recording()) {
    return SMT_ERR_INVALID_PARAM;
  }
  lPoint pt1(pt_a.x, pt_a.y), pt2(pt_b.x, pt_b.y);

  if (!b_dp) {
    c_->lp_to_dp(pt_a.x, pt_a.y, pt1.x, pt1.y);
    c_->lp_to_dp(pt_b.x, pt_b.y, pt2.x, pt2.y);
  }

  POINT pts[2] = {{pt1.x, pt1.y}, {pt2.x, pt2.y}};
  if (c_->is_recording()) {
    active_encoder()->polyline(pts, 2);
  } else {
    MoveToEx(c_->h_cur_dc_, pt1.x, pt1.y, nullptr);
    LineTo(c_->h_cur_dc_, pt2.x, pt2.y);
  }

  return SMT_ERR_NONE;
}

int GdiPrimitivesDraw::draw_text(const char* anno, float angle, float c_height,
                              float c_width, float c_space, const fPoint& point,
                              bool b_dp) {
  if (anno == nullptr) return SMT_ERR_INVALID_PARAM;

  c_height *= c_->rc_->fblc;
  c_width *= c_->rc_->fblc;
  c_space *= c_->rc_->fblc;

  unsigned char c1, c2;
  fPoint pt;
  long x, y;
  char bz[4];
  const char* ls1;
  ls1 = anno;

  if (!b_dp) {
    c_->lp_to_dp(point.x, point.y, x, y);
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
      TextOut(c_->h_cur_dc_, pt.x, pt.y, (LPCSTR)bz, 2);
      n_str_length -= 2;
      pt.x += (c_width * 2 + c_space) * cos(angle);
      pt.y += (c_width * 2 + c_space) * sin(angle);
    } else {
      strncpy(bz, ls1, 1);
      bz[1] = 0;
      ls1++;
      TextOut(c_->h_cur_dc_, pt.x, pt.y, (LPCSTR)bz, 1);
      n_str_length -= 1;

      pt.x += (c_width + c_space / 2.) * cos(angle);
      pt.y += (c_width + c_space / 2.) * sin(angle);
    }
  }

  if (c_->rd_options_->bShowPoint) {
    int r = c_->rd_options_->lPointRaduis;
    long lX, lY;
    if (!b_dp) {
      c_->lp_to_dp(point.x, point.y, lX, lY);
    } else {
      lX = point.x;
      lY = point.y;
    }

    detail::ScopedPaintBackend(c_->h_cur_dc_)->draw_cross( lX, lY, r);
  }

  return SMT_ERR_NONE;
}

int GdiPrimitivesDraw::draw_image(const char* image_buf, int image_buf_size,
                               const fRect& frect, long code_type) {
  lRect lrt;
  c_->lrect_to_drect(frect, lrt);

  CxImage tmpImage;
  tmpImage.Decode((BYTE*)image_buf, image_buf_size, code_type);
  tmpImage.Draw(c_->h_cur_dc_, lrt.lb.x, lrt.rt.y, lrt.width(), lrt.height());

  return SMT_ERR_NONE;
}

int GdiPrimitivesDraw::stretch_image(const char* image_buf, int image_buf_size,
                                  const fRect& frect, long code_type) {
  lRect lrt;
  c_->lrect_to_drect(frect, lrt);

  CxImage tmpImage;
  tmpImage.Decode((BYTE*)image_buf, image_buf_size, code_type);
  tmpImage.Stretch(c_->h_cur_dc_, lrt.lb.x, lrt.rt.y, lrt.width(), lrt.height());

  return SMT_ERR_NONE;
}


}  // namespace detail
}  // namespace render
