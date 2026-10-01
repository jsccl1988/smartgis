// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI_IMPL_D3D_BUFFER_VERTEXBUFFER_H_
#define LEGACY_RENDER_RHI_IMPL_D3D_BUFFER_VERTEXBUFFER_H_

#include <cstdlib>

#include "legacy/render/rhi3d/impl/d3d/prerequisites.h"
#include "legacy/render/rhi3d/public/resource/vertex_buffer.h"

namespace render {
namespace detail {

inline float* alloc_vb_floats(size_t count) {
  if (count == 0) {
    return nullptr;
  }
  // Prefer CRT heap for attribute arrays — leftover china line batches are
  // large enough that the TLS arena path has faulted under Debug.
  return static_cast<float*>(std::malloc(sizeof(float) * count));
}

inline void free_vb_floats(float* p, size_t count) {
  if (!p || count == 0) {
    return;
  }
  std::free(p);
}

}  // namespace detail

// System-memory vertex buffer for leftover D3D11 path (GPU upload deferred).
class SmtD3DVertexBuffer : public SmtVertexBuffer {
 protected:
  SmtD3DVertexBuffer();
  SmtD3DVertexBuffer(const SmtD3DVertexBuffer&);
  SmtD3DVertexBuffer& operator=(const SmtD3DVertexBuffer&);

 public:
  SmtD3DVertexBuffer(int count, ulong format, bool isDynamic = true)
      : locked_(false),
        dynamic_(isDynamic),
        vertex_count_(static_cast<ulong>(count)),
        format_(format),
        stride_(0),
        coord_num_(0),
        cur_vertex_(nullptr),
        cur_color_(nullptr),
        cur_normal_(nullptr),
        cur_texcoord_(nullptr),
        positions_(nullptr),
        colors_(nullptr),
        normals_(nullptr),
        texcoords_(nullptr) {
    ulong size = 0;
    if (format & VF_XYZ) {
      coord_num_ = 3;
      size += sizeof(float) * 3;
    } else if (format & VF_XYZRHW) {
      coord_num_ = 4;
      size += sizeof(float) * 4;
    }
    if (format & VF_NORMAL) size += sizeof(float) * 3;
    if (format & VF_DIFFUSE) size += sizeof(ulong);
    if (format & VF_TEXCOORD) size += sizeof(float) * 2;
    stride_ = size;

    if ((format_ & VF_XYZ) || (format_ & VF_XYZRHW))
      positions_ = detail::alloc_vb_floats(coord_num_ * vertex_count_);
    if (format_ & VF_NORMAL)
      normals_ = detail::alloc_vb_floats(3 * vertex_count_);
    if (format_ & VF_DIFFUSE)
      colors_ = detail::alloc_vb_floats(4 * vertex_count_);
    if (format_ & VF_TEXCOORD)
      texcoords_ = detail::alloc_vb_floats(2 * vertex_count_);
  }
  ~SmtD3DVertexBuffer() override {
    if (gpu_vb_) {
      gpu_vb_->Release();
      gpu_vb_ = nullptr;
    }
    detail::free_vb_floats(positions_, coord_num_ * vertex_count_);
    detail::free_vb_floats(normals_, 3 * vertex_count_);
    detail::free_vb_floats(colors_, 4 * vertex_count_);
    detail::free_vb_floats(texcoords_, 2 * vertex_count_);
    positions_ = nullptr;
    normals_ = nullptr;
    colors_ = nullptr;
    texcoords_ = nullptr;
  }

  long PrepareForDrawing() override { return SMT_ERR_NONE; }
  long EndDrawing() override { return SMT_ERR_NONE; }

  long Lock() override {
    locked_ = true;
    cur_vertex_ = positions_;
    cur_color_ = colors_;
    cur_normal_ = normals_;
    cur_texcoord_ = texcoords_;
    return SMT_ERR_NONE;
  }
  long Unlock() override {
    locked_ = false;
    cur_vertex_ = nullptr;
    cur_color_ = nullptr;
    cur_normal_ = nullptr;
    cur_texcoord_ = nullptr;
    gpu_dirty_ = true;
    return SMT_ERR_NONE;
  }
  bool IsLocked() const override { return locked_; }

  void* GetVertexData() override { return positions_; }

  ulong GetVertexCount() const override { return vertex_count_; }
  ulong GetVertexFormat() const override { return format_; }
  ulong GetVertexStride() const override { return stride_; }

  void Vertex(float x, float y, float z) override {
    cur_vertex_[0] = x;
    cur_vertex_[1] = y;
    cur_vertex_[2] = z;
    cur_vertex_ += 3;
  }
  void Vertex(float x, float y, float z, float w) override {
    cur_vertex_[0] = x;
    cur_vertex_[1] = y;
    cur_vertex_[2] = z;
    cur_vertex_[3] = w;
    cur_vertex_ += 4;
  }
  void Normal(float x, float y, float z) override {
    cur_normal_[0] = x;
    cur_normal_[1] = y;
    cur_normal_[2] = z;
    cur_normal_ += 3;
  }
  void Diffuse(float r, float g, float b, float a = 1.0f) override {
    cur_color_[0] = r;
    cur_color_[1] = g;
    cur_color_[2] = b;
    cur_color_[3] = a;
    cur_color_ += 4;
  }
  void TexVertex(float u, float v) override {
    cur_texcoord_[0] = u;
    cur_texcoord_[1] = v;
    cur_texcoord_ += 2;
  }

  // CPU-side attribute arrays for GPU upload (leftover D3D draw path).
  const float* positions() const { return positions_; }
  const float* normals() const { return normals_; }
  const float* colors() const { return colors_; }
  const float* texcoords() const { return texcoords_; }

  // Cached interleaved GPU VB (MeshVertex). Invalidated on Unlock.
  bool gpu_dirty() const { return gpu_dirty_; }
  void mark_gpu_dirty() { gpu_dirty_ = true; }
  ID3D11Buffer* ensure_gpu_vb(ID3D11Device* device, const void* interleaved,
                              UINT bytes) {
    if (gpu_vb_ && !gpu_dirty_ &&
        (bytes == 0 || gpu_vb_bytes_ == bytes)) {
      return gpu_vb_;
    }
    if (!device || !interleaved || bytes == 0) {
      return nullptr;
    }
    if (gpu_vb_) {
      gpu_vb_->Release();
      gpu_vb_ = nullptr;
      gpu_vb_bytes_ = 0;
    }
    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth = bytes;
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = interleaved;
    if (FAILED(device->CreateBuffer(&vbd, &init, &gpu_vb_)) || !gpu_vb_) {
      gpu_vb_ = nullptr;
      return nullptr;
    }
    gpu_vb_bytes_ = bytes;
    gpu_dirty_ = false;
    return gpu_vb_;
  }

 private:
  bool locked_;
  bool dynamic_;
  ulong vertex_count_;
  ulong format_;
  ulong stride_;
  ulong coord_num_;

  float* cur_vertex_;
  float* cur_color_;
  float* cur_normal_;
  float* cur_texcoord_;

  float* positions_;
  float* colors_;
  float* normals_;
  float* texcoords_;

  ID3D11Buffer* gpu_vb_ = nullptr;
  UINT gpu_vb_bytes_ = 0;
  bool gpu_dirty_ = true;
};

}  // namespace render

#endif  // LEGACY_RENDER_RHI_IMPL_D3D_BUFFER_VERTEXBUFFER_H_
