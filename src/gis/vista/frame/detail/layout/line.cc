// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/vista/frame/detail/layout/line.h"

#include <utility>
#include <vector>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "gis/present/style/paint_resolve.h"
#include "gis/present/style/style_rules.h"
#include "gis/present/style/style_types.h"
#include "gis/vista/frame/detail/layout/attrs.h"
#include "gis/vista/frame/detail/layout/geom_mesh.h"
#include "gis/vista/world/terrain/mesh/tessellate.h"
#include "ogrsf_frmts.h"

namespace gis {
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
                double wupp, MapFrame* frame) {
  if (line_layers.empty()) {
    return;
  }
  std::vector<LineJob> jobs;
  for (size_t li = 0; li < line_layers.size(); ++li) {
    const gis::style::StyleLayer& layer = *line_layers[li];
    for (const LayerBatch& batch : layers) {
      if (!layer_uses_batch(layer, batch)) {
        continue;
      }
      for (size_t i = 0; i < batch.geoms.size(); ++i) {
        const OGRGeometry* geom = batch.geoms[i];
        if (!geom) {
          continue;
        }
        const gis::style::AttrMap attrs = attrs_at(batch, i);
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
    }
  }
  if (jobs.empty()) {
    return;
  }
  if (jobs.size() < kParallelTessMinGeoms) {
    for (const LineJob& job : jobs) {
      gis::TessMesh mesh;
      if (!gis::tessellate_line(job.line, job.opts, mesh) ||
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
        gis::TessMesh mesh;
        if (!gis::tessellate_line(jobs[i].line, jobs[i].opts, mesh) ||
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
               MapFrame* frame) {
  std::vector<const gis::style::StyleLayer*> one{&layer};
  emit_lines(one, in, layers, wupp, frame);
}

}  // namespace detail
}  // namespace vista
}  // namespace gis
