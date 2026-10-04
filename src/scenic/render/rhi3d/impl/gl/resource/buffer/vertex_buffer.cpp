// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi3d/impl/gl/resource/buffer/vertex_buffer.h"

#include "base/memory/arena.h"

namespace scenic {
namespace detail {
namespace {

float* alloc_floats(size_t count) {
  if (count == 0) {
    return nullptr;
  }
  return static_cast<float*>(base::allocate(sizeof(float) * count));
}

void free_floats(float* p, size_t count) {
  if (!p || count == 0) {
    return;
  }
  base::deallocate(p, sizeof(float) * count);
}

}  // namespace

GlVertexBuffer::GlVertexBuffer(int count, ulong format, bool isDynamic)
    : VertexBuffer() {
  ulong size = 0;

  m_dwFormat = format;
  m_dwVertexCount = count;

  if (format & VF_XYZ) {
    m_dwVertexCoordNum = 3;
    size += sizeof(float) * 3;
  } else if (format & VF_XYZRHW) {
    m_dwVertexCoordNum = 4;
    size += sizeof(float) * 4;
  }

  if (format & VF_NORMAL) {
    size += sizeof(float) * 3;
  }

  if (format & VF_DIFFUSE) {
    size += sizeof(ulong);
  }

  if (format & VF_TEXCOORD) {
    size += sizeof(float) * 2;
  }

  m_dwStrideVertex = size;
  m_bDynamic = isDynamic;
  m_bLocked = false;

  m_pVertex = nullptr;
  m_pColor = nullptr;
  m_pNormal = nullptr;
  m_pTexCoord = nullptr;

  m_pGLVertices = nullptr;
  m_pGLNormals = nullptr;
  m_pGLColors = nullptr;
  m_pGLTexCoords = nullptr;

  if ((m_dwFormat & VF_XYZ) || (m_dwFormat & VF_XYZRHW))
    m_pGLVertices = alloc_floats(m_dwVertexCoordNum * m_dwVertexCount);

  if (m_dwFormat & VF_NORMAL)
    m_pGLNormals = alloc_floats(3 * m_dwVertexCount);

  if (m_dwFormat & VF_DIFFUSE)
    m_pGLColors = alloc_floats(4 * m_dwVertexCount);

  if (m_dwFormat & VF_TEXCOORD)
    m_pGLTexCoords = alloc_floats(2 * m_dwVertexCount);
}

GlVertexBuffer::~GlVertexBuffer() {
  free_floats(m_pGLVertices, m_dwVertexCoordNum * m_dwVertexCount);
  free_floats(m_pGLNormals, 3 * m_dwVertexCount);
  free_floats(m_pGLColors, 4 * m_dwVertexCount);
  free_floats(m_pGLTexCoords, 2 * m_dwVertexCount);
  m_pGLVertices = nullptr;
  m_pGLNormals = nullptr;
  m_pGLColors = nullptr;
  m_pGLTexCoords = nullptr;
}

long GlVertexBuffer::Lock() {
  m_bLocked = true;

  m_pVertex = m_pGLVertices;
  m_pColor = m_pGLColors;
  m_pNormal = m_pGLNormals;
  m_pTexCoord = m_pGLTexCoords;

  return SMT_ERR_NONE;
}

long GlVertexBuffer::Unlock() {
  m_bLocked = false;

  m_pVertex = nullptr;
  m_pColor = nullptr;
  m_pNormal = nullptr;
  m_pTexCoord = nullptr;

  return SMT_ERR_NONE;
}

void *GlVertexBuffer::GetVertexData() { return m_pGLVertices; }

void GlVertexBuffer::Vertex(float x, float y, float z) {
  m_pVertex[0] = x;
  m_pVertex[1] = y;
  m_pVertex[2] = z;

  m_pVertex += 3;
}

void GlVertexBuffer::Vertex(float x, float y, float z, float w) {
  m_pVertex[0] = x;
  m_pVertex[1] = y;
  m_pVertex[2] = z;
  m_pVertex[3] = w;

  m_pVertex += 4;
}

void GlVertexBuffer::Normal(float x, float y, float z) {
  m_pNormal[0] = x;
  m_pNormal[1] = y;
  m_pNormal[2] = z;

  m_pNormal += 3;
}

void GlVertexBuffer::Diffuse(float r, float g, float b, float a) {
  m_pColor[0] = r;
  m_pColor[1] = g;
  m_pColor[2] = b;
  m_pColor[3] = a;

  m_pColor += 4;
}

void GlVertexBuffer::TexVertex(float u, float v) {
  m_pTexCoord[0] = u;
  m_pTexCoord[1] = v;

  m_pTexCoord += 2;
}

long GlVertexBuffer::PrepareForDrawing() {
  // Set pointers to arrays
  //--
  if (m_pGLVertices) {
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(m_dwVertexCoordNum, GL_FLOAT, 0, m_pGLVertices);
  } else {
    glDisableClientState(GL_VERTEX_ARRAY);
  }

  if (m_pGLNormals) {
    glEnableClientState(GL_NORMAL_ARRAY);
    glNormalPointer(GL_FLOAT, 0, m_pGLNormals);
  } else {
    glDisableClientState(GL_NORMAL_ARRAY);
  }

  if (m_pGLColors) {
    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_FLOAT, 0, m_pGLColors);
  } else {
    glDisableClientState(GL_COLOR_ARRAY);
  }

  if (m_pGLTexCoords) {
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, m_pGLTexCoords);
  } else {
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  }

  return SMT_ERR_NONE;
}

long GlVertexBuffer::EndDrawing() {
  if (m_pGLVertices) glDisableClientState(GL_VERTEX_ARRAY);

  if (m_pGLNormals) glDisableClientState(GL_NORMAL_ARRAY);

  if (m_pGLColors) glDisableClientState(GL_COLOR_ARRAY);

  if (m_pGLTexCoords) glDisableClientState(GL_TEXTURE_COORD_ARRAY);

  return SMT_ERR_NONE;
}
}  // namespace detail
}  // namespace scenic