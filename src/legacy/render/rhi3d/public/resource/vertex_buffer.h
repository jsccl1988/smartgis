// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_VTERTEXBUFFER_H
#define _RD3D_VTERTEXBUFFER_H
#include "legacy/render/rhi3d/public/device/render_defs.h"

// Abstract vertex buffer over GL/D3D (and similar) backends.
namespace render {
class SmtVertexBuffer {
 public:
  virtual ~SmtVertexBuffer() {}

  virtual long Lock() = 0;
  virtual long Unlock() = 0;
  virtual bool IsLocked() const = 0;

  // Valid only while locked.
  virtual void *GetVertexData() = 0;

  virtual ulong GetVertexCount() const = 0;
  virtual ulong GetVertexFormat() const = 0;
  virtual ulong GetVertexStride() const = 0;

  // Last per-vertex write (after Normal/Diffuse/TexVertex). FORMAT_XYZ.
  virtual void Vertex(float x, float y, float z) = 0;
  // Last per-vertex write. FORMAT_XYZRHW.
  virtual void Vertex(float x, float y, float z, float w) = 0;

  // FORMAT_NORMAL / FORMAT_DIFFUSE / FORMAT_TEXTUREFLAG respectively.
  virtual void Normal(float x, float y, float z) = 0;
  virtual void Diffuse(float r, float g, float b, float a = 1.0f) = 0;
  // Call once per texture coordinate set when multi-texturing.
  virtual void TexVertex(float u, float v) = 0;

  virtual long PrepareForDrawing() = 0;
  virtual long EndDrawing() = 0;
};
}  // namespace render

#endif  //_RD3D_VTERTEXBUFFER_H
