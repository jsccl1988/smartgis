#include "base/core/log.h"
#include "base/trace/event/process_trace.h"
#include "scenic/render/frame.h"
#include "scenic/render/rhi3d/impl/gl/resource/buffer/index_buffer.h"
#include "scenic/render/rhi3d/impl/gl/resource/buffer/vertex_buffer.h"
#include "scenic/render/rhi3d/impl/gl/host/render_device.h"
#include "scenic/render/rhi3d/impl/gl/resource/text/text.h"

using namespace base;

namespace scenic {
namespace detail {
// Rendering functions
long GlRenderDevice::BeginRender() {
  BASE_TRACE_EVENT("BeginRender", "rhi3d.gl");
  if (!m_hWnd || !m_hRC) {
    return kErrFailure;
  }
  if (!m_hPaintDC) {
    m_hPaintDC = ::GetDC(m_hWnd);
  }
  if (!m_hPaintDC) {
    return kErrFailure;
  }
  // Keep the DC for the whole frame. ReleaseDC while the RC is current
  // leaves later glDraw*/SwapBuffers on a stale HDC.
  return wglMakeCurrent(m_hPaintDC, m_hRC) ? kErrNone : kErrFailure;
}

long GlRenderDevice::EndRender() {
  BASE_TRACE_EVENT("EndRender", "rhi3d.gl");
  ::glFlush();
  return kErrNone;
}

long GlRenderDevice::SwapBuffers() {
  BASE_TRACE_EVENT("SwapBuffers", "rhi3d.gl");
  HDC hDC = m_hPaintDC ? m_hPaintDC : ::GetDC(m_hWnd);
  if (hDC) {
    ::SwapBuffers(hDC);
    if (!m_hPaintDC) {
      ::ReleaseDC(m_hWnd, hDC);
    }
  }
  detail::finish_frame_memory_sample();
  detail::log_frame_flow("rhi3d.gl SwapBuffers");
  return kErrNone;
}

long GlRenderDevice::DrawPrimitives(PrimitiveType primitiveType,
                                       VertexBuffer* pVB, DWORD baseVertex,
                                       DWORD primitiveCount) {
  if (pVB == nullptr) return kErrInvalidParam;

  // Convert primitive type
  GLenum PT;
  ulong count;
  if (kErrNone !=
      GetOpenGLPrimitiveType(primitiveType, primitiveCount, &PT, &count))
    return kErrFailure;

  // Say that the VB will be the source for our draw primitive calls
  //--
  if (kErrNone != pVB->PrepareForDrawing()) return kErrFailure;

  // Draw primitives
  //--
  glDrawArrays(PT, baseVertex, count);

  if (kErrNone != pVB->EndDrawing()) return kErrFailure;

  return kErrNone;
}

long GlRenderDevice::DrawIndexedPrimitives(PrimitiveType primitiveType,
                                              VertexBuffer* pVB,
                                              IndexBuffer* pIB,
                                              ulong baseIndex,
                                              ulong primitiveCount) {
  if (pVB == nullptr || pIB == nullptr) return kErrInvalidParam;

  // Convert primitive type
  GLenum PT;
  ulong count;
  if (kErrNone !=
      GetOpenGLPrimitiveType(primitiveType, primitiveCount, &PT, &count))
    return kErrFailure;

  // Say that the VB will be the source for our draw primitive calls
  //--
  if (kErrNone != pVB->PrepareForDrawing() ||
      kErrNone != pIB->PrepareForDrawing())
    return kErrFailure;

  // Draw primitives
  //--
  const void* indices = pIB->GetIndexData();
  if (!indices) {
    return kErrFailure;
  }
  glDrawElements(PT, count, GL_UNSIGNED_INT, indices);

  if (kErrNone != pVB->EndDrawing() || kErrNone != pIB->EndDrawing())
    return kErrFailure;

  return kErrNone;
}

inline long GlRenderDevice::GetOpenGLPrimitiveType(
    const PrimitiveType pt, const ulong nInitialPrimitiveCount,
    GLenum* GLPrimitiveType, ulong* nGLPrimitiveCount) {
  switch (pt) {
    case PT_POINTLIST:
      *GLPrimitiveType = GL_POINTS;
      *nGLPrimitiveCount = nInitialPrimitiveCount;
      return kErrNone;

    case PT_LINELIST:
      *GLPrimitiveType = GL_LINES;
      *nGLPrimitiveCount = 2 * nInitialPrimitiveCount;
      return kErrNone;

    case PT_LINESTRIP:
      *GLPrimitiveType = GL_LINE_STRIP;
      *nGLPrimitiveCount = nInitialPrimitiveCount;
      return kErrNone;

    case PT_TRIANGLELIST:
      *GLPrimitiveType = GL_TRIANGLES;
      *nGLPrimitiveCount = 3 * nInitialPrimitiveCount;
      return kErrNone;

    case PT_TRIANGLESTRIP:
      *GLPrimitiveType = GL_TRIANGLE_STRIP;
      *nGLPrimitiveCount = nInitialPrimitiveCount + 2;
      return kErrNone;

    case PT_TRIANGLEFAN:
      *GLPrimitiveType = GL_TRIANGLE_FAN;
      *nGLPrimitiveCount = nInitialPrimitiveCount + 2;
      return kErrNone;

    default:
      *GLPrimitiveType = GL_POINTS;
      *nGLPrimitiveCount = nInitialPrimitiveCount;
      return kErrNone;
  }

  return kErrFailure;
}

long GlRenderDevice::DrawText(uint unID, float x, float y, float z,
                                 const Color& color, const char* str, ...) {
  if (str == nullptr || unID >= m_vTextPtrs.size()) return kErrInvalidParam;

  GlText* pText = m_vTextPtrs.at(unID).get();
  if (nullptr == pText) return kErrInvalidParam;

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

  return kErrNone;
}

long GlRenderDevice::DrawText(uint unID, float x, float y,
                                 const Color& color, const char* str, ...) {
  if (str == nullptr || unID >= m_vTextPtrs.size()) return kErrInvalidParam;

  GlText* pText = m_vTextPtrs.at(unID).get();
  if (nullptr == pText) return kErrInvalidParam;

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

  return kErrNone;
}
}  // namespace detail
}  // namespace scenic