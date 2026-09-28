// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI_IMPL_D3D_BUFFER_INDEXBUFFER_H_
#define LEGACY_RENDER_RHI_IMPL_D3D_BUFFER_INDEXBUFFER_H_

#include "legacy/render/rhi3d/impl/d3d/prerequisites.h"
#include "legacy/render/rhi3d/public/resource/indexbuffer.h"

namespace render {

// System-memory index buffer for leftover D3D11 path (GPU upload deferred).
class SmtD3DIndexBuffer : public SmtIndexBuffer {
 protected:
  SmtD3DIndexBuffer();
  SmtD3DIndexBuffer(const SmtD3DIndexBuffer&);
  SmtD3DIndexBuffer& operator=(const SmtD3DIndexBuffer&);

 public:
  explicit SmtD3DIndexBuffer(int count);
  ~SmtD3DIndexBuffer() override;

  long PrepareForDrawing() override;
  long EndDrawing() override;

  long Lock() override;
  long Unlock() override;
  bool IsLocked() const override { return locked_; }

  void* GetIndexData() override;
  ulong GetIndexCount() const override { return index_count_; }
  ulong GetIndexStride() const { return stride_; }

  void Index(uint index) override;

 private:
  bool locked_;
  ulong index_count_;
  ulong stride_;
  uint* cur_index_;
  uint* indices_;
};

}  // namespace render

#endif  // LEGACY_RENDER_RHI_IMPL_D3D_BUFFER_INDEXBUFFER_H_
