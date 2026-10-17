// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/layout/paint/point.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

#include "base/process/switches.h"
#include "gis/style/paint_resolve.h"
#include "gis/style/eval/style_rules.h"
#include "gis/style/style_types.h"
#include "vista/component/map/layout.h"
#include "vista/component/map/layout/geom/attrs.h"
#include "vista/component/map/layout/geom/clip.h"
#include "vista/component/map/layout/geom/geom_walk.h"
#include "vista/component/map/layout/geom/mesh_emit.h"
#include "vista/component/map/layout/policy/gen.h"
#include "vista/component/map/layout/policy/view_metrics.h"
#include "ogrsf_frmts.h"

namespace vista {
namespace detail {
namespace {

bool heatmap_splat_fallback() {
  if (base::switch_is_one("map2d-heatmap-splat")) {
    return true;
  }
  const char* e = std::getenv("MAP2D_HEATMAP_SPLAT");
  return e && e[0] == '1' && e[1] == '\0';
}

// Collect point samples for heatmap (MultiPoint expands; else centroid).
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

struct HeatSample {
  double x = 0;
  double y = 0;
  float weight = 1.f;
};

// Density grid → RGBA8 texture + one kRaster DrawItem (host load_raster).
void emit_heatmap_density(const gis::style::StyleLayer& layer,
                          const LayoutInput& in,
                          const std::vector<HeatSample>& samples, double wupp,
                          MapIR* frame) {
  if (!frame || samples.empty() || !screen_ready(in.view) || wupp <= 0) {
    return;
  }
  gis::style::ResolvedPaint base_paint;
  gis::style::fill_resolved_paint(layer, nullptr, {}, in.zoom, &base_paint);
  const float intensity = base_paint.heatmap_intensity;
  const float layer_opacity = base_paint.heatmap_opacity;
  if (intensity <= 0.f || layer_opacity <= 0.f) {
    return;
  }
  // Kernel radius in pixels (Style Spec); floor grid so upload stays bounded.
  const float radius_px =
      (std::max)(1.f, base_paint.heatmap_radius > 0.f ? base_paint.heatmap_radius
                                                      : 30.f);
  const int gw = (std::clamp)(static_cast<int>(in.view.width_px / 2), 48, 512);
  const int gh =
      (std::clamp)(static_cast<int>(in.view.height_px / 2), 48, 512);
  std::vector<float> dens(static_cast<size_t>(gw) * static_cast<size_t>(gh),
                          0.f);
  const double span_x = in.view.max_x - in.view.min_x;
  const double span_y = in.view.max_y - in.view.min_y;
  if (!(span_x > 0.0) || !(span_y > 0.0)) {
    return;
  }
  const float sigma = radius_px * 0.5f;
  const float two_sigma2 = 2.f * sigma * sigma;
  const int r_cells = (std::max)(
      1, static_cast<int>(std::ceil(radius_px * 2.f *
                                    static_cast<float>(gw) /
                                    static_cast<float>(in.view.width_px))));

  for (const HeatSample& s : samples) {
    const ScreenPt sp = to_screen(in.view, s.x, s.y);
    const float gx =
        (sp.x / static_cast<float>(in.view.width_px)) * static_cast<float>(gw);
    const float gy =
        (sp.y / static_cast<float>(in.view.height_px)) * static_cast<float>(gh);
    const int cx = static_cast<int>(std::lround(gx));
    const int cy = static_cast<int>(std::lround(gy));
    const float w = s.weight > 0.f ? s.weight : 0.f;
    if (w <= 0.f) {
      continue;
    }
    for (int dy = -r_cells; dy <= r_cells; ++dy) {
      for (int dx = -r_cells; dx <= r_cells; ++dx) {
        const int ix = cx + dx;
        const int iy = cy + dy;
        if (ix < 0 || iy < 0 || ix >= gw || iy >= gh) {
          continue;
        }
        const float ddx = static_cast<float>(ix) + 0.5f - gx;
        const float ddy = static_cast<float>(iy) + 0.5f - gy;
        const float px = ddx * static_cast<float>(in.view.width_px) /
                         static_cast<float>(gw);
        const float py = ddy * static_cast<float>(in.view.height_px) /
                         static_cast<float>(gh);
        const float dist2 = px * px + py * py;
        if (dist2 > radius_px * radius_px * 4.f) {
          continue;
        }
        const float k = std::exp(-dist2 / two_sigma2);
        dens[static_cast<size_t>(iy) * static_cast<size_t>(gw) +
             static_cast<size_t>(ix)] += w * k;
      }
    }
  }

  float max_d = 0.f;
  for (float v : dens) {
    max_d = (std::max)(max_d, v);
  }
  if (max_d <= 1e-6f) {
    return;
  }

  const uint32_t argb = base_paint.heatmap_color;
  const uint8_t cr = static_cast<uint8_t>((argb >> 16) & 0xFFu);
  const uint8_t cg = static_cast<uint8_t>((argb >> 8) & 0xFFu);
  const uint8_t cb = static_cast<uint8_t>(argb & 0xFFu);
  const float base_a =
      static_cast<float>((argb >> 24) & 0xFFu) / 255.f * layer_opacity *
      intensity;

  std::vector<uint8_t> rgba(static_cast<size_t>(gw) * static_cast<size_t>(gh) *
                                4u,
                            0);
  for (int iy = 0; iy < gh; ++iy) {
    for (int ix = 0; ix < gw; ++ix) {
      const float d =
          dens[static_cast<size_t>(iy) * static_cast<size_t>(gw) +
               static_cast<size_t>(ix)] /
          max_d;
      if (d < 0.02f) {
        continue;
      }
      float a = base_a * d;
      if (a > 1.f) {
        a = 1.f;
      }
      const size_t o =
          (static_cast<size_t>(iy) * static_cast<size_t>(gw) +
           static_cast<size_t>(ix)) *
          4u;
      rgba[o + 0] = cr;
      rgba[o + 1] = cg;
      rgba[o + 2] = cb;
      rgba[o + 3] = static_cast<uint8_t>(std::lround(a * 255.f));
    }
  }

  if (HeatmapBake* bake = layout_heatmap_bake()) {
    bake->rgba = std::move(rgba);
    bake->width = gw;
    bake->height = gh;
  }

  DrawItem item;
  item.kind = DrawKind::kRaster;
  item.blend = DrawBlend::kOver;
  item.pixel_space = false;
  item.opacity = 1.f;
  item.rgba = 0xffffffffu;
  item.codepoint = kHeatmapTextureKey;
  const float x0 = static_cast<float>(in.view.min_x);
  const float x1 = static_cast<float>(in.view.max_x);
  const float y0 = static_cast<float>(in.view.min_y);
  const float y1 = static_cast<float>(in.view.max_y);
  // v=0 on north (max_y), matching hillshade / StretchBlt convention.
  item.vertices.push_back(Vertex{x0, y1, 0, 0, 0});
  item.vertices.push_back(Vertex{x1, y1, 0, 1, 0});
  item.vertices.push_back(Vertex{x1, y0, 0, 1, 1});
  item.vertices.push_back(Vertex{x0, y0, 0, 0, 1});
  item.indices = {0, 1, 2, 0, 2, 3};
  frame->items.push_back(std::move(item));
  (void)wupp;
}

void emit_heatmap_splat(const gis::style::StyleLayer& layer,
                        const LayoutInput& in,
                        const std::vector<LayerBatch>& layers, double wupp,
                        MapIR* frame, const LayoutTile* clip_tile) {
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

// Heatmap: density → RGBA8 + kRaster (kHeatmapTextureKey). Host load_raster.
// MAP2D_HEATMAP_SPLAT=1 / --map2d-heatmap-splat keeps legacy circle splat.
void emit_heatmap(const gis::style::StyleLayer& layer, const LayoutInput& in,
                  const std::vector<LayerBatch>& layers, double wupp,
                  MapIR* frame, const LayoutTile* clip_tile) {
  if (wupp <= 0 || !frame) {
    return;
  }
  if (heatmap_splat_fallback()) {
    emit_heatmap_splat(layer, in, layers, wupp, frame, clip_tile);
    return;
  }

  std::vector<HeatSample> samples;
  samples.reserve(64);
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
      std::vector<std::pair<double, double>> xy;
      heatmap_sample_xy(geom, &xy);
      for (const auto& p : xy) {
        HeatSample s;
        s.x = p.first;
        s.y = p.second;
        s.weight = paint.heatmap_weight;
        samples.push_back(s);
      }
    }
  }
  emit_heatmap_density(layer, in, samples, wupp, frame);
}

}  // namespace detail
}  // namespace vista
