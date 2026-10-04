// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_VIDEOBUFFER_H
#define _RD3D_VIDEOBUFFER_H

#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_defs.h"

namespace scenic {
namespace detail {
enum VideoBufferStoreMethod {
  STATIC_DRAW,
  STATIC_READ,
  STATIC_COPY,
  DYNAMIC_DRAW,
  DYNAMIC_READ,
  DYNAMIC_COPY,
  STREAM_DRAW,
  STREAM_READ,
  STREAM_COPY
};

enum ArrayType {
  TEXTURE_COORD_ARRAY, /*!< Texture coordinates array. */
  COLOR_ARRAY,         /*!< Color values array.        */
  INDEX_ARRAY,         /*!< Array of indices.          */
  NORMAL_ARRAY,        /*!< Array of normals.          */
  VERTEX_ARRAY         /*!< Vertex coordiantes array.  */
};

class RenderDevice3d;
typedef class RenderDevice3d *LP3DRENDERDEVICE;

class SCENIC_IMPL_EXPORT VideoBuffer {
 public:
  VideoBuffer(LP3DRENDERDEVICE p3DRenderDevice, uint handle, ArrayType type);
  virtual ~VideoBuffer();

 public:
  inline uint GetHandle() { return m_unHandle; }

  inline ArrayType GetArrayType() { return m_arType; }

  virtual long Use();
  virtual long Unuse();

  virtual long Update(void *data, unsigned int size,
                      VideoBufferStoreMethod method);

  virtual void *Map(AccessMode access);
  virtual long Unmap();

 protected:
  LP3DRENDERDEVICE m_p3DRenderDevice;
  uint m_unHandle;

  ArrayType m_arType;
};
}  // namespace detail
}  // namespace scenic

#if !defined(SCENIC_IMPL_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  //_RD3D_VIDEOBUFFER_H

// Bodies call RenderDevice3d. This header is included before that type is
// complete, so the bodies are emitted only from the re-include at the bottom
// of render_device.h.
#if defined(SCENIC_3DRENDERDEVICE_COMPLETE) && !defined(_RD3D_VIDEOBUFFER_METHODS)
#define _RD3D_VIDEOBUFFER_METHODS

namespace scenic {
namespace detail {

inline VideoBuffer::VideoBuffer(LP3DRENDERDEVICE p3DRenderDevice,
                                      uint handle, ArrayType type)
    : m_p3DRenderDevice(p3DRenderDevice), m_unHandle(handle), m_arType(type) {}

inline VideoBuffer::~VideoBuffer() { ; }

inline long VideoBuffer::Use() {
  if (m_arType == INDEX_ARRAY) {
    m_p3DRenderDevice->BindIndexBuffer(this);
  } else {
    m_p3DRenderDevice->BindBuffer(this);
  }

  return kErrNone;
}

inline long VideoBuffer::Unuse() {
  if (m_arType == INDEX_ARRAY) {
    m_p3DRenderDevice->UnbindIndexBuffer();
  } else {
    m_p3DRenderDevice->UnbindBuffer();
  }

  return kErrNone;
}

inline void *VideoBuffer::Map(AccessMode access) {
  if (m_arType == INDEX_ARRAY) {
    return m_p3DRenderDevice->MapIndexBuffer(this, access);
  } else {
    return m_p3DRenderDevice->MapBuffer(this, access);
  }
}

inline long VideoBuffer::Unmap() {
  if (m_arType == INDEX_ARRAY) {
    m_p3DRenderDevice->UnmapIndexBuffer(this);
  } else {
    m_p3DRenderDevice->UnmapBuffer(this);
  }
  return kErrNone;
}

inline long VideoBuffer::Update(void *data, unsigned int size,
                                   VideoBufferStoreMethod method) {
  if (m_arType == INDEX_ARRAY) {
    m_p3DRenderDevice->UpdateIndexBuffer(this, data, size, method);
  } else {
    m_p3DRenderDevice->UpdateBuffer(this, data, size, method);
  }
  return kErrNone;
}

}  // namespace detail
}  // namespace scenic

#endif  // _RD3D_VIDEOBUFFER_METHODS