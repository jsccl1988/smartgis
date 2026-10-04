// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/paint/map/map_feature_prep.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <span>
#include <vector>

#include "base/math/simd/simd.h"
#include "gis/datasource/ogr/ogr_feature_codec.h"
#include "gis/geo/ops/geometry_traits.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/feature_kind.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_api.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_ogr.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/frame/carto_frame.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/ogr_xy.h"

using namespace gis;

namespace scenic {
namespace detail {
namespace {

bool append_projected_ring(const OGRLinearRing* ring, const LpToDp2& xform,
                           std::vector<POINT>* pts, std::vector<int>* counts) {
  if (!ring || !pts || !counts) {
    return false;
  }
  const int n = ring->getNumPoints();
  if (n < 2) {
    return false;
  }
  // Overview: subsample before LP→DP when rings are denser than ~1 vert/px.
  const int step = overview_vertex_step(n, xform.scale);
  thread_local std::vector<float> xy;
  thread_local std::vector<POINT> projected;
  int n_out = pack_curve_xy_strided(ring, step, &xy, /*keep_last=*/false);
  // Keep ring closed after subsample.
  if (n_out >= 2) {
    const float x0 = xy[0];
    const float y0 = xy[1];
    float& xl = xy[static_cast<size_t>(n_out - 1) * 2u];
    float& yl = xy[static_cast<size_t>(n_out - 1) * 2u + 1u];
    if (xl != x0 || yl != y0) {
      if (static_cast<int>(xy.size()) < (n_out + 1) * 2) {
        xy.resize(static_cast<size_t>(n_out + 1) * 2u);
      }
      xy[static_cast<size_t>(n_out) * 2u] = x0;
      xy[static_cast<size_t>(n_out) * 2u + 1u] = y0;
      ++n_out;
    }
  }
  projected.resize(static_cast<size_t>(n_out));
  transform_xy_batch(
      xform, std::span<const float>(xy.data(), static_cast<size_t>(n_out) * 2u),
      std::span<long>(reinterpret_cast<long*>(projected.data()),
                      static_cast<size_t>(n_out) * 2u));
  const size_t base = pts->size();
  const int kept = thin_device_ring(projected.data(), n_out, pts,
                                    overview_thin_chebyshev(xform.scale));
  if (kept < 3) {
    pts->resize(base);
    return false;
  }
  counts->push_back(kept);
  return true;
}

bool append_projected_line(const OGRLineString* line, const LpToDp2& xform,
                           PrepPart* part) {
  if (!line || !part) {
    return false;
  }
  const int n = line->getNumPoints();
  if (n < 2) {
    return false;
  }
  const int step = overview_vertex_step(n, xform.scale);
  thread_local std::vector<float> xy;
  thread_local std::vector<POINT> projected;
  const int n_out =
      pack_curve_xy_strided(line, step, &xy, /*keep_last=*/true);
  projected.resize(static_cast<size_t>(n_out));
  transform_xy_batch(
      xform, std::span<const float>(xy.data(), static_cast<size_t>(n_out) * 2u),
      std::span<long>(reinterpret_cast<long*>(projected.data()),
                      static_cast<size_t>(n_out) * 2u));
  const size_t base = part->pts.size();
  const int kept =
      thin_device_polyline(projected.data(), n_out, &part->pts,
                           overview_thin_chebyshev(xform.scale));
  if (kept < 2) {
    part->pts.resize(base);
    return false;
  }
  part->counts.push_back(kept);
  return true;
}

bool prepare_polygon_part(const OGRPolygon* poly, const LpToDp2& xform,
                          PrepPart* part) {
  if (!poly || !part) {
    return false;
  }
  const OGRLinearRing* exterior = poly->getExteriorRing();
  if (!exterior ||
      !append_projected_ring(exterior, xform, &part->pts, &part->counts)) {
    return false;
  }
  // Overview: interior rings are sub-pixel / invisible �?skip hole transform.
  if (xform.scale >= 12.f) {
    const int n_holes = poly->getNumInteriorRings();
    for (int i = 0; i < n_holes; ++i) {
      const OGRLinearRing* hole = poly->getInteriorRing(i);
      if (hole) {
        (void)append_projected_ring(hole, xform, &part->pts, &part->counts);
      }
    }
  }
  return !part->counts.empty();
}

bool env_too_small_on_device(const gis::Envelope& env, float scale) {
  const float dx = static_cast<float>((env.MaxX - env.MinX) * scale);
  const float dy = static_cast<float>((env.MaxY - env.MinY) * scale);
  // Overview: cull slightly larger noise envelopes (sub-3px blobs).
  const float min_px = scale < 12.f ? 3.f : 2.f;
  return dx < min_px && dy < min_px;
}

const char* cached_field(OGRFeature* feature, int index) {
  if (!feature || index < 0) {
    return "";
  }
  const char* v = feature->GetFieldAsString(index);
  return v ? v : "";
}

bool parse_html_rgb_local(const char* s, COLORREF* out) {
  if (!s || s[0] != '#' || !out) {
    return false;
  }
  unsigned r = 0;
  unsigned g = 0;
  unsigned b = 0;
  if (std::sscanf(s, "#%2x%2x%2x", &r, &g, &b) != 3) {
    return false;
  }
  *out = RGB(r, g, b);
  return true;
}

COLORREF cached_color(OGRFeature* feature, int index, COLORREF fallback) {
  if (!feature || index < 0) {
    return fallback;
  }
  COLORREF c = fallback;
  if (parse_html_rgb_local(feature->GetFieldAsString(index), &c)) {
    return c;
  }
  return fallback;
}

// Lightweight style for parallel prep: cached field indices, no anno infer.
void fill_prep_style(OGRFeature* feature, const PrepFieldCache& cache,
                     float fblc, Style* dst) {
  if (!dst) {
    return;
  }
  if (cache.style >= 0) {
    std::unique_ptr<Style> owned(
        gis::datasource::copy_ogr_style_from_ogr(feature));
    if (owned) {
      *dst = *owned;
      return;
    }
  }
  const char* kind = cached_field(feature, cache.kind);
  const bool river = kind[0] && (std::strcmp(kind, "river") == 0 ||
                                 std::strcmp(kind, "water") == 0);
  PenDesc pen;
  pen.lPenStyle = PS_SOLID;
  if (river) {
    pen.lPenColor = cached_color(feature, cache.stroke, RGB(100, 160, 208));
    pen.fPenWidth = fblc > 0.01f ? (0.9f / fblc) : 0.14f;
  } else {
    pen.lPenColor = cached_color(feature, cache.stroke, RGB(196, 190, 176));
    pen.fPenWidth = fblc > 0.01f ? (1.15f / fblc) : 0.2f;
  }
  BrushDesc brush;
  COLORREF fill = RGB(245, 243, 233);
  if (cache.fill >= 0) {
    (void)parse_html_rgb_local(feature->GetFieldAsString(cache.fill), &fill);
  }
  brush.lBrushColor = river ? RGB(163, 204, 255) : fill;
  dst->set_pen_desc(pen);
  dst->set_brush_desc(brush);
  dst->set_style_type(ST_PenDesc | ST_BrushDesc);
}

}  // namespace

void warm_prep_fields(OGRFeature* feature, PrepFieldCache* cache) {
  if (!feature || !cache || cache->warmed) {
    return;
  }
  cache->style = feature->GetFieldIndex("style");
  cache->kind = feature->GetFieldIndex("kind");
  cache->cls = feature->GetFieldIndex("class");
  cache->fill = feature->GetFieldIndex("fill");
  cache->stroke = feature->GetFieldIndex("stroke");
  cache->name = feature->GetFieldIndex("name");
  cache->anno = feature->GetFieldIndex("anno");
  cache->text = feature->GetFieldIndex("text");
  cache->adcode = feature->GetFieldIndex("adcode");
  cache->angle = feature->GetFieldIndex("angle");
  cache->warmed = true;
}

void prepare_one_feature(OGRFeature* feature, const gis::Envelope& env_viewp,
                         const LpToDp2& xform, float fblc,
                         PrepFieldCache* fields, PreparedFeature* out) {
  out->kind = PrepKind::Skip;
  out->parts.clear();
  out->anno[0] = '\0';
  out->label_priority = 5;
  out->is_river = false;
  out->road_class = 0;
  out->anno_angle = 0.f;
  out->feature_type = FtUnknown;
  if (!feature || !out) {
    return;
  }

  PrepFieldCache local_cache;
  PrepFieldCache* cache = fields ? fields : &local_cache;
  warm_prep_fields(feature, cache);

  // Borrow feature geometry �?no decode_ogr_geometry clone/delete.
  OGRGeometry* geom = feature->GetGeometryRef();
  if (!geom || geom->IsEmpty()) {
    return;
  }

  Envelope env_feature;
  geo::fill_envelope(*geom, &env_feature);
  if (!env_feature.intersects(env_viewp)) {
    return;
  }

  const OGRwkbGeometryType type = wkbFlatten(geom->getGeometryType());
  if (type != wkbPoint && env_too_small_on_device(env_feature, xform.scale)) {
    return;
  }

  // Points / labels: project in parallel; occupancy runs on serial play.
  if (type == wkbPoint) {
    const auto* pt = static_cast<const OGRPoint*>(geom);
    float xy[2] = {static_cast<float>(pt->getX()),
                   static_cast<float>(pt->getY())};
    long dp[2] = {};
    transform_xy_batch(xform, std::span<const float>(xy, 2),
                       std::span<long>(dp, 2));
    PrepPart part;
    part.pts.push_back(POINT{dp[0], dp[1]});
    part.counts.push_back(1);
    out->parts.push_back(std::move(part));

    const char* anno_f = cached_field(feature, cache->anno);
    const char* name = cached_field(feature, cache->name);
    const char* text = cached_field(feature, cache->text);
    // Nonempty anno / name / text �?label (china_city text layer uses anno;
    // point layer often only has name). Else POI disc.
    const char* label_src = carto2d_label_text(anno_f, name, text);
    if (label_src && label_src[0]) {
      const char* kind = cached_field(feature, cache->kind);
      const char* cls = cached_field(feature, cache->cls);
      const char* adcode = cached_field(feature, cache->adcode);
      strncpy_s(out->anno, label_src, _TRUNCATE);
      out->label_priority =
          carto2d_label_priority(out->anno, kind, cls, adcode);
      if (out->label_priority > carto2d_lod_max_priority(fblc)) {
        out->parts.clear();
        return;  // Skip �?below LOD
      }
      if (cache->angle >= 0) {
        out->anno_angle =
            static_cast<float>(feature->GetFieldAsDouble(cache->angle));
      }
      out->feature_type = FtAnno;
      out->kind = PrepKind::Anno;
      return;
    }

    out->feature_type = FtDot;
    out->kind = PrepKind::Point;
    return;
  }
  if (type == wkbMultiPoint) {
    const auto* multi = static_cast<const OGRMultiPoint*>(geom);
    const int n = multi->getNumGeometries();
    PrepPart part;
    part.pts.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
      const auto* pt = static_cast<const OGRPoint*>(multi->getGeometryRef(i));
      if (!pt) {
        continue;
      }
      float xy[2] = {static_cast<float>(pt->getX()),
                     static_cast<float>(pt->getY())};
      long dp[2] = {};
      transform_xy_batch(xform, std::span<const float>(xy, 2),
                         std::span<long>(dp, 2));
      part.pts.push_back(POINT{dp[0], dp[1]});
    }
    if (part.pts.empty()) {
      return;
    }
    part.counts.push_back(static_cast<int>(part.pts.size()));
    out->parts.push_back(std::move(part));
    out->feature_type = FtDot;
    out->kind = PrepKind::Point;
    return;
  }

