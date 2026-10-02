// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/common/paint/carto/draw/carto_draw.h"

#include <math.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "legacy/gis/present/carto/style_api.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/frame/carto_frame.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/encode/encoder_tls.h"
#include "legacy/render/rhi2d/impl/common/paint/backend/paint_backend.h"

using namespace base;
using namespace geo;

namespace render {
namespace detail {

int GdiDeviceDraw::draw_device_polyline(const POINT* pts, int n) {
  // Fail loud on partial-rebuild ODR (this TU vs carto_draw.obj). Friend
  // access lets offsetof see rd_options_; the out-of-line helper bakes the
  // carto_draw.cc layout.
  static const bool layout_ok = [] {
    return offsetof(Rhi2dCartoDraw, rd_options_) ==
           Rhi2dCartoDraw::rd_options_offset();
  }();
  if (!layout_ok) {
    std::abort();
  }
  if (!pts || n < 2 || !c_->rc_ || (!c_->h_cur_dc_ && !c_->is_recording())) {
    return SMT_ERR_INVALID_PARAM;
  }

  const bool recording = c_->is_recording();

  if (c_->rd_options_ && c_->rd_options_->bShowPoint && !recording) {
    constexpr int kMaxDebugCrosses = 48;
    const int r = c_->rd_options_->lPointRaduis > 0 ? c_->rd_options_->lPointRaduis : 3;
    const int n_draw = (std::min)(n, kMaxDebugCrosses);
    for (int i = 0; i < n_draw; ++i) {
      detail::ScopedPaintBackend(c_->h_cur_dc_)->draw_cross( pts[i].x, pts[i].y, r);
    }
  }

  // Roads: same dual-GDI-pen path for encode and immediate (GdiBackend).
  bool drew = false;
  if (c_->road_class_ > 0) {
    const int fill_w = carto2d_road_width_px(c_->rc_->fblc, c_->road_class_);
    const COLORREF fill_c = carto2d_road_fill_color(c_->road_class_);
    const COLORREF case_c = carto2d_road_casing_color(c_->road_class_);
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
      c_->style_.release_from_dc(c_->h_cur_dc_);
      c_->style_.invalidate_cache();
      detail::ScopedPaintBackend(c_->h_cur_dc_)
          ->road_polyline(pts, n, case_c, case_w, fill_c, fill_w);
      drew = true;
    }
  }
  if (!drew) {
    if (recording) {
      active_encoder()->polyline(pts, n);
    } else {
      detail::ScopedPaintBackend(c_->h_cur_dc_)->polyline(pts, n);
    }
  }

  if (c_->carto2d_ && (c_->is_river_ || c_->road_class_ > 0) && c_->sz_anno_[0]) {
    thread_local std::vector<int> label_xy;
    label_xy.clear();
    label_xy.reserve(static_cast<size_t>(n) * 2u);
    for (int i = 0; i < n; ++i) {
      label_xy.push_back(static_cast<int>(pts[i].x));
      label_xy.push_back(static_cast<int>(pts[i].y));
    }
    MapCartoLineLabel pose;
    if (carto2d_line_label_pose(label_xy.data(), n, &pose)) {
      const int px_h = carto2d_label_px(c_->label_priority_, c_->rc_->fblc);
      const MapCartoBox box = carto2d_label_box_rotated(
          pose.x, pose.y, c_->sz_anno_, px_h, c_->label_priority_, pose.angle_deg);
      if (c_->carto2d_->try_keep_label(box)) {
        const int text_n = static_cast<int>(std::strlen(c_->sz_anno_));
        if (recording) {
          active_encoder()->text(pose.x, pose.y, c_->sz_anno_, text_n, px_h,
                         carto2d_halo_px(c_->label_priority_), pose.angle_deg);
        } else {
          detail::ScopedPaintBackend(c_->h_cur_dc_)->draw_anno_text( pose.x, pose.y, c_->sz_anno_, px_h,
                         carto2d_halo_px(c_->label_priority_), pose.angle_deg);
        }
      }
    }
  }

