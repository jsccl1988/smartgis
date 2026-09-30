// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/d3d/resource/buffer/vertex_buffer.h"

#include "base/memory/arena.h"

namespace render {

namespace {

float* alloc_floats(size_t count) {
  if (count == 0) {
    return nullptr;
  }
  return static_cast<float*>(base::allocate(sizeof(float) * count));
}

void free_floats(float* p, size_t count) {
  if (!p || count == 0) {
    return;
  }
  base::deallocate(p, sizeof(float) * count);
}

}  // namespace

SmtD3DVertexBuffer::SmtD3DVertexBuffer(int count, ulong format, bool isDynamic)
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
    positions_ = alloc_floats(coord_num_ * vertex_count_);
  if (format_ & VF_NORMAL) normals_ = alloc_floats(3 * vertex_count_);
  if (format_ & VF_DIFFUSE) colors_ = alloc_floats(4 * vertex_count_);
  if (format_ & VF_TEXCOORD) texcoords_ = alloc_floats(2 * vertex_count_);
}

SmtD3DVertexBuffer::~SmtD3DVertexBuffer() {
  if (gpu_vb_) {
    gpu_vb_->Release();
    gpu_vb_ = nullptr;
  }
  free_floats(positions_, coord_num_ * vertex_count_);
  free_floats(normals_, 3 * vertex_count_);
  free_floats(colors_, 4 * vertex_count_);
  free_floats(texcoords_, 2 * vertex_count_);
  positions_ = nullptr;
  normals_ = nullptr;
  colors_ = nullptr;
  texcoords_ = nullptr;
}

long SmtD3DVertexBuffer::Lock() {
  locked_ = true;
  cur_vertex_ = positions_;
  cur_color_ = colors_;
  cur_normal_ = normals_;
  cur_texcoord_ = texcoords_;
  return SMT_ERR_NONE;
}

long SmtD3DVertexBuffer::Unlock() {
  locked_ = false;
  cur_vertex_ = nullptr;
  cur_color_ = nullptr;
  cur_normal_ = nullptr;
  cur_texcoord_ = nullptr;
  gpu_dirty_ = true;
  return SMT_ERR_NONE;
}

void* SmtD3DVertexBuffer::GetVertexData() { return positions_; }

void SmtD3DVertexBuffer::Vertex(float x, float y, float z) {
  cur_vertex_[0] = x;
  cur_vertex_[1] = y;
  cur_vertex_[2] = z;
  cur_vertex_ += 3;
}

void SmtD3DVertexBuffer::Vertex(float x, float y, float z, float w) {
  cur_vertex_[0] = x;
  cur_vertex_[1] = y;
  cur_vertex_[2] = z;
  cur_vertex_[3] = w;
  cur_vertex_ += 4;
}

void SmtD3DVertexBuffer::Normal(float x, float y, float z) {
  cur_normal_[0] = x;
  cur_normal_[1] = y;
  cur_normal_[2] = z;
  cur_normal_ += 3;
}

void SmtD3DVertexBuffer::Diffuse(float r, float g, float b, float a) {
  cur_color_[0] = r;
  cur_color_[1] = g;
  cur_color_[2] = b;
  cur_color_[3] = a;
  cur_color_ += 4;
}

void SmtD3DVertexBuffer::TexVertex(float u, float v) {
  cur_texcoord_[0] = u;
  cur_texcoord_[1] = v;
  cur_texcoord_ += 2;
}

long SmtD3DVertexBuffer::PrepareForDrawing() { return SMT_ERR_NONE; }

long SmtD3DVertexBuffer::EndDrawing() { return SMT_ERR_NONE; }

ID3D11Buffer* SmtD3DVertexBuffer::ensure_gpu_vb(ID3D11Device* device,
                                                const void* interleaved,
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

}  // namespace render