  const char* kind = cached_field(feature, cache->kind);
  const char* cls = cached_field(feature, cache->cls);
  out->is_river = carto2d_is_river_kind(kind);
  out->road_class = carto2d_road_class(kind, cls);

  switch (type) {
    case wkbPolygon:
    case wkbTriangle:
    case wkbMultiPolygon:
    case wkbTIN:
      out->feature_type = FtSurface;
      break;
    case wkbLineString:
    case wkbMultiLineString:
      out->feature_type = FtCurve;
      break;
    default:
      out->kind = PrepKind::Fallback;
      return;
  }

  // Line labels only (polygons skip name/adcode/anno lookups).
  if (out->feature_type == FtCurve &&
      (out->is_river || out->road_class > 0)) {
    const char* name = cached_field(feature, cache->name);
    const char* anno = cached_field(feature, cache->anno);
    const char* text = cached_field(feature, cache->text);
    const char* adcode = cached_field(feature, cache->adcode);
    if (const char* picked = carto2d_label_text(anno, name, text)) {
      strncpy_s(out->anno, picked, _TRUNCATE);
    }
    out->label_priority = carto2d_label_priority(
        out->anno[0] ? out->anno : name, kind, cls, adcode);
  }

  fill_prep_style(feature, *cache, fblc, &out->style);

