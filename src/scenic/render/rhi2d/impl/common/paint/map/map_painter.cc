// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/paint/map/map_painter.h"

#include <cstdint>
#include <functional>
#include <vector>

#include "scenic/render/rhi2d/impl/common/cc/scheduler.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/device_geom.h"
#include "scenic/render/rhi2d/impl/common/paint/map/map_feature_draw.h"
#include "scenic/render/rhi2d/impl/common/paint/map/map_layer_draw.h"
#include "scenic/render/rhi2d/impl/common/paint/map/map_submit.h"
#include "scenic/render/rhi2d/impl/common/paint/map/map_view.h"

using namespace gis;

namespace scenic {
namespace detail {

Rhi2dPainter::Rhi2dPainter(Rhi2dCartoDraw* carto_draw,
                           Rhi2dOwnedSurface* back_buf,
                           Rhi2dOwnedSurface* shared_front,
                           base::Viewport* vir_vp1, base::Viewport* vir_vp2,
                           std::mutex* shared_front_mu)
    : carto_draw_(carto_draw),
      back_buf_(back_buf),
      shared_front_(shared_front),
      vir_vp1_(vir_vp1),
      vir_vp2_(vir_vp2),
      shared_front_mu_(shared_front_mu),
      carto2d_(std::make_unique<GdiCartoFrame>()) {
  if (carto_draw_) {
    carto_draw_->set_carto(carto2d_.get());
  }
}

Rhi2dPainter::~Rhi2dPainter() = default;

void Rhi2dPainter::set_scheduler(Rhi2dScheduler* sched) {
  scheduler_ = sched;
}

void Rhi2dPainter::set_layer_tree(Rhi2dLayerTreeImpl* tree) {
  layer_tree_ = tree;
}

void Rhi2dPainter::set_context(RenderContext* rc) {
  rc_ = rc;
  if (carto_draw_ && rc_) {
    carto_draw_->set_context(rc_);
  }
}

void Rhi2dPainter::set_render_options(const RenderOptions2d* options) {
  rd_options_ = options;
  if (carto_draw_) {
    carto_draw_->set_render_options(options);
  }
}

RenderContext& Rhi2dPainter::context() {
  if (rc_) {
    return *rc_;
  }
  return scheduler_->context();
}

const RenderContext& Rhi2dPainter::context() const {
  if (rc_) {
    return *rc_;
  }
  return scheduler_->context();
}

void Rhi2dPainter::sync_carto_draw_links() {
  if (!carto_draw_) {
    return;
  }
  carto_draw_->set_carto(carto2d_.get());
  if (rc_) {
    carto_draw_->set_context(rc_);
  } else if (scheduler_) {
    carto_draw_->set_context(&scheduler_->context());
  }
  if (rd_options_) {
    carto_draw_->set_render_options(rd_options_);
  }
}

int Rhi2dPainter::render_map(const Map* map, int x, int y, int w, int h,
                             int op) {
  if (w == 0 || h == 0) {
    return kErrInvalidParam;
  }
  if (!carto_draw_ || !back_buf_ || !shared_front_) {
    return kErrInvalidParam;
  }

  const uint64_t job_gen =
      scheduler_ ? scheduler_->job_generation() : uint64_t{0};
  const auto frame_aborted = [this, job_gen]() {
    return scheduler_ && scheduler_->should_abort(job_gen);
  };
  if (frame_aborted()) {
    return kErrNone;
  }

  sync_carto_draw_links();

  const Rhi2dParallelMode parallel_mode = rhi2d_parallel_mode();
  MapEncodePass pass(carto_draw_, &back_buf_->surface(),
                     parallel_mode == Rhi2dParallelMode::kLayer);
  pass.begin(kMapOceanClear);
  carto2d_->reset(context().fblc, w, h);

  Envelope env_viewp;
  if (map != nullptr) {
    map_view_envelope(carto_draw_, context(), &env_viewp);
    const FeatureFallbackFn map_fallback =
        [this, &env_viewp](OGRFeature* feat, int draw_op) {
          draw_unprepared_feature(carto_draw_, context(), rd_options_, feat,
                                  draw_op, nullptr, &env_viewp);
        };
    const LpToDp2 xform = make_lp_to_dp(context());

    std::vector<OgrLayerBatch> ogr_batches;
    std::vector<const datasource::OgrRasterLayer*> raster_layers;
    std::vector<const tile::ProviderTileLayer*> tile_layers;
    collect_visible_map_layers(map, env_viewp, frame_aborted, &ogr_batches,
                               &raster_layers, &tile_layers);
    prepare_ogr_batches(&ogr_batches, env_viewp, xform, context().fblc);

    for (OgrLayerBatch& b : ogr_batches) {
      if (frame_aborted() || b.culled || !b.layer) {
        destroy_ogr_feats(&b.feats);
        continue;
      }
      pass.seal_layer();
      encode_ogr_batch(carto_draw_, &b, op, map_fallback);
      pass.mark_cmds();
    }
    for (const datasource::OgrRasterLayer* lyr : raster_layers) {
      if (frame_aborted()) {
        break;
      }
      pass.seal_layer();
      render_layer(lyr, op);
      pass.mark_cmds();
    }
    for (const tile::ProviderTileLayer* lyr : tile_layers) {
      if (frame_aborted()) {
        break;
      }
      pass.seal_layer();
      render_layer(lyr, op);
      pass.mark_cmds();
    }
  }

  pass.end_record();
  MapExecuteArgs exec;
  exec.back = back_buf_;
  exec.tree = layer_tree_;
  exec.mode = parallel_mode;
  exec.w = w;
  exec.h = h;
  exec.job_gen = job_gen;
  exec.have_map = map != nullptr;
  exec.aborted = frame_aborted;
  if (!execute_map_pass(&pass, exec)) {
    return kErrNone;
  }

  MapPublishArgs pub;
  pub.back = back_buf_;
  pub.shared_front = shared_front_;
  pub.shared_front_mu = shared_front_mu_;
  pub.scheduler = scheduler_;
  pub.tree = layer_tree_;
  pub.vir_vp1 = vir_vp1_;
  pub.vir_vp2 = vir_vp2_;
  pub.src_vp = &context().viewport;
  pub.x = x;
  pub.y = y;
  pub.w = w;
  pub.h = h;
  pub.job_gen = job_gen;
  pub.have_map = map != nullptr;
  pub.aborted = frame_aborted;
  publish_map_front(pub);
  return kErrNone;
}

int Rhi2dPainter::render_layer(const MapLayer* layer, int op) {
  if (!layer) {
    return kErrInvalidParam;
  }
  if (layer->ogr()) {
    return render_layer(const_cast<OGRLayer*>(layer->ogr()), op);
  }
  if (layer->raster()) {
    return render_layer(layer->raster(), op);
  }
  if (layer->tile()) {
    return render_layer(layer->tile(), op);
  }
  return kErrFailure;
}

int Rhi2dPainter::render_layer(OGRLayer* layer, int op) {
  return paint_ogr_layer(
      carto_draw_, context(), rd_options_, layer, op,
      [this](OGRFeature* feat, int draw_op) { render_feature(feat, draw_op); });
}

int Rhi2dPainter::render_layer(const datasource::OgrRasterLayer* layer,
                               int op) {
  (void)op;
  return paint_raster_layer(carto_draw_, context(), layer);
}

int Rhi2dPainter::render_layer(const tile::ProviderTileLayer* layer, int op) {
  (void)op;
  return paint_tile_layer(carto_draw_, context(), layer);
}

int Rhi2dPainter::render_feature(OGRFeature* feature, int op) {
  Envelope env_viewp;
  const Envelope* env = nullptr;
  if (map_view_envelope(carto_draw_, context(), &env_viewp)) {
    env = &env_viewp;
  }
  return draw_unprepared_feature(carto_draw_, context(), rd_options_, feature,
                                 op, nullptr, env);
}

int Rhi2dPainter::render_geometry(const OGRGeometry* geom, const Style* style,
                                  int op) {
  Envelope env_viewp;
  const Envelope* env = nullptr;
  if (map_view_envelope(carto_draw_, context(), &env_viewp)) {
    env = &env_viewp;
  }
  return draw_unprepared_geometry(carto_draw_, context(), rd_options_, geom,
                                  style, op, env);
}

}  // namespace detail
}  // namespace scenic
