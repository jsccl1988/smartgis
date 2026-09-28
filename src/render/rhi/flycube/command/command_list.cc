// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/flycube/command/command_list.h"
#include "render/rhi/flycube/resource/resources.h"

namespace render {
namespace rhi {
namespace detail {

#ifdef SMT_HAS_FLYCUBE

namespace {

PassDesc pass_desc_from(const RenderPassDesc& desc) {
  PassDesc out;
  out.clear_r = desc.clear_r;
  out.clear_g = desc.clear_g;
  out.clear_b = desc.clear_b;
  out.clear_a = desc.clear_a;
  out.clear_color = desc.load_op == ColorLoadOp::kClear;
  out.enable_depth = desc.enable_depth;
  out.clear_depth = desc.depth_load_op == DepthLoadOp::kClear;
  out.depth_clear = desc.depth_clear;
  return out;
}

}  // namespace

void FlycubeCommandList::begin_render_pass(const RenderPassDesc& desc) {
  recorder.begin_pass(pass_desc_from(desc));
  StubCommandList::begin_render_pass(desc);
}

void FlycubeCommandList::end_render_pass() {
  StubCommandList::end_render_pass();
}

void FlycubeCommandList::bind_camera(const CameraMatrices& matrices) {
  recorder.bind_camera(matrices.view, matrices.proj);
  StubCommandList::bind_camera(matrices);
}

void FlycubeCommandList::set_pipeline(Pipeline* pipeline) {
  recorder.set_pipeline(pipeline);
  StubCommandList::set_pipeline(pipeline);
}

void FlycubeCommandList::set_constants(uint32_t slot, const void* data,
                                       uint32_t byte_size) {
  recorder.set_constants(slot, data, byte_size);
  StubCommandList::set_constants(slot, data, byte_size);
}

void FlycubeCommandList::set_blend_mode(BlendMode mode) {
  // Blend is fixed on the PSO created with the pipeline. Do not switch it.
  StubCommandList::set_blend_mode(mode);
}

void FlycubeCommandList::set_depth_mode(DepthMode mode) {
  recorder.set_depth(mode);
  StubCommandList::set_depth_mode(mode);
}

void FlycubeCommandList::bind_compute_srv(Texture* texture, uint32_t slot) {
  recorder.bind_srv(slot, as_gpu_texture(texture));
  StubCommandList::bind_compute_srv(texture, slot);
}

void FlycubeCommandList::bind_compute_uav(Texture* texture, uint32_t slot) {
  recorder.bind_uav(slot, as_gpu_texture(texture));
  StubCommandList::bind_compute_uav(texture, slot);
}

void FlycubeCommandList::dispatch(uint32_t group_count_x, uint32_t group_count_y,
                                  uint32_t group_count_z) {
  recorder.dispatch(group_count_x, group_count_y, group_count_z);
  StubCommandList::dispatch(group_count_x, group_count_y, group_count_z);
}

void FlycubeCommandList::uav_barrier() {
  recorder.mark_barrier();
  StubCommandList::uav_barrier();
}

void FlycubeCommandList::bind_vertex_buffer(Buffer* buffer, uint32_t offset,
                                           uint32_t vertex_stride) {
  recorder.bind_vertex(as_gpu_buffer(buffer), offset, vertex_stride);
  StubCommandList::bind_vertex_buffer(buffer, offset, vertex_stride);
}

void FlycubeCommandList::bind_index_buffer(Buffer* buffer, uint32_t offset) {
  recorder.bind_index(as_gpu_buffer(buffer), offset);
  StubCommandList::bind_index_buffer(buffer, offset);
}

void FlycubeCommandList::bind_texture(Texture* texture, uint32_t slot) {
  recorder.bind_texture(slot, as_gpu_texture(texture));
  StubCommandList::bind_texture(texture, slot);
}

void FlycubeCommandList::draw_indexed(uint32_t index_count,
                                     uint32_t instance_count,
                                     uint32_t first_index,
                                     int32_t vertex_offset,
                                     uint32_t first_instance) {
  recorder.draw_indexed(index_count, instance_count, first_index, vertex_offset,
                        first_instance);
  StubCommandList::draw_indexed(index_count, instance_count, first_index,
                                vertex_offset, first_instance);
}

#endif  // SMT_HAS_FLYCUBE

}  // namespace detail
}  // namespace rhi
}  // namespace render
