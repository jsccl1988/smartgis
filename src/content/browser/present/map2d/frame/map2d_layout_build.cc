// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/frame/map2d_layout_build.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/frame/map2d_batches.h"
#include "content/browser/present/map2d/frame/map2d_carto.h"
#include "content/browser/present/map2d/frame/map2d_tile_math.h"
#include "content/browser/present/map2d/map2d_phase_profile.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/futures/combinators/async.h"
#include "base/memory/arena.h"
#include "base/trace/event/process_trace.h"
#include "base/process/switches.h"
#include "gis/style/document/style_document.h"
#include "vista/map/ir.h"
#include "vista/map_gpu/pass.h"
#include "vista/map/hillshade_bake.h"
#include "vista/terrain/dem/dem_raster.h"

namespace content {
namespace detail {

bool build_map2d_layout(const Map2dLayoutParams& in, Map2dLayoutOutput* out) {
  if (!out || !in.scene || !in.frame || in.cam.width_px == 0 ||
      in.cam.height_px == 0) {
    return false;
  }
  *out = Map2dLayoutOutput{};
  BASE_TRACE_EVENT("layout", "map2d.layout");

  if (in.scratch) {
    if (base::MemoryResource* scratch = in.scratch->memory_resource.get()) {
      scratch->clear(1 << 20);
    }
  }
  if (base::MemoryResource* tls = base::tls_memory_resource()) {
    tls->clear(base::Arena::kInitialSize);
  }

  bool use_carto = true;
  const gis::style::StyleDocument* style =
      resolve_present_style(in.scene->style_document(), &use_carto);

  vista::Layout layout;
  vista::LayoutInput layout_in;
  layout_in.view = {in.cam.width_px, in.cam.height_px, in.cam.min_x,
                    in.cam.min_y, in.cam.max_x, in.cam.max_y};
  layout_in.style = style;
  layout_in.zoom = zoom_from_scale(in.frame->scale());
  layout_in.metrics = nullptr;
  layout_in.tiles = {};
  layout_in.layout_gen = in.layout_gen;
  layout_in.live_layout_gen = in.live_layout_gen;

  const bool reuse_world = in.action == Map2dFrameCache::PresentAction::kSettleRebuild &&
                           in.prev_published != nullptr;
  layout_in.reuse_world_items = reuse_world;

  const char* no_hs = base::switch_cstr("map2d-no-hillshade");
  const char* force_hs = base::switch_cstr("map2d-force-hillshade");
  const bool force_hillshade =
      force_hs && force_hs[0] == '1' && force_hs[1] == '\0';
  const bool skip_hillshade =
      !force_hillshade &&
      ((reuse_world && in.hillshade_ready) ||
       (no_hs && no_hs[0] == '1' && no_hs[1] == '\0') ||
       (in.scene && !in.scene->has_china_extent()));

  if (!skip_hillshade && in.hillshade_ready &&
      in.hillshade_slot.texture_key != 0 && layout_in.hillshade_tiles.empty()) {
    layout_in.hillshade_tiles.push_back(in.hillshade_slot);
  }
  if (!skip_hillshade && layout_in.hillshade_tiles.empty()) {
    if (const gis::style::StyleLayer* hs =
            find_hillshade_layer(style, layout_in.zoom)) {
      const std::string dem_path = vista::find_sample_dem_path();
      if (dem_path.empty()) {
        std::fprintf(stderr, "map2d: hillshade skip - china_dem not found\n");
      } else {
        const auto hs_t0 = std::chrono::steady_clock::now();
        vista::HillshadeBake baked = vista::bake_hillshade_slot(
            dem_path, layout_in.zoom, *hs, kMap2dHillshadeTextureKey);
        if (baked.ok && baked.width > 0 && baked.height > 0 &&
            !baked.rgba.empty()) {
          layout_in.hillshade_tiles.push_back(baked.slot);
          out->baked_w = baked.width;
          out->baked_h = baked.height;
          out->hillshade_slot = baked.slot;
          out->baked_rgba = std::make_shared<const std::vector<uint8_t>>(
              std::move(baked.rgba));
          std::fprintf(stderr,
                       "map2d: hillshade baked %dx%d from %s tiles=%zu "
                       "opacity=%.2f key=0x%08x\n",
                       out->baked_w, out->baked_h, dem_path.c_str(),
                       layout_in.hillshade_tiles.size(),
                       layout_in.hillshade_tiles.back().opacity,
                       layout_in.hillshade_tiles.back().texture_key);
        }
        out->hillshade_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - hs_t0)
                .count();
      }
    } else {
      std::fprintf(stderr,
                   "map2d: hillshade skip - no style layer @ zoom=%.2f\n",
                   layout_in.zoom);
    }
  }

