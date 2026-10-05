// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/flycube/command/recorder.h"
#include "render/rhi/flycube/pipeline/program.h"

#include <cstring>
#include <utility>

namespace render {
namespace rhi {
namespace detail {

#ifdef HAS_FLYCUBE

namespace {

void upsert_texture(std::vector<TextureBind>* list, uint32_t slot,
                    GpuTexture* texture) {
  for (TextureBind& bind : *list) {
    if (bind.slot == slot) {
      bind.texture = texture;
      return;
    }
  }
  list->push_back(TextureBind{slot, texture});
}

}  // namespace

Recorder::Recorder() {
  camera_view_[0] = camera_view_[5] = camera_view_[10] = camera_view_[15] = 1.f;
  camera_proj_[0] = camera_proj_[5] = camera_proj_[10] = camera_proj_[15] = 1.f;
}

bool Recorder::has_draws() const {
  for (const Pass& pass : passes) {
    if (!pass.draws.empty()) {
      return true;
    }
  }
  return false;
}

void Recorder::begin_pass(const PassDesc& desc) {
  Pass pass;
  pass.desc = desc;
  passes.push_back(std::move(pass));
}

void Recorder::bind_camera(const float view[16], const float proj[16]) {
  if (!view || !proj) {
    return;
  }
  std::memcpy(camera_view_, view, sizeof(camera_view_));
  std::memcpy(camera_proj_, proj, sizeof(camera_proj_));
}

void Recorder::set_pipeline(Pipeline* pipeline) { pipeline_ = pipeline; }

void Recorder::set_constants(uint32_t slot, const void* data,
                             uint32_t byte_size) {
  if (!data || byte_size == 0) {
    return;
  }
  const auto* bytes = static_cast<const uint8_t*>(data);
  for (ConstantBytes& existing : constants_) {
    if (existing.slot == slot) {
      existing.bytes.assign(bytes, bytes + byte_size);
      return;
    }
  }
  ConstantBytes created;
  created.slot = slot;
  created.bytes.assign(bytes, bytes + byte_size);
  constants_.push_back(std::move(created));
}

void Recorder::set_depth(DepthMode depth) { depth_ = depth; }

void Recorder::bind_vertex(GpuBuffer* buffer, uint32_t offset, uint32_t) {
  vb_ = buffer;
  vb_offset_ = offset;
}

void Recorder::bind_index(GpuBuffer* buffer, uint32_t offset) {
  ib_ = buffer;
  ib_offset_ = offset;
}

void Recorder::bind_texture(uint32_t slot, GpuTexture* texture) {
  upsert_texture(&textures_, slot, texture);
}

std::vector<ConstantBytes> Recorder::snapshot_constants() const {
  auto* program = dynamic_cast<FlycubeProgram*>(pipeline_);
  std::vector<ConstantBytes> out;
  for (const ConstantBytes& slot : constants_) {
    if (slot.bytes.empty()) {
      continue;
    }
    if (program) {
      const uint32_t declared = program->constant_slot_size(slot.slot);
      if (slot.bytes.size() > declared) {
        continue;
      }
    }
    out.push_back(slot);
  }
  return out;
}

void Recorder::draw_indexed(uint32_t index_count, uint32_t instance_count,
                            uint32_t first_index, int32_t vertex_offset,
                            uint32_t first_instance) {
  if (passes.empty()) {
    passes.push_back(Pass{});
  }
  Draw draw;
  draw.pipeline = pipeline_;
  draw.constants = snapshot_constants();
  draw.textures = textures_;
  std::memcpy(draw.camera_view, camera_view_, sizeof(draw.camera_view));
  std::memcpy(draw.camera_proj, camera_proj_, sizeof(draw.camera_proj));
  draw.depth = depth_;
  draw.vertex = vb_;
  draw.vb_offset = vb_offset_;
  draw.index = ib_;
  draw.ib_offset = ib_offset_;
  draw.index_count = index_count;
  draw.instance_count = instance_count;
  draw.first_index = first_index;
  draw.vertex_offset = vertex_offset;
  draw.first_instance = first_instance;
  passes.back().draws.push_back(std::move(draw));
}

void Recorder::bind_srv(uint32_t slot, GpuTexture* texture) {
  upsert_texture(&srvs_, slot, texture);
}

void Recorder::bind_uav(uint32_t slot, GpuTexture* texture) {
  upsert_texture(&uavs_, slot, texture);
}

void Recorder::dispatch(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) {
  Dispatch item;
  item.pipeline = pipeline_;
  item.constants = snapshot_constants();
  item.srvs = srvs_;
  item.uavs = uavs_;
  item.groups_x = groups_x;
  item.groups_y = groups_y;
  item.groups_z = groups_z;
  dispatches.push_back(std::move(item));
}

void Recorder::mark_barrier() {
  if (!dispatches.empty()) {
    dispatches.back().barrier_after = true;
  }
}

#endif  // HAS_FLYCUBE

}  // namespace detail
}  // namespace rhi
}  // namespace render
