// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#include "legacy/render/rhi2d/impl/common/host/render_device.h"

#include "legacy/render/rhi2d/impl/common/cc/layer_tree_host.h"

namespace render {

int SmtRhi2dRenderDevice::RenderMap(const SmtMap* pMap, int op) {
  if (!layer_tree_host_) {
    return SMT_ERR_FAILURE;
  }
  if (layer_tree_host_->is_busy()) {
    return SMT_ERR_FAILURE;
  }
  const SmtRenderContext ctx(
      m_Viewport, m_Windowport, static_cast<float>(m_fblc), pMap,
      static_cast<int>(m_Viewport.m_fVOX), static_cast<int>(m_Viewport.m_fVOY),
      static_cast<int>(m_Viewport.m_fVWidth),
      static_cast<int>(m_Viewport.m_fVHeight), op);
  return layer_tree_host_->paint_map_sync(ctx, m_rdOptions);
}

int SmtRhi2dRenderDevice::RenderLayer(const SmtLayer* pLayer, int op) {
  sync_host_paint_context();
  return overlay_painter_->render_layer(pLayer, op);
}

int SmtRhi2dRenderDevice::RenderLayer(OGRLayer* pLayer, int op) {
  sync_host_paint_context();
  return overlay_painter_->render_layer(pLayer, op);
}

int SmtRhi2dRenderDevice::RenderLayer(const SmtRasterLayer* pLayer, int op) {
  sync_host_paint_context();
  return overlay_painter_->render_layer(pLayer, op);
}

int SmtRhi2dRenderDevice::RenderLayer(const SmtTileLayer* pLayer, int op) {
  sync_host_paint_context();
  return overlay_painter_->render_layer(pLayer, op);
}

int SmtRhi2dRenderDevice::RenderFeature(OGRFeature* pFeature, int op) {
  sync_host_paint_context();
  return overlay_painter_->render_feature(pFeature, op);
}

int SmtRhi2dRenderDevice::RenderGeometry(const OGRGeometry* pGeom,
                                         const SmtStyle* pStyle, int op) {
  sync_host_paint_context();
  return overlay_painter_->render_geometry(pGeom, pStyle, op);
}

int SmtRhi2dRenderDevice::DrawMultiLineString(
    const OGRMultiLineString* pMultiLinestring) {
  return carto_draw_->draw_multi_line_string(pMultiLinestring);
}

int SmtRhi2dRenderDevice::DrawMultiPoint(const SmtStyle* pStyle,
                                         const OGRMultiPoint* pMultiPoint) {
  return carto_draw_->draw_multi_point(pStyle, pMultiPoint);
}

int SmtRhi2dRenderDevice::DrawMultiPolygon(
    const OGRMultiPolygon* pMultiPolygon) {
  return carto_draw_->draw_multi_polygon(pMultiPolygon);
}

int SmtRhi2dRenderDevice::DrawPoint(const SmtStyle* pStyle,
                                    const OGRPoint* pPoint) {
  return carto_draw_->draw_point(pStyle, pPoint);
}

int SmtRhi2dRenderDevice::DrawAnno(const char* szAnno, float fangel,
                                   float fCHeight, float fCWidth, float fCSpace,
                                   const OGRPoint* pPoint) {
  return carto_draw_->draw_anno(szAnno, fangel, fCHeight, fCWidth, fCSpace,
                                pPoint);
}

int SmtRhi2dRenderDevice::DrawSymbol(HICON hIcon, long lHeight, long lWidth,
                                     const OGRPoint* pPoint) {
  return carto_draw_->draw_symbol(hIcon, lHeight, lWidth, pPoint);
}

int SmtRhi2dRenderDevice::DrawLineSpline(const OGRLineString* pSpline) {
  return carto_draw_->draw_line_spline(pSpline);
}

int SmtRhi2dRenderDevice::DrawLineString(const OGRLineString* pLinestring) {
  return carto_draw_->draw_line_string(pLinestring);
}

int SmtRhi2dRenderDevice::DrawLinearRing(const OGRLinearRing* pLinearRing) {
  return carto_draw_->draw_linear_ring(pLinearRing);
}

int SmtRhi2dRenderDevice::DrawPloygon(const OGRPolygon* pPloygon) {
  return carto_draw_->draw_polygon(pPloygon);
}

int SmtRhi2dRenderDevice::DrawTin(const SmtTin* pTin) {
  return carto_draw_->draw_tin(pTin);
}

int SmtRhi2dRenderDevice::DrawGrid(const SmtGrid* pGrid) {
  return carto_draw_->draw_grid(pGrid);
}

int SmtRhi2dRenderDevice::DrawFan(const OGRPolygon* pFan) {
  return carto_draw_->draw_fan(pFan);
}

int SmtRhi2dRenderDevice::DrawArc(const OGRLineString* pArc) {
  return carto_draw_->draw_arc(pArc);
}

int SmtRhi2dRenderDevice::DrawEllipse(float left, float top, float right,
                                      float bottom, bool bDP) {
  return carto_draw_->draw_ellipse(left, top, right, bottom, bDP);
}

int SmtRhi2dRenderDevice::DrawRect(const fRect& rect, bool bDP) {
  return carto_draw_->draw_rect(rect, bDP);
}

int SmtRhi2dRenderDevice::DrawLine(fPoint* pfPoints, int nCount, bool bDP) {
  return carto_draw_->draw_line(pfPoints, nCount, bDP);
}

int SmtRhi2dRenderDevice::DrawLine(const fPoint& ptA, const fPoint& ptB,
                                   bool bDP) {
  return carto_draw_->draw_line(ptA, ptB, bDP);
}

int SmtRhi2dRenderDevice::DrawText(const char* szAnno, float fangel,
                                   float fCHeight, float fCWidth, float fCSpace,
                                   const fPoint& point, bool bDP) {
  return carto_draw_->draw_text(szAnno, fangel, fCHeight, fCWidth, fCSpace,
                                point, bDP);
}

int SmtRhi2dRenderDevice::DrawImage(const char* szImageBuf, int nImageBufSize,
                                    const fRect& frect, long lCodeType,
                                    eRDBufferLayer eMRDBufLyr) {
  return buffer_image_.draw_image(szImageBuf, nImageBufSize, frect, lCodeType,
                                  eMRDBufLyr);
}

int SmtRhi2dRenderDevice::StrethImage(const char* szImageBuf, int nImageBufSize,
                                      const fRect& frect, long lCodeType,
                                      eRDBufferLayer eMRDBufLyr) {
  return buffer_image_.stretch_image(szImageBuf, nImageBufSize, frect,
                                     lCodeType, eMRDBufLyr);
}

int SmtRhi2dRenderDevice::SaveImage(const char* szFilePath,
                                    eRDBufferLayer eMRDBufLyr,
                                    bool bBgTransparent) {
  return buffer_image_.save_image(szFilePath, eMRDBufLyr, bBgTransparent);
}

int SmtRhi2dRenderDevice::Save2ImageBuf(char*& szImageBuf, long& lImageBufSize,
                                        long lCodeType,
                                        eRDBufferLayer eMRDBufLyr,
                                        bool bBgTransparent) {
  return buffer_image_.save2_image_buf(szImageBuf, lImageBufSize, lCodeType,
                                       eMRDBufLyr, bBgTransparent);
}

int SmtRhi2dRenderDevice::FreeImageBuf(char*& szImageBuf) {
  return buffer_image_.free_image_buf(szImageBuf);
}

}  // namespace render
