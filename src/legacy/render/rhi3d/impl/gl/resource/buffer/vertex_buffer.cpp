#include "legacy/render/rhi3d/impl/gl/resource/buffer/vertex_buffer.h"

#include "base/memory/arena.h"

namespace render {
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

SmtGLVertexBuffer::SmtGLVertexBuffer(int count, ulong format, bool isDynamic)
    : SmtVertexBuffer() {
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

  m_pVertex = NULL;
  m_pColor = NULL;
  m_pNormal = NULL;
  m_pTexCoord = NULL;

  m_pGLVertices = NULL;
  m_pGLNormals = NULL;
  m_pGLColors = NULL;
  m_pGLTexCoords = NULL;

  if ((m_dwFormat & VF_XYZ) || (m_dwFormat & VF_XYZRHW))
    m_pGLVertices = alloc_floats(m_dwVertexCoordNum * m_dwVertexCount);

  if (m_dwFormat & VF_NORMAL)
    m_pGLNormals = alloc_floats(3 * m_dwVertexCount);

  if (m_dwFormat & VF_DIFFUSE)
    m_pGLColors = alloc_floats(4 * m_dwVertexCount);

  if (m_dwFormat & VF_TEXCOORD)
    m_pGLTexCoords = alloc_floats(2 * m_dwVertexCount);
}

SmtGLVertexBuffer::~SmtGLVertexBuffer() {
  free_floats(m_pGLVertices, m_dwVertexCoordNum * m_dwVertexCount);
  free_floats(m_pGLNormals, 3 * m_dwVertexCount);
  free_floats(m_pGLColors, 4 * m_dwVertexCount);
  free_floats(m_pGLTexCoords, 2 * m_dwVertexCount);
  m_pGLVertices = NULL;
  m_pGLNormals = NULL;
  m_pGLColors = NULL;
  m_pGLTexCoords = NULL;
}

long SmtGLVertexBuffer::Lock() {
  m_bLocked = true;

  m_pVertex = m_pGLVertices;
  m_pColor = m_pGLColors;
  m_pNormal = m_pGLNormals;
  m_pTexCoord = m_pGLTexCoords;

  return SMT_ERR_NONE;
}

long SmtGLVertexBuffer::Unlock() {
  m_bLocked = false;

  m_pVertex = NULL;
  m_pColor = NULL;
  m_pNormal = NULL;
  m_pTexCoord = NULL;

  return SMT_ERR_NONE;
}

void *SmtGLVertexBuffer::GetVertexData() { return m_pGLVertices; }

void SmtGLVertexBuffer::Vertex(float x, float y, float z) {
  m_pVertex[0] = x;
  m_pVertex[1] = y;
  m_pVertex[2] = z;

  m_pVertex += 3;
}

void SmtGLVertexBuffer::Vertex(float x, float y, float z, float w) {
  m_pVertex[0] = x;
  m_pVertex[1] = y;
  m_pVertex[2] = z;
  m_pVertex[3] = w;

  m_pVertex += 4;
}

void SmtGLVertexBuffer::Normal(float x, float y, float z) {
  m_pNormal[0] = x;
  m_pNormal[1] = y;
  m_pNormal[2] = z;

  m_pNormal += 3;
}

void SmtGLVertexBuffer::Diffuse(float r, float g, float b, float a) {
  m_pColor[0] = r;
  m_pColor[1] = g;
  m_pColor[2] = b;
  m_pColor[3] = a;

  m_pColor += 4;
}

void SmtGLVertexBuffer::TexVertex(float u, float v) {
  m_pTexCoord[0] = u;
  m_pTexCoord[1] = v;

  m_pTexCoord += 2;
}

long SmtGLVertexBuffer::PrepareForDrawing() {
  // Set pointers to arrays
  //--
  if (m_pGLVertices != NULL) {
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(m_dwVertexCoordNum, GL_FLOAT, 0, m_pGLVertices);
  } else {
    glDisableClientState(GL_VERTEX_ARRAY);
  }

  if (m_pGLNormals != NULL) {
    glEnableClientState(GL_NORMAL_ARRAY);
    glNormalPointer(GL_FLOAT, 0, m_pGLNormals);
  } else {
    glDisableClientState(GL_NORMAL_ARRAY);
  }

  if (m_pGLColors != NULL) {
    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_FLOAT, 0, m_pGLColors);
  } else {
    glDisableClientState(GL_COLOR_ARRAY);
  }

  if (m_pGLTexCoords != NULL) {
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, m_pGLTexCoords);
  } else {
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  }

  return SMT_ERR_NONE;
}

long SmtGLVertexBuffer::EndDrawing() {
  if (m_pGLVertices != NULL) glDisableClientState(GL_VERTEX_ARRAY);

  if (m_pGLNormals != NULL) glDisableClientState(GL_NORMAL_ARRAY);

  if (m_pGLColors != NULL) glDisableClientState(GL_COLOR_ARRAY);

  if (m_pGLTexCoords != NULL) glDisableClientState(GL_TEXTURE_COORD_ARRAY);

  return SMT_ERR_NONE;
}
}  // namespace render