// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI_IMPL_D3D_BUFFER_INDEXBUFFER_H_
#define LEGACY_RENDER_RHI_IMPL_D3D_BUFFER_INDEXBUFFER_H_

#include "base/memory/arena.h"
#include "scenic/render/rhi3d/impl/d3d/prerequisites.h"
#include "scenic/render/rhi3d/public/resource/index_buffer.h"

namespace scenic {
namespace detail {

inline uint* alloc_ib_uints(ulong count) {
  if (count == 0) {
    return nullptr;
  }
  return static_cast<uint*>(base::allocate(sizeof(uint) * count));
}

inline void free_ib_uints(uint* p, ulong count) {
  if (!p || count == 0) {
    return;
  }
  base::deallocate(p, sizeof(uint) * count);
}

}  // namespace detail

namespace detail {

// System-memory index buffer for leftover D3D11 path (GPU upload deferred).
class D3dIndexBuffer : public IndexBuffer {
 protected:
  D3dIndexBuffer();
  D3dIndexBuffer(const D3dIndexBuffer&);
  D3dIndexBuffer& operator=(const D3dIndexBuffer&);

 public:
  explicit D3dIndexBuffer(int count)
      : locked_(false),
        index_count_(static_cast<ulong>(count)),
        stride_(sizeof(uint)),
        cur_index_(nullptr),
        indices_(alloc_ib_uints(static_cast<ulong>(count))) {}
  ~D3dIndexBuffer() override {
    if (gpu_ib_) {
      gpu_ib_->Release();
      gpu_ib_ = nullptr;
    }
    free_ib_uints(indices_, index_count_);
    indices_ = nullptr;
  }

  long PrepareForDrawing() override { return kErrNone; }
  long EndDrawing() override { return kErrNone; }

  long Lock() override {
    locked_ = true;
    cur_index_ = indices_;
    return kErrNone;
  }
  long Unlock() override {
    locked_ = false;
    cur_index_ = nullptr;
    gpu_dirty_ = true;
    return kErrNone;
  }
  bool IsLocked() const override { return locked_; }

  void* GetIndexData() override { return indices_; }
  ulong GetIndexCount() const override { return index_count_; }
  ulong GetIndexStride() const { return stride_; }

  void Index(uint index) override {
    *cur_index_ = index;
    ++cur_index_;
  }

  const uint* indices() const { return indices_; }

  bool gpu_dirty() const { return gpu_dirty_; }
  void mark_gpu_dirty() { gpu_dirty_ = true; }
  ID3D11Buffer* ensure_gpu_ib(ID3D11Device* device, const void* indices,
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

 private:
  bool locked_;
  ulong index_count_;
  ulong stride_;
  uint* cur_index_;
  uint* indices_;

  ID3D11Buffer* gpu_ib_ = nullptr;
  UINT gpu_ib_bytes_ = 0;
  bool gpu_dirty_ = true;
};

}  // namespace detail
}  // namespace scenic

#endif  // LEGACY_RENDER_RHI_IMPL_D3D_BUFFER_INDEXBUFFER_H_
