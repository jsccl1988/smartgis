// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/paint/map/map_submit.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "base/process/switches.h"
#include "base/trace/event/process_trace.h"
#include "scenic/render/rhi2d/impl/common/cc/layer_tree_impl.h"
#include "scenic/render/rhi2d/impl/common/cc/scheduler.h"
#include "scenic/render/rhi2d/impl/common/cc/tile_graph_runner.h"
#include "scenic/render/rhi2d/impl/common/paint/backend/paint_backend.h"
#include "scenic/render/rhi2d/impl/common/paint/map/map_view.h"
#include "scenic/render/rhi2d/impl/common/surface/composer/composer.h"

namespace scenic {
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

bool switch_flag_on(const char* name) {
  const char* v = base::switch_cstr(name);
  return v && v[0] != '\0' && _stricmp(v, "0") != 0 &&
         _stricmp(v, "off") != 0 && _stricmp(v, "false") != 0;
}

// Resident raster pools live in the port DLL. Joining them from DllMain
// (FreeLibrary or process-exit PROCESS_DETACH) AVs after matrix BMP write.
Rhi2dTileGraphRunner g_layer_runner;
Rhi2dTileGraphRunner g_tile_runner;
std::vector<std::unique_ptr<Rhi2dOwnedSurface>> g_tile_surfs;
std::vector<std::unique_ptr<Rhi2dOwnedSurface>> g_layer_surfs;

bool aborted(const MapExecuteArgs& args) {
  return args.aborted && args.aborted();
}

void ensure_surf_pool(std::vector<std::unique_ptr<Rhi2dOwnedSurface>>* pool,
                      size_t n) {
  if (pool->size() < n) {
    pool->resize(n);
  }
}

bool execute_layer_parallel(std::vector<Rhi2dCommandBuffer>& layer_bufs,
                            const MapExecuteArgs& args) {
  if (layer_bufs.empty()) {
    args.back->clear(0, 0, args.w, args.h, kMapOceanClear);
    return true;
  }
  const int workers = rhi2d_parallel_worker_count(layer_bufs.size());
  g_layer_runner.ensure_workers(workers);
  g_layer_runner.clear_cancel();
  ensure_surf_pool(&g_layer_surfs, layer_bufs.size());

  HWND hwnd = args.back->wnd();
  g_layer_runner.run_tiles(layer_bufs.size(), [&](size_t i) {
    if (aborted(args)) {
      g_layer_runner.request_cancel();
      return;
    }
    if (g_layer_runner.is_cancel_requested()) {
      return;
    }
    if (!g_layer_surfs[i]) {
      g_layer_surfs[i] = std::make_unique<Rhi2dOwnedSurface>();
    }
    Rhi2dOwnedSurface& surf = *g_layer_surfs[i];
    surf.set_wnd(hwnd);
    // set_size clears to ocean; TransparentBlt keys on that so later
    // layers do not wipe earlier cartography with opaque ocean pads.
    if (surf.set_size(args.w, args.h) != kErrNone) {
      return;
    }
    if (!execute(layer_bufs[i], surf.surface())) {
      return;
    }
  });

  if (aborted(args)) {
    return false;
  }
  args.back->clear(0, 0, args.w, args.h, kMapOceanClear);
  for (size_t i = 0; i < layer_bufs.size(); ++i) {
    if (!g_layer_surfs[i] || !g_layer_surfs[i]->bitmap()) {
      continue;
    }
    blit_owned_to(*g_layer_surfs[i], *args.back, 0, 0, args.w, args.h, 0, 0,
                  args.w, args.h, Rhi2dBlitMode::kColorKey, SRCCOPY,
                  kMapOceanClear);
  }
  return true;
}

bool execute_tile_or_serial(Rhi2dCommandBuffer recorded,
                            const MapExecuteArgs& args) {
  const bool use_tiles = args.mode == Rhi2dParallelMode::kTile &&
                         args.w >= rhi2d_tile_pixel_size() &&
                         args.h >= rhi2d_tile_pixel_size() && args.have_map;
  if (!use_tiles) {
    execute(recorded, args.back->surface());
    return true;
  }

  BASE_TRACE_EVENT("execute_tiles", "gdi.encode");
  RECT damage{0, 0, args.w, args.h};
  bool full_damage = true;
  if (args.tree) {
    damage = args.tree->resolved_damage(args.w, args.h);
    full_damage = args.tree->full_damage();
  }
  auto tiles =
      enumerate_viewport_tiles(args.w, args.h, damage, full_damage, args.job_gen);
  // Full-frame china: large province polygons hit many tiles and are
  // redrawn per hit — slower than one serial execute. Keep parallel
  // tile grain for partial damage (pan) where AABB cull wins.
  const bool force_tile = switch_flag_on("rhi2d-tile-force");
  const bool tile_parallel_win =
      (!full_damage || force_tile) && !tiles.empty() && tiles.size() > 1;
  if (!tile_parallel_win) {
    execute(recorded, args.back->surface());
    return true;
  }

  const int workers = rhi2d_parallel_worker_count(tiles.size());
  g_tile_runner.ensure_workers(workers);
  g_tile_runner.clear_cancel();
  ensure_surf_pool(&g_tile_surfs, tiles.size());

  HWND hwnd = args.back->wnd();
  g_tile_runner.run_tiles(tiles.size(), [&](size_t i) {
    if (aborted(args)) {
      g_tile_runner.request_cancel();
      return;
    }
    if (g_tile_runner.is_cancel_requested()) {
      return;
    }
    const Rhi2dRasterTile& tile = tiles[i];
    if (!g_tile_surfs[i]) {
      g_tile_surfs[i] = std::make_unique<Rhi2dOwnedSurface>();
    }
    Rhi2dOwnedSurface& surf = *g_tile_surfs[i];
    surf.set_wnd(hwnd);
    const int tw = raster_tile_width(tile);
    const int th = raster_tile_height(tile);
    if (surf.set_size(tw, th) != kErrNone) {
      return;
    }
    (void)execute_tile(recorded, surf.surface(), tile.paint.left,
                       tile.paint.top);
  });

  if (aborted(args)) {
    return false;
  }
  for (size_t i = 0; i < tiles.size(); ++i) {
    if (!g_tile_surfs[i] || !g_tile_surfs[i]->bitmap()) {
      continue;
    }
    const Rhi2dRasterTile& tile = tiles[i];
    const int src_x = tile.center.left - tile.paint.left;
    const int src_y = tile.center.top - tile.paint.top;
    const int cw = tile.center.right - tile.center.left;
    const int ch = tile.center.bottom - tile.center.top;
    blit_owned_to(*g_tile_surfs[i], *args.back, tile.center.left,
                  tile.center.top, cw, ch, src_x, src_y, cw, ch,
                  Rhi2dBlitMode::kOpaque, SRCCOPY);
  }
  return true;
}

}  // namespace

