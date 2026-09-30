// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_VIDEOBUFFER_H
#define _RD3D_VIDEOBUFFER_H

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/render_defs.h"

namespace render {
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

class Smt3DRenderDevice;
typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

class LEGACY_RENDER_EXPORT SmtVideoBuffer {
 public:
  SmtVideoBuffer(LP3DRENDERDEVICE p3DRenderDevice, uint handle, ArrayType type);
  virtual ~SmtVideoBuffer();

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
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_RD3D_VIDEOBUFFER_H

// Bodies call Smt3DRenderDevice. This header is included before that type is
// complete, so the bodies are emitted only from the re-include at the bottom
// of render_device.h.
#if defined(SMT_3DRENDERDEVICE_COMPLETE) && !defined(_RD3D_VIDEOBUFFER_METHODS)
#define _RD3D_VIDEOBUFFER_METHODS

namespace render {

inline SmtVideoBuffer::SmtVideoBuffer(LP3DRENDERDEVICE p3DRenderDevice,
                                      uint handle, ArrayType type)
    : m_p3DRenderDevice(p3DRenderDevice), m_unHandle(handle), m_arType(type) {}

inline SmtVideoBuffer::~SmtVideoBuffer() { ; }

inline long SmtVideoBuffer::Use() {
  if (m_arType == INDEX_ARRAY) {
    m_p3DRenderDevice->BindIndexBuffer(this);
  } else {
    m_p3DRenderDevice->BindBuffer(this);
  }

  return SMT_ERR_NONE;
}

inline long SmtVideoBuffer::Unuse() {
  if (m_arType == INDEX_ARRAY) {
    m_p3DRenderDevice->UnbindIndexBuffer();
  } else {
    m_p3DRenderDevice->UnbindBuffer();
  }

  return SMT_ERR_NONE;
}

inline void *SmtVideoBuffer::Map(AccessMode access) {
  if (m_arType == INDEX_ARRAY) {
    return m_p3DRenderDevice->MapIndexBuffer(this, access);
  } else {
    return m_p3DRenderDevice->MapBuffer(this, access);
  }
}

inline long SmtVideoBuffer::Unmap() {
  if (m_arType == INDEX_ARRAY) {
    m_p3DRenderDevice->UnmapIndexBuffer(this);
  } else {
    m_p3DRenderDevice->UnmapBuffer(this);
  }
  return SMT_ERR_NONE;
}

inline long SmtVideoBuffer::Update(void *data, unsigned int size,
                                   VideoBufferStoreMethod method) {
  if (m_arType == INDEX_ARRAY) {
    m_p3DRenderDevice->UpdateIndexBuffer(this, data, size, method);
  } else {
    m_p3DRenderDevice->UpdateBuffer(this, data, size, method);
  }
  return SMT_ERR_NONE;
}

}  // namespace render

#endif  // _RD3D_VIDEOBUFFER_METHODS