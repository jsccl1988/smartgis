// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/layout/line.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

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

}  // namespace

void emit_lines(const std::vector<const gis::style::StyleLayer*>& line_layers,
                const LayoutInput& in, const std::vector<LayerBatch>& layers,
                double wupp, MapIR* frame, const LayoutTile* clip_tile) {
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
    batches.visit(layer, layers, [&](const LayerBatch& batch) {
      for (size_t i = 0; i < batch.geoms.size(); ++i) {
        if (layout_gen_stale(in)) {
          return;
        }
        const OGRGeometry* raw = batch.geoms[i];
        const OGRGeometry* geom = raw;
        if (clip_tile) {
          if (!prepare_tile_clip(raw, clip_tile, &clip_store, &geom) || !geom) {
            continue;
          }
        }
        const gis::style::AttrMap& attrs = attrs_at(batch, i);
        if (!gis::style::eval_filter(layer.filter, attrs)) {
          continue;
        }
        gis::style::ResolvedPaint paint;
        gis::style::fill_resolved_paint(layer, nullptr, attrs, in.zoom, &paint);
        const LineTessOptions opts = line_options(paint, wupp);
        for_each_line(geom, [&](const OGRLineString* line) {
          if (line_skips_tessellation(line, opts)) {
            return;
          }
          jobs.push_back(LineJob{li, line, opts, paint.line_color,
                                 paint.line_opacity});
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
               MapIR* frame, const LayoutTile* clip_tile) {
  std::vector<const gis::style::StyleLayer*> one{&layer};
  emit_lines(one, in, layers, wupp, frame, clip_tile);
}

}  // namespace detail
}  // namespace vista
