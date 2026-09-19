#include "base/core/log.h"
#include "legacy/render/gl/gl_3drenderdevice.h"
#include "legacy/render/gl/gl_text.h"

using namespace base;

namespace render {
//////////////////////////////////////////////////////////////////////////
// print implement
long SmtGLRenderDevice::CreateFont(const char *chType, int nHeight, int nWidth,
                                   int nWeight, bool bItalic, bool bUnderline,
                                   bool bStrike, ulong dwSize, uint &unID) {
  HDC hDC = m_hPaintDC ? m_hPaintDC : ::GetDC(m_hWnd);
  if (!hDC) {
    return SMT_ERR_FAILURE;
  }

  SmtGLText *pText = new SmtGLText();
  if (SMT_ERR_NONE != pText->CreateFont(hDC, chType, nHeight, nWidth, nWeight,
                                        bItalic, bUnderline, bStrike, dwSize))
    return SMT_ERR_FAILURE;

  if (!m_hPaintDC) {
    ::ReleaseDC(m_hWnd, hDC);
  }

  m_vTextPtrs.push_back(pText);
  unID = m_vTextPtrs.size() - 1;

  return SMT_ERR_NONE;
}
}  // namespace render