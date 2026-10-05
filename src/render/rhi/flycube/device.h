// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_RHI_FLYCUBE_DEVICE_H_
#define RENDER_RHI_FLYCUBE_DEVICE_H_

// RHI adapter for FlyCube. Programs are keyed by Pipeline*.

#include "render/rhi/rhi.h"

#ifdef HAS_FLYCUBE
#include "render/rhi/flycube/pipeline/program.h"

#include "CommandList/CommandList.h"
#include "CommandQueue/CommandQueue.h"
#include "Device/Device.h"
#include "Fence/Fence.h"
#include "Instance/BaseTypes.h"
#include "Instance/Instance.h"
#include "Resource/Resource.h"
#include "Swapchain/Swapchain.h"
#include "Utilities/Common.h"
#include "Utilities/FormatHelper.h"
#include "View/View.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <windows.h>
#endif

namespace render {
namespace rhi {
namespace detail {

#ifdef HAS_FLYCUBE

struct Pass;
struct Dispatch;
class FlycubeCommandList;
class GpuTexture;

// FlyCube GPU device. Compiled programs live in programs_.
class FlycubeDevice : public Device {
 public:
  explicit FlycubeDevice(Backend backend);
  bool initialize(const DeviceDesc& desc) override;
  void shutdown() override;
  void present() override;
  Backend backend() const override;
  uint32_t gpu_sampled_draws() const override;
  bool supports_compute() const override;
  CommandList* create_command_list() override;
  void destroy_command_list(CommandList* list) override;
  bool execute(CommandList* list) override;
  Buffer* create_buffer(uint32_t byte_size, BufferUsage usage) override;
  void destroy_buffer(Buffer* buffer) override;
  bool upload(Buffer* buffer, const void* data, uint32_t byte_size) override;
  Texture* create_texture(const TextureDesc& desc) override;
  void destroy_texture(Texture* texture) override;
  bool upload_texture(Texture* texture, const void* data,
                      uint32_t byte_size) override;
  Pipeline* create_graphics_pipeline(const GraphicsPipelineDesc& desc) override;
  Pipeline* create_compute_pipeline(const ComputePipelineDesc& desc) override;
  void destroy_pipeline(Pipeline* pipeline) override;

  bool import_shared_nt_handle(void* nt_handle, uint32_t width_px,
                               uint32_t height_px) override;
  bool copy_bgra_to_imported_shared(const uint8_t* bgra, uint32_t stride_bytes,
                                    uint32_t width_px,
                                    uint32_t height_px) override;
  bool has_imported_shared() const override;
  bool composed_into_imported_shared() const override;
  Texture* shared_depth_texture() override;

 private:
  void clear_depth_sample_facade();
  // Triple-buffer: with 2, window Present stalls on the last timed world3d
  // frame (~20–30 ms) even with ALLOW_TEARING under windowed DWM.
  static constexpr uint32_t kFrameCount = 3;
  void wait_for_idle();
  // WM_SIZE re-enters initialize on a live device. Drop only the swapchain
  // and depth target so Pipeline* / Buffer* owned by passes stay valid.
  // A full shutdown() frees programs_ and those pointers become 0xDD.
  bool recreate_swapchain(const DeviceDesc& desc);
  bool upload_gpu_texture(GpuTexture* gpu, const void* data,
                          uint32_t byte_size);
  bool ensure_depth_buffer(uint32_t w, uint32_t h);
  bool ensure_graphics();
  bool ensure_compute();
  bool output_color_format(gli::format* format) const;
  FlycubeProgram* find_program(Pipeline* pipeline) const;
  void clear_imported_shared();
  void replay_draws(::CommandList* fc_list, const Pass& segment,
                    bool pass_has_depth);
  bool execute_recorded(FlycubeCommandList* recorded);
  bool execute_to_imported(FlycubeCommandList* recorded);
  bool execute_offscreen(FlycubeCommandList* recorded);
  void replay_dispatches(::CommandList* fc_list, const Dispatch* dispatches,
                         size_t count);

  Backend backend_;
  HWND hwnd_ = nullptr;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t adapter_index_ = 0;
  uint64_t fence_value_ = 0;
  uint64_t graphics_fence_values_[kFrameCount] = {};
  uint32_t gpu_sampled_draws_ = 0;
  uint32_t depth_w_ = 0;
  uint32_t depth_h_ = 0;
  bool composed_into_imported_ = false;
  std::shared_ptr<Instance> instance_;
  // DXDevice / VKDevice store Adapter& only. Keep the shared_ptr so the
  // adapter outlives CreateSwapchain / GetInstance after initialize returns.
  std::shared_ptr<Adapter> adapter_;
  std::shared_ptr<::Device> fc_device_;
  std::shared_ptr<CommandQueue> command_queue_;
  std::shared_ptr<Swapchain> swapchain_;
  std::shared_ptr<Fence> fence_;
  std::shared_ptr<::CommandList> graphics_lists_[kFrameCount];
  std::vector<std::shared_ptr<View>> back_buffer_views_;
  std::shared_ptr<Resource> depth_texture_;
  std::shared_ptr<View> depth_view_;
  // Sampleable SRV over the same depth resource (fog / soft particles).
  std::shared_ptr<View> depth_srv_;
  // Non-owning facade returned by shared_depth_texture(); reset with depth.
  Texture* depth_sample_facade_ = nullptr;
  // DXGI NT shared texture opened on the DX12 device (browser OutputSurface).
  std::shared_ptr<Resource> imported_shared_;
  std::shared_ptr<View> imported_rtv_;
  void* imported_nt_handle_ = nullptr;
  std::unordered_map<Pipeline*, std::unique_ptr<FlycubeProgram>> programs_;
};

#endif  // HAS_FLYCUBE

}  // namespace detail
}  // namespace rhi
}  // namespace render

#endif  // RENDER_RHI_FLYCUBE_DEVICE_H_