  return SMT_ERR_NONE;
}

int GdiDeviceDraw::draw_device_polylines(const POINT* pts,
                                          const int* poly_counts, int n_polys) {
  if (!pts || !poly_counts || n_polys < 1 || !c_->rc_ ||
      (!c_->h_cur_dc_ && !c_->is_recording())) {
    return SMT_ERR_INVALID_PARAM;
  }
  // River line labels still need the per-polyline path.
  if (c_->is_river_ && c_->sz_anno_[0]) {
    size_t offset = 0;
    for (int i = 0; i < n_polys; ++i) {
      const int n = poly_counts[i];
      if (n >= 2) {
        c_->draw_device_polyline(pts + offset, n);
      }
      offset += static_cast<size_t>((std::max)(0, n));
    }
    return SMT_ERR_NONE;
  }
  // Roads: one casing PolyPolyline + one fill PolyPolyline.
  if (c_->road_class_ > 0) {
    const int fill_w = carto2d_road_width_px(c_->rc_->fblc, c_->road_class_);
    const COLORREF fill_c = carto2d_road_fill_color(c_->road_class_);
    const COLORREF case_c = carto2d_road_casing_color(c_->road_class_);
    const int case_w = fill_w + 2;
    if (c_->is_recording()) {
      if (fill_w <= 1) {
        active_encoder()->set_pen(fill_c, 1);
        if (!active_encoder()->poly_polyline(pts, poly_counts, n_polys)) {
          return SMT_ERR_FAILURE;
        }
      } else {
        active_encoder()->set_pen(case_c, case_w);
        if (!active_encoder()->poly_polyline(pts, poly_counts, n_polys)) {
          return SMT_ERR_FAILURE;
        }
        active_encoder()->set_pen(fill_c, fill_w);
        if (!active_encoder()->poly_polyline(pts, poly_counts, n_polys)) {
          return SMT_ERR_FAILURE;
        }
      }
    } else {
      c_->style_.release_from_dc(c_->h_cur_dc_);
      c_->style_.invalidate_cache();
      detail::ScopedPaintBackend(c_->h_cur_dc_)
          ->road_poly_polyline(pts, poly_counts, n_polys, case_c, case_w,
                               fill_c, fill_w);
    }
    return SMT_ERR_NONE;
  }
  if (c_->is_recording()) {
    if (!active_encoder()->poly_polyline(pts, poly_counts, n_polys)) {
      return SMT_ERR_FAILURE;
    }
  } else {
    detail::ScopedPaintBackend(c_->h_cur_dc_)->poly_polyline(pts, poly_counts, n_polys);
  }
  return SMT_ERR_NONE;
}

int GdiDeviceDraw::draw_device_polygon(const POINT* pts,
                                        const int* ring_counts, int n_rings) {
  if (!pts || !ring_counts || n_rings < 1 ||
      (!c_->h_cur_dc_ && !c_->is_recording())) {
    return SMT_ERR_INVALID_PARAM;
  }

  if (c_->is_recording()) {
    active_encoder()->poly_polygon(pts, ring_counts, n_rings);
    return SMT_ERR_NONE;
  }

  if (c_->rd_options_ && c_->rd_options_->bShowPoint) {
    // Debug overlay: cap crosses so dense china rings cannot dominate paint.
    constexpr int kMaxDebugCrosses = 48;
    const int r = c_->rd_options_->lPointRaduis > 0 ? c_->rd_options_->lPointRaduis : 3;
    int total = 0;
    for (int i = 0; i < n_rings; ++i) {
      total += ring_counts[i];
    }
    const int n_draw = (std::min)(total, kMaxDebugCrosses);
    for (int i = 0; i < n_draw; ++i) {
      detail::ScopedPaintBackend(c_->h_cur_dc_)->draw_cross( pts[i].x, pts[i].y, r);
    }
  }

  ::PolyPolygon(c_->h_cur_dc_, const_cast<POINT*>(pts),
                const_cast<int*>(ring_counts), n_rings);
  return SMT_ERR_NONE;
}

int GdiDeviceDraw::draw_device_point(int x, int y) {
  if (!c_->rc_ || !c_->carto2d_ || (!c_->h_cur_dc_ && !c_->is_recording())) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (!c_->carto2d_->try_keep_point(x, y)) {
    return SMT_ERR_NONE;
  }
  const int radius = carto2d_point_radius(c_->rc_->fblc);
  if (c_->is_recording()) {
    active_encoder()->ellipse(x - radius, y - radius, x + radius, y + radius);
  } else {
    detail::ScopedPaintBackend(c_->h_cur_dc_)->draw_point_disc( x, y, radius);
  }
  return SMT_ERR_NONE;
}

int GdiDeviceDraw::draw_device_anno(int x, int y, const char* text) {
  if (!c_->rc_ || !c_->carto2d_ || !text || !text[0] ||
      (!c_->h_cur_dc_ && !c_->is_recording())) {
    return SMT_ERR_INVALID_PARAM;
  }
  // Match draw_anno offset using carto px height (style fHeight*fblc ~= same).
  const int px_h = carto2d_label_px(c_->label_priority_, c_->rc_->fblc);
  const float c_height = static_cast<float>(px_h);
  long dx = x - static_cast<long>(2 * c_height * sin(c_->anno_angle_));
  long dy = y - static_cast<long>(2 * c_height * cos(c_->anno_angle_));
  const MapCartoBox box = carto2d_label_box(
      static_cast<int>(dx), static_cast<int>(dy), text, px_h, c_->label_priority_);
  if (!c_->carto2d_->try_keep_label(box)) {
    return SMT_ERR_NONE;
  }
  const int text_n = static_cast<int>(std::strlen(text));
  if (c_->is_recording()) {
    active_encoder()->text(static_cast<int>(dx), static_cast<int>(dy), text, text_n,
                   px_h, carto2d_halo_px(c_->label_priority_), c_->anno_angle_);
  } else {
    detail::ScopedPaintBackend(c_->h_cur_dc_)->draw_anno_text( dx, dy, text, px_h,
                   carto2d_halo_px(c_->label_priority_), c_->anno_angle_);
  }
  return SMT_ERR_NONE;
}


}  // namespace detail
}  // namespace render
