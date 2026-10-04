// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _GLINDEXBUFFER_H
#define _GLINDEXBUFFER_H

#include "scenic/render/rhi3d/impl/gl/prerequisites.h"
#include "scenic/render/rhi3d/public/resource/index_buffer.h"

namespace scenic {
namespace detail {
class GlIndexBuffer : public IndexBuffer {
 protected:
  GlIndexBuffer();
  GlIndexBuffer(const GlIndexBuffer&);
  GlIndexBuffer& operator=(const GlIndexBuffer&);

 public:
  GlIndexBuffer(int count);
  ~GlIndexBuffer();

  long PrepareForDrawing();
  long EndDrawing();

  long Lock();
  long Unlock();
  bool IsLocked() const { return m_bLocked; }

  void* GetIndexData();
  ulong GetIndexCount() const { return m_dwIndexCount; }
  ulong GetIndexStride() const { return m_dwStrideIndex; }

  void Index(uint index);

 private:
  bool m_bLocked;  // flag to specify if buffer is locked

  ulong m_dwIndexCount;   // number of vertex in buffer
  ulong m_dwStrideIndex;  // stride of entire index data
  uint* m_pIndex;         // pointer to head of current vertex

  // OpenGL related members
  //--
  uint* m_pGLIndex;  // Buffer containing index data
};
}  // namespace detail
}  // namespace scenic

#endif  //_GLINDEXBUFFER_H