// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/layout/point.h"

#include <memory>
#include <utility>
#include <vector>

#include "gis/style/paint_resolve.h"
#include "gis/style/eval/style_rules.h"
#include "gis/style/style_types.h"
#include "vista/component/map/layout/attrs.h"
#include "vista/component/map/layout/emit.h"
#include "vista/component/map/layout/geom_walk.h"
#include "vista/component/map/layout/mesh_emit.h"
#include "vista/component/map/layout/pack.h"
#include "ogrsf_frmts.h"

namespace vista {
namespace detail {
namespace {

// Collect point samples for heatmap splat (MultiPoint expands; else centroid).
void heatmap_sample_xy(const OGRGeometry* geom,
                       std::vector<std::pair<double, double>>* out) {
  if (!geom || geom->IsEmpty() || !out) {
    return;
  }
  if (const auto* pt = dynamic_cast<const OGRPoint*>(geom)) {
    out->emplace_back(pt->getX(), pt->getY());
    return;
  }
  if (const auto* multi = dynamic_cast<const OGRMultiPoint*>(geom)) {
    const int n = multi->getNumGeometries();
    for (int i = 0; i < n; ++i) {
      heatmap_sample_xy(multi->getGeometryRef(i), out);
    }
    return;
  }
  double x = 0;
  double y = 0;
  if (anchor_xy(geom, &x, &y)) {
    out->emplace_back(x, y);
  }
}

}  // namespace

void emit_circles(const gis::style::StyleLayer& layer, const LayoutInput& in,
                  const std::vector<LayerBatch>& layers, double wupp,
                  MapIR* frame, const LayoutTile* clip_tile) {
  if (wupp <= 0) {
    return;
  }
  std::vector<std::unique_ptr<OGRGeometry>> clip_store;
  for (const LayerBatch& batch : layers) {
    if (!layer_uses_batch(layer, batch)) {
      continue;
    }
    for (size_t i = 0; i < batch.geoms.size(); ++i) {
      if (layout_gen_stale(in)) {
        return;
      }
      const OGRGeometry* raw = batch.geoms[i];
      const OGRGeometry* geom = nullptr;
      if (!prepare_tile_clip(raw, clip_tile, &clip_store, &geom) || !geom) {
        continue;
      }
      double x = 0;
      double y = 0;
      if (!anchor_xy(geom, &x, &y)) {
        continue;
      }
      const gis::style::AttrMap attrs = attrs_at(batch, i);
      if (!gis::style::eval_filter(layer.filter, attrs)) {
        continue;
      }
      gis::style::ResolvedPaint paint;
      gis::style::fill_resolved_paint(layer, nullptr, attrs, in.zoom, &paint);
      const double radius = static_cast<double>(paint.circle_radius) * wupp;
      if (radius <= 0) {
        continue;
      }
      DrawItem item;
      item.kind = DrawKind::kCircle;
      item.pixel_space = false;
      item.rgba = paint.circle_color;
      item.opacity = paint.circle_opacity;
      emit_circle(&item, x, y, radius);
      frame->items.push_back(std::move(item));
    }
  }
}

// Heatmap v1: Style Spec paint constants 鈫?soft circle splats (kernel underlay
// stand-in). No GPU density texture / weight property expressions yet.
void emit_heatmap(const gis::style::StyleLayer& layer, const LayoutInput& in,
                  const std::vector<LayerBatch>& layers, double wupp,
                  MapIR* frame, const LayoutTile* clip_tile) {
  if (wupp <= 0 || !frame) {
    return;
  }
  std::vector<std::unique_ptr<OGRGeometry>> clip_store;
  for (const LayerBatch& batch : layers) {
    if (!layer_uses_batch(layer, batch)) {
      continue;
    }
    for (size_t i = 0; i < batch.geoms.size(); ++i) {
      if (layout_gen_stale(in)) {
        return;
      }
      const OGRGeometry* raw = batch.geoms[i];
      const OGRGeometry* geom = nullptr;
      if (!prepare_tile_clip(raw, clip_tile, &clip_store, &geom) || !geom) {
        continue;
      }
      const gis::style::AttrMap attrs = attrs_at(batch, i);
      if (!gis::style::eval_filter(layer.filter, attrs)) {
        continue;
      }
      gis::style::ResolvedPaint paint;
      gis::style::fill_resolved_paint(layer, nullptr, attrs, in.zoom, &paint);
      const double radius = static_cast<double>(paint.heatmap_radius) * wupp;
      if (radius <= 0) {
        continue;
      }
      float opacity =
          paint.heatmap_opacity * paint.heatmap_weight * paint.heatmap_intensity;
      if (opacity <= 0.f) {
        continue;
      }
      if (opacity > 1.f) {
        opacity = 1.f;
      }
      std::vector<std::pair<double, double>> samples;
      heatmap_sample_xy(geom, &samples);
      for (const auto& sample : samples) {
        DrawItem item;
        item.kind = DrawKind::kCircle;
        item.pixel_space = false;
        item.rgba = paint.heatmap_color;
        item.opacity = opacity;
        emit_circle(&item, sample.first, sample.second, radius);
        frame->items.push_back(std::move(item));
      }
    }
  }
}

}  // namespace detail
}  // namespace vista
