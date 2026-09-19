#include "legacy/render/render3d/framebuffer.h"

#include "legacy/render/render3d/3drenderdevice.h"

namespace render {
SmtFrameBuffer::SmtFrameBuffer(LP3DRENDERDEVICE p3DRenderDevice, uint handle)
    : m_p3DRenderDevice(p3DRenderDevice), m_unHandle(handle) {}

SmtFrameBuffer::~SmtFrameBuffer() {}

long SmtFrameBuffer::Use() { return m_p3DRenderDevice->BindFrameBuffer(this); }

long SmtFrameBuffer::Unuse() { return m_p3DRenderDevice->UnbindFrameBuffer(); }

FrameBufferStatus SmtFrameBuffer::CheckStatus() {
  return m_p3DRenderDevice->CheckFrameBufferStatus();
}

long SmtFrameBuffer::AttachRenderBuffer(SmtRenderBuffer *renderBuffer,
                                        RenderBufferSlot slot) {
  return m_p3DRenderDevice->AttachRenderBuffer(this, renderBuffer, slot);
}

long SmtFrameBuffer::AttachTexture2D(SmtTexture *texture2D,
                                     RenderBufferSlot slot) {
  return m_p3DRenderDevice->AttachTexture(this, texture2D, slot);
}
}  // namespace render