#include "base/core/log.h"
#include "legacy/render/gl/gl_3drenderdevice.h"
#include "legacy/render/gl/gl_indexbuffer.h"
#include "legacy/render/gl/gl_text.h"
#include "legacy/render/gl/gl_vertexbuffer.h"

using namespace base;

namespace render {
//////////////////////////////////////////////////////////////////////////
// Rendering functions
long SmtGLRenderDevice::BeginRender() {
  if (!m_hWnd || !m_hRC) {
    return SMT_ERR_FAILURE;
  }
  if (!m_hPaintDC) {
    m_hPaintDC = ::GetDC(m_hWnd);
  }
  if (!m_hPaintDC) {
    return SMT_ERR_FAILURE;
  }
  // Keep the DC for the whole frame. ReleaseDC while the RC is current
  // leaves later glDraw*/SwapBuffers on a stale HDC.
  return wglMakeCurrent(m_hPaintDC, m_hRC) ? SMT_ERR_NONE : SMT_ERR_FAILURE;
}

long SmtGLRenderDevice::EndRender() {
  ::glFlush();
  return SMT_ERR_NONE;
}

long SmtGLRenderDevice::SwapBuffers() {
  HDC hDC = m_hPaintDC ? m_hPaintDC : ::GetDC(m_hWnd);
  if (hDC) {
    ::SwapBuffers(hDC);
    if (!m_hPaintDC) {
      ::ReleaseDC(m_hWnd, hDC);
    }
  }
  return SMT_ERR_NONE;
}

long SmtGLRenderDevice::DrawPrimitives(PrimitiveType primitiveType,
                                       SmtVertexBuffer* pVB, DWORD baseVertex,
                                       DWORD primitiveCount) {
  if (pVB == NULL) return SMT_ERR_INVALID_PARAM;

  // Convert primitive type
  GLenum PT;
  ulong count;
  if (SMT_ERR_NONE !=
      GetOpenGLPrimitiveType(primitiveType, primitiveCount, &PT, &count))
    return SMT_ERR_FAILURE;

  // Say that the VB will be the source for our draw primitive calls
  //--
  if (SMT_ERR_NONE != pVB->PrepareForDrawing()) return SMT_ERR_FAILURE;

  // Draw primitives
  //--
  glDrawArrays(PT, baseVertex, count);

  if (SMT_ERR_NONE != pVB->EndDrawing()) return SMT_ERR_FAILURE;

  return SMT_ERR_NONE;
}

long SmtGLRenderDevice::DrawIndexedPrimitives(PrimitiveType primitiveType,
                                              SmtVertexBuffer* pVB,
                                              SmtIndexBuffer* pIB,
                                              ulong baseIndex,
                                              ulong primitiveCount) {
  if (pVB == NULL || pIB == NULL) return SMT_ERR_INVALID_PARAM;

  // Convert primitive type
  GLenum PT;
  ulong count;
  if (SMT_ERR_NONE !=
      GetOpenGLPrimitiveType(primitiveType, primitiveCount, &PT, &count))
    return SMT_ERR_FAILURE;

  // Say that the VB will be the source for our draw primitive calls
  //--
  if (SMT_ERR_NONE != pVB->PrepareForDrawing() ||
      SMT_ERR_NONE != pIB->PrepareForDrawing())
    return SMT_ERR_FAILURE;

  // Draw primitives
  //--
  const void* indices = pIB->GetIndexData();
  if (!indices) {
    return SMT_ERR_FAILURE;
  }
  // Do not open leftover_session here. The process-wide recorder may
  // already own a D3D/FlyCube device on another HWND; begin/record_3d
  // mid-GL-frame has caused STATUS_FATAL_APP_EXIT (0xC000041D).
  glDrawElements(PT, count, GL_UNSIGNED_INT, indices);

  if (SMT_ERR_NONE != pVB->EndDrawing() || SMT_ERR_NONE != pIB->EndDrawing())
    return SMT_ERR_FAILURE;

  return SMT_ERR_NONE;
}

inline long SmtGLRenderDevice::GetOpenGLPrimitiveType(
    const PrimitiveType pt, const ulong nInitialPrimitiveCount,
    GLenum* GLPrimitiveType, ulong* nGLPrimitiveCount) {
  switch (pt) {
    case PT_POINTLIST:
      *GLPrimitiveType = GL_POINTS;
      *nGLPrimitiveCount = nInitialPrimitiveCount;
      return SMT_ERR_NONE;

    case PT_LINELIST:
      *GLPrimitiveType = GL_LINES;
      *nGLPrimitiveCount = 2 * nInitialPrimitiveCount;
      return SMT_ERR_NONE;

    case PT_LINESTRIP:
      *GLPrimitiveType = GL_LINE_STRIP;
      *nGLPrimitiveCount = nInitialPrimitiveCount;
      return SMT_ERR_NONE;

    case PT_TRIANGLELIST:
      *GLPrimitiveType = GL_TRIANGLES;
      *nGLPrimitiveCount = 3 * nInitialPrimitiveCount;
      return SMT_ERR_NONE;

    case PT_TRIANGLESTRIP:
      *GLPrimitiveType = GL_TRIANGLE_STRIP;
      *nGLPrimitiveCount = nInitialPrimitiveCount + 2;
      return SMT_ERR_NONE;

    case PT_TRIANGLEFAN:
      *GLPrimitiveType = GL_TRIANGLE_FAN;
      *nGLPrimitiveCount = nInitialPrimitiveCount + 2;
      return SMT_ERR_NONE;

    default:
      *GLPrimitiveType = GL_POINTS;
      *nGLPrimitiveCount = nInitialPrimitiveCount;
      return SMT_ERR_NONE;
  }

  return SMT_ERR_FAILURE;
}

long SmtGLRenderDevice::DrawText(uint unID, float x, float y, float z,
                                 const SmtColor& color, const char* str, ...) {
  if (str == NULL || unID > m_vTextPtrs.size()) return SMT_ERR_INVALID_PARAM;

  SmtGLText* pText = m_vTextPtrs.at(unID);
  if (NULL == pText) return SMT_ERR_INVALID_PARAM;

  char text[256];
  memset(text, '\0', 256);
  va_list args;

  va_start(args, str);
  vsprintf(text, str, args);
  va_end(args);

  glColor4f(color.fRed, color.fGreen, color.fBlue, color.fA);

  glDisable(GL_LIGHTING);
  glDisable(GL_TEXTURE_2D);

  HDC hDC = m_hPaintDC ? m_hPaintDC : ::GetDC(m_hWnd);
  if (hDC) {
    pText->DrawText(hDC, x, y, z, text);
    if (!m_hPaintDC) {
      ::ReleaseDC(m_hWnd, hDC);
    }
  }

  glEnable(GL_TEXTURE_2D);

  return SMT_ERR_NONE;
}

long SmtGLRenderDevice::DrawText(uint unID, float x, float y,
                                 const SmtColor& color, const char* str, ...) {
  if (str == NULL || unID > m_vTextPtrs.size()) return SMT_ERR_INVALID_PARAM;

  SmtGLText* pText = m_vTextPtrs.at(unID);
  if (NULL == pText) return SMT_ERR_INVALID_PARAM;

  char text[256];
  memset(text, '\0', 256);
  va_list args;

  va_start(args, str);
  vsprintf(text, str, args);
  va_end(args);

  glColor4f(color.fRed, color.fGreen, color.fBlue, color.fA);

  const GLboolean had_depth = glIsEnabled(GL_DEPTH_TEST);
  const GLboolean had_blend = glIsEnabled(GL_BLEND);
  const GLboolean had_texture = glIsEnabled(GL_TEXTURE_2D);
  const GLboolean had_lighting = glIsEnabled(GL_LIGHTING);

  glDisable(GL_LIGHTING);
  glDisable(GL_TEXTURE_2D);
  // Screen-space bitmaps must ignore the terrain depth buffer or glyphs clip.
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D(0, viewport[2], viewport[3], 0);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  HDC hDC = m_hPaintDC ? m_hPaintDC : ::GetDC(m_hWnd);
  if (hDC) {
    pText->DrawText(hDC, x, y, text);
    if (!m_hPaintDC) {
      ::ReleaseDC(m_hWnd, hDC);
    }
  }

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  if (had_depth) {
    glEnable(GL_DEPTH_TEST);
  }
  if (had_lighting) {
    glEnable(GL_LIGHTING);
  }
  if (had_blend) {
    glEnable(GL_BLEND);
  } else {
    glDisable(GL_BLEND);
  }
  if (had_texture) {
    glEnable(GL_TEXTURE_2D);
  } else {
    glDisable(GL_TEXTURE_2D);
  }

  return SMT_ERR_NONE;
}
}  // namespace render