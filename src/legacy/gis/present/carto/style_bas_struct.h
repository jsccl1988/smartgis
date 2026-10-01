// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_PRESENT_CARTO_STYLE_BAS_STRUCT_H_
#define GIS_PRESENT_CARTO_STYLE_BAS_STRUCT_H_

namespace base {
enum RenderBaseApi { RD_GDI, RD_GDIPLUS, RD_SKIA };

struct Viewport {
  float m_fVOX;
  float m_fVOY;
  float m_fVHeight;
  float m_fVWidth;

  Viewport() : m_fVOX(0), m_fVOY(0), m_fVHeight(0), m_fVWidth(0) {}
};

struct Windowport {
  float m_fWOX;
  float m_fWOY;
  float m_fWHeight;
  float m_fWWidth;

  Windowport() : m_fWOX(0), m_fWOY(0), m_fWHeight(0), m_fWWidth(0) {}
};

struct Smt2DRenderOptions {
  bool bShowMBR;
  bool bShowPoint;
  long lPointRaduis;

  Smt2DRenderOptions() : bShowMBR(true), bShowPoint(true), lPointRaduis(2) { ; }
};
}  // namespace base

#endif  // GIS_PRESENT_CARTO_STYLE_BAS_STRUCT_H_
