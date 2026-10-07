// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/layout/line.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gis/style/eval/expression.h"
#include "gis/style/eval/style_rules.h"
#include "gis/style/paint_resolve.h"
#include "gis/style/style_types.h"
#include "vista/component/map/layout/attrs.h"
#include "vista/component/map/layout/clip.h"
#include "vista/component/map/layout/geom_walk.h"
#include "vista/component/map/layout/mesh_emit.h"
#include "vista/component/map/layout/source_index.h"
#include "vista/component/map/layout/tess_jobs.h"
#include "vista/component/map/layout/gen.h"
#include "vista/mesh/line/line_tess.h"
#include "vista/mesh/tessellate.h"
#include "ogrsf_frmts.h"

namespace vista {
namespace detail {
namespace {

struct LineJob {
  size_t layer_ord = 0;
  const OGRLineString* line = nullptr;
  LineTessOptions opts;
  uint32_t rgba = 0;
  float opacity = 1.f;
};

bool map_value_is_expression(const std::map<std::string, std::string>& m,
                             const char* key) {
  const auto it = m.find(key);
  return it != m.end() && gis::style::looks_like_expression(it->second);
}

// China carto line paints are literals; hoist resolve+line_options off the
// per-feature loop so emit_line wall is tess, not paint_resolve.
bool line_paint_is_feature_constant(const gis::style::StyleLayer& layer) {
  static constexpr const char* kPaintKeys[] = {
      "line-color", "line-width", "line-opacity", "line-dasharray",
      "line-cap",   "line-join"};
  for (const char* key : kPaintKeys) {
    if (map_value_is_expression(layer.paint, key)) {
      return false;
    }
  }
  return !map_value_is_expression(layer.layout, "line-cap") &&
         !map_value_is_expression(layer.layout, "line-join");
}

}  // namespace

void emit_lines(const std::vector<const gis::style::StyleLayer*>& line_layers,
                const LayoutInput& in, const std::vector<LayerBatch>& layers,
                double wupp, MapIR* frame, const LayoutTile* clip_tile,
                bool intersect_clip) {
  if (line_layers.empty() || !frame) {
    return;
  }
  const SourceBatchIndex batches(layers);

  std::vector<LineJob> jobs;
  jobs.reserve(4096);
  std::vector<std::unique_ptr<OGRGeometry>> clip_store;
  for (size_t li = 0; li < line_layers.size(); ++li) {
    if (layout_gen_stale(in)) {
      return;
    }
    const gis::style::StyleLayer& layer = *line_layers[li];
    const bool constant_paint = line_paint_is_feature_constant(layer);
    gis::style::ResolvedPaint layer_paint;
    LineTessOptions layer_opts;
    uint32_t layer_rgba = 0;
    float layer_opacity = 1.f;
    if (constant_paint) {
      static const gis::style::AttrMap kEmptyAttrs;
      gis::style::fill_resolved_paint(layer, nullptr, kEmptyAttrs, in.zoom,
                                      &layer_paint);
      layer_opts = line_options(layer_paint, wupp);
      layer_rgba = layer_paint.line_color;
      layer_opacity = layer_paint.line_opacity;
    }
    batches.visit(layer, layers, [&](const LayerBatch& batch) {
      for (size_t i = 0; i < batch.geoms.size(); ++i) {
        if (layout_gen_stale(in)) {
          return;
        }
        const OGRGeometry* raw = batch.geoms[i];
        const OGRGeometry* geom = raw;
        if (clip_tile) {
          if (!prepare_tile_clip(raw, clip_tile, &clip_store, &geom,
                                 intersect_clip) ||
              !geom) {
            continue;
          }
        }
        const gis::style::AttrMap& attrs = attrs_at(batch, i);
        if (!gis::style::eval_filter(layer.filter, attrs)) {
          continue;
        }
        LineTessOptions opts = layer_opts;
        uint32_t rgba = layer_rgba;
        float opacity = layer_opacity;
        if (!constant_paint) {
          gis::style::ResolvedPaint paint;
          gis::style::fill_resolved_paint(layer, nullptr, attrs, in.zoom,
                                          &paint);
          opts = line_options(paint, wupp);
          rgba = paint.line_color;
          opacity = paint.line_opacity;
        }
        for_each_line(geom, [&](const OGRLineString* line) {
          if (line_skips_tessellation(line, opts)) {
            return;
          }
          jobs.push_back(LineJob{li, line, opts, rgba, opacity});
        });
      }
    });
  }
  if (jobs.empty()) {
    return;
  }
  auto to_item = [](const LineJob& job) -> std::optional<DrawItem> {
    vista::TessMesh mesh;
    if (!vista::tessellate_line(job.line, job.opts, mesh) ||
        mesh.indices.empty()) {
      return std::nullopt;
    }
    return mesh_item(mesh, DrawKind::kLine, job.rgba, job.opacity);
  };
  run_ordered_tess(in, line_layers.size(), jobs, to_item, &frame->items);
}

void emit_line(const gis::style::StyleLayer& layer, const LayoutInput& in,
               const std::vector<LayerBatch>& layers, double wupp,
               MapIR* frame, const LayoutTile* clip_tile,
               bool intersect_clip) {
  std::vector<const gis::style::StyleLayer*> one{&layer};
  emit_lines(one, in, layers, wupp, frame, clip_tile, intersect_clip);
}

}  // namespace detail
}  // namespace vista
