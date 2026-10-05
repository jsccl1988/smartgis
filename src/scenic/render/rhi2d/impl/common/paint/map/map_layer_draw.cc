// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/paint/map/map_layer_draw.h"

#include <chrono>
#include <string>

#include "base/trace/event/process_trace.h"
#include "gis/map/map_layer.h"
#include "scenic/render/frame.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/device_geom.h"
#include "scenic/render/rhi2d/impl/common/paint/map/map_feature_draw.h"
#include "scenic/render/rhi2d/impl/common/paint/map/map_geom_trace.h"
#include "scenic/render/rhi2d/impl/common/paint/map/map_prep_pipeline.h"
#include "scenic/render/rhi2d/impl/common/paint/map/map_view.h"
#include "ogrsf_frmts.h"

using namespace gis;

namespace scenic {
namespace detail {
namespace {

void collect_ogr_features(OGRLayer* ogr, OgrLayerBatch* batch) {
  ogr->ResetReading();
  while (OGRFeature* feat = ogr->GetNextFeature()) {
    batch->feats.push_back(feat);
  }
  batch->force_serial = force_serial_ogr_layer(ogr, batch->feats);
}

void stretch_layer_image(Rhi2dCartoDraw* carto, const char* bytes, int size,
                         const Envelope& loc_env, long code_type) {
  fRect loc_rect;
  loc_rect.lb.x = static_cast<float>(loc_env.MinX);
  loc_rect.lb.y = static_cast<float>(loc_env.MinY);
  loc_rect.rt.x = static_cast<float>(loc_env.MaxX);
  loc_rect.rt.y = static_cast<float>(loc_env.MaxY);
  carto->stretch_image(bytes, size, loc_rect, code_type);
}

}  // namespace

bool ogr_layer_envelope(OGRLayer* layer, gis::Envelope* out) {
  if (!layer || !out) {
    return false;
  }
  OGREnvelope ogr_env;
  if (layer->GetExtent(&ogr_env, FALSE) != OGRERR_NONE &&
      layer->GetExtent(&ogr_env, TRUE) != OGRERR_NONE) {
    return false;
  }
  out->MinX = ogr_env.MinX;
  out->MinY = ogr_env.MinY;
  out->MaxX = ogr_env.MaxX;
  out->MaxY = ogr_env.MaxY;
  return true;
}

void collect_visible_map_layers(
    const gis::Map* map, const gis::Envelope& env_viewp,
    const FrameAbortFn& aborted, std::vector<OgrLayerBatch>* ogr_batches,
    std::vector<const datasource::OgrRasterLayer*>* rasters,
    std::vector<const tile::ProviderTileLayer*>* tiles) {
  if (!map || !ogr_batches || !rasters || !tiles) {
    return;
  }
  const int n = map->layer_count();
  ogr_batches->reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    if (aborted && aborted()) {
      break;
    }
    if (!map->is_layer_visible(i)) {
      continue;
    }
    if (OGRLayer* ogr = const_cast<OGRLayer*>(map->ogr_layer(i))) {
      OgrLayerBatch batch;
      batch.layer = ogr;
      Envelope env_layer;
      if (ogr_layer_envelope(ogr, &env_layer)) {
        batch.culled = !env_layer.intersects(env_viewp);
      }
      if (!batch.culled) {
        collect_ogr_features(ogr, &batch);
      }
      ogr_batches->push_back(std::move(batch));
    } else if (const MapLayer* ml = map->map_layer(i)) {
      if (ml->raster()) {
        rasters->push_back(ml->raster());
      } else if (ml->tile()) {
        tiles->push_back(ml->tile());
      }
    }
  }
}

void prepare_ogr_batches(std::vector<OgrLayerBatch>* ogr_batches,
                         const gis::Envelope& env_viewp, const LpToDp2& xform,
                         float fblc) {
  if (!ogr_batches) {
    return;
  }
  struct PrepJob {
    size_t batch = 0;
    size_t feat = 0;
  };
  thread_local std::vector<PrepJob> prep_jobs;
  prep_jobs.clear();

  bool any_parallel_layer = false;
  for (OgrLayerBatch& b : *ogr_batches) {
    if (b.culled || b.force_serial) {
      continue;
    }
    if (b.feats.size() >= kMinParallelFeatures) {
      any_parallel_layer = true;
      break;
    }
  }
  if (!any_parallel_layer) {
    return;
  }

  for (size_t bi = 0; bi < ogr_batches->size(); ++bi) {
    OgrLayerBatch& b = (*ogr_batches)[bi];
    if (b.culled || b.force_serial || b.feats.empty()) {
      continue;
    }
    warm_prep_fields(b.feats.front(), &b.fields);
    b.prepared.resize(b.feats.size());
    for (size_t fi = 0; fi < b.feats.size(); ++fi) {
      prep_jobs.push_back(PrepJob{bi, fi});
    }
  }
  if (prep_jobs.empty()) {
    return;
  }

  BASE_TRACE_EVENT("prep", "gdi.map");
  log_frame_flow("gdi.prep Pipeline run");
  // Debug Edit/china: parallel prep has intermittently AV'd after
  // "Pipeline done" (legacy.browse.2d / SmartGis heap+AV). Keep the
  // Pipeline shape in Release; Debug runs the same body serially.
#if defined(_DEBUG)
  for (const PrepJob& job : prep_jobs) {
    OgrLayerBatch& b = (*ogr_batches)[job.batch];
    prepare_one_feature(b.feats[job.feat], env_viewp, xform, fblc, &b.fields,
                        &b.prepared[job.feat]);
  }
#else
  run_chunked_prep_pipeline(prep_jobs.size(), [&](size_t ji) {
    const PrepJob& job = prep_jobs[ji];
    OgrLayerBatch& b = (*ogr_batches)[job.batch];
    prepare_one_feature(b.feats[job.feat], env_viewp, xform, fblc, &b.fields,
                        &b.prepared[job.feat]);
  });
#endif
  log_frame_flow("gdi.prep Pipeline done");
}

void encode_ogr_batch(Rhi2dCartoDraw* carto, OgrLayerBatch* b, int op,
                      const FeatureFallbackFn& fallback) {
  if (!b || !b->layer) {
    return;
  }
  const std::string name = trace_name(b->layer);
  BASE_TRACE_EVENT(name, "gdi.layer");
  GeomTraceScope geom;

  if (!b->prepared.empty()) {
    BASE_TRACE_EVENT("draw", "gdi.layer");
    draw_prepared_batch(carto, op, &b->feats, &b->prepared, fallback,
                        DrawBatchMode::All);
  } else {
    for (OGRFeature* feat : b->feats) {
      if (fallback) {
        fallback(feat, op);
      }
      OGRFeature::DestroyFeature(feat);
    }
    b->feats.clear();
  }
}

int paint_ogr_layer(Rhi2dCartoDraw* carto, const RenderContext& ctx,
                    const RenderOptions2d* options, OGRLayer* layer, int op,
                    const FeatureFallbackFn& fallback) {
  if (layer == nullptr) {
    return kErrInvalidParam;
  }
  const std::string name = trace_name(layer);
  BASE_TRACE_EVENT(name, "gdi.layer");

  Envelope env_layer;
  Envelope env_viewp;
  if (!map_view_envelope(carto, ctx, &env_viewp)) {
    return kErrInvalidParam;
  }
  if (ogr_layer_envelope(layer, &env_layer) &&
      !env_layer.intersects(env_viewp)) {
    return kErrNone;
  }

  GeomTraceScope geom;
  OgrLayerBatch batch;
  batch.layer = layer;
  collect_ogr_features(layer, &batch);

  if (batch.force_serial || batch.feats.size() < kMinParallelFeatures) {
    if (!batch.feats.empty()) {
      warm_prep_fields(batch.feats.front(), &batch.fields);
    }
    for (OGRFeature* feat : batch.feats) {
      draw_unprepared_feature(carto, ctx, options, feat, op, &batch.fields,
                              &env_viewp);
      OGRFeature::DestroyFeature(feat);
    }
    return kErrNone;
  }

  const LpToDp2 xform = make_lp_to_dp(ctx);
  warm_prep_fields(batch.feats.front(), &batch.fields);
  batch.prepared.resize(batch.feats.size());
  {
    BASE_TRACE_EVENT("prep", "gdi.layer");
    run_chunked_prep_pipeline(batch.feats.size(), [&](size_t i) {
      prepare_one_feature(batch.feats[i], env_viewp, xform, ctx.fblc,
                          &batch.fields, &batch.prepared[i]);
    });
  }

  {
    BASE_TRACE_EVENT("draw", "gdi.layer");
    draw_prepared_batch(carto, op, &batch.feats, &batch.prepared, fallback,
                        DrawBatchMode::All);
  }
  return kErrNone;
}

int paint_raster_layer(Rhi2dCartoDraw* carto, const RenderContext& ctx,
                       const datasource::OgrRasterLayer* layer) {
  if (layer == nullptr) {
    return kErrInvalidParam;
  }

  const std::string name = layer->name()
                               ? std::string("gdi.raster.") + layer->name()
                               : std::string("gdi.raster");
  BASE_TRACE_EVENT(name, "gdi.layer");

  Envelope env_layer;
  layer->get_envelope(env_layer);
  Envelope env_viewp;
  if (!map_view_envelope(carto, ctx, &env_viewp) ||
      !env_layer.intersects(env_viewp)) {
    return kErrNone;
  }

  char* raster_buf = nullptr;
  long raster_buf_size = 0;
  long code_type = -1;
  Envelope loc_env;

  GeomTraceScope geom;
  const auto t0 = base::trace::Trace::time_point::clock::now();
  if (datasource::k_raster_ok ==
      layer->get_raster_no_clone(raster_buf, raster_buf_size, loc_env,
                                 code_type)) {
    stretch_layer_image(carto, raster_buf, static_cast<int>(raster_buf_size),
                        loc_env, code_type);
  }
  geom.accum.image +=
      std::chrono::duration_cast<std::chrono::microseconds>(
          base::trace::Trace::time_point::clock::now() - t0)
          .count();
  return kErrNone;
}

int paint_tile_layer(Rhi2dCartoDraw* carto, const RenderContext& ctx,
                     const tile::ProviderTileLayer* layer) {
  if (layer == nullptr) {
    return kErrInvalidParam;
  }

  const std::string name = layer->name()
                               ? std::string("gdi.tile.") + layer->name()
                               : std::string("gdi.tile");
  BASE_TRACE_EVENT(name, "gdi.layer");

  Envelope env_layer;
  layer->get_envelope(env_layer);
  Envelope env_viewp;
  if (!map_view_envelope(carto, ctx, &env_viewp) ||
      !env_layer.intersects(env_viewp)) {
    return kErrNone;
  }

  GeomTraceScope geom;
  const auto t0 = base::trace::Trace::time_point::clock::now();
  for (const tile::TileImage& tile : layer->images()) {
    if (tile.bytes.empty()) {
      continue;
    }
    stretch_layer_image(carto, tile.bytes.data(),
                        static_cast<int>(tile.bytes.size()), tile.world_rect,
                        tile.image_code);
  }
  geom.accum.image +=
      std::chrono::duration_cast<std::chrono::microseconds>(
          base::trace::Trace::time_point::clock::now() - t0)
          .count();
  return kErrNone;
}

}  // namespace detail
}  // namespace scenic
