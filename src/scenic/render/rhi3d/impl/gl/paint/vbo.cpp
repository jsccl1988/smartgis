#include "base/core/log.h"
#include "scenic/render/rhi3d/impl/gl/host/render_device.h"

using namespace base;

namespace scenic {
namespace detail {
// video buffer
VideoBuffer *GlRenderDevice::CreateVideoBuffer(ArrayType type) {
  GLhandleARB newHandle;
  m_pFuncVBO->glGenBuffers(1, &newHandle);
  VideoBuffer *newVideoBuffer = new VideoBuffer(this, newHandle, type);

  return newVideoBuffer;
}

long GlRenderDevice::BindBuffer(VideoBuffer *buffer) {
  GLhandleARB handle = buffer->GetHandle();
  m_pFuncVBO->glBindBuffer(GL_ARRAY_BUFFER, handle);

  return SMT_ERR_NONE;
}

long GlRenderDevice::BindIndexBuffer(VideoBuffer *buffer) {
  GLhandleARB handle = buffer->GetHandle();
  m_pFuncVBO->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle);

  return SMT_ERR_NONE;
}

long GlRenderDevice::UnbindBuffer() {
  m_pFuncVBO->glBindBuffer(GL_ARRAY_BUFFER, 0);

  return SMT_ERR_NONE;
}

long GlRenderDevice::UnbindIndexBuffer() {
  m_pFuncVBO->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  return SMT_ERR_NONE;
}

long GlRenderDevice::UpdateBuffer(VideoBuffer *buffer, void *data,
                                     uint size, VideoBufferStoreMethod method) {
  if (nullptr == buffer) return SMT_ERR_INVALID_PARAM;

  BindBuffer(buffer);
  GLenum GLMethod = ConvertVideoBufferStoreMethod(method);
  m_pFuncVBO->glBufferData(GL_ARRAY_BUFFER, size, data, GLMethod);

  return SMT_ERR_NONE;
}

long GlRenderDevice::UpdateIndexBuffer(VideoBuffer *buffer, void *data,
                                          uint size,
                                          VideoBufferStoreMethod method) {
  if (nullptr == buffer || nullptr == data) return SMT_ERR_INVALID_PARAM;

  BindIndexBuffer(buffer);

  GLenum GLMethod = ConvertVideoBufferStoreMethod(method);
  m_pFuncVBO->glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, GLMethod);

  return SMT_ERR_NONE;
}

void *GlRenderDevice::MapBuffer(VideoBuffer *buffer, AccessMode access) {
  if (nullptr == buffer) return nullptr;

  GLhandleARB handle = buffer->GetHandle();
  GLenum glAccess = ConvertAccess(access);

  void *result = m_pFuncVBO->glMapBuffer(GL_ARRAY_BUFFER, glAccess);

  return result;
}

long GlRenderDevice::UnmapBuffer(VideoBuffer *buffer) {
  if (nullptr == buffer) return SMT_ERR_INVALID_PARAM;

  GLboolean result = m_pFuncVBO->glUnmapBuffer(GL_ARRAY_BUFFER);

  return SMT_ERR_NONE;
}

void *GlRenderDevice::MapIndexBuffer(VideoBuffer *buffer,
                                        AccessMode access) {
  if (nullptr == buffer) return nullptr;

  GLhandleARB handle = buffer->GetHandle();
  GLenum glAccess = ConvertAccess(access);

  void *result = m_pFuncVBO->glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, glAccess);

  return result;
}

long GlRenderDevice::UnmapIndexBuffer(VideoBuffer *buffer) {
  if (nullptr == buffer) return SMT_ERR_INVALID_PARAM;

  GLboolean result = m_pFuncVBO->glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

  return SMT_ERR_NONE;
}

long GlRenderDevice::DestroyBuffer(VideoBuffer *buffer) {
  GLhandleARB handle = buffer->GetHandle();
  m_pFuncVBO->glDeleteBuffers(1, &handle);

  return SMT_ERR_NONE;
}

long GlRenderDevice::DestroyIndexBuffer(VideoBuffer *buffer) {
  GLhandleARB handle = buffer->GetHandle();
  m_pFuncVBO->glDeleteBuffers(1, &handle);

  return SMT_ERR_NONE;
}
}  // namespace detail
}  // namespace scenic