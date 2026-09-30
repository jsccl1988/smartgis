#include "legacy/render/rhi3d/impl/gl/resource/text/text.h"

#include <algorithm>
#include <string>

#include "gis/datasource/provider/impl/ogr/text/ogr_text_encoding.h"

namespace render {
SmtGLText::SmtGLText() {
  for (int i = 0; i < 65536; i++) {
    m_TextTb[i] = 0;
  }
}

SmtGLText::~SmtGLText() {
  ::DeleteObject(m_hFont);
  // Lists are context state. If close already deleted the RC, skip them.
  if (!::wglGetCurrentContext()) {
    return;
  }
  for (int i = 0; i < 65536; i++) {
    if (m_TextTb[i] != 0) {
      glDeleteLists(m_TextTb[i], 1);
    }
  }
}

// print implement
long SmtGLText::CreateFont(HDC hDC, const char *chType, int nHeight, int nWidth,
                           int nWeight, bool bItalic, bool bUnderline,
                           bool bStrike, ulong ulSize) {
  if (nHeight == 0 && nWidth == 0) {
    m_nHeight = MulDiv(ulSize, GetDeviceCaps(hDC, LOGPIXELSY), 72);
    // m_nWidth  = MulDiv(dwSize, GetDeviceCaps(hDC, LOGPIXELSX), 72);
  } else {
    m_nHeight = nHeight;
    m_nWidth = nWidth;
  }

  m_nWeight = nWeight;

  if (stricmp(chType, "symbol") == 0) {
    m_hFont = ::CreateFont(m_nHeight, m_nWidth, 0, 0, m_nWeight, bItalic,
                           bUnderline, bStrike, SYMBOL_CHARSET, OUT_TT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           FF_DONTCARE | DEFAULT_PITCH, chType);
  } else {
    m_hFont = ::CreateFont(m_nHeight, m_nWidth, 0, 0, m_nWeight, bItalic,
                           bUnderline, bStrike, DEFAULT_CHARSET, OUT_TT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           FF_DONTCARE | DEFAULT_PITCH, chType);
  }

  if (m_hFont == NULL) return SMT_FALSE;

  return SMT_OK;
}

HFONT SmtGLText::GetFont() { return m_hFont; }

long SmtGLText::DrawText(HDC hDC, float x, float y, float z, const char *str) {
  if (str == NULL) return SMT_ERR_INVALID_PARAM;

  ::SelectObject(hDC, m_hFont);

  glRasterPos3f(x, y, z);
  const std::wstring wide = gis::datasource::ogr_bytes_to_wide(str);
  const int len =
      static_cast<int>((std::min)(wide.size(), static_cast<size_t>(399)));
  HDC font_dc = ::wglGetCurrentDC();
  if (!font_dc) {
    font_dc = hDC;
  }
  ::SelectObject(font_dc, m_hFont);
  for (int i = 0; i < len; i++) {
    const unsigned letter = static_cast<unsigned>(wide[static_cast<size_t>(i)]);
    if (wide[static_cast<size_t>(i)] == L'\n') {
      y += m_nHeight;
      glRasterPos3f(x, y + m_nHeight, z);
    } else if (letter < 65536u) {
      if (m_TextTb[letter] == 0) {
        const GLuint list = glGenLists(1);
        if (list == 0) {
          continue;
        }
        if (!wglUseFontBitmapsW(font_dc, static_cast<DWORD>(letter), 1, list)) {
          glDeleteLists(list, 1);
          continue;
        }
        m_TextTb[letter] = list;
      }
      glCallList(m_TextTb[letter]);
    }
  }

  return SMT_ERR_NONE;
}

HRESULT SmtGLText::DrawText(HDC hDC, float x, float y, const char *str) {
  if (str == NULL) return SMT_ERR_INVALID_PARAM;
  /*
  int length;
  length = (int)strlen(str);
  glRasterPos2f(x,y);
  for (int m = 0 ; m < length; m++)
  {
  glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12,str[m]);
  }
  */

  ::SelectObject(hDC, m_hFont);

  glRasterPos2f(x, y);
  const std::wstring wide = gis::datasource::ogr_bytes_to_wide(str);
  const int len =
      static_cast<int>((std::min)(wide.size(), static_cast<size_t>(399)));
  // Font bitmaps must use the DC bound to the current RC.
  HDC font_dc = ::wglGetCurrentDC();
  if (!font_dc) {
    font_dc = hDC;
  }
  ::SelectObject(font_dc, m_hFont);
  for (int i = 0; i < len; i++) {
    const unsigned letter = static_cast<unsigned>(wide[static_cast<size_t>(i)]);
    if (wide[static_cast<size_t>(i)] == L'\n') {
      y += m_nHeight;
      glRasterPos2f(x, y + m_nHeight);
    } else if (letter < 65536u) {
      if (m_TextTb[letter] == 0) {
        const GLuint list = glGenLists(1);
        if (list == 0) {
          continue;
        }
        if (!wglUseFontBitmapsW(font_dc, static_cast<DWORD>(letter), 1, list)) {
          glDeleteLists(list, 1);
          continue;
        }
        m_TextTb[letter] = list;
      }
      glCallList(m_TextTb[letter]);
    }
  }

  return SMT_ERR_NONE;
}
}  // namespace render