// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI_IMPL_D3D_BUFFER_VERTEXBUFFER_H_
#define LEGACY_RENDER_RHI_IMPL_D3D_BUFFER_VERTEXBUFFER_H_

#include "legacy/render/rhi3d/impl/d3d/prerequisites.h"
#include "legacy/render/rhi3d/public/resource/vertex_buffer.h"

namespace render {

// System-memory vertex buffer for leftover D3D11 path (GPU upload deferred).
class SmtD3DVertexBuffer : public SmtVertexBuffer {
 protected:
  SmtD3DVertexBuffer();
  SmtD3DVertexBuffer(const SmtD3DVertexBuffer&);
  SmtD3DVertexBuffer& operator=(const SmtD3DVertexBuffer&);

 public:
  SmtD3DVertexBuffer(int count, ulong format, bool isDynamic = true);
  ~SmtD3DVertexBuffer() override;

  long PrepareForDrawing() override;
  long EndDrawing() override;

  long Lock() override;
  long Unlock() override;
  bool IsLocked() const override { return locked_; }

  void* GetVertexData() override;

  ulong GetVertexCount() const override { return vertex_count_; }
  ulong GetVertexFormat() const override { return format_; }
  ulong GetVertexStride() const override { return stride_; }

  void Vertex(float x, float y, float z) override;
  void Vertex(float x, float y, float z, float w) override;
  void Normal(float x, float y, float z) override;
  void Diffuse(float r, float g, float b, float a = 1.0f) override;
  void TexVertex(float u, float v) override;

  // CPU-side attribute arrays for GPU upload (leftover D3D draw path).
  const float* positions() const { return positions_; }
  const float* normals() const { return normals_; }
  const float* colors() const { return colors_; }
  const float* texcoords() const { return texcoords_; }

  // Cached interleaved GPU VB (MeshVertex). Invalidated on Unlock.
  bool gpu_dirty() const { return gpu_dirty_; }
  void mark_gpu_dirty() { gpu_dirty_ = true; }
  ID3D11Buffer* ensure_gpu_vb(ID3D11Device* device, const void* interleaved,
                              UINT bytes);

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
