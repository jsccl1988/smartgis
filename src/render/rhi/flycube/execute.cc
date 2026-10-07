// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/flycube/command/command_list.h"
#include "render/rhi/flycube/device.h"

#include "base/core/log.h"

namespace render {
namespace rhi {
namespace detail {

#ifdef HAS_FLYCUBE

void FlycubeDevice::replay_draws(::CommandList* fc_list, const Pass& segment,
                                 bool pass_has_depth) {
  uint32_t ok = 0;
  uint32_t skip = 0;
  for (const Draw& draw : segment.draws) {
    if (!draw.vertex || !draw.index || !draw.vertex->shared() ||
        !draw.index->shared() || draw.index_count == 0) {
      ++skip;
      continue;
    }
    FlycubeProgram* program = find_program(draw.pipeline);
    if (!program) {
      LOGGING(LOG_WARNING,
              "rhi.flycube replay_draws skip: no program pipeline=%p "
              "pass_depth=%d idx=%u",
              draw.pipeline, pass_has_depth ? 1 : 0, draw.index_count);
      ++skip;
      continue;
    }
    bool sampled = false;
    if (program->replay_draw(fc_list, draw, pass_has_depth, &sampled)) {
      ++ok;
      if (sampled) {
        ++gpu_sampled_draws_;
      }
    } else {
      ++skip;
    }
  }
  if ((ok > 0 || skip > 0) && pass_has_depth) {
    // Once per process: per-frame INFO flooded stderr and cost measurable FPS.
    static bool logged_once = false;
    if (!logged_once) {
      logged_once = true;
      LOGGING(LOG_INFO,
              "rhi.flycube replay_draws ok=%u skip=%u pass_depth=1 draws=%zu "
              "size=%ux%u (further frames silent)",
              ok, skip, segment.draws.size(), width_, height_);
    }
  }
  // Depth skips used to be silent after the first ok frame — that hid globe
  // PSO/binding failures behind a sky-only clear.
  if (skip > 0) {
    LOGGING(LOG_WARNING,
            "rhi.flycube replay_draws skip=%u ok=%u pass_depth=%d draws=%zu "
            "size=%ux%u",
            skip, ok, pass_has_depth ? 1 : 0, segment.draws.size(), width_,
            height_);
  }
}

void FlycubeDevice::barrier_depth(::CommandList* fc_list, ResourceState after) {
  if (!fc_list || !depth_texture_ || depth_state_ == after) {
    return;
  }
  fc_list->ResourceBarrier({{depth_texture_, depth_state_, after}});
  depth_state_ = after;
}

void FlycubeDevice::replay_dispatches(::CommandList* fc_list,
                                      const Dispatch* dispatches, size_t count) {
  if (!fc_list || !dispatches || count == 0) {
    return;
  }
  for (size_t i = 0; i < count; ++i) {
    const Dispatch& item = dispatches[i];
    FlycubeProgram* program = find_program(item.pipeline);
    if (!program) {
      continue;
    }
    program->replay_dispatch(fc_list, item);
  }
}

bool FlycubeDevice::execute_recorded(FlycubeCommandList* recorded) {
    if (!recorded || !swapchain_ || !command_queue_ || !fence_ || !fc_device_) {
      return false;
    }
    const uint32_t frame_index = swapchain_->NextImage(fence_, ++fence_value_);
    if (frame_index >= back_buffer_views_.size() ||
        frame_index >= kFrameCount) {
      LOGGING(LOG_ERROR,
              "rhi.flycube execute: bad frame_index=%u views=%zu size=%ux%u",
              frame_index, back_buffer_views_.size(), width_, height_);
      return false;
    }
    command_queue_->Wait(fence_, fence_value_);
    std::shared_ptr<Resource> back_buffer =
        swapchain_->GetBackBuffer(frame_index);
    // Per-frame command list: wait for this slot's prior Execute before Reset
    // (avoids D3D12 COMMAND_ALLOCATOR_SYNC when FlyCube pools allocators).
    if (graphics_fence_values_[frame_index] != 0) {
      fence_->Wait(graphics_fence_values_[frame_index]);
    }
    if (!graphics_lists_[frame_index]) {
      graphics_lists_[frame_index] =
          fc_device_->CreateCommandList(::CommandListType::kGraphics);
    }
    auto fc_list = graphics_lists_[frame_index];
    if (!fc_list || !back_buffer || !back_buffer_views_[frame_index]) {
      LOGGING(LOG_ERROR,
              "rhi.flycube execute: missing backbuffer/RTV frame=%u "
              "size=%ux%u",
              frame_index, width_, height_);
      return false;
    }
    const bool want_draw = recorded->has_draws();
    const bool graphics_ok = ensure_graphics();
    const bool can_draw = want_draw && graphics_ok;
    bool need_depth = false;
    for (const Pass& seg : recorded->recorder.passes) {
      if (seg.desc.enable_depth) {
        need_depth = true;
        break;
      }
    }
    if (want_draw && !graphics_ok) {
      LOGGING(LOG_ERROR,
              "rhi.flycube execute: ensure_graphics failed size=%ux%u "
              "(draws skipped)",
              width_, height_);
    }
    if (need_depth && !ensure_depth_buffer(width_, height_)) {
      LOGGING(LOG_ERROR,
              "rhi.flycube execute: ensure_depth_buffer failed size=%ux%u",
              width_, height_);
      return false;
    }

    fc_list->Reset();
    fc_list->SetViewport(0, 0, static_cast<float>(width_),
                         static_cast<float>(height_), 0.0f, 1.0f);
    fc_list->SetScissorRect(0, 0, width_, height_);
    fc_list->ResourceBarrier({{back_buffer, ResourceState::kPresent,
                               ResourceState::kRenderTarget}});

    // Ocean GPU FFT (and other compute) runs before any color pass.
    if (recorded->has_dispatches()) {
      if (!ensure_compute()) {
        return false;
      }
      replay_dispatches(fc_list.get(), recorded->recorder.dispatches.data(),
                        recorded->recorder.dispatches.size());
    }

    // Sequential GPU passes: each facade begin/end becomes Begin/EndRenderPass.
    std::vector<Pass> segments = recorded->recorder.passes;
    if (segments.empty() && want_draw) {
      Pass one;
      one.desc.clear_r = recorded->last_pass.clear_r;
      one.desc.clear_g = recorded->last_pass.clear_g;
      one.desc.clear_b = recorded->last_pass.clear_b;
      one.desc.clear_a = recorded->last_pass.clear_a;
      segments.push_back(std::move(one));
    }

    bool depth_cleared = false;
    for (size_t i = 0; i < segments.size(); ++i) {
      const Pass& seg = segments[i];
      ::RenderPassDesc pass;
      pass.render_area = {0, 0, width_, height_};
      RenderPassColorDesc color;
      color.view = back_buffer_views_[frame_index];
      color.store_op = RenderPassStoreOp::kStore;
      if (seg.desc.clear_color) {
        color.load_op = RenderPassLoadOp::kClear;
        color.clear_value = {seg.desc.clear_r, seg.desc.clear_g,
                             seg.desc.clear_b, seg.desc.clear_a};
      } else {
        color.load_op = RenderPassLoadOp::kLoad;
      }
      pass.colors.push_back(std::move(color));

      const bool use_depth = seg.desc.enable_depth && depth_view_;
      if (use_depth) {
        pass.depth_stencil_view = depth_view_;
        pass.depth.store_op = RenderPassStoreOp::kStore;
        if (seg.desc.clear_depth || !depth_cleared) {
          pass.depth.load_op = RenderPassLoadOp::kClear;
          pass.depth.clear_value = seg.desc.depth_clear;
          depth_cleared = true;
        } else {
          pass.depth.load_op = RenderPassLoadOp::kLoad;
        }
        // D32_FLOAT has no stencil plane — PRESERVE/LOAD on stencil trips
        // RENDER_PASS_LOCAL_DEPTH_STENCIL_ERROR under the DX12 debug layer.
        pass.stencil.load_op = RenderPassLoadOp::kDontCare;
        pass.stencil.store_op = RenderPassStoreOp::kDontCare;
        // CreateTexture leaves D32 in COMMON; BeginRenderPass requires
        // DEPTH_WRITE (debug layer INVALID_SUBRESOURCE_STATE).
        barrier_depth(fc_list.get(), ResourceState::kDepthStencilWrite);
      }

      fc_list->BeginRenderPass(pass);
      if (can_draw) {
        replay_draws(fc_list.get(), seg, use_depth);
      }
      fc_list->EndRenderPass();
    }

    fc_list->ResourceBarrier({{back_buffer, ResourceState::kRenderTarget,
                               ResourceState::kPresent}});
    fc_list->Close();
    command_queue_->ExecuteCommandLists({fc_list});
    command_queue_->Signal(fence_, ++fence_value_);
    graphics_fence_values_[frame_index] = fence_value_;
    return true;
  }

bool FlycubeDevice::execute_to_imported(FlycubeCommandList* recorded) {
    if (!recorded || !imported_shared_ || !imported_rtv_ || !command_queue_ ||
        !fence_ || !fc_device_) {
      return false;
    }
    composed_into_imported_ = false;
    wait_for_idle();
    auto fc_list = fc_device_->CreateCommandList(::CommandListType::kGraphics);
    if (!fc_list) {
      return false;
    }
    const bool want_draw = recorded->has_draws();
    const bool can_draw = want_draw && ensure_graphics();

    bool need_depth = false;
    for (const Pass& seg : recorded->recorder.passes) {
      if (seg.desc.enable_depth) {
        need_depth = true;
        break;
      }
    }
    if (need_depth && !ensure_depth_buffer(width_, height_)) {
      return false;
    }

    fc_list->Reset();
    fc_list->SetViewport(0, 0, static_cast<float>(width_),
                         static_cast<float>(height_), 0.0f, 1.0f);
    fc_list->SetScissorRect(0, 0, width_, height_);
    fc_list->ResourceBarrier({{imported_shared_, ResourceState::kCommon,
                               ResourceState::kRenderTarget}});

    if (recorded->has_dispatches()) {
      if (!ensure_compute()) {
        return false;
      }
      replay_dispatches(fc_list.get(), recorded->recorder.dispatches.data(),
                        recorded->recorder.dispatches.size());
    }

    std::vector<Pass> segments = recorded->recorder.passes;
    if (segments.empty() && want_draw) {
      Pass one;
      one.desc.clear_r = recorded->last_pass.clear_r;
      one.desc.clear_g = recorded->last_pass.clear_g;
      one.desc.clear_b = recorded->last_pass.clear_b;
      one.desc.clear_a = recorded->last_pass.clear_a;
      segments.push_back(std::move(one));
    }

    bool depth_cleared = false;
    for (size_t i = 0; i < segments.size(); ++i) {
      const Pass& seg = segments[i];
      ::RenderPassDesc pass;
      pass.render_area = {0, 0, width_, height_};
      RenderPassColorDesc color;
      color.view = imported_rtv_;
      color.store_op = RenderPassStoreOp::kStore;
      if (seg.desc.clear_color) {
        color.load_op = RenderPassLoadOp::kClear;
        color.clear_value = {seg.desc.clear_r, seg.desc.clear_g,
                             seg.desc.clear_b, seg.desc.clear_a};
      } else {
        color.load_op = RenderPassLoadOp::kLoad;
      }
      pass.colors.push_back(std::move(color));

      const bool use_depth = seg.desc.enable_depth && depth_view_;
      if (use_depth) {
        pass.depth_stencil_view = depth_view_;
        pass.depth.store_op = RenderPassStoreOp::kStore;
        if (seg.desc.clear_depth || !depth_cleared) {
          pass.depth.load_op = RenderPassLoadOp::kClear;
          pass.depth.clear_value = seg.desc.depth_clear;
          depth_cleared = true;
        } else {
          pass.depth.load_op = RenderPassLoadOp::kLoad;
        }
        pass.stencil.load_op = RenderPassLoadOp::kDontCare;
        pass.stencil.store_op = RenderPassStoreOp::kDontCare;
        barrier_depth(fc_list.get(), ResourceState::kDepthStencilWrite);
      }

      fc_list->BeginRenderPass(pass);
      if (can_draw) {
        replay_draws(fc_list.get(), seg, use_depth);
      }
      fc_list->EndRenderPass();
    }

    fc_list->ResourceBarrier({{imported_shared_, ResourceState::kRenderTarget,
                               ResourceState::kCommon}});
    fc_list->Close();
    command_queue_->ExecuteCommandLists({fc_list});
    wait_for_idle();
    composed_into_imported_ = can_draw && want_draw;
    return true;
  }

bool FlycubeDevice::execute_offscreen(FlycubeCommandList* recorded) {
    if (!recorded || !fc_device_ || !command_queue_) {
      return true;
    }
    if (!recorded->has_dispatches()) {
      return true;
    }
    if (!ensure_compute()) {
      return false;
    }
    wait_for_idle();
    auto fc_list = fc_device_->CreateCommandList(::CommandListType::kGraphics);
    if (!fc_list) {
      return false;
    }
    fc_list->Reset();
    replay_dispatches(fc_list.get(), recorded->recorder.dispatches.data(),
                      recorded->recorder.dispatches.size());
    fc_list->Close();
    command_queue_->ExecuteCommandLists({fc_list});
    wait_for_idle();
    return true;
  }

#endif  // HAS_FLYCUBE

}  // namespace detail
}  // namespace rhi
}  // namespace render
