// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/map/layout/line.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "gis/style/paint_resolve.h"
#include "gis/style/eval/style_rules.h"
#include "gis/style/style_types.h"
#include "vista/map/layout/attrs.h"
#include "vista/map/layout/emit.h"
#include "vista/map/layout/geom_walk.h"
#include "vista/map/layout/mesh_emit.h"
#include "vista/map/layout/pack.h"
#include "vista/map/layout/tess_grain.h"
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

}  // namespace

void emit_lines(const std::vector<const gis::style::StyleLayer*>& line_layers,
                const LayoutInput& in, const std::vector<LayerBatch>& layers,
                double wupp, MapIR* frame, const LayoutTile* clip_tile) {
  if (line_layers.empty()) {
    return;
  }
  std::unordered_map<std::string, std::vector<const LayerBatch*>> by_source;
  by_source.reserve(layers.size() * 2);
  for (const LayerBatch& batch : layers) {
    by_source[batch.source_layer].push_back(&batch);
  }
  auto visit_batches = [&](const gis::style::StyleLayer& layer, auto&& fn) {
    if (layer.source_layer.empty()) {
      for (const LayerBatch& batch : layers) {
        fn(batch);
      }
      return;
    }
    const auto it = by_source.find(layer.source_layer);
    if (it == by_source.end()) {
      return;
    }
    for (const LayerBatch* batch : it->second) {
      fn(*batch);
    }
  };

  std::vector<LineJob> jobs;
  std::vector<std::unique_ptr<OGRGeometry>> clip_store;
  for (size_t li = 0; li < line_layers.size(); ++li) {
    if (layout_gen_stale(in)) {
      return;
    }
    const gis::style::StyleLayer& layer = *line_layers[li];
    visit_batches(layer, [&](const LayerBatch& batch) {
      for (size_t i = 0; i < batch.geoms.size(); ++i) {
        if (layout_gen_stale(in)) {
          return;
        }
        const OGRGeometry* raw = batch.geoms[i];
        const OGRGeometry* geom = nullptr;
        if (!prepare_tile_clip(raw, clip_tile, &clip_store, &geom) || !geom) {
          continue;
        }
        const gis::style::AttrMap& attrs = attrs_at(batch, i);
        if (!gis::style::eval_filter(layer.filter, attrs)) {
          continue;
        }
        gis::style::ResolvedPaint paint;
        gis::style::fill_resolved_paint(layer, nullptr, attrs, in.zoom, &paint);
        const LineTessOptions opts = line_options(paint, wupp);
        for_each_line(geom, [&](const OGRLineString* line) {
          jobs.push_back(LineJob{li, line, opts, paint.line_color,
                                 paint.line_opacity});
        });
      }
    });
  }
  if (jobs.empty()) {
    return;
  }
  if (!vista_layout_parallel_enabled() || jobs.size() < kParallelTessMinGeoms) {
    for (const LineJob& job : jobs) {
      if (layout_gen_stale(in)) {
        return;
      }
      vista::TessMesh mesh;
      if (!vista::tessellate_line(job.line, job.opts, mesh) ||
          mesh.indices.empty()) {
        continue;
      }
      frame->items.push_back(
          mesh_item(mesh, DrawKind::kLine, job.rgba, job.opacity));
    }
    return;
  }
  std::vector<DrawItem> items(jobs.size());
  std::vector<char> valid(jobs.size(), 0);
  base::execution::GlobalNThreadPoolExecutor executor;
  base::execution::parallel_for(
      executor, size_t{0}, jobs.size(),
      [&](size_t i) {
        if (layout_gen_stale(in)) {
          return;
        }
        vista::TessMesh mesh;
        if (!vista::tessellate_line(jobs[i].line, jobs[i].opts, mesh) ||
            mesh.indices.empty()) {
          return;
        }
        items[i] =
            mesh_item(mesh, DrawKind::kLine, jobs[i].rgba, jobs[i].opacity);
        valid[i] = 1;
      },
      kParallelTessGrain);
  for (size_t li = 0; li < line_layers.size(); ++li) {
    for (size_t i = 0; i < jobs.size(); ++i) {
      if (valid[i] && jobs[i].layer_ord == li) {
        frame->items.push_back(std::move(items[i]));
      }
    }
  }
}

void emit_line(const gis::style::StyleLayer& layer, const LayoutInput& in,
               const std::vector<LayerBatch>& layers, double wupp,
               MapIR* frame, const LayoutTile* clip_tile) {
  std::vector<const gis::style::StyleLayer*> one{&layer};
  emit_lines(one, in, layers, wupp, frame, clip_tile);
}

}  // namespace detail
}  // namespace vista
