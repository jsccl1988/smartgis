// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/paint/carto/draw/carto_draw.h"

#include <math.h>

#include <algorithm>
#include <cstring>
#include <span>
#include <vector>

#include "base/math/simd/simd.h"
#include "base/math/linear/vector.h"
#include "gis/feature/feature.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/feature_kind.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_api.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/frame/carto_frame.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/device_geom.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/points.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/ogr_xy.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/encode/encoder_tls.h"
#include "scenic/render/rhi2d/impl/common/paint/backend/paint_backend.h"
#include "ogrsf_frmts.h"

using namespace gis;
using namespace base;

namespace scenic {
namespace detail {

int GdiOgrDraw::draw_multi_line_string(
    const OGRMultiLineString* multi_linestring) {
  const int n_lines = multi_linestring->getNumGeometries();
  for (int i = 0; i < n_lines; ++i) {
    c_->draw_line_string(static_cast<const OGRLineString*>(
        multi_linestring->getGeometryRef(i)));
  }
  return kErrFailure;
}

int GdiOgrDraw::draw_multi_point(const Style* style,
                                 const OGRMultiPoint* multi_point) {
  const int n_points = multi_point->getNumGeometries();
  for (int i = 0; i < n_points; ++i) {
    c_->draw_point(style,
                   static_cast<const OGRPoint*>(multi_point->getGeometryRef(i)));
  }
  return kErrFailure;
}

int GdiOgrDraw::draw_multi_polygon(const OGRMultiPolygon* multi_polygon) {
  const int n_polygons = multi_polygon->getNumGeometries();
  for (int i = 0; i < n_polygons; ++i) {
    c_->draw_polygon(
        static_cast<const OGRPolygon*>(multi_polygon->getGeometryRef(i)));
  }
  return kErrFailure;
}

int GdiOgrDraw::draw_point(const Style* style, const OGRPoint* point) {
  if (!point) {
    return kErrInvalidParam;
  }
  if (!style) {
    long lX = 0;
    long lY = 0;
    c_->lp_to_dp(point->getX(), point->getY(), lX, lY);
    return c_->draw_device_point(static_cast<int>(lX), static_cast<int>(lY));
  }
  ulong format = style->get_style_type();
  (void)format;
  if (c_->feature_type_ == FeatureType::FtAnno) {
    AnnotationDesc anno = style->get_anno_desc();
    return c_->draw_anno(c_->sz_anno_, c_->anno_angle_, abs(anno.fHeight), abs(anno.fWidth),
                     abs(anno.fSpace), point);
  } else if (c_->feature_type_ == FeatureType::FtChildImage) {
    SymbolDesc symbol = style->get_symbol_desc();
    return c_->draw_symbol(c_->style_.icon(), symbol.fSymbolHeight, symbol.fSymbolWidth,
                       point);
  } else if (c_->feature_type_ == FeatureType::FtDot) {
    long lX, lY;
    c_->lp_to_dp(point->getX(), point->getY(), lX, lY);
    return c_->draw_device_point(static_cast<int>(lX), static_cast<int>(lY));
  }

  return kErrFailure;
}

int GdiOgrDraw::draw_anno(const char* anno, float angle, float c_height,
                              float c_width, float c_space,
                              const OGRPoint* point) {
  if (anno == nullptr || !point) return kErrInvalidParam;

  (void)c_width;
  (void)c_space;
  c_height *= c_->rc_->fblc;

  long x = 0;
  long y = 0;
  c_->lp_to_dp(point->getX(), point->getY(), x, y);
  const Vector2 offset(2.f * c_height * static_cast<float>(sin(angle)),
                       2.f * c_height * static_cast<float>(cos(angle)));
  x -= static_cast<long>(offset.x);
  y -= static_cast<long>(offset.y);
  const int px_h = carto2d_label_px(c_->label_priority_, c_->rc_->fblc);
  const MapCartoBox box = carto2d_label_box(
      static_cast<int>(x), static_cast<int>(y), anno, px_h, c_->label_priority_);
  if (!c_->carto2d_->try_keep_label(box)) {
    return kErrNone;
  }
  const int text_n = static_cast<int>(std::strlen(anno));
  if (c_->is_recording()) {
    active_encoder()->text(static_cast<int>(x), static_cast<int>(y), anno, text_n, px_h,
                   carto2d_halo_px(c_->label_priority_), c_->anno_angle_);
  } else {
    detail::ScopedPaintBackend(c_->h_cur_dc_)->draw_anno_text( x, y, anno, px_h, carto2d_halo_px(c_->label_priority_),
                   c_->anno_angle_);
  }

  if (c_->rd_options_->bShowPoint && c_->h_cur_dc_) {
    int r = c_->rd_options_->lPointRaduis;
    long lX, lY;
    c_->lp_to_dp(point->getX(), point->getY(), lX, lY);
    detail::ScopedPaintBackend(c_->h_cur_dc_)->draw_cross( lX, lY, r);
  }

  return kErrNone;
}

int GdiOgrDraw::draw_symbol(HICON icon, long height, long width,
                                const OGRPoint* point) {
  if (!point || (!c_->h_cur_dc_ && !c_->is_recording())) {
    return kErrInvalidParam;
  }
  // Icon draw needs an HDC; skip while record-only (rare on map vector path).
  if (c_->is_recording() && !c_->h_cur_dc_) {
    return kErrNone;
  }
  height *= c_->rc_->fblc;
  width *= c_->rc_->fblc;

  long lX, lY;
  c_->lp_to_dp(point->getX(), point->getY(), lX, lY);
  //::DrawIcon(c_->h_cur_dc_,pt.x-width,pt.y-height,icon);
  ::DrawIconEx(c_->h_cur_dc_, lX - width / 2, lY + height / 2, icon, width, height,
               0, nullptr, DI_NORMAL);

  if (c_->rd_options_->bShowPoint) {
    int r = c_->rd_options_->lPointRaduis;
    detail::ScopedPaintBackend(c_->h_cur_dc_)->draw_cross( lX, lY, r);
  }

  return kErrNone;
}

int GdiOgrDraw::draw_line_spline(const OGRLineString* spline) {
  const int n_points = spline->getNumPoints();
  if (n_points < 2) return kErrInvalidParam;

  ScopedGdiPoints pts(n_points);
  if (!pts) return kErrFailure;

  thread_local std::vector<float> xy;
  xy.resize(static_cast<size_t>(n_points) * 2u);
  for (int i = 0; i < n_points; ++i) {
    xy[static_cast<size_t>(i) * 2u] = static_cast<float>(spline->getX(i));
    xy[static_cast<size_t>(i) * 2u + 1u] = static_cast<float>(spline->getY(i));
  }
  const LpToDp2 xform = make_lp_to_dp(*c_->rc_);
  transform_xy_batch(
      xform, std::span<const float>(xy.data(), xy.size()),
      std::span<long>(reinterpret_cast<long*>(pts.data),
                      static_cast<size_t>(n_points) * 2u));

  MoveToEx(c_->h_cur_dc_, pts.data[0].x, pts.data[0].y, nullptr);
  PolylineTo(c_->h_cur_dc_, pts.data, n_points);

  if (c_->rd_options_->bShowPoint) {
    const int r = c_->rd_options_->lPointRaduis;
    for (int i = 0; i < n_points; ++i) {
      Ellipse(c_->h_cur_dc_, pts.data[i].x - r, pts.data[i].y - r,
              pts.data[i].x + r, pts.data[i].y + r);
    }
  }

  return kErrNone;
}

int GdiOgrDraw::draw_line_string(const OGRLineString* linestring) {
  if (!linestring || !c_->rc_ || (!c_->h_cur_dc_ && !c_->is_recording())) {
    return kErrInvalidParam;
  }
  const int n_points = linestring->getNumPoints();
  if (n_points < 2) {
    return kErrInvalidParam;
  }

  thread_local std::vector<float> xy;
  thread_local std::vector<POINT> projected;
  thread_local std::vector<POINT> thinned;

  const LpToDp2 xform = make_lp_to_dp(*c_->rc_);
  const int step = overview_vertex_step(n_points, xform.scale);
  const int n_out =
      pack_curve_xy_strided(linestring, step, &xy, /*keep_last=*/true);
  if (n_out < 2) {
    return kErrNone;
  }

  projected.resize(static_cast<size_t>(n_out));
  transform_xy_batch(
      xform, std::span<const float>(xy.data(), static_cast<size_t>(n_out) * 2u),
      std::span<long>(reinterpret_cast<long*>(projected.data()),
                      static_cast<size_t>(n_out) * 2u));

  thinned.clear();
  thinned.reserve(static_cast<size_t>(n_out));
  const int kept = thin_device_polyline(projected.data(), n_out, &thinned,
                                        overview_thin_chebyshev(xform.scale));
  if (kept < 2) {
    return kErrNone;
  }
  return c_->draw_device_polyline(thinned.data(), kept);
}

int GdiOgrDraw::draw_linear_ring(const OGRLinearRing* linear_ring) {
  if (!linear_ring || !c_->rc_) {
    return kErrInvalidParam;
  }
  const int n_points = linear_ring->getNumPoints();
  if (n_points < 2) {
    return kErrInvalidParam;
  }
  if (!c_->h_cur_dc_ && !c_->is_recording()) {
    return kErrInvalidParam;
  }

  thread_local std::vector<float> xy;
  ScopedGdiPoints pts(n_points);
  if (!pts) {
    return kErrFailure;
  }

  const int n_out =
      pack_curve_xy_strided(linear_ring, /*step=*/1, &xy, /*keep_last=*/true);
  if (n_out < 2) {
    return kErrNone;
  }
  const LpToDp2 xform = make_lp_to_dp(*c_->rc_);
  transform_xy_batch(
      xform, std::span<const float>(xy.data(), static_cast<size_t>(n_out) * 2u),
      std::span<long>(reinterpret_cast<long*>(pts.data),
                      static_cast<size_t>(n_out) * 2u));

  if (c_->rd_options_->bShowPoint && c_->h_cur_dc_) {
    const int r = c_->rd_options_->lPointRaduis;
    for (int i = 0; i < n_out; ++i) {
      detail::ScopedPaintBackend(c_->h_cur_dc_)
          ->draw_cross(pts.data[i].x, pts.data[i].y, r);
    }
  }

  if (c_->is_recording()) {
    active_encoder()->polyline(pts.data, n_out);
  } else {
    detail::ScopedPaintBackend(c_->h_cur_dc_)->polyline(pts.data, n_out);
  }

  return kErrNone;
}

int GdiOgrDraw::draw_polygon(const OGRPolygon* polygon) {
  if (!polygon || !c_->rc_ || (!c_->h_cur_dc_ && !c_->is_recording())) {
    return kErrInvalidParam;
  }
  const OGRLinearRing* exterior = polygon->getExteriorRing();
  if (!exterior) {
    return kErrInvalidParam;
  }
  const int n_exterior_pts = exterior->getNumPoints();
  if (n_exterior_pts < 2) {
    return kErrInvalidParam;
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
    return kErrInvalidParam;
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
    return kErrInvalidParam;
  }

  projected.resize(static_cast<size_t>(n_count));
  const LpToDp2 xform = make_lp_to_dp(*c_->rc_);
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
                         overview_thin_chebyshev(c_->rc_->fblc));
    if (kept < 3) {
      thinned.resize(base);
      if (r == 0) {
        return kErrNone;
      }
      continue;
    }
    ring_counts.push_back(kept);
  }
  if (ring_counts.empty()) {
    return kErrNone;
  }
  return c_->draw_device_polygon(thinned.data(), ring_counts.data(),
                             static_cast<int>(ring_counts.size()));
}

int GdiOgrDraw::draw_fan(const OGRPolygon* fan) {
  return c_->draw_polygon(fan);
#if 0
  // Legacy Pie path retained in source thread; not active.
#endif
}

int GdiOgrDraw::draw_arc(const OGRLineString* arc) {
  return c_->draw_line_string(arc);
#if 0
  // Legacy Arc path retained in source thread; not active.
#endif
}


}  // namespace detail
}  // namespace scenic