  const double map_scale = in.frame->scale();
  vista::LayerBatchSet batches;
  {
    BASE_TRACE_EVENT("batches", "map2d.layout");
    batches = visible_layer_batches(in.scene->layers(), use_carto, map_scale);
  }
  vista::MapIR built;
  if (reuse_world) {
    built.background_rgba = in.prev_published->background_rgba;
    built.background_opacity = in.prev_published->background_opacity;
    built.items.reserve(in.prev_published->items.size() + 16);
    for (const vista::DrawItem& item : in.prev_published->items) {
      if (item.kind != vista::DrawKind::kIcon &&
          item.kind != vista::DrawKind::kText) {
        built.items.push_back(item);
      }
    }
  }
  {
    BASE_TRACE_EVENT("build", "map2d.layout");
    if (base::MemoryResource* tls = base::tls_memory_resource()) {
      tls->clear(base::Arena::kInitialSize);
    }
    vista::MapIR overlays = layout.build(layout_in, batches.batches);
    if (in.live_layout_gen &&
        in.live_layout_gen->load(std::memory_order_acquire) != in.layout_gen) {
      return true;
    }
    if (reuse_world) {
      built.items.insert(built.items.end(),
                         std::make_move_iterator(overlays.items.begin()),
                         std::make_move_iterator(overlays.items.end()));
    } else {
      built = std::move(overlays);
    }
  }
  out->frame = std::move(built);
  out->ok = true;
  return true;
}

}  // namespace detail

bool Map2dFrameCache::rebuild_layout(const CameraKey& cam,
                                     const ContentFingerprint& fp,
                                     PresentAction action,
                                     uint64_t expected_gen) {
  if (!scene_ || !frame_ || cam.width_px == 0 || cam.height_px == 0) {
    return false;
  }

  const auto layout_wall_t0 = std::chrono::steady_clock::now();
  detail::Map2dLayoutParams params;
  {
    std::lock_guard<std::mutex> lock(mu_);
    params.prev_published = published_;
    params.hillshade_ready = hillshade_ready_.load(std::memory_order_relaxed);
    params.hillshade_slot = hillshade_slot_;
  }
  params.scene = scene_;
  params.frame = frame_;
  params.cam = cam;
  params.action = action;
  params.scratch = &layout_scratch_;
  params.layout_gen = expected_gen;
  params.live_layout_gen = mailbox_.live_layout_gen();

  detail::Map2dLayoutOutput built;
  if (!detail::build_map2d_layout(params, &built) || !built.ok) {
    std::lock_guard<std::mutex> lock(mu_);
    if (expected_gen != mailbox_.request_gen()) {
      return true;
    }
    return false;
  }

  std::lock_guard<std::mutex> lock(mu_);
  if (expected_gen != mailbox_.request_gen()) {
    return true;
  }
  if (built.baked_rgba && built.baked_w > 0 && built.baked_h > 0) {
    hillshade_rgba_ = built.baked_rgba;
    hillshade_w_ = built.baked_w;
    hillshade_h_ = built.baked_h;
    hillshade_slot_ = built.hillshade_slot;
    hillshade_ready_.store(true, std::memory_order_relaxed);
  }
  publish_unlocked(std::move(built.frame), cam, fp);
  has_pending_gpu_action_ = true;
  pending_gpu_action_ = action;
  if (action != PresentAction::kInteractiveReuse) {
    last_present_was_interactive_ = false;
  }
  mailbox_.cv_.notify_all();
  const int64_t wall_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - layout_wall_t0)
                              .count();
  const int64_t layout_ms = wall_ms > built.hillshade_ms
                                ? (wall_ms - built.hillshade_ms)
                                : wall_ms;
  note_map2d_phase_layout(layout_ms, built.hillshade_ms);
  return true;
}

void Map2dFrameCache::prefetch_hillshade() {
  double zoom = 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!scene_ || !frame_ || !scene_->has_china_extent()) {
      return;
    }
    zoom = detail::zoom_from_scale(frame_->scale());
  }
  const char* no_hs = base::switch_cstr("map2d-no-hillshade");
  const char* force_hs = base::switch_cstr("map2d-force-hillshade");
  const bool force_hillshade =
      force_hs && force_hs[0] == '1' && force_hs[1] == '\0';
  if (!force_hillshade && no_hs && no_hs[0] == '1' && no_hs[1] == '\0') {
    return;
  }
  const gis::style::StyleDocument* style = detail::default_carto_style();
  const gis::style::StyleLayer* hs = detail::find_hillshade_layer(style, zoom);
  if (!hs) {
    return;
  }
  const std::string dem_path = vista::find_sample_dem_path();
  if (dem_path.empty()) {
    return;
  }
  const uint32_t tex = detail::kMap2dHillshadeTextureKey;
  base::execution::GlobalNThreadPoolExecutor executor;
  base::execution::async(executor, [this, dem_path, zoom, hs, tex]() {
    BASE_TRACE_EVENT("HillshadePrefetch", "startup");
    vista::HillshadeBake baked =
        vista::bake_hillshade_slot(dem_path, zoom, *hs, tex);
    if (!baked.ok || baked.rgba.empty() || baked.width <= 0 ||
        baked.height <= 0) {
      return;
    }
    auto pixels = std::make_shared<const std::vector<uint8_t>>(
        std::move(baked.rgba));
    std::lock_guard<std::mutex> lock(mu_);
    hillshade_w_ = baked.width;
    hillshade_h_ = baked.height;
    hillshade_rgba_ = std::move(pixels);
    hillshade_slot_ = baked.slot;
    hillshade_ready_.store(true, std::memory_order_relaxed);
  });
}

}  // namespace content
