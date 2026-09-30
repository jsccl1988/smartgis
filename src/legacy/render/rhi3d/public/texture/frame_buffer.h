// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_FRAMEBUFFER_H
#define _RD3D_FRAMEBUFFER_H

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/render_defs.h"
#include "legacy/render/rhi3d/public/resource/render_buffer.h"

namespace render {
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

class Smt3DRenderDevice;
typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

class LEGACY_RENDER_EXPORT SmtFrameBuffer {
 public:
  SmtFrameBuffer(LP3DRENDERDEVICE p3DRenderDevice, uint handle);
  virtual ~SmtFrameBuffer();

 public:
  inline uint GetHandle() { return m_unHandle; }

  long Use();
  long Unuse();

  FrameBufferStatus CheckStatus();
  long AttachRenderBuffer(SmtRenderBuffer *renderBuffer, RenderBufferSlot slot);
  long AttachTexture2D(SmtTexture *texture2D, RenderBufferSlot slot);

 protected:
  LP3DRENDERDEVICE m_p3DRenderDevice;
  uint m_unHandle;
};
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_RD3D_FRAMEBUFFER_H

// Bodies call Smt3DRenderDevice. This header is included before that type is
// complete, so the bodies are emitted only from the re-include at the bottom
// of render_device.h.
#if defined(SMT_3DRENDERDEVICE_COMPLETE) && !defined(_RD3D_FRAMEBUFFER_METHODS)
#define _RD3D_FRAMEBUFFER_METHODS

namespace render {

inline SmtFrameBuffer::SmtFrameBuffer(LP3DRENDERDEVICE p3DRenderDevice,
                                      uint handle)
    : m_p3DRenderDevice(p3DRenderDevice), m_unHandle(handle) {}

inline SmtFrameBuffer::~SmtFrameBuffer() {}

inline long SmtFrameBuffer::Use() {
  return m_p3DRenderDevice->BindFrameBuffer(this);
}

inline long SmtFrameBuffer::Unuse() {
  return m_p3DRenderDevice->UnbindFrameBuffer();
}

inline FrameBufferStatus SmtFrameBuffer::CheckStatus() {
  return m_p3DRenderDevice->CheckFrameBufferStatus();
}

inline long SmtFrameBuffer::AttachRenderBuffer(SmtRenderBuffer *renderBuffer,
                                               RenderBufferSlot slot) {
  return m_p3DRenderDevice->AttachRenderBuffer(this, renderBuffer, slot);
}

inline long SmtFrameBuffer::AttachTexture2D(SmtTexture *texture2D,
                                            RenderBufferSlot slot) {
  return m_p3DRenderDevice->AttachTexture(this, texture2D, slot);
}

}  // namespace render

#endif  // _RD3D_FRAMEBUFFER_METHODS