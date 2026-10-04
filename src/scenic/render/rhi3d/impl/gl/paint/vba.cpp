#include "base/core/log.h"
#include "scenic/render/rhi3d/impl/gl/resource/buffer/index_buffer.h"
#include "scenic/render/rhi3d/impl/gl/resource/buffer/vertex_buffer.h"
#include "scenic/render/rhi3d/impl/gl/host/render_device.h"

using namespace base;

namespace scenic {
namespace detail {
VertexBuffer* GlRenderDevice::CreateVertexBuffer(int nCount,
                                                       DWORD dwFormat,
                                                       bool bDynamic) {
  GlVertexBuffer* pVB = new GlVertexBuffer(nCount, dwFormat, bDynamic);

  return pVB;
}

IndexBuffer* GlRenderDevice::CreateIndexBuffer(int nCount) {
  GlIndexBuffer* pIndex = new GlIndexBuffer(nCount);

  return pIndex;
}

// vba
long GlRenderDevice::SetVertexArray(int components, Type type, int stride,
                                       void* data) {
  glVertexPointer(components, ConvertType(type), stride, data);

  return SMT_ERR_NONE;
}

long GlRenderDevice::SetTextureCoordsArray(int components, Type type,
                                              int stride, void* data) {
  glTexCoordPointer(components, ConvertType(type), stride, data);

  return SMT_ERR_NONE;
}

long GlRenderDevice::SetNormalArray(Type type, int stride, void* data) {
  glNormalPointer(ConvertType(type), stride, data);

  return SMT_ERR_NONE;
}

long GlRenderDevice::SetIndexArray(Type type, int stride, void* data) {
  glIndexPointer(ConvertType(type), stride, data);

  return SMT_ERR_NONE;
}

long GlRenderDevice::EnableArray(ArrayType type, bool enabled) {
  if (enabled == true) {
    glEnableClientState(ConvertArrayType(type));
  } else {
    glDisableClientState(ConvertArrayType(type));
  }

  return SMT_ERR_NONE;
}
}  // namespace detail
}  // namespace scenic