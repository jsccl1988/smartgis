// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_RHI_FLYCUBE_PIPELINE_PROGRAM_H_
#define RENDER_RHI_FLYCUBE_PIPELINE_PROGRAM_H_

#include "render/rhi/flycube/command/recorder.h"
#include "render/rhi/rhi.h"

#include <cstdint>
#include <utility>
#include <vector>

#ifdef HAS_FLYCUBE
#include "BindingSet/BindingSet.h"
#include "BindingSetLayout/BindingSetLayout.h"
#include "CommandList/CommandList.h"
#include "Device/Device.h"
#include "Instance/BaseTypes.h"
#include "Pipeline/Pipeline.h"
#include "Resource/Resource.h"
#include "Shader/Shader.h"
#include "View/View.h"

#include <cstdint>
#include <memory>
#include <vector>
#endif

namespace render {
namespace rhi {
namespace detail {

#ifdef HAS_FLYCUBE

// Compiled FlyCube program. Holds shaders, the binding layout, one PSO per
// requested depth variant, upload constant buffers, and one linear-clamp
// sampler when the layout declares a sampler slot.
class FlycubeProgram : public Pipeline {
 public:
  static std::unique_ptr<FlycubeProgram> compile_graphics(
      const std::shared_ptr<::Device>& device, const GraphicsPipelineDesc& desc,
      gli::format color_format, gli::format depth_format);
  static std::unique_ptr<FlycubeProgram> compile_compute(
      const std::shared_ptr<::Device>& device, const ComputePipelineDesc& desc);

  bool is_compute() const { return compute_; }

  // 0 when this program has no constant buffer at slot.
  uint32_t constant_slot_size(uint32_t slot) const;

  // False when the depth variant or a required view is missing.
  // |sampled| is set when the issued draw bound an SRV.
  bool replay_draw(::CommandList* list, const Draw& draw, bool pass_depth,
                   bool* sampled);
  bool replay_dispatch(::CommandList* list, const Dispatch& item);

 private:
  struct SlotBinding {
    BindingKind kind = BindingKind::kConstantBuffer;
    uint32_t slot = 0;
    BindKey key;
    uint32_t size_bytes = 0;
  };

  struct UploadCb {
    uint32_t slot = 0;
    uint32_t size_bytes = 0;
    std::shared_ptr<::Resource> buffer;
    std::shared_ptr<::View> view;
    // One upload allocation per draw in the open command list. A single
    // buffer is recorded by reference and the GPU reads it after Close, so
    // the last ColorCB write painted every solid the same color.
    std::vector<std::shared_ptr<::Resource>> ring_buffers;
    std::vector<std::shared_ptr<::View>> ring_views;
    uint32_t cursor = 0;
  };

  FlycubeProgram() = default;

  bool compile_shader(const ShaderSource& source, ::ShaderType type,
                      const char* tag, std::shared_ptr<::Shader>* out);
  bool attach_bindings(const BindingSlot* bindings, uint32_t count);
  std::shared_ptr<::Pipeline> make_graphics_pso(gli::format color,
                                                gli::format depth_format,
                                                bool use_depth,
                                                const ::DepthStencilDesc& depth,
                                                const ::BlendDesc& blend) const;
  std::shared_ptr<::Shader> shader_for(ShaderStage stage) const;
  const UploadCb* find_cb(uint32_t slot) const;
  UploadCb* find_cb_mut(uint32_t slot);
  void upload_slot(uint32_t slot, const void* data, uint32_t byte_size);
  std::shared_ptr<::View> take_cb_view(uint32_t slot, const void* data,
                                      uint32_t byte_size);
  std::shared_ptr<::Pipeline> select_depth(bool pass_depth, DepthMode mode) const;
  std::shared_ptr<::BindingSet> make_binding_set(
      const std::vector<TextureBind>& srvs,
      const std::vector<TextureBind>& uavs,
      const std::vector<std::pair<uint32_t, std::shared_ptr<::View>>>*
          cb_views) const;

  bool compute_ = false;
  int32_t camera_slot_ = -1;
  std::shared_ptr<::Device> device_;
  std::shared_ptr<::Shader> vs_;
  std::shared_ptr<::Shader> ps_;
  std::shared_ptr<::Shader> cs_;
  std::shared_ptr<::BindingSetLayout> layout_;
  std::vector<InputLayoutDesc> input_;
  // Color-only passes (no depth attach).
  std::shared_ptr<::Pipeline> depth_off_;
  // Depth attach present, test/write off — D3D12 requires the PSO depth
  // format to match the pass even when testing is disabled (sky / fog).
  std::shared_ptr<::Pipeline> depth_off_ds_;
  std::shared_ptr<::Pipeline> depth_write_;
  std::shared_ptr<::Pipeline> depth_test_;
  std::shared_ptr<::Pipeline> compute_pipeline_;
  std::shared_ptr<::Resource> sampler_;
  std::shared_ptr<::View> sampler_view_;
  std::vector<SlotBinding> bindings_;
  std::vector<UploadCb> constants_;
  uint64_t cb_epoch_seen_ = 0;
};

// Bump once per Execute, after the previous frame's fence wait, so the
// constant ring can reuse slots the GPU has finished reading.
void bump_flycube_constant_epoch();
uint64_t flycube_constant_epoch();

#endif  // HAS_FLYCUBE

}  // namespace detail
}  // namespace rhi
}  // namespace render

#endif  // RENDER_RHI_FLYCUBE_PIPELINE_PROGRAM_H_
