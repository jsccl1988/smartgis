// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_RHI_FLYCUBE_COMMAND_COMMAND_LIST_H_
#define RENDER_RHI_FLYCUBE_COMMAND_COMMAND_LIST_H_

#include "render/rhi/flycube/command/recorder.h"
#include "render/rhi/rhi.h"

namespace render {
namespace rhi {
namespace detail {

#ifdef HAS_FLYCUBE

// RHI command list. Recording state lives in Recorder.
class FlycubeCommandList : public StubCommandList {
 public:
  Recorder recorder;

  bool had_pass() const { return recorder.had_pass(); }
  bool has_draws() const { return recorder.has_draws(); }
  bool has_dispatches() const { return recorder.has_dispatches(); }

  void begin_render_pass(const RenderPassDesc& desc) override;
  void end_render_pass() override;
  void bind_camera(const CameraMatrices& matrices) override;
  void set_pipeline(Pipeline* pipeline) override;
  void set_constants(uint32_t slot, const void* data, uint32_t byte_size) override;
  void set_blend_mode(BlendMode mode) override;
  void set_depth_mode(DepthMode mode) override;
  void bind_compute_srv(Texture* texture, uint32_t slot) override;
  void bind_compute_uav(Texture* texture, uint32_t slot) override;
  void dispatch(uint32_t group_count_x, uint32_t group_count_y,
                uint32_t group_count_z) override;
  void uav_barrier() override;
  void bind_vertex_buffer(Buffer* buffer, uint32_t offset,
                          uint32_t vertex_stride) override;
  void bind_index_buffer(Buffer* buffer, uint32_t offset) override;
  void bind_texture(Texture* texture, uint32_t slot) override;
  void draw_indexed(uint32_t index_count, uint32_t instance_count,
                    uint32_t first_index, int32_t vertex_offset,
                    uint32_t first_instance) override;
};

#endif  // HAS_FLYCUBE

}  // namespace detail
}  // namespace rhi
}  // namespace render

#endif  // RENDER_RHI_FLYCUBE_COMMAND_COMMAND_LIST_H_
