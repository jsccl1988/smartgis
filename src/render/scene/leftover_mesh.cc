// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/scene/leftover_mesh.h"

#include "render/render3d/indexbuffer.h"
#include "render/render3d/vertexbuffer.h"

#include <vector>

namespace render {
namespace scene {
namespace {

// Packed leftover VB (VF_XYZ / VF_XYZRHW). No GL client arrays or device.
class HostVertexBuffer : public render::SmtVertexBuffer {
 public:
  HostVertexBuffer(int count, uint32_t format)
      : locked_(false),
        format_(format),
        count_(static_cast<ulong>(count)),
        coords_((format & render::VF_XYZRHW) ? 4u : 3u),
        cursor_(nullptr) {
    stride_ = coords_ * static_cast<ulong>(sizeof(float));
    positions_.assign(static_cast<size_t>(count_) * coords_, 0.f);
  }

  long Lock() override {
    locked_ = true;
    cursor_ = positions_.data();
    return SMT_ERR_NONE;
  }

  long Unlock() override {
    locked_ = false;
    cursor_ = nullptr;
    return SMT_ERR_NONE;
  }

  bool IsLocked() const override { return locked_; }

  void* GetVertexData() override { return positions_.data(); }

  ulong GetVertexCount() const override { return count_; }
  ulong GetVertexFormat() const override { return format_; }
  ulong GetVertexStride() const override { return stride_; }

  void Vertex(float x, float y, float z) override {
    if (!cursor_) {
      return;
    }
    cursor_[0] = x;
    cursor_[1] = y;
    cursor_[2] = z;
    cursor_ += 3;
  }

  void Vertex(float x, float y, float z, float w) override {
    if (!cursor_) {
      return;
    }
    cursor_[0] = x;
    cursor_[1] = y;
    cursor_[2] = z;
    cursor_[3] = w;
    cursor_ += 4;
  }

  void Normal(float, float, float) override {}
  void Diffuse(float, float, float, float) override {}
  void TexVertex(float, float) override {}

  long PrepareForDrawing() override { return SMT_ERR_NONE; }
  long EndDrawing() override { return SMT_ERR_NONE; }

 private:
  bool locked_;
  uint32_t format_;
  ulong count_;
  ulong coords_;
  ulong stride_;
  float* cursor_;
  std::vector<float> positions_;
};

// Packed leftover IB. Indices are leftover `uint` (32-bit on Win32/64).
class HostIndexBuffer : public render::SmtIndexBuffer {
 public:
  explicit HostIndexBuffer(int count)
      : locked_(false),
        count_(static_cast<ulong>(count)),
        cursor_(nullptr) {
    indices_.assign(static_cast<size_t>(count_), 0);
  }

  long Lock() override {
    locked_ = true;
    cursor_ = indices_.data();
    return SMT_ERR_NONE;
  }

  long Unlock() override {
    locked_ = false;
    cursor_ = nullptr;
    return SMT_ERR_NONE;
  }

  bool IsLocked() const override { return locked_; }

  void* GetIndexData() override { return indices_.data(); }

  ulong GetIndexCount() const override { return count_; }

  void Index(uint index) override {
    if (!cursor_) {
      return;
    }
    *cursor_ = index;
    ++cursor_;
  }

  long PrepareForDrawing() override { return SMT_ERR_NONE; }
  long EndDrawing() override { return SMT_ERR_NONE; }

