// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _GL_TEXT_H
#define _GL_TEXT_H

#include "scenic/render/rhi3d/impl/gl/prerequisites.h"

namespace scenic {
namespace detail {
class GlText {
 public:
  GlText();
  virtual ~GlText(void);

  // create font
  long CreateFont(HDC hDC, const char *chType, int nHeight, int nWidth,
                  int nWeight, bool bItalic, bool bUnderline, bool bStrike,
                  ulong ulSize);
  HFONT GetFont();
  // draw text
  HRESULT DrawText(HDC hDC, float xpos, float ypos, float zpos,
                   const char *);  // 3d pos
  HRESULT DrawText(HDC hDC, float xscreen, float yscreen,
                   const char *);  // screen pos

 protected:
  GLuint m_TextTb[65536];
  HFONT m_hFont;
  int m_nHeight, m_nWidth, m_nWeight;
};
}  // namespace detail
}  // namespace scenic
#endif  //_GL_TEXT_H