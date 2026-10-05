// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/flycube/command/command_list.h"
#include "render/rhi/flycube/device.h"
#include "render/rhi/flycube/pipeline/hlsl.h"

#include "base/core/log.h"

#ifdef HAS_FLYCUBE
#include "ApiType/ApiType.h"
#endif

#include <memory>

namespace render {
namespace rhi {

namespace detail {
#ifdef HAS_FLYCUBE

ApiType flycube_api(Backend backend) {
  return backend == Backend::kDx12 ? ApiType::kDX12 : ApiType::kVulkan;
}

FlycubeDevice::FlycubeDevice(Backend backend) : backend_(backend) {}

bool FlycubeDevice::recreate_swapchain(const DeviceDesc& desc) {
    // Keep instance_, fc_device_, and programs_. Passes cache Pipeline* into
    // programs_; shutdown() would delete those objects under them.
    width_ = desc.width > 0 ? desc.width : 1;
    height_ = desc.height > 0 ? desc.height : 1;
    hwnd_ = static_cast<HWND>(desc.native_window);
    wait_for_idle();
    for (uint32_t i = 0; i < kFrameCount; ++i) {
      graphics_lists_[i].reset();
      graphics_fence_values_[i] = 0;
    }
    depth_view_.reset();
    depth_srv_.reset();
    depth_texture_.reset();
    depth_state_ = ResourceState::kCommon;
    clear_depth_sample_facade();
    depth_w_ = 0;
    depth_h_ = 0;
    back_buffer_views_.clear();
    swapchain_.reset();
    if (!hwnd_) {
      return fc_device_ != nullptr;
    }
    NativeSurface surface = Win32Surface{GetModuleHandleW(nullptr), hwnd_};
    swapchain_ = fc_device_->CreateSwapchain(surface, width_, height_,
                                             kFrameCount, false);
    if (!swapchain_) {
      return false;
    }
    back_buffer_views_.resize(kFrameCount);
    for (uint32_t i = 0; i < kFrameCount; ++i) {
      ViewDesc view_desc = {
          .view_type = ViewType::kRenderTarget,
          .dimension = ViewDimension::kTexture2D,
      };
      back_buffer_views_[i] =
          fc_device_->CreateView(swapchain_->GetBackBuffer(i), view_desc);
      if (!back_buffer_views_[i]) {
        // Partial RTVs + live swapchain makes execute_recorded AV on the
        // null view. Tear down fully so the next present soft-fails cleanly.
        LOGGING(LOG_ERROR,
                "rhi.flycube recreate_swapchain CreateView fail i=%u "
                "hwnd=%p %ux%u",
                i, hwnd_, width_, height_);
        back_buffer_views_.clear();
        swapchain_.reset();
        return false;
      }
    }
    return true;
  }

bool FlycubeDevice::initialize(const DeviceDesc& desc) {
    // WM_SIZE / MapViewport resize re-call initialize on a live device.
    // DXGI allows only one flip-model swapchain per HWND; leaving the old
    // chain bound makes CreateSwapChainForHwnd fail and FlyCube CHECK abort().
    // Recreate the chain only — do not shutdown() — or every pass's Pipeline*
    // dangles (programs_.clear) and the next record/destroy AVs on 0xDD.
    if (fc_device_ && instance_) {
      const bool ok = recreate_swapchain(desc);
      if (!ok) {
        LOGGING(LOG_ERROR,
                "rhi.flycube recreate_swapchain fail hwnd=%p %ux%u",
                desc.native_window, desc.width, desc.height);
      } else {
        LOGGING(LOG_INFO, "rhi.flycube recreate_swapchain ok hwnd=%p %ux%u",
                desc.native_window, desc.width, desc.height);
      }
      return ok;
    }
    if (instance_ || swapchain_) {
      shutdown();
    }
    width_ = desc.width;
    height_ = desc.height;
    hwnd_ = static_cast<HWND>(desc.native_window);
    adapter_index_ = desc.adapter_index;
    composed_into_imported_ = false;

    instance_ = CreateInstance(flycube_api(backend_));
    if (!instance_) {
      LOGGING(LOG_ERROR, "rhi.flycube CreateInstance fail backend=%d",
              static_cast<int>(backend_));
      return false;
    }

    const auto adapters = instance_->EnumerateAdapters();
    if (adapters.empty()) {
      LOGGING(LOG_ERROR, "rhi.flycube EnumerateAdapters empty");
      return false;
    }
    size_t idx = static_cast<size_t>(adapter_index_);
    if (idx >= adapters.size()) {
      idx = 0;
    }

    adapter_ = adapters[idx];
    fc_device_ = adapter_->CreateDevice();
    if (!fc_device_) {
      LOGGING(LOG_ERROR, "rhi.flycube Adapter::CreateDevice fail idx=%zu", idx);
      adapter_.reset();
      return false;
    }

    command_queue_ = fc_device_->GetCommandQueue(::CommandListType::kGraphics);
    if (!command_queue_) {
      LOGGING(LOG_ERROR, "rhi.flycube GetCommandQueue(Graphics) fail");
      return false;
    }

    fence_ = fc_device_->CreateFence(0);
    if (!fence_) {
      LOGGING(LOG_ERROR, "rhi.flycube CreateFence fail");
      return false;
    }

    if (hwnd_ && width_ > 0 && height_ > 0) {
      NativeSurface surface = Win32Surface{GetModuleHandleW(nullptr), hwnd_};
      swapchain_ = fc_device_->CreateSwapchain(surface, width_, height_,
                                               kFrameCount, false);
      if (!swapchain_) {
        LOGGING(LOG_ERROR,
                "rhi.flycube CreateSwapchain fail hwnd=%p %ux%u (DXGI often "
                "rejects a second flip chain on the same HWND)",
                hwnd_, width_, height_);
        return false;
      }
      back_buffer_views_.resize(kFrameCount);
      for (uint32_t i = 0; i < kFrameCount; ++i) {
        ViewDesc view_desc = {
            .view_type = ViewType::kRenderTarget,
            .dimension = ViewDimension::kTexture2D,
        };
        back_buffer_views_[i] =
            fc_device_->CreateView(swapchain_->GetBackBuffer(i), view_desc);
        if (!back_buffer_views_[i]) {
          LOGGING(LOG_ERROR, "rhi.flycube CreateView(backbuffer %u) fail", i);
          back_buffer_views_.clear();
          swapchain_.reset();
          return false;
        }
      }
    }

    LOGGING(LOG_INFO,
            "rhi.flycube initialize ok hwnd=%p %ux%u adapter=%zu backend=%d",
            hwnd_, width_, height_, idx, static_cast<int>(backend_));
    return fc_device_ != nullptr;
  }

void FlycubeDevice::shutdown() {
    wait_for_idle();
    clear_imported_shared();
    for (uint32_t i = 0; i < kFrameCount; ++i) {
      graphics_lists_[i].reset();
      graphics_fence_values_[i] = 0;
    }
    programs_.clear();
    depth_view_.reset();
    depth_srv_.reset();
    depth_texture_.reset();
    depth_state_ = ResourceState::kCommon;
    clear_depth_sample_facade();
    back_buffer_views_.clear();
    swapchain_.reset();
    fence_.reset();
    command_queue_.reset();
    fc_device_.reset();
    adapter_.reset();
    instance_.reset();
    hwnd_ = nullptr;
    fence_value_ = 0;
    gpu_sampled_draws_ = 0;
    depth_w_ = 0;
    depth_h_ = 0;
    composed_into_imported_ = false;
  }

void FlycubeDevice::present() {
    if (!swapchain_ || !command_queue_ || !fence_) {
      return;
    }
    // CPU-wait for GPU work before DXGI Present. FlyCube's Swapchain::Present
    // only inserts a queue Wait then calls Present immediately.
    command_queue_->Signal(fence_, ++fence_value_);
    fence_->Wait(fence_value_);
    swapchain_->Present(fence_, fence_value_);
  }

Backend FlycubeDevice::backend() const { return backend_; }

uint32_t FlycubeDevice::gpu_sampled_draws() const { return gpu_sampled_draws_; }

bool FlycubeDevice::supports_compute() const { return fc_device_ != nullptr; }

CommandList* FlycubeDevice::create_command_list() {
    return new FlycubeCommandList();
  }

void FlycubeDevice::destroy_command_list(CommandList* list) { delete list; }

bool FlycubeDevice::execute(CommandList* list) {
    composed_into_imported_ = false;
    auto* recorded = static_cast<StubCommandList*>(list);
    if (!recorded || !recorded->closed) {
      return false;
    }
    if (!command_queue_ || !fc_device_) {
      return true;
    }
    auto* fly_list = static_cast<FlycubeCommandList*>(list);
    if (swapchain_ && fence_ &&
        (fly_list->had_pass() || fly_list->has_draws() ||
         fly_list->has_dispatches())) {
      return execute_recorded(fly_list);
    }
    if (imported_shared_ && imported_rtv_ && fence_ &&
        (fly_list->had_pass() || fly_list->has_draws() ||
         fly_list->has_dispatches())) {
      return execute_to_imported(fly_list);
    }
    if (fly_list->has_draws() || fly_list->has_dispatches()) {
      return execute_offscreen(fly_list);
    }
    // Empty close: still serialize before Reset — FlyCube may pool allocators.
    wait_for_idle();
    auto fc_list = fc_device_->CreateCommandList(::CommandListType::kGraphics);
    if (fc_list) {
      fc_list->Reset();
      fc_list->Close();
      command_queue_->ExecuteCommandLists({fc_list});
      wait_for_idle();
    }
    return true;
  }

bool FlycubeDevice::output_color_format(gli::format* format) const {
    if (!format) {
      return false;
    }
    if (swapchain_) {
      *format = swapchain_->GetFormat();
      return true;
    }
    // Imported browser surface is BGRA8, the same choice ensure_graphics uses
    // when there is no swapchain.
    if (imported_shared_) {
      *format = gli::FORMAT_BGRA8_UNORM_PACK8;
      return true;
    }
    return false;
}

bool FlycubeDevice::ensure_graphics() {
    if (!fc_device_) {
      return false;
    }
    gli::format color_format = gli::format::FORMAT_UNDEFINED;
    if (!output_color_format(&color_format) ||
        color_format == gli::format::FORMAT_UNDEFINED) {
      return false;
    }
    // Depth is allocated on demand when a pass sets enable_depth. Requiring a
    // DSV here made can_draw false whenever CreateTexture(DSV|SRV) failed at
    // large interactive HWND sizes — Scene3d cleared to background only while
    // the same mesh/camera at 640x480 (atmosphere-showcase) still drew DEM.
    return true;
  }

bool FlycubeDevice::ensure_compute() { return fc_device_ != nullptr; }

FlycubeProgram* FlycubeDevice::find_program(Pipeline* pipeline) const {
    if (!pipeline) {
      return nullptr;
    }
    const auto it = programs_.find(pipeline);
    if (it == programs_.end()) {
      return nullptr;
    }
    return it->second.get();
}

Pipeline* FlycubeDevice::create_graphics_pipeline(const GraphicsPipelineDesc& desc) {
    if (!graphics_pipeline_desc_ok(desc) || !fc_device_ ||
        !ensure_dxc_beside_exe()) {
      return nullptr;
    }
    gli::format color = {};
    if (!output_color_format(&color)) {
      return nullptr;
    }
    // Same format ensure_depth_buffer allocates when the texture is not up yet.
    const gli::format depth = depth_texture_
                                  ? depth_texture_->GetFormat()
                                  : gli::format::FORMAT_D32_SFLOAT_PACK32;
    std::unique_ptr<FlycubeProgram> program =
        FlycubeProgram::compile_graphics(fc_device_, desc, color, depth);
    if (!program) {
      return nullptr;
    }
    Pipeline* key = program.get();
    programs_.emplace(key, std::move(program));
    return key;
}

Pipeline* FlycubeDevice::create_compute_pipeline(const ComputePipelineDesc& desc) {
    if (!compute_pipeline_desc_ok(desc) || !fc_device_ ||
        !ensure_dxc_beside_exe()) {
      return nullptr;
    }
    std::unique_ptr<FlycubeProgram> program =
        FlycubeProgram::compile_compute(fc_device_, desc);
    if (!program) {
      return nullptr;
    }
    Pipeline* key = program.get();
    programs_.emplace(key, std::move(program));
    return key;
}

void FlycubeDevice::destroy_pipeline(Pipeline* pipeline) {
    if (!pipeline) {
      return;
    }
    programs_.erase(pipeline);
}

#endif  // HAS_FLYCUBE
}  // namespace detail

#ifdef HAS_FLYCUBE

Device* create_flycube_device(Backend backend) {
  return new detail::FlycubeDevice(backend);
}

#else

namespace detail {
class FlycubeDevice : public Device {
 public:
  explicit FlycubeDevice(Backend backend) : backend_(backend) {}

  bool initialize(const DeviceDesc&) override { return false; }
  void shutdown() override {}
  void present() override {}
  Backend backend() const override { return backend_; }

  CommandList* create_command_list() override { return new StubCommandList(); }
  void destroy_command_list(CommandList* list) override { delete list; }
  bool execute(CommandList* list) override {
    auto* stub = static_cast<StubCommandList*>(list);
    return stub != nullptr && stub->closed;
  }
  Buffer* create_buffer(uint32_t byte_size, BufferUsage usage) override {
    return make_stub_buffer(byte_size, usage);
  }
  void destroy_buffer(Buffer* buffer) override {
    destroy_stub_buffer(buffer);
  }
  bool upload(Buffer* buffer, const void* data, uint32_t byte_size) override {
    return upload_stub_buffer(buffer, data, byte_size);
  }

 private:
  Backend backend_;
};

}  // namespace detail

Device* create_flycube_device(Backend backend) {
  return new detail::FlycubeDevice(backend);
}

#endif  // HAS_FLYCUBE

}  // namespace rhi
}  // namespace render
