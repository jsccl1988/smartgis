// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/d3d/resource/buffer/index_buffer.h"

namespace render {

SmtD3DIndexBuffer::SmtD3DIndexBuffer(int count)
    : locked_(false),
      index_count_(static_cast<ulong>(count)),
      stride_(sizeof(uint)),
      cur_index_(nullptr),
      indices_(new uint[static_cast<ulong>(count)]) {}

SmtD3DIndexBuffer::~SmtD3DIndexBuffer() {
  if (gpu_ib_) {
    gpu_ib_->Release();
    gpu_ib_ = nullptr;
  }
  delete[] indices_;
}

long SmtD3DIndexBuffer::Lock() {
  locked_ = true;
  cur_index_ = indices_;
  return SMT_ERR_NONE;
}

long SmtD3DIndexBuffer::Unlock() {
  locked_ = false;
  cur_index_ = nullptr;
  gpu_dirty_ = true;
  return SMT_ERR_NONE;
}

void* SmtD3DIndexBuffer::GetIndexData() { return indices_; }

void SmtD3DIndexBuffer::Index(uint index) {
  *cur_index_ = index;
  ++cur_index_;
}

long SmtD3DIndexBuffer::PrepareForDrawing() { return SMT_ERR_NONE; }

long SmtD3DIndexBuffer::EndDrawing() { return SMT_ERR_NONE; }

ID3D11Buffer* SmtD3DIndexBuffer::ensure_gpu_ib(ID3D11Device* device,
                                               const void* indices,
                                               UINT bytes) {
  if (gpu_ib_ && !gpu_dirty_ &&
      (bytes == 0 || gpu_ib_bytes_ == bytes)) {
    return gpu_ib_;
  }
  if (!device || !indices || bytes == 0) {
    return nullptr;
  }
  if (gpu_ib_) {
    gpu_ib_->Release();
    gpu_ib_ = nullptr;
    gpu_ib_bytes_ = 0;
  }
  D3D11_BUFFER_DESC ibd = {};
  ibd.ByteWidth = bytes;
  ibd.Usage = D3D11_USAGE_DEFAULT;
  ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
  D3D11_SUBRESOURCE_DATA init = {};
  init.pSysMem = indices;
  if (FAILED(device->CreateBuffer(&ibd, &init, &gpu_ib_)) || !gpu_ib_) {
    gpu_ib_ = nullptr;
    return nullptr;
  }
  gpu_ib_bytes_ = bytes;
  gpu_dirty_ = false;
  return gpu_ib_;
}

}  // namespace render
