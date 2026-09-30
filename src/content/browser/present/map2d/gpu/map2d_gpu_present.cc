// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/gpu/map2d_gpu_present.h"

#include <exception>
#include <memory>
#include <mutex>

#include "base/core/log.h"
#include "effect/map/map_effect.h"
#include "effect/map/pass.h"
#include "gis/vista/frame/frame.h"
#include "render/graph/frame_graph.h"
#include "render/rhi/rhi.h"
#include "base/trace/event/process_trace.h"

namespace content {

Map2dGpuPresent::Map2dGpuPresent() = default;
Map2dGpuPresent::~Map2dGpuPresent() = default;

void Map2dGpuPresent::bind(const MapScene* scene, const ViewFrame* frame,
                           Map2dFrameCache* cache) {
  scene_ = scene;
  frame_ = frame;
  cache_ = cache;
  invalidate_frame_cache();
}

void Map2dGpuPresent::invalidate_frame_cache() {
  if (cache_) {
    std::lock_guard<std::recursive_mutex> lock(cache_->mutex());
    cache_->invalidate();
    map2d_pass_.reset();
    shell_overlay_.abandon_gpu();
    shell_overlay_.clear();
    return;
  }
  map2d_pass_.reset();
  shell_overlay_.abandon_gpu();
  shell_overlay_.clear();
}

uint64_t Map2dGpuPresent::layout_build_count() const {
  return cache_ ? cache_->layout_build_count() : 0;
}

bool Map2dGpuPresent::last_present_reused_layout() const {
  return cache_ ? cache_->last_present_reused_layout() : false;
}

bool Map2dGpuPresent::present_frame(render::rhi::Device* device,
                                    const Map2dFrameCache::CameraKey& cam,
                                    bool record_all,
                                    const ui::gfx::ShellRaster* shell,
                                    uint64_t shell_generation) {
  BASE_TRACE_EVENT("present_frame", "map2d.present");
  if (!device || !cache_ || !cache_->has_frame()) {
    return false;
  }
  if (!map2d_pass_) {
    map2d_pass_ = std::make_unique<effect::map::Pass>();
  }

  effect::map::WindowsGlyphRasterizer windows_rasterizer;
  gis::vista::View view{cam.width_px, cam.height_px, cam.min_x, cam.min_y,
                        cam.max_x,    cam.max_y};
  const render::rhi::CameraMatrices camera = render::rhi::make_ortho_camera(
      static_cast<float>(cam.min_x), static_cast<float>(cam.max_x),
      static_cast<float>(cam.min_y), static_cast<float>(cam.max_y), -1.f, 1.f);

  effect::map::MapEffect map_effect(
      render::graph::EffectSlot::kOpaque, map2d_pass_.get(), &cache_->frame(),
      &view, &windows_rasterizer, {}, {}, record_all);
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
  last_present_ok_ = false;
  if (!device || !scene_ || !frame_ || !cache_ || width_px == 0 ||
      height_px == 0) {
    LOGGING(LOG_ERROR,
            "map2d.present fail: bad args device=%p scene=%p frame=%p "
            "cache=%p size=%ux%u",
            device, scene_, frame_, cache_, width_px, height_px);
    return false;
  }

  std::lock_guard<std::recursive_mutex> lock(cache_->mutex());
  try {
    Map2dFrameCache::PresentAction action =
        Map2dFrameCache::PresentAction::kRebuildFull;
    if (!cache_->prepare_for_present(width_px, height_px, &action)) {
      LOGGING(LOG_ERROR,
              "map2d.present fail: prepare_for_present size=%ux%u "
              "(extent/layout)",
              width_px, height_px);
      return false;
    }

    const bool record_all =
        action != Map2dFrameCache::PresentAction::kInteractiveReuse;
    if (action == Map2dFrameCache::PresentAction::kRebuildFull ||
        action == Map2dFrameCache::PresentAction::kSettleRebuild) {
      if (map2d_pass_) {
        map2d_pass_->invalidate_uploaded();
      }
    }

    const bool ok =
        present_frame(device, cache_->camera(), record_all, shell,
                      shell_generation);
    cache_->note_present_outcome(action);
    last_present_ok_ = ok;
    if (!ok) {
      const auto& cam = cache_->camera();
      LOGGING(LOG_ERROR,
              "map2d.present fail: graph::present size=%ux%u cam=[%.3f,%.3f]-"
              "[%.3f,%.3f] shell=%d",
              width_px, height_px, cam.min_x, cam.min_y, cam.max_x, cam.max_y,
              (shell && shell->bgra) ? 1 : 0);
    }
    return ok;
  } catch (const std::exception& ex) {
    LOGGING(LOG_ERROR, "map2d.present exception: %s", ex.what());
    map2d_pass_.reset();
    shell_overlay_.abandon_gpu();
    shell_overlay_.clear();
    cache_->invalidate();
    last_present_ok_ = false;
    return false;
  }
}

}  // namespace content
