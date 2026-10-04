// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_FRAMEBUFFER_H
#define _RD3D_FRAMEBUFFER_H

#include "scenic/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_defs.h"
#include "scenic/render/rhi3d/public/resource/render_buffer.h"

namespace scenic {
namespace detail {
enum RenderBufferSlot {
  COLOR_ATTACHMENT0 = 0,
  COLOR_ATTACHMENT1,
  COLOR_ATTACHMENT2,
  COLOR_ATTACHMENT3,
  COLOR_ATTACHMENT4,
  COLOR_ATTACHMENT5,
  COLOR_ATTACHMENT6,
  COLOR_ATTACHMENT7,
  DEPTH_ATTACHMENT,
  STENCIL_ATTACHMENT
};

enum FrameBufferStatus {
  FRAMEBUFFER_COMPLETE = 0,
  FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
  FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT,
  FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER,
  FRAMEBUFFER_INCOMPLETE_READ_BUFFER,
  FRAMEBUFFER_UNSUPPORTED
};

class RenderDevice3d;
typedef class RenderDevice3d *LP3DRENDERDEVICE;

class LEGACY_RENDER_EXPORT FrameBuffer {
 public:
  FrameBuffer(LP3DRENDERDEVICE p3DRenderDevice, uint handle);
  virtual ~FrameBuffer();

 public:
  inline uint GetHandle() { return m_unHandle; }

  long Use();
  long Unuse();

  FrameBufferStatus CheckStatus();
  long AttachRenderBuffer(RenderBuffer *renderBuffer, RenderBufferSlot slot);
  long AttachTexture2D(Texture *texture2D, RenderBufferSlot slot);

 protected:
  LP3DRENDERDEVICE m_p3DRenderDevice;
  uint m_unHandle;
};
}  // namespace detail
}  // namespace scenic

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  //_RD3D_FRAMEBUFFER_H

// Bodies call RenderDevice3d. This header is included before that type is
// complete, so the bodies are emitted only from the re-include at the bottom
// of render_device.h.
#if defined(SMT_3DRENDERDEVICE_COMPLETE) && !defined(_RD3D_FRAMEBUFFER_METHODS)
#define _RD3D_FRAMEBUFFER_METHODS

namespace scenic {
namespace detail {

inline FrameBuffer::FrameBuffer(LP3DRENDERDEVICE p3DRenderDevice,
                                      uint handle)
    : m_p3DRenderDevice(p3DRenderDevice), m_unHandle(handle) {}

inline FrameBuffer::~FrameBuffer() {}

inline long FrameBuffer::Use() {
  return m_p3DRenderDevice->BindFrameBuffer(this);
}

inline long FrameBuffer::Unuse() {
  return m_p3DRenderDevice->UnbindFrameBuffer();
}

inline FrameBufferStatus FrameBuffer::CheckStatus() {
  return m_p3DRenderDevice->CheckFrameBufferStatus();
}

inline long FrameBuffer::AttachRenderBuffer(RenderBuffer *renderBuffer,
                                               RenderBufferSlot slot) {
  return m_p3DRenderDevice->AttachRenderBuffer(this, renderBuffer, slot);
}

inline long FrameBuffer::AttachTexture2D(Texture *texture2D,
                                            RenderBufferSlot slot) {
  return m_p3DRenderDevice->AttachTexture(this, texture2D, slot);
}

}  // namespace detail
}  // namespace scenic

#endif  // _RD3D_FRAMEBUFFER_METHODS