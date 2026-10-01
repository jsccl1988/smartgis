// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/common/paint/map/map_painter.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "base/math/simd.h"
#include "base/trace/event/process_trace.h"
#include "gis/datasource/provider/impl/ogr/codec/ogr_feature_codec.h"
#include "gis/model/envelope.h"
#include "legacy/gis/present/carto/style_api.h"
#include "legacy/render/detail/frame_pipeline.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/frame/carto_frame.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/draw/device_geom.h"
#include "legacy/render/rhi2d/impl/common/paint/map/map_feature_prep.h"
#include "legacy/render/rhi2d/impl/common/paint/map/map_geom_trace.h"
#include "legacy/render/rhi2d/impl/common/paint/map/map_draw_batch.h"
#include "legacy/render/rhi2d/impl/common/paint/map/map_prep_pipeline.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/encode/command_encoder.h"
#include "legacy/render/rhi2d/impl/common/paint/backend/paint_backend.h"
#include "legacy/render/rhi2d/impl/common/surface/composer/composer.h"
#include "legacy/render/rhi2d/impl/common/cc/layer_tree_impl.h"
#include "legacy/render/rhi2d/impl/common/cc/raster_tile.h"
#include "legacy/render/rhi2d/impl/common/cc/scheduler.h"
#include "legacy/render/rhi2d/impl/common/cc/tile_graph_runner.h"
#include "ogrsf_frmts.h"

using namespace gis;
using namespace base;
using namespace geo;

