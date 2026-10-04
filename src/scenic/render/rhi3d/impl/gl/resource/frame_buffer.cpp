#include "base/core/log.h"
#include "scenic/render/rhi3d/impl/gl/host/render_device.h"
using namespace base;

namespace scenic {
namespace detail {
FrameBuffer *GlRenderDevice::CreateFrameBuffer() {
  GLhandleARB handle;
  m_pFuncFBO->glGenFramebuffers(1, &handle);
  FrameBuffer *newFB = new FrameBuffer(this, handle);

  return newFB;
}

long GlRenderDevice::DestroyFrameBuffer(FrameBuffer *frameBuffer) {
  if (frameBuffer) {
    GLhandleARB handle = frameBuffer->GetHandle();
    m_pFuncFBO->glDeleteFramebuffers(1, &handle);
  }

  return kErrNone;
}

long GlRenderDevice::BindFrameBuffer(FrameBuffer *frameBuffer) {
  if (frameBuffer) {
    GLhandleARB handle = frameBuffer->GetHandle();
    m_pFuncFBO->glBindFramebuffer(GL_FRAMEBUFFER_EXT, handle);
  }

  return kErrNone;
}

long GlRenderDevice::UnbindFrameBuffer() {
  m_pFuncFBO->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
  return kErrNone;
}

RenderBuffer *GlRenderDevice::CreateRenderBuffer(TextureFormat format,
                                                       uint width,
                                                       uint height) {
  GLhandleARB handle;
  GLenum rbFormat = ConvertTexFormat(format);

  m_pFuncFBO->glGenRenderbuffers(1, &handle);
  m_pFuncFBO->glBindRenderbuffer(GL_RENDERBUFFER_EXT, handle);
  m_pFuncFBO->glRenderbufferStorage(GL_RENDERBUFFER_EXT, rbFormat, width,
                                    height);

  RenderBuffer *pRenderBuf =
      new RenderBuffer(this, handle, format, width, height);

  return pRenderBuf;
}

long GlRenderDevice::DestroyRenderBuffer(RenderBuffer *renderBuffer) {
  GLhandleARB handle = renderBuffer->GetHandle();
  m_pFuncFBO->glDeleteRenderbuffers(1, &handle);
  return kErrNone;
}

long GlRenderDevice::AttachRenderBuffer(FrameBuffer *frameBuffer,
                                           RenderBuffer *renderBuffer,
                                           RenderBufferSlot slot) {
  if (!frameBuffer || !renderBuffer) {
    return kErrInvalidParam;
  }
  GLhandleARB frmHandle = frameBuffer->GetHandle();
  GLhandleARB rdhandle = renderBuffer->GetHandle();

  GLenum rbSlot = ConvertRenderBufferSlot(slot);
  if (rbSlot == static_cast<GLenum>(-1)) {
    return kErrInvalidParam;
  }
  // Attach targets the bound draw FBO; bind the explicit handle first.
  m_pFuncFBO->glBindFramebuffer(GL_FRAMEBUFFER_EXT, frmHandle);
  m_pFuncFBO->glFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT, rbSlot,
                                        GL_RENDERBUFFER_EXT, rdhandle);

  return kErrNone;
}

long GlRenderDevice::AttachTexture(FrameBuffer *frameBuffer,
                                      Texture *texture2D,
                                      RenderBufferSlot slot) {
  if (!frameBuffer || !texture2D) {
    return kErrInvalidParam;
  }
  GLenum rbSlot = ConvertRenderBufferSlot(slot);
  if (rbSlot == static_cast<GLenum>(-1)) {
    return kErrInvalidParam;
  }
  GLhandleARB id = texture2D->GetHandle();
  GLhandleARB frmHandle = frameBuffer->GetHandle();
  m_pFuncFBO->glBindFramebuffer(GL_FRAMEBUFFER_EXT, frmHandle);
  m_pFuncFBO->glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, rbSlot, GL_TEXTURE_2D,
                                     id, 0);
  return kErrNone;
}

FrameBufferStatus GlRenderDevice::CheckFrameBufferStatus() {
  GLenum result = m_pFuncFBO->glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT);
  switch (result) {
    case GL_FRAMEBUFFER_COMPLETE_EXT:
      return FRAMEBUFFER_COMPLETE;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
      return FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
      return FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
      return FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
      return FRAMEBUFFER_INCOMPLETE_READ_BUFFER;
    case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
      return FRAMEBUFFER_UNSUPPORTED;
    default:
      return FRAMEBUFFER_UNSUPPORTED;
  }
}

GLenum GlRenderDevice::ConvertRenderBufferSlot(RenderBufferSlot slot) {
  // COLOR_ATTACHMENT0 == 0; must accept index 0 (legacy used index > 0).
  static const GLenum map[] = {
      GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT,
      GL_COLOR_ATTACHMENT2_EXT, GL_COLOR_ATTACHMENT3_EXT,
      GL_COLOR_ATTACHMENT4_EXT, GL_COLOR_ATTACHMENT5_EXT,
      GL_COLOR_ATTACHMENT6_EXT, GL_COLOR_ATTACHMENT7_EXT,
      GL_DEPTH_ATTACHMENT_EXT,  GL_STENCIL_ATTACHMENT_EXT};

  const int index = static_cast<int>(slot);
  const int count = static_cast<int>(sizeof(map) / sizeof(map[0]));
  if (index >= 0 && index < count) {
    return map[index];
  }
  return static_cast<GLenum>(-1);
}
}  // namespace detail
}  // namespace scenic