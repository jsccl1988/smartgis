// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/paint/map/map_feature_draw.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "base/math/simd/simd.h"
#include "base/trace/event/process_trace.h"
#include "gis/datasource/ogr/ogr_feature_codec.h"
#include "gis/geo/ops/geometry_traits.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/device_geom.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/feature_kind.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_api.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_ogr.h"
#include "scenic/render/rhi2d/impl/common/paint/map/map_geom_trace.h"
#include "scenic/render/rhi2d/impl/common/paint/map/map_view.h"
#include "ogrsf_frmts.h"

using namespace gis;

namespace scenic {
namespace detail {

int draw_unprepared_geometry(Rhi2dCartoDraw* carto, const RenderContext& ctx,
                             const RenderOptions2d* options,
                             const OGRGeometry* geom, const Style* style,
                             int op, const gis::Envelope* env_viewp) {
  if (!geom || !carto) {
    return kErrInvalidParam;
  }

  const OGRwkbGeometryType type = wkbFlatten(geom->getGeometryType());

  Envelope env_feature;
  Envelope env_view;
  geo::fill_envelope(*geom, &env_feature);
  if (env_viewp) {
    env_view = *env_viewp;
  } else if (!map_view_envelope(carto, ctx, &env_view)) {
    return kErrInvalidParam;
  }

  fRect fenv;
  lRect lenv;
  envelope_to_rect(fenv, env_feature);
  carto->lrect_to_drect(fenv, lenv);

  if (!env_feature.intersects(env_view) ||
      (type != wkbPoint && lenv.height() < 2 && lenv.width() < 2)) {
    return kErrNone;
  }

  HDC dc = carto->dc();
  const bool need_save_dc =
      options && options->bShowMBR;  // MBR stroking mutates DC pen state
  if (need_save_dc) {
    ::SaveDC(dc);
  }

  if (!carto->lock_style()) {
    carto->prepare_for_drawing(style, op);
  }

  if (options && options->bShowMBR) {
    const float xy[10] = {
        static_cast<float>(env_feature.MinX),
        static_cast<float>(env_feature.MinY),
        static_cast<float>(env_feature.MaxX),
        static_cast<float>(env_feature.MinY),
        static_cast<float>(env_feature.MaxX),
        static_cast<float>(env_feature.MaxY),
        static_cast<float>(env_feature.MinX),
        static_cast<float>(env_feature.MaxY),
        static_cast<float>(env_feature.MinX),
        static_cast<float>(env_feature.MinY),
    };
    long out[10] = {};
    transform_xy_batch(make_lp_to_dp(ctx), xy, out);
    MoveToEx(dc, out[0], out[1], nullptr);
    LineTo(dc, out[2], out[3]);
    LineTo(dc, out[4], out[5]);
    LineTo(dc, out[6], out[7]);
    LineTo(dc, out[8], out[9]);
  }

  const auto draw_begin = base::trace::Trace::time_point::clock::now();
  switch (type) {
    case wkbPoint:
      carto->draw_point(style, static_cast<const OGRPoint*>(geom));
      break;
    case wkbLineString:
      carto->draw_line_string(static_cast<const OGRLineString*>(geom));
      break;
    case wkbPolygon:
    case wkbTriangle:
      carto->draw_polygon(static_cast<const OGRPolygon*>(geom));
      break;
    case wkbMultiPoint:
      carto->draw_multi_point(style, static_cast<const OGRMultiPoint*>(geom));
      break;
    case wkbMultiLineString:
      carto->draw_multi_line_string(
          static_cast<const OGRMultiLineString*>(geom));
      break;
    case wkbMultiPolygon:
    case wkbTIN:
      carto->draw_multi_polygon(static_cast<const OGRMultiPolygon*>(geom));
      break;
    case wkbLinearRing:
      carto->draw_linear_ring(static_cast<const OGRLinearRing*>(geom));
      break;
    default:
      break;
  }
  const int64_t us = std::chrono::duration_cast<std::chrono::microseconds>(
                         base::trace::Trace::time_point::clock::now() - draw_begin)
                         .count();
  add_geom_draw_us(static_cast<int>(type), carto->feature_type() == FtAnno, us);
  if (!carto->lock_style()) {
    carto->end_drawing();
  }

  if (need_save_dc) {
    ::RestoreDC(dc, -1);
  }

  return kErrNone;
}

int draw_unprepared_feature(Rhi2dCartoDraw* carto, const RenderContext& ctx,
                            const RenderOptions2d* options, OGRFeature* feature,
                            int op, const PrepFieldCache* fields,
                            const gis::Envelope* env_viewp) {
  if (feature == nullptr || !carto) {
    return kErrInvalidParam;
  }

  carto->feature_type() = leftover_feature_type_of(feature);
  std::unique_ptr<Style> owned_style(
      gis::datasource::copy_ogr_style_from_ogr(feature));
  Style fallback;
  Style* style = owned_style.get();
  if (!style) {
    gis::datasource::fill_default_draw_style(feature, &fallback, ctx.fblc);
    style = &fallback;
  }
  OGRGeometry* geom = leftover_decode_geometry(
      feature, static_cast<FeatureType>(carto->feature_type()));
  char* anno = carto->anno_buf();
  PrepFieldCache local;
  const PrepFieldCache* cache = fields;
  if (!cache || !cache->warmed) {
    warm_prep_fields(feature, &local);
    cache = &local;
  }
  if (carto->feature_type() == FtAnno) {
    if (cache->anno >= 0) {
      const char* text = feature->GetFieldAsString(cache->anno);
      if (text) {
        strncpy_s(anno, 2000, text, _TRUNCATE);
      }
    }
    if (cache->angle >= 0) {
      carto->anno_angle() =
          static_cast<float>(feature->GetFieldAsDouble(cache->angle));
    }
  }
  const char* name = prep_field_cstr(feature, cache->name);
  const char* kind = prep_field_cstr(feature, cache->kind);
  const char* cls = prep_field_cstr(feature, cache->cls);
  const char* adcode = prep_field_cstr(feature, cache->adcode);
  const char* label =
      (carto->feature_type() == FtAnno && anno[0]) ? anno : name;
  carto->label_priority() = carto2d_label_priority(label, kind, cls, adcode);
  carto->is_river() = carto2d_is_river_kind(kind);
  carto->road_class() = carto2d_road_class(kind, cls);
  if (!(carto->feature_type() == FtAnno && anno[0])) {
    if (const char* picked = carto2d_label_text(
            prep_field_cstr(feature, cache->anno), name,
            prep_field_cstr(feature, cache->text))) {
      strncpy(anno, picked, 1999);
      anno[1999] = '\0';
    }
  }
  const int rc =
      draw_unprepared_geometry(carto, ctx, options, geom, style, op, env_viewp);
  delete geom;
  return rc;
}

}  // namespace detail
}  // namespace scenic