namespace render {
namespace detail {
namespace {

const char* parallel_mode_name(Rhi2dParallelMode mode) {
  switch (mode) {
    case Rhi2dParallelMode::kTile:
      return "tile";
    case Rhi2dParallelMode::kLayer:
      return "layer";
    case Rhi2dParallelMode::kSerial:
    default:
      return "serial";
  }
}

// Burn strategy (+ port) into the composed map buffer so matrix BMPs self-label.
void draw_parallel_strategy_label(Rhi2dOwnedSurface* back,
                                  Rhi2dParallelMode mode) {
  if (!back) {
    return;
  }
  Rhi2dSurface& surf = back->surface();
  if (!surf.bitmap || surf.width < 64 || surf.height < 32) {
    return;
  }
  char label[96] = {};
  std::snprintf(label, sizeof(label), "%s / %s", parallel_mode_name(mode),
                rhi2d_port_name());
  const int n = static_cast<int>(std::strlen(label));

  HDC hdc = ::CreateCompatibleDC(nullptr);
  if (!hdc) {
    return;
  }
  HBITMAP old_bmp =
      static_cast<HBITMAP>(::SelectObject(hdc, surf.bitmap));
  if (!old_bmp) {
    ::DeleteDC(hdc);
    return;
  }

  const int box_w = (std::min)(surf.width - 8, 12 + n * 11);
  const int box_h = 28;
  RECT box{4, 4, 4 + box_w, 4 + box_h};
  HBRUSH brush = ::CreateSolidBrush(RGB(255, 220, 0));
  if (brush) {
    ::FillRect(hdc, &box, brush);
    ::DeleteObject(brush);
  }
  ::SetBkMode(hdc, TRANSPARENT);
  ::SetTextColor(hdc, RGB(20, 20, 20));
  HFONT font = ::CreateFontA(
      18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
      DEFAULT_PITCH | FF_SWISS, "Segoe UI");
  HFONT old_font = nullptr;
  if (font) {
    old_font = static_cast<HFONT>(::SelectObject(hdc, font));
  }
  ::TextOutA(hdc, 10, 8, label, n);
  if (old_font) {
    ::SelectObject(hdc, old_font);
  }
  if (font) {
    ::DeleteObject(font);
  }
  ::SelectObject(hdc, old_bmp);
  ::DeleteDC(hdc);
  surf.mark_dirty(4, 4, box_w, box_h);
  surf.bump_generation();
}

}  // namespace

Rhi2dPainter::Rhi2dPainter(Rhi2dCartoDraw* carto_draw,
                                 Rhi2dOwnedSurface* back_buf,
                                 Rhi2dOwnedSurface* shared_front,
                                 Viewport* vir_vp1, Viewport* vir_vp2,
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

void Rhi2dPainter::set_context(SmtRenderContext* rc) {
  rc_ = rc;
  if (carto_draw_ && rc_) {
    carto_draw_->set_context(rc_);
  }
}

void Rhi2dPainter::set_render_options(const Smt2DRenderOptions* options) {
  rd_options_ = options;
  if (carto_draw_) {
    carto_draw_->set_render_options(options);
  }
}

SmtRenderContext& Rhi2dPainter::context() {
  if (rc_) {
    return *rc_;
  }
  return scheduler_->context();
}

const SmtRenderContext& Rhi2dPainter::context() const {
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

int Rhi2dPainter::render_map(const SmtMap* map, int x, int y, int w, int h,
                                int op) {
  if (w == 0 || h == 0) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (!carto_draw_ || !back_buf_ || !shared_front_) {
    return SMT_ERR_INVALID_PARAM;
  }

  // Capture job gen at paint start. Publish to the shared front only if this
  // generation is still current (cancel / newer resume abandons the frame).
  const uint64_t job_gen =
      scheduler_ ? scheduler_->job_generation() : uint64_t{0};
  const auto frame_aborted = [this, job_gen]() {
    return scheduler_ && scheduler_->should_abort(job_gen);
  };
  if (frame_aborted()) {
    return SMT_ERR_NONE;
  }

  sync_carto_draw_links();

  // Record-only map pass: Draw* → encoder → execute (no mid-pass HDC).
  constexpr COLORREF kOceanClear = RGB(170, 211, 223);
  const Rhi2dParallelMode parallel_mode = rhi2d_parallel_mode();
  const bool layer_parallel = parallel_mode == Rhi2dParallelMode::kLayer;

  Rhi2dCommandEncoder pass_encoder;
  pass_encoder.begin_pass(&back_buf_->surface());
  carto_draw_->set_encoder(&pass_encoder);
  // Layer mode: ocean clear happens on the back buffer at compose time;
  // each GIS layer records into its own command buffer (no per-layer clear).
  if (!layer_parallel) {
    pass_encoder.clear(kOceanClear);
  }
  carto2d_->reset(context().fblc, w, h);

  std::vector<Rhi2dCommandBuffer> layer_bufs;
  bool layer_has_cmds = false;
  const auto seal_layer_buffer = [&]() {
    if (!layer_parallel || !layer_has_cmds) {
      return;
    }
    carto_draw_->flush_style();
    if (pass_encoder.is_recording()) {
      pass_encoder.end_pass();
    }
    layer_bufs.push_back(pass_encoder.take_buffer());
    pass_encoder.begin_pass(&back_buf_->surface());
    carto_draw_->set_encoder(&pass_encoder);
    layer_has_cmds = false;
  };

  if (map != nullptr) {
    Envelope env_viewp;
    {
      lRect l_viewp;
      fRect f_viewp;
      viewport_to_rect(l_viewp, context().viewport);
      carto_draw_->drect_to_lrect(l_viewp, f_viewp);
      rect_to_envelope(env_viewp, f_viewp);
    }
    const LpToDp2 xform = make_lp_to_dp(context());
    const float fblc = context().fblc;

    std::vector<OgrLayerBatch> ogr_batches;
    std::vector<const SmtLayer*> leftover_layers;
    ogr_batches.reserve(static_cast<size_t>(map->GetLayerCount()));

    for (int i = 0; i < map->GetLayerCount(); ++i) {
      if (frame_aborted()) {
        break;
      }
      if (!map->IsLayerVisible(i)) {
        continue;
      }
      if (OGRLayer* ogr = const_cast<OGRLayer*>(map->GetOgrLayer(i))) {
        OgrLayerBatch batch;
        batch.layer = ogr;
        Envelope env_layer;
        OGREnvelope ogr_env;
        if (ogr->GetExtent(&ogr_env, TRUE) == OGRERR_NONE) {
          env_layer.MinX = ogr_env.MinX;
          env_layer.MinY = ogr_env.MinY;
          env_layer.MaxX = ogr_env.MaxX;
          env_layer.MaxY = ogr_env.MaxY;
          batch.culled = !env_layer.intersects(env_viewp);
        }
        if (!batch.culled) {
          ogr->ResetReading();
          while (OGRFeature* feat = ogr->GetNextFeature()) {
            batch.feats.push_back(feat);
          }
          batch.force_serial = force_serial_ogr_layer(ogr, batch.feats);
        }
        ogr_batches.push_back(std::move(batch));
      } else if (const SmtLayer* lyr = map->GetLeftoverLayer(i)) {
        leftover_layers.push_back(lyr);
      }
    }

    // Cross-layer prep wave: area+line prepare in one parallel_for so wall time
    // is ~max(layer prep) instead of sum (HDC play stays ordered + serial).
    struct PrepJob {
      size_t batch = 0;
      size_t feat = 0;
    };
    std::vector<PrepJob> prep_jobs;
    bool any_parallel_layer = false;
    for (size_t bi = 0; bi < ogr_batches.size(); ++bi) {
      OgrLayerBatch& b = ogr_batches[bi];
      if (b.culled || b.force_serial) {
        continue;
      }
      if (b.feats.size() >= kMinParallelFeatures) {
        any_parallel_layer = true;
      }
    }
    if (any_parallel_layer) {
      for (size_t bi = 0; bi < ogr_batches.size(); ++bi) {
        OgrLayerBatch& b = ogr_batches[bi];
        if (b.culled || b.force_serial || b.feats.empty()) {
          continue;
        }
        warm_prep_fields(b.feats.front(), &b.fields);
        b.prepared.resize(b.feats.size());
        for (size_t fi = 0; fi < b.feats.size(); ++fi) {
          prep_jobs.push_back(PrepJob{bi, fi});
        }
      }
    }

    if (!prep_jobs.empty()) {
      BASE_TRACE_EVENT("prep", "gdi.map");
      log_legacy_flow("gdi.prep Pipeline run");
      // Debug Edit/china: parallel prep has intermittently AV'd after
      // "Pipeline done" (legacy.browse.2d / SmartGis heap+AV). Keep the
      // Pipeline shape in Release; Debug runs the same body serially.
#if defined(_DEBUG)
      for (size_t ji = 0; ji < prep_jobs.size(); ++ji) {
        const PrepJob& job = prep_jobs[ji];
        OgrLayerBatch& b = ogr_batches[job.batch];
        prepare_one_feature(b.feats[job.feat], env_viewp, xform, fblc,
                            &b.fields, &b.prepared[job.feat]);
      }
#else
      run_chunked_prep_pipeline(prep_jobs.size(), [&](size_t ji) {
        const PrepJob& job = prep_jobs[ji];
        OgrLayerBatch& b = ogr_batches[job.batch];
        prepare_one_feature(b.feats[job.feat], env_viewp, xform, fblc,
                            &b.fields, &b.prepared[job.feat]);
      });
#endif
      log_legacy_flow("gdi.prep Pipeline done");
    }

    const FeatureFallbackFn fallback = [this](OGRFeature* feat, int draw_op) {
      render_feature(feat, draw_op);
    };

    // Serial ordered encode (HDC play stays in IR). Prep above is Pipeline
    // produce→map→sink. Layer-parallel mode seals one command buffer per GIS
    // layer for parallel execute + ocean color-key compose (keeps cartography
    // across overlapping layer DIBs).
    for (size_t bi = 0; bi < ogr_batches.size(); ++bi) {
      OgrLayerBatch& b = ogr_batches[bi];
      if (frame_aborted()) {
        destroy_ogr_feats(&b.feats);
        continue;
      }
      if (b.culled || !b.layer) {
        destroy_ogr_feats(&b.feats);
        continue;
      }
      seal_layer_buffer();
      const std::string name = trace_name(b.layer);
      BASE_TRACE_EVENT(name, "gdi.layer");
      GeomUsAccum geom{};
      g_active_geom = &geom;

      if (!b.prepared.empty()) {
        BASE_TRACE_EVENT("draw", "gdi.layer");
        draw_prepared_batch(carto_draw_, op, &b.feats, &b.prepared, fallback,
                            DrawBatchMode::All);
      } else {
        for (OGRFeature* feat : b.feats) {
          render_feature(feat, op);
          OGRFeature::DestroyFeature(feat);
        }
        b.feats.clear();
      }
      layer_has_cmds = true;

      g_active_geom = nullptr;
      flush_geom_us(geom);
    }

    for (const SmtLayer* lyr : leftover_layers) {
      if (frame_aborted()) {
        break;
      }
      seal_layer_buffer();
      render_layer(lyr, op);
      layer_has_cmds = true;
    }
    // Leftover tessellate is not run on the GDI worker paint path (white
    // canvas + heap overflow on large OGR packs). See gdi_renderdevice.cpp.
  }

  carto_draw_->flush_style();
  carto_draw_->set_encoder(nullptr);
  if (pass_encoder.is_recording()) {
    pass_encoder.end_pass();
  }
  if (layer_parallel && layer_has_cmds) {
    layer_bufs.push_back(pass_encoder.take_buffer());
    layer_has_cmds = false;
  }
  {
    BASE_TRACE_EVENT("execute", "gdi.encode");
    const auto t0 = std::chrono::steady_clock::now();
    if (layer_parallel) {
      BASE_TRACE_EVENT("execute_layers", "gdi.encode");
      if (layer_bufs.empty()) {
        back_buf_->clear(0, 0, w, h, kOceanClear);
      } else {
        static Rhi2dTileGraphRunner s_layer_runner;
        const int workers = rhi2d_parallel_worker_count(layer_bufs.size());
        s_layer_runner.ensure_workers(workers);
        s_layer_runner.clear_cancel();

        std::vector<std::unique_ptr<Rhi2dOwnedSurface>> layer_surfs(
            layer_bufs.size());
        HWND hwnd = back_buf_->wnd();
        s_layer_runner.run_tiles(layer_bufs.size(), [&](size_t i) {
          if (frame_aborted()) {
            s_layer_runner.request_cancel();
            return;
          }
          if (s_layer_runner.is_cancel_requested()) {
            return;
          }
          auto surf = std::make_unique<Rhi2dOwnedSurface>();
          surf->set_wnd(hwnd);
          // set_size clears to ocean; TransparentBlt keys on that so later
          // layers do not wipe earlier cartography with opaque ocean pads.
          if (surf->set_size(w, h) != SMT_ERR_NONE) {
            return;
          }
          if (!execute(layer_bufs[i], surf->surface())) {
            return;
          }
          layer_surfs[i] = std::move(surf);
        });

        if (frame_aborted()) {
          return SMT_ERR_NONE;
        }
        back_buf_->clear(0, 0, w, h, kOceanClear);
        for (size_t i = 0; i < layer_surfs.size(); ++i) {
          if (!layer_surfs[i]) {
            continue;
          }
          blit_owned_to(*layer_surfs[i], *back_buf_, 0, 0, w, h, 0, 0, w, h,
                        Rhi2dBlitMode::kColorKey, SRCCOPY, kOceanClear);
        }
      }
    } else {
      Rhi2dCommandBuffer recorded = pass_encoder.take_buffer();
      const bool use_tiles =
          parallel_mode == Rhi2dParallelMode::kTile &&
          w >= rhi2d_tile_pixel_size() && h >= rhi2d_tile_pixel_size() &&
          map != nullptr;
      if (use_tiles) {
        BASE_TRACE_EVENT("execute_tiles", "gdi.encode");
        RECT damage{0, 0, w, h};
        bool full_damage = true;
        if (layer_tree_) {
          damage = layer_tree_->resolved_damage(w, h);
          full_damage = layer_tree_->full_damage();
        }
        auto tiles =
            enumerate_viewport_tiles(w, h, damage, full_damage, job_gen);
        // Full-frame china: large province polygons hit many tiles and are
        // redrawn per hit — slower than one serial execute. Keep parallel
        // tile grain for partial damage (pan) where AABB cull wins.
        // SMT_RHI2D_TILE_FORCE=1 forces parallel even on full damage (A/B).
        const bool force_tile = []() {
          const char* v = std::getenv("SMT_RHI2D_TILE_FORCE");
          return v && v[0] != '\0' && _stricmp(v, "0") != 0 &&
                 _stricmp(v, "off") != 0 && _stricmp(v, "false") != 0;
        }();
        const bool tile_parallel_win =
            (!full_damage || force_tile) && !tiles.empty() && tiles.size() > 1;
        if (!tile_parallel_win) {
          execute(recorded, back_buf_->surface());
        } else {
          static Rhi2dTileGraphRunner s_tile_runner;
          // Reuse tile DIBs across frames (pool-backed set_size).
          static std::vector<std::unique_ptr<Rhi2dOwnedSurface>> s_tile_surfs;
          const int workers = rhi2d_parallel_worker_count(tiles.size());
          s_tile_runner.ensure_workers(workers);
          s_tile_runner.clear_cancel();
          if (s_tile_surfs.size() < tiles.size()) {
            s_tile_surfs.resize(tiles.size());
          }

          HWND hwnd = back_buf_->wnd();
          s_tile_runner.run_tiles(tiles.size(), [&](size_t i) {
            if (frame_aborted()) {
              s_tile_runner.request_cancel();
              return;
            }
            if (s_tile_runner.is_cancel_requested()) {
              return;
            }
            const Rhi2dRasterTile& tile = tiles[i];
            if (!s_tile_surfs[i]) {
              s_tile_surfs[i] = std::make_unique<Rhi2dOwnedSurface>();
            }
            Rhi2dOwnedSurface& surf = *s_tile_surfs[i];
            surf.set_wnd(hwnd);
            const int tw = raster_tile_width(tile);
            const int th = raster_tile_height(tile);
            if (surf.set_size(tw, th) != SMT_ERR_NONE) {
              return;
            }
            if (!execute_tile(recorded, surf.surface(), tile.paint.left,
                              tile.paint.top)) {
              return;
            }
          });

          if (frame_aborted()) {
            return SMT_ERR_NONE;
          }
          for (size_t i = 0; i < tiles.size(); ++i) {
            if (!s_tile_surfs[i] || !s_tile_surfs[i]->bitmap()) {
              continue;
            }
            const Rhi2dRasterTile& tile = tiles[i];
            const int src_x = tile.center.left - tile.paint.left;
            const int src_y = tile.center.top - tile.paint.top;
            const int cw = tile.center.right - tile.center.left;
            const int ch = tile.center.bottom - tile.center.top;
            blit_owned_to(*s_tile_surfs[i], *back_buf_, tile.center.left,
                          tile.center.top, cw, ch, src_x, src_y, cw, ch,
                          Rhi2dBlitMode::kOpaque, SRCCOPY);
          }
        }
      } else {
        execute(recorded, back_buf_->surface());
      }
    }
    if (const char* log = std::getenv("SMT_RHI2D_PARALLEL_LOG");
        log != nullptr && log[0] != '\0' && _stricmp(log, "0") != 0 &&
        _stricmp(log, "off") != 0) {
      const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - t0)
                          .count();
      std::fprintf(stderr, "SMT_RHI2D_PARALLEL=%s execute_ms=%lld\n",
                   parallel_mode_name(parallel_mode),
                   static_cast<long long>(ms));
    }
    draw_parallel_strategy_label(back_buf_, parallel_mode);
  }

  // Never clear the shared map buffer on a null-map wake — that wipes a
  // realtime ZoomToRect paint left by the UI thread. Also skip after stop /
  // cancel / superseded job: device Viewport refs may be mid-teardown, and
  // a stale blit must not overwrite the last good front.
  const bool tree_ok =
      !layer_tree_ || layer_tree_->is_current(job_gen);
  if (map != nullptr && !frame_aborted() && tree_ok) {
    std::unique_lock<std::mutex> front_lock;
    if (shared_front_mu_) {
      front_lock = std::unique_lock<std::mutex>(*shared_front_mu_);
    }
    shared_front_->clear(x, y, w, h);
    // Stretch the full map buffer — TransparentBlt keyed on white can drop
    // near-white carto fills and leave the shared buffer empty.
    blit_owned_to(*back_buf_, *shared_front_, x, y, w, h, x, y, w, h,
                  Rhi2dBlitMode::kOpaque, SRCCOPY);

    if (vir_vp1_) {
      *vir_vp1_ = context().viewport;
    }
    if (vir_vp2_) {
      *vir_vp2_ = context().viewport;
    }
    if (scheduler_) {
      scheduler_->mark_published(job_gen);
    }

    // IR hand-off when a GPU/process sink is registered; no-op otherwise
    // (HWND BitBlt remains the leftover present path).
    submit_surface(shared_front_->surface());
  }

  return SMT_ERR_NONE;
}

int Rhi2dPainter::render_layer(const SmtLayer* layer, int op) {
  if (!layer) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (layer->GetLayerType() == LYR_RASTER) {
    return render_layer(static_cast<const SmtRasterLayer*>(layer), op);
  }
  if (layer->GetLayerType() == LYR_TITLE) {
    return render_layer(static_cast<const SmtTileLayer*>(layer), op);
  }

  return SMT_ERR_FAILURE;
}

int Rhi2dPainter::render_layer(OGRLayer* layer, int op) {
  if (layer == nullptr) {
    return SMT_ERR_INVALID_PARAM;
  }
  const std::string name = trace_name(layer);
  BASE_TRACE_EVENT(name, "gdi.layer");

  Envelope env_layer;
  OGREnvelope ogr_env;
  if (layer->GetExtent(&ogr_env, TRUE) == OGRERR_NONE) {
    env_layer.MinX = ogr_env.MinX;
    env_layer.MinY = ogr_env.MinY;
    env_layer.MaxX = ogr_env.MaxX;
    env_layer.MaxY = ogr_env.MaxY;
  }
  Envelope env_viewp;

  lRect l_viewp;
  fRect f_viewp;

  viewport_to_rect(l_viewp, context().viewport);
  carto_draw_->drect_to_lrect(l_viewp, f_viewp);
  rect_to_envelope(env_viewp, f_viewp);

  if (!env_layer.intersects(env_viewp)) {
    return SMT_ERR_NONE;
  }

  GeomUsAccum geom{};
  g_active_geom = &geom;

  std::vector<OGRFeature*> feats;
  layer->ResetReading();
  while (OGRFeature* feat = layer->GetNextFeature()) {
    feats.push_back(feat);
  }
  const bool force_serial = force_serial_ogr_layer(layer, feats);

  if (force_serial || feats.size() < kMinParallelFeatures) {
    for (OGRFeature* feat : feats) {
      render_feature(feat, op);
      OGRFeature::DestroyFeature(feat);
    }
    g_active_geom = nullptr;
    flush_geom_us(geom);
    return SMT_ERR_NONE;
  }

  const LpToDp2 xform = make_lp_to_dp(context());
  const float fblc = context().fblc;
  PrepFieldCache fields;
  warm_prep_fields(feats.front(), &fields);
  std::vector<PreparedFeature> prepared(feats.size());
  {
    BASE_TRACE_EVENT("prep", "gdi.layer");
    run_chunked_prep_pipeline(feats.size(), [&](size_t i) {
      prepare_one_feature(feats[i], env_viewp, xform, fblc, &fields,
                          &prepared[i]);
    });
  }

  {
    BASE_TRACE_EVENT("draw", "gdi.layer");
    draw_prepared_batch(
        carto_draw_, op, &feats, &prepared,
        [this](OGRFeature* feat, int draw_op) {
          render_feature(feat, draw_op);
        },
        DrawBatchMode::All);
  }

  g_active_geom = nullptr;
  flush_geom_us(geom);

  return SMT_ERR_NONE;
}

int Rhi2dPainter::render_layer(const SmtRasterLayer* layer, int op) {
  (void)op;
  if (layer == nullptr) {
    return SMT_ERR_INVALID_PARAM;
  }

  if (!layer->IsVisible()) {
    return SMT_ERR_NONE;
  }

  const std::string name = trace_name(layer);
  BASE_TRACE_EVENT(name, "gdi.layer");

  Envelope env_layer;
  layer->get_envelope(env_layer);
  Envelope env_viewp;

  lRect l_viewp;
  fRect f_viewp;

  viewport_to_rect(l_viewp, context().viewport);
  carto_draw_->drect_to_lrect(l_viewp, f_viewp);
  rect_to_envelope(env_viewp, f_viewp);

  if (!env_layer.intersects(env_viewp)) {
    return SMT_ERR_NONE;
  }

  char* raster_buf = nullptr;
  long raster_buf_size = 0;
  long code_type = -1;
  fRect loc_rect;

  GeomUsAccum geom{};
  g_active_geom = &geom;
  const auto t0 = base::trace::Trace::time_point::clock::now();
  if (SMT_ERR_NONE == layer->GetRasterNoClone(raster_buf, raster_buf_size,
                                              loc_rect, code_type)) {
    carto_draw_->stretch_image(raster_buf, raster_buf_size, loc_rect, code_type);
  }
  if (g_active_geom) {
    g_active_geom->image +=
        std::chrono::duration_cast<std::chrono::microseconds>(
            base::trace::Trace::time_point::clock::now() - t0)
            .count();
  }
  g_active_geom = nullptr;
  flush_geom_us(geom);

  return SMT_ERR_NONE;
}

int Rhi2dPainter::render_layer(const SmtTileLayer* layer, int op) {
  (void)op;
  if (layer == nullptr) {
    return SMT_ERR_INVALID_PARAM;
  }

  if (!layer->IsVisible()) {
    return SMT_ERR_NONE;
  }

  const std::string name = trace_name(layer);
  BASE_TRACE_EVENT(name, "gdi.layer");

  Envelope env_layer;
  layer->get_envelope(env_layer);
  Envelope env_viewp;

  lRect l_viewp;
  fRect f_viewp;

  viewport_to_rect(l_viewp, context().viewport);
  carto_draw_->drect_to_lrect(l_viewp, f_viewp);
  rect_to_envelope(env_viewp, f_viewp);

  if (!env_layer.intersects(env_viewp)) {
    return SMT_ERR_NONE;
  }

  GeomUsAccum geom{};
  g_active_geom = &geom;
  const auto t0 = base::trace::Trace::time_point::clock::now();
  layer->MoveFirst();
  while (!layer->IsEnd()) {
    SmtTile* tile = layer->GetTile();
    if (tile != nullptr && tile->bVisible) {
      carto_draw_->stretch_image(tile->pTileBuf, tile->lTileBufSize,
                             tile->rtTileRect, tile->lImageCode);
    }

    layer->MoveNext();
  }
  if (g_active_geom) {
    g_active_geom->image +=
        std::chrono::duration_cast<std::chrono::microseconds>(
            base::trace::Trace::time_point::clock::now() - t0)
            .count();
  }
  g_active_geom = nullptr;
  flush_geom_us(geom);

  return SMT_ERR_NONE;
}

int Rhi2dPainter::render_feature(OGRFeature* feature, int op) {
  if (feature == nullptr || !carto_draw_) {
    return SMT_ERR_INVALID_PARAM;
  }

  carto_draw_->feature_type() = gis::datasource::infer_feature_type(
      feature, SmtFeatureType::SmtFtUnknown);
  SmtStyle* style = gis::datasource::copy_ogr_style_from_ogr(feature);
  const bool owned_style = style != nullptr;
  SmtStyle fallback;
  if (!style) {
    gis::datasource::fill_default_draw_style(feature, &fallback,
                                             context().fblc);
    style = &fallback;
  }
  OGRGeometry* geom = gis::datasource::decode_ogr_geometry(
      feature, static_cast<SmtFeatureType>(carto_draw_->feature_type()));
  char* anno = carto_draw_->anno_buf();
  if (carto_draw_->feature_type() == SmtFeatureType::SmtFtAnno) {
    const int ai = feature->GetFieldIndex("anno");
    const int gi = feature->GetFieldIndex("angle");
    if (ai >= 0) {
      const char* text = feature->GetFieldAsString(ai);
      if (text) {
        strncpy_s(anno, 2000, text, _TRUNCATE);
      }
    }
    if (gi >= 0) {
      carto_draw_->anno_angle() = static_cast<float>(feature->GetFieldAsDouble(gi));
    }
  }
  auto field = [feature](const char* key) -> const char* {
    if (!feature || !key) {
      return "";
    }
    const int i = feature->GetFieldIndex(key);
    if (i < 0) {
      return "";
    }
    const char* v = feature->GetFieldAsString(i);
    return v ? v : "";
  };
  const char* label =
      (carto_draw_->feature_type() == SmtFeatureType::SmtFtAnno && anno[0])
          ? anno
          : field("name");
  carto_draw_->label_priority() = carto2d_label_priority(
      label, field("kind"), field("class"), field("adcode"));
  carto_draw_->is_river() = carto2d_is_river_kind(field("kind"));
  carto_draw_->road_class() = carto2d_road_class(field("kind"), field("class"));
  if (!(carto_draw_->feature_type() == SmtFeatureType::SmtFtAnno && anno[0])) {
    if (const char* picked =
            carto2d_label_text(field("anno"), field("name"), field("text"))) {
      strncpy(anno, picked, 1999);
      anno[1999] = '\0';
    }
  }
  const int rc = render_geometry(geom, style, op);
  delete geom;
  if (owned_style) {
    delete style;
  }
  return rc;
}

int Rhi2dPainter::render_geometry(const OGRGeometry* geom,
                                     const SmtStyle* style, int op) {
  if (!geom || !carto_draw_) {
    return SMT_ERR_INVALID_PARAM;
  }

  const OGRwkbGeometryType type = wkbFlatten(geom->getGeometryType());

  Envelope env_feature, env_viewp;
  geo::copy_envelope(*geom, &env_feature);

  lRect l_viewp;
  fRect f_viewp;
  fRect fenv;
  lRect lenv;

  viewport_to_rect(l_viewp, context().viewport);
  carto_draw_->drect_to_lrect(l_viewp, f_viewp);
  rect_to_envelope(env_viewp, f_viewp);

  envelope_to_rect(fenv, env_feature);
  carto_draw_->lrect_to_drect(fenv, lenv);

  if (!env_feature.intersects(env_viewp) ||
      (type != wkbPoint && lenv.height() < 2 && lenv.width() < 2)) {
    return SMT_ERR_NONE;
  }

  HDC dc = carto_draw_->dc();
  const bool need_save_dc =
      rd_options_ && rd_options_->bShowMBR;  // MBR stroking mutates DC pen state
  if (need_save_dc) {
    ::SaveDC(dc);
  }

  if (!carto_draw_->lock_style()) {
    carto_draw_->prepare_for_drawing(style, op);
  }

  if (rd_options_ && rd_options_->bShowMBR) {
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
    transform_xy_batch(make_lp_to_dp(context()), xy, out);
    MoveToEx(dc, out[0], out[1], nullptr);
    LineTo(dc, out[2], out[3]);
    LineTo(dc, out[4], out[5]);
    LineTo(dc, out[6], out[7]);
    LineTo(dc, out[8], out[9]);
  }

  const auto draw_begin = base::trace::Trace::time_point::clock::now();
  switch (type) {
    case wkbPoint:
      carto_draw_->draw_point(style, static_cast<const OGRPoint*>(geom));
      break;
    case wkbLineString:
      carto_draw_->draw_line_string(static_cast<const OGRLineString*>(geom));
      break;
    case wkbPolygon:
    case wkbTriangle:
      carto_draw_->draw_polygon(static_cast<const OGRPolygon*>(geom));
      break;
    case wkbMultiPoint:
      carto_draw_->draw_multi_point(style, static_cast<const OGRMultiPoint*>(geom));
      break;
    case wkbMultiLineString:
      carto_draw_->draw_multi_line_string(
          static_cast<const OGRMultiLineString*>(geom));
      break;
    case wkbMultiPolygon:
    case wkbTIN:
      carto_draw_->draw_multi_polygon(static_cast<const OGRMultiPolygon*>(geom));
      break;
    case wkbLinearRing:
      carto_draw_->draw_linear_ring(static_cast<const OGRLinearRing*>(geom));
      break;
    default:
      break;
  }
  if (g_active_geom) {
    const int64_t us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            base::trace::Trace::time_point::clock::now() - draw_begin)
            .count();
    if (carto_draw_->feature_type() == SmtFeatureType::SmtFtAnno) {
      g_active_geom->anno += us;
    } else {
      switch (type) {
        case wkbPoint:
          g_active_geom->point += us;
          break;
        case wkbLineString:
          g_active_geom->line += us;
          break;
        case wkbPolygon:
        case wkbTriangle:
          g_active_geom->polygon += us;
          break;
        case wkbMultiPoint:
          g_active_geom->multipoint += us;
          break;
        case wkbMultiLineString:
          g_active_geom->multiline += us;
          break;
        case wkbMultiPolygon:
        case wkbTIN:
          g_active_geom->multipolygon += us;
          break;
        case wkbLinearRing:
          g_active_geom->ring += us;
          break;
        default:
          break;
      }
    }
  }
  if (!carto_draw_->lock_style()) {
    carto_draw_->end_drawing();
  }

  if (need_save_dc) {
    ::RestoreDC(dc, -1);
  }

  return SMT_ERR_NONE;
}

}  // namespace detail
}  // namespace render
