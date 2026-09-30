// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_INDEXBUFFER_H
#define _RD3D_INDEXBUFFER_H
#include "legacy/render/rhi3d/public/device/render_defs.h"

// Abstract index buffer over GL/D3D (and similar) backends.
namespace render {
class SmtIndexBuffer {
 public:
  virtual ~SmtIndexBuffer() {}

  virtual long Lock() = 0;
  virtual long Unlock() = 0;
  virtual bool IsLocked() const = 0;

  // Valid only while locked.
  virtual void *GetIndexData() = 0;

  virtual ulong GetIndexCount() const = 0;
  virtual void Index(uint index) = 0;

  virtual long PrepareForDrawing() = 0;
  virtual long EndDrawing() = 0;
};
}  // namespace render

#endif  //_RD3D_INDEXBUFFER_H
