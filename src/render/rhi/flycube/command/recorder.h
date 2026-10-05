// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_RHI_FLYCUBE_COMMAND_RECORDER_H_
#define RENDER_RHI_FLYCUBE_COMMAND_RECORDER_H_

#include "render/rhi/flycube/resource/gpu.h"
#include "render/rhi/rhi.h"

#include <cstdint>
#include <vector>

namespace render {
namespace rhi {
namespace detail {

#ifdef HAS_FLYCUBE

// One begin/end pass, in FlyCube terms. No RHI pass descriptor.
struct PassDesc {
  float clear_r = 0.f;
  float clear_g = 0.f;
  float clear_b = 0.f;
  float clear_a = 1.f;
  bool clear_color = true;
  bool enable_depth = false;
  bool clear_depth = true;
  float depth_clear = 1.f;
};

// Bytes for one constant slot. Copied at draw / dispatch time.
struct ConstantBytes {
  uint32_t slot = 0;
  std::vector<uint8_t> bytes;
};

// GpuTexture bound to a shader slot. The pointer is not owned.
struct TextureBind {
  uint32_t slot = 0;
  GpuTexture* texture = nullptr;
};

struct Draw {
  Pipeline* pipeline = nullptr;
  std::vector<ConstantBytes> constants;
  std::vector<TextureBind> textures;
  float camera_view[16] = {};
  float camera_proj[16] = {};
  DepthMode depth = DepthMode::kDisabled;
  GpuBuffer* vertex = nullptr;
  uint32_t vb_offset = 0;
  GpuBuffer* index = nullptr;
  uint32_t ib_offset = 0;
  uint32_t index_count = 0;
  uint32_t instance_count = 1;
  uint32_t first_index = 0;
  int32_t vertex_offset = 0;
  uint32_t first_instance = 0;
};

struct Dispatch {
  Pipeline* pipeline = nullptr;
  std::vector<ConstantBytes> constants;
  std::vector<TextureBind> srvs;
  std::vector<TextureBind> uavs;
  uint32_t groups_x = 1;
  uint32_t groups_y = 1;
  uint32_t groups_z = 1;
  bool barrier_after = false;
};

struct Pass {
  PassDesc desc;
  std::vector<Draw> draws;
};

// Records draws and dispatches against GpuBuffer / GpuTexture.
// The RHI command list translates into this before execute.
class Recorder {
 public:
  Recorder();

  std::vector<Pass> passes;
  std::vector<Dispatch> dispatches;

  bool had_pass() const { return !passes.empty(); }
  bool has_draws() const;
  bool has_dispatches() const { return !dispatches.empty(); }

  void begin_pass(const PassDesc& desc);
  void bind_camera(const float view[16], const float proj[16]);
  void set_pipeline(Pipeline* pipeline);
  void set_constants(uint32_t slot, const void* data, uint32_t byte_size);
  void set_depth(DepthMode depth);
  void bind_vertex(GpuBuffer* buffer, uint32_t offset, uint32_t stride);
  void bind_index(GpuBuffer* buffer, uint32_t offset);
  void bind_texture(uint32_t slot, GpuTexture* texture);
  void draw_indexed(uint32_t index_count, uint32_t instance_count,
                    uint32_t first_index, int32_t vertex_offset,
                    uint32_t first_instance);

  void bind_srv(uint32_t slot, GpuTexture* texture);
  void bind_uav(uint32_t slot, GpuTexture* texture);
  void dispatch(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z);
  void mark_barrier();

 private:
  // Drops a slot whose byte_size exceeds the bound program's declared size.
  std::vector<ConstantBytes> snapshot_constants() const;

  float camera_view_[16] = {};
  float camera_proj_[16] = {};
  Pipeline* pipeline_ = nullptr;
  DepthMode depth_ = DepthMode::kDisabled;
  std::vector<ConstantBytes> constants_;
  std::vector<TextureBind> textures_;
  GpuBuffer* vb_ = nullptr;
  uint32_t vb_offset_ = 0;
  GpuBuffer* ib_ = nullptr;
  uint32_t ib_offset_ = 0;
  std::vector<TextureBind> srvs_;
  std::vector<TextureBind> uavs_;
};

#endif  // HAS_FLYCUBE

}  // namespace detail
}  // namespace rhi
}  // namespace render

#endif  // RENDER_RHI_FLYCUBE_COMMAND_RECORDER_H_
