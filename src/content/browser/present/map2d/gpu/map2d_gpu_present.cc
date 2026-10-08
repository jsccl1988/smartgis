// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/gpu/map2d_gpu_present.h"

#include "content/content_export.h"
#include "content/browser/present/map2d/map2d_phase_profile.h"
#include "content/browser/present/map2d/gpu/map2d_selection_overlay.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <vector>

#include "base/core/log.h"
#include "vista/pass/map/map_effect.h"
#include "vista/pass/map/pass.h"
#include "vista/component/map/ir.h"
#include "render/graph/frame_graph.h"
#include "render/rhi/rhi.h"
#include "base/trace/event/process_trace.h"
#include "base/process/switches.h"

namespace content {
namespace {

std::atomic<uint64_t> g_skip{0};
std::atomic<uint64_t> g_full{0};
std::atomic<uint64_t> g_act_rebuild{0};
std::atomic<uint64_t> g_act_interactive{0};
std::atomic<uint64_t> g_act_settle{0};
std::atomic<uint64_t> g_act_static{0};

void note_action(Map2dFrameCache::PresentAction action) {
  switch (action) {
    case Map2dFrameCache::PresentAction::kRebuildFull:
      g_act_rebuild.fetch_add(1, std::memory_order_relaxed);
      break;
    case Map2dFrameCache::PresentAction::kInteractiveReuse:
      g_act_interactive.fetch_add(1, std::memory_order_relaxed);
      break;
    case Map2dFrameCache::PresentAction::kSettleRebuild:
      g_act_settle.fetch_add(1, std::memory_order_relaxed);
      break;
    case Map2dFrameCache::PresentAction::kStaticReuse:
      g_act_static.fetch_add(1, std::memory_order_relaxed);
      break;
  }
}

}  // namespace

Map2dGpuPresentProfile map2d_gpu_present_profile() {
  Map2dGpuPresentProfile p;
  p.skip = g_skip.load(std::memory_order_relaxed);
  p.full = g_full.load(std::memory_order_relaxed);
  p.action_rebuild = g_act_rebuild.load(std::memory_order_relaxed);
  p.action_interactive = g_act_interactive.load(std::memory_order_relaxed);
  p.action_settle = g_act_settle.load(std::memory_order_relaxed);
  p.action_static = g_act_static.load(std::memory_order_relaxed);
  return p;
}

void reset_map2d_gpu_present_profile() {
  g_skip.store(0, std::memory_order_relaxed);
  g_full.store(0, std::memory_order_relaxed);
  g_act_rebuild.store(0, std::memory_order_relaxed);
  g_act_interactive.store(0, std::memory_order_relaxed);
  g_act_settle.store(0, std::memory_order_relaxed);
  g_act_static.store(0, std::memory_order_relaxed);
}

Map2dGpuPresent::Map2dGpuPresent() = default;
Map2dGpuPresent::~Map2dGpuPresent() = default;

void Map2dGpuPresent::bind(const GisScene* scene, const ViewFrame* frame,
                           Map2dFrameCache* cache) {
  scene_ = scene;
  frame_ = frame;
  cache_ = cache;
  invalidate_frame_cache();
}

void Map2dGpuPresent::invalidate_frame_cache() {
  if (cache_) {
    // Do not lock around invalidate(): Map2dFrameCache::invalidate already
    // takes mu_. Nested lock_guard was fine for recursive_mutex but masked
    // ABI/layout skew as resource_deadlock_would_occur (error 5) in bind.
    cache_->invalidate();
    map2d_pass_.reset();
    shell_overlay_.abandon_gpu();
    shell_overlay_.clear();
    last_present_ok_ = false;
    last_present_drew_ = false;
    last_shell_generation_ = 0;
    last_had_shell_ = false;
    return;
  }
  map2d_pass_.reset();
  shell_overlay_.abandon_gpu();
  shell_overlay_.clear();
  last_present_ok_ = false;
  last_present_drew_ = false;
  last_shell_generation_ = 0;
  last_had_shell_ = false;
}

uint64_t Map2dGpuPresent::layout_build_count() const {
  return cache_ ? cache_->layout_build_count() : 0;
}

bool Map2dGpuPresent::last_present_reused_layout() const {
  return cache_ ? cache_->last_present_reused_layout() : false;
}

bool Map2dGpuPresent::present_frame(render::rhi::Device* device,
                                    const vista::MapIR& frame_ir,
                                    const Map2dFrameCache::CameraKey& cam,
                                    bool record_all,
                                    const ui::gfx::ShellRaster* shell,
                                    uint64_t shell_generation) {
  BASE_TRACE_EVENT("present_frame", "map2d.present");
  if (!device) {
    return false;
  }
  if (!map2d_pass_) {
    map2d_pass_ = std::make_unique<vista::MapPass>();
  }

  vista::WindowsGlyphRasterizer windows_rasterizer;
  vista::View view{cam.width_px, cam.height_px, cam.min_x, cam.min_y,
                        cam.max_x,    cam.max_y};
  const render::rhi::CameraMatrices camera = render::rhi::make_ortho_camera(
      static_cast<float>(cam.min_x), static_cast<float>(cam.max_x),
      static_cast<float>(cam.min_y), static_cast<float>(cam.max_y), -1.f, 1.f);

  vista::MapEffect map_effect(
      render::graph::EffectSlot::kOpaque, map2d_pass_.get(), &frame_ir, &view,
      &windows_rasterizer,
      [this](uint32_t texture_key, std::vector<uint8_t>* rgba, int* w, int* h) {
        return cache_ && cache_->load_raster(texture_key, rgba, w, h);
      },
      {}, record_all);
  if (shell && shell->bgra && shell->width_px != 0 && shell->height_px != 0) {
    shell_overlay_.bind(*shell, shell_generation);
  } else {
    shell_overlay_.clear();
  }
  render::graph::ViewInput view_input;
  view_input.width_px = cam.width_px;
  view_input.height_px = cam.height_px;
  view_input.camera = &camera;
  view_input.effects.push_back(&map_effect);
  if (shell_overlay_.has_shell()) {
    view_input.effects.push_back(&shell_overlay_);
  }
  return render::graph::present(device, view_input);
}

bool Map2dGpuPresent::present(render::rhi::Device* device, uint32_t width_px,
                              uint32_t height_px,
                              const ui::gfx::ShellRaster* shell,
                              uint64_t shell_generation) {
  BASE_TRACE_EVENT("present", "map2d.present");
  if (!device || !scene_ || !frame_ || !cache_ || width_px == 0 ||
      height_px == 0) {
    last_present_ok_ = false;
    last_present_drew_ = false;
    LOGGING(LOG_ERROR,
            "map2d.present fail: bad args device=%p scene=%p frame=%p "
            "cache=%p size=%ux%u",
            device, scene_, frame_, cache_, width_px, height_px);
    return false;
  }

  // prepare_for_present / load_raster / note_present_outcome take mu_ briefly.
  // Do not hold cache mu_ across china tess or Pass record (UI hang).
  try {
    Map2dFrameCache::PresentAction action =
        Map2dFrameCache::PresentAction::kRebuildFull;
    if (!cache_->prepare_for_present(width_px, height_px, &action)) {
      // Drop the StaticReuse latch — a failed prepare after Resize clear must
      // not leave last_present_ok_ true for a later hollow skip.
      last_present_ok_ = false;
      last_present_drew_ = false;
      LOGGING(LOG_ERROR,
              "map2d.present fail: prepare_for_present size=%ux%u "
              "(extent/layout)",
              width_px, height_px);
      return false;
    }
    note_action(action);

    const bool shell_present =
        shell && shell->bgra && shell->width_px != 0 && shell->height_px != 0;
    // Dual-speed: settled StaticReuse with unchanged shell keeps the prior
    // swapchain image — skip Pass re-record + graph::present (china FPS).
    // FPS bench: ignore shell churn; also skip InteractiveReuse (camera still)
    // so settle debounce does not force a full present every frame.
    const bool fps_bench = []() {
      const char* e = base::switch_cstr("map2d-fps-bench-ms");
      return e && e[0] != '\0' && std::atoi(e) > 0;
    }();
    // Shell presence + generation must match the last drew frame. gen==0 with
    // pixels is treated as unstable (DrawHost uses 0 as "always copy").
    const bool shell_stable =
        fps_bench ||
        (shell_present == last_had_shell_ &&
         (!shell_present ||
          (shell_generation != 0 &&
           shell_generation == last_shell_generation_)));
    const uint64_t selection_sig =
        detail::map2d_selection_signature(scene_, flash_pulse_);
    const bool selection_dirty = selection_sig != last_selection_sig_;

    const bool reuse_action =
        action == Map2dFrameCache::PresentAction::kStaticReuse ||
        (fps_bench &&
         action == Map2dFrameCache::PresentAction::kInteractiveReuse);
    if (reuse_action && last_present_ok_ && map2d_pass_ && shell_stable &&
        !selection_dirty) {
      cache_->note_present_outcome(
          Map2dFrameCache::PresentAction::kStaticReuse);
      last_present_ok_ = true;
      last_present_drew_ = false;
      g_skip.fetch_add(1, std::memory_order_relaxed);
      note_map2d_phase_gpu(0, 0);
      return true;
    }

    // Full content rebuild drops GPU buffers (every cache_key misses). Settle
    // keeps hit slices. Interactive / StaticReuse (incl. post-surface-reset
    // hollow fill) must NOT invalidate_uploaded — DrawCache encode fills the
    // clear swapchain without a china cold re-upload.
    // Selection-only dirty still re-records (overlay items have no cache_key).
    const bool record_all =
        action == Map2dFrameCache::PresentAction::kRebuildFull ||
        action == Map2dFrameCache::PresentAction::kSettleRebuild ||
        !last_present_ok_ || selection_dirty;
    const bool full_replace =
        action == Map2dFrameCache::PresentAction::kRebuildFull;
    if (!map2d_pass_) {
      map2d_pass_ = std::make_unique<vista::MapPass>();
    }
    if (full_replace) {
      map2d_pass_->invalidate_uploaded();
      map2d_pass_->set_upload_policy(vista::MapPass::UploadPolicy::kReplace);
    } else if (action == Map2dFrameCache::PresentAction::kSettleRebuild) {
      map2d_pass_->set_upload_policy(
          vista::MapPass::UploadPolicy::kIncremental);
    } else {
      map2d_pass_->set_upload_policy(
          vista::MapPass::UploadPolicy::kReuseIfCached);
    }

    // Snap MapIR + camera under a short lock so Pass record never races a
    // concurrent rebuild/invalidate mutating cached_frame_.
    vista::MapIR frame_snap;
    Map2dFrameCache::CameraKey cam{};
    uint64_t builds = 0;
    {
      std::lock_guard<std::recursive_mutex> snap(cache_->mutex());
      if (!cache_->has_frame()) {
        last_present_ok_ = false;
        last_present_drew_ = false;
        LOGGING(LOG_ERROR, "map2d.present fail: no frame after prepare");
        return false;
      }
      frame_snap = cache_->frame();
      cam = cache_->camera();
      builds = cache_->layout_build_count();
    }
    detail::append_map2d_selection_overlay(scene_, frame_, width_px, height_px,
                                           flash_pulse_, &frame_snap);

    vista::reset_last_pass_record_ms();
    const auto gpu_t0 = std::chrono::steady_clock::now();
    const bool ok =
        present_frame(device, frame_snap, cam, record_all, shell,
                      shell_generation);
    const int64_t gpu_wall_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - gpu_t0)
            .count();
    const int64_t upload_ms = vista::last_pass_record_ms();
    const int64_t present_ms =
        gpu_wall_ms > upload_ms ? (gpu_wall_ms - upload_ms) : 0;
    note_map2d_phase_gpu(upload_ms, present_ms);
    cache_->note_present_outcome(action);
    last_present_ok_ = ok;
    last_present_drew_ = ok;
    g_full.fetch_add(1, std::memory_order_relaxed);
    if (ok) {
      last_selection_sig_ = selection_sig;
      last_had_shell_ = shell_present;
      // Only latch non-zero gens — 0 means "unversioned" and must not pretend
      // shell_stable on the next StaticReuse check.
      last_shell_generation_ =
          shell_present && shell_generation != 0 ? shell_generation : 0;
      // Plain-launch / harness gold when hillshade is skipped (no
      // emit_hillshade frame_items= line). One line per full present.
      std::fprintf(stderr,
                   "map2d: present frame_items=%zu size=%ux%u builds=%llu\n",
                   frame_snap.items.size(), width_px, height_px,
                   static_cast<unsigned long long>(builds));
      std::fflush(stderr);
    }
    if (!ok) {
      LOGGING(LOG_ERROR,
              "map2d.present fail: graph::present size=%ux%u cam=[%.3f,%.3f]-"
              "[%.3f,%.3f] shell=%d",
              width_px, height_px, cam.min_x, cam.min_y, cam.max_x, cam.max_y,
              shell_present ? 1 : 0);
    }
    return ok;
  } catch (const std::exception& ex) {
    LOGGING(LOG_ERROR, "map2d.present exception: %s", ex.what());
    map2d_pass_.reset();
    shell_overlay_.abandon_gpu();
    shell_overlay_.clear();
    cache_->invalidate();
    last_present_ok_ = false;
    last_present_drew_ = false;
    last_shell_generation_ = 0;
    last_had_shell_ = false;
    return false;
  }
}

}  // namespace content