  switch (type) {
    case wkbPolygon:
    case wkbTriangle: {
      PrepPart part;
      if (prepare_polygon_part(static_cast<const OGRPolygon*>(geom), xform,
                               &part)) {
        out->parts.push_back(std::move(part));
        out->kind = PrepKind::Polygon;
      }
      break;
    }
    case wkbMultiPolygon:
    case wkbTIN: {
      const auto* multi = static_cast<const OGRMultiPolygon*>(geom);
      const int n = multi->getNumGeometries();
      for (int i = 0; i < n; ++i) {
        PrepPart part;
        if (prepare_polygon_part(
                static_cast<const OGRPolygon*>(multi->getGeometryRef(i)), xform,
                &part)) {
          out->parts.push_back(std::move(part));
        }
      }
      out->kind = out->parts.empty() ? PrepKind::Skip : PrepKind::Polygon;
      break;
    }
    case wkbLineString: {
      PrepPart part;
      if (append_projected_line(static_cast<const OGRLineString*>(geom), xform,
                                &part)) {
        out->parts.push_back(std::move(part));
        out->kind = PrepKind::Line;
      }
      break;
    }
    case wkbMultiLineString: {
      const auto* multi = static_cast<const OGRMultiLineString*>(geom);
      const int n = multi->getNumGeometries();
      PrepPart part;
      for (int i = 0; i < n; ++i) {
        (void)append_projected_line(
            static_cast<const OGRLineString*>(multi->getGeometryRef(i)), xform,
            &part);
      }
      if (!part.counts.empty()) {
        out->parts.push_back(std::move(part));
        out->kind = PrepKind::Line;
      }
      break;
    }
    default:
      out->kind = PrepKind::Fallback;
      break;
  }
}

void destroy_ogr_feats(std::vector<OGRFeature*>* feats) {
  if (!feats) {
    return;
  }
  for (OGRFeature* f : *feats) {
    OGRFeature::DestroyFeature(f);
  }
  feats->clear();
}

bool force_serial_ogr_layer(OGRLayer* layer,
                            const std::vector<OGRFeature*>& feats) {
  (void)feats;
  if (!layer) {
    return true;
  }
  // Grid / raster-tile multipoints still need the decode path; plain
  // point/anno layers prepare device coords in parallel like area/line.
  if (layer->FindFieldIndex("grid_row", TRUE) >= 0 ||
      layer->FindFieldIndex("grid_col", TRUE) >= 0) {
    return true;
  }
  return false;
}

}  // namespace detail
}  // namespace scenic