 private:
  bool locked_;
  ulong count_;
  uint* cursor_;
  std::vector<uint> indices_;
};

bool upload_into(render::rhi::Device* device, const void* vb_data,
                 uint32_t vb_bytes, uint32_t stride, const void* ib_data,
                 uint32_t ib_bytes, uint32_t index_count,
                 LeftoverGpuMesh* out) {
  if (!device || !out || !vb_data || !ib_data || vb_bytes == 0 ||
      ib_bytes == 0 || index_count == 0 || stride == 0) {
    return false;
  }
  LeftoverGpuMesh mesh;
  mesh.vertex =
      device->create_buffer(vb_bytes, render::rhi::BufferUsage::kVertex);
  mesh.index = device->create_buffer(ib_bytes, render::rhi::BufferUsage::kIndex);
  if (!mesh.vertex || !mesh.index) {
    destroy_leftover_mesh(device, &mesh);
    return false;
  }
  if (!device->upload(mesh.vertex, vb_data, vb_bytes) ||
      !device->upload(mesh.index, ib_data, ib_bytes)) {
    destroy_leftover_mesh(device, &mesh);
    return false;
  }
  mesh.index_count = index_count;
  mesh.stride = stride;
  *out = mesh;
  return true;
}

}  // namespace

render::SmtVertexBuffer* create_host_vertex_buffer(int count,
                                                     uint32_t format) {
  if (count <= 0) {
    return nullptr;
  }
  const bool has_xyz = (format & render::VF_XYZ) != 0;
  const bool has_xyzw = (format & render::VF_XYZRHW) != 0;
  if (!has_xyz && !has_xyzw) {
    return nullptr;
  }
  return new HostVertexBuffer(count, format);
}

void destroy_host_vertex_buffer(render::SmtVertexBuffer* vb) { delete vb; }

render::SmtIndexBuffer* create_host_index_buffer(int count) {
  if (count <= 0) {
    return nullptr;
  }
  return new HostIndexBuffer(count);
}

void destroy_host_index_buffer(render::SmtIndexBuffer* ib) { delete ib; }

bool upload_leftover_buffers(render::rhi::Device* device,
                             render::SmtVertexBuffer* vb,
                             render::SmtIndexBuffer* ib,
                             LeftoverGpuMesh* out) {
  if (!device || !vb || !ib || !out) {
    return false;
  }
  const bool lock_v = !vb->IsLocked();
  const bool lock_i = !ib->IsLocked();
  if (lock_v && vb->Lock() != SMT_ERR_NONE) {
    return false;
  }
  if (lock_i && ib->Lock() != SMT_ERR_NONE) {
    if (lock_v) {
      vb->Unlock();
    }
    return false;
  }

  const void* vdata = vb->GetVertexData();
  const void* idata = ib->GetIndexData();
  const uint32_t vcount = static_cast<uint32_t>(vb->GetVertexCount());
  const uint32_t stride = static_cast<uint32_t>(vb->GetVertexStride());
  const uint32_t icount = static_cast<uint32_t>(ib->GetIndexCount());
  const uint32_t vb_bytes = vcount * stride;
  const uint32_t ib_bytes = icount * static_cast<uint32_t>(sizeof(uint));
  const bool ok = upload_into(device, vdata, vb_bytes, stride, idata, ib_bytes,
                              icount, out);

  if (lock_v) {
    vb->Unlock();
  }
  if (lock_i) {
    ib->Unlock();
  }
  return ok;
}

bool upload_xyz_mesh(render::rhi::Device* device, const float* xyz,
                     size_t xyz_floats, const uint32_t* indices,
                     size_t index_count, LeftoverGpuMesh* out) {
  if (!xyz || !indices || xyz_floats < 3 || (xyz_floats % 3) != 0) {
    return false;
  }
  const uint32_t stride = 3 * static_cast<uint32_t>(sizeof(float));
  const uint32_t vb_bytes =
      static_cast<uint32_t>(xyz_floats * sizeof(float));
  const uint32_t ib_bytes =
      static_cast<uint32_t>(index_count * sizeof(uint32_t));
  return upload_into(device, xyz, vb_bytes, stride, indices, ib_bytes,
                     static_cast<uint32_t>(index_count), out);
}

bool record_leftover_draw(render::rhi::CommandList* list,
                          const LeftoverGpuMesh& mesh) {
  if (!list || !mesh.vertex || !mesh.index || mesh.index_count == 0 ||
      mesh.stride == 0) {
    return false;
  }
  list->bind_vertex_buffer(mesh.vertex, 0, mesh.stride);
  list->bind_index_buffer(mesh.index, 0);
  list->draw_indexed(mesh.index_count, 1, 0, 0, 0);
  return true;
}

void destroy_leftover_mesh(render::rhi::Device* device, LeftoverGpuMesh* mesh) {
  if (!mesh) {
    return;
  }
  if (device) {
    device->destroy_buffer(mesh->vertex);
    device->destroy_buffer(mesh->index);
  }
  mesh->vertex = nullptr;
  mesh->index = nullptr;
  mesh->index_count = 0;
  mesh->stride = 0;
}

}  // namespace scene
}  // namespace render