MapEncodePass::MapEncodePass(Rhi2dCartoDraw* carto, Rhi2dSurface* target,
                             bool layer_parallel)
    : carto_(carto), target_(target), layer_parallel_(layer_parallel) {}

void MapEncodePass::begin(COLORREF clear_if_serial) {
  encoder_.begin_pass(target_);
  if (carto_) {
    carto_->set_encoder(&encoder_);
  }
  // Layer mode: ocean clear happens on the back buffer at compose time.
  if (!layer_parallel_) {
    encoder_.clear(clear_if_serial);
  }
}

void MapEncodePass::seal_layer() {
  if (!layer_parallel_ || !has_cmds_) {
    return;
  }
  if (carto_) {
    carto_->flush_style();
  }
  if (encoder_.is_recording()) {
    encoder_.end_pass();
  }
  layer_bufs_.push_back(encoder_.take_buffer());
  encoder_.begin_pass(target_);
  if (carto_) {
    carto_->set_encoder(&encoder_);
  }
  has_cmds_ = false;
}

void MapEncodePass::mark_cmds() {
  has_cmds_ = true;
}

void MapEncodePass::end_record() {
  if (carto_) {
    carto_->flush_style();
    carto_->set_encoder(nullptr);
  }
  if (encoder_.is_recording()) {
    encoder_.end_pass();
  }
  if (layer_parallel_ && has_cmds_) {
    layer_bufs_.push_back(encoder_.take_buffer());
    has_cmds_ = false;
  }
}

void rhi2d_shutdown_static_raster_runners() {
  g_layer_runner.shutdown();
  g_tile_runner.shutdown();
  g_layer_surfs.clear();
  g_tile_surfs.clear();
}

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
  HBITMAP old_bmp = static_cast<HBITMAP>(::SelectObject(hdc, surf.bitmap));
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

bool execute_map_pass(MapEncodePass* pass, const MapExecuteArgs& args) {
  if (!pass || !args.back) {
    return true;
  }
  BASE_TRACE_EVENT("execute", "gdi.encode");
  const auto t0 = std::chrono::steady_clock::now();
  bool ok = true;
  if (pass->layer_parallel()) {
    BASE_TRACE_EVENT("execute_layers", "gdi.encode");
    ok = execute_layer_parallel(pass->layer_bufs(), args);
  } else {
    ok = execute_tile_or_serial(pass->encoder().take_buffer(), args);
  }
  if (switch_flag_on("rhi2d-parallel-log")) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
    std::fprintf(stderr, "rhi2d_parallel=%s execute_ms=%lld\n",
                 parallel_mode_name(args.mode),
                 static_cast<long long>(ms));
  }
  draw_parallel_strategy_label(args.back, args.mode);
  return ok;
}

void publish_map_front(const MapPublishArgs& args) {
  // Never clear the shared map buffer on a null-map wake — that wipes a
  // realtime ZoomToRect paint left by the UI thread. Also skip after stop /
  // cancel / superseded job: device Viewport refs may be mid-teardown, and
  // a stale blit must not overwrite the last good front.
  const bool tree_ok = !args.tree || args.tree->is_current(args.job_gen);
  const bool abort = args.aborted && args.aborted();
  if (!args.have_map || abort || !tree_ok || !args.back || !args.shared_front) {
    return;
  }
  std::unique_lock<std::mutex> front_lock;
  if (args.shared_front_mu) {
    front_lock = std::unique_lock<std::mutex>(*args.shared_front_mu);
  }
  args.shared_front->clear(args.x, args.y, args.w, args.h);
  blit_owned_to(*args.back, *args.shared_front, args.x, args.y, args.w, args.h,
                args.x, args.y, args.w, args.h, Rhi2dBlitMode::kOpaque,
                SRCCOPY);
  if (args.src_vp) {
    if (args.vir_vp1) {
      *args.vir_vp1 = *args.src_vp;
    }
    if (args.vir_vp2) {
      *args.vir_vp2 = *args.src_vp;
    }
  }
  if (args.scheduler) {
    args.scheduler->mark_published(args.job_gen);
  }
  submit_surface(args.shared_front->surface());
}

}  // namespace detail
}  // namespace scenic
