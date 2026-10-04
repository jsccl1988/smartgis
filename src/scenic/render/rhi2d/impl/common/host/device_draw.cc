// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#include "scenic/render/rhi2d/impl/common/host/render_device.h"

#include "scenic/render/rhi2d/impl/common/cc/layer_tree_host.h"

namespace scenic {
namespace detail {

int Rhi2dRenderDevice::RenderMap(const Map* pMap, int op) {
  if (!layer_tree_host_) {
    return kErrFailure;
  }
  if (layer_tree_host_->is_busy()) {
    return kErrFailure;
  }
  const RenderContext ctx(
      m_Viewport, m_Windowport, static_cast<float>(m_fblc), pMap,
      static_cast<int>(m_Viewport.m_fVOX), static_cast<int>(m_Viewport.m_fVOY),
      static_cast<int>(m_Viewport.m_fVWidth),
      static_cast<int>(m_Viewport.m_fVHeight), op);
  return layer_tree_host_->paint_map_sync(ctx, m_rdOptions);
}

int Rhi2dRenderDevice::RenderLayer(const MapLayer* pLayer, int op) {
  sync_host_paint_context();
  return overlay_painter_->render_layer(pLayer, op);
}

int Rhi2dRenderDevice::RenderLayer(OGRLayer* pLayer, int op) {
  sync_host_paint_context();
  return overlay_painter_->render_layer(pLayer, op);
}

int Rhi2dRenderDevice::RenderLayer(const datasource::OgrRasterLayer* pLayer,
                                   int op) {
  sync_host_paint_context();
  return overlay_painter_->render_layer(pLayer, op);
}

int Rhi2dRenderDevice::RenderLayer(const tile::ProviderTileLayer* pLayer,
                                   int op) {
  sync_host_paint_context();
  return overlay_painter_->render_layer(pLayer, op);
}

int Rhi2dRenderDevice::RenderFeature(OGRFeature* pFeature, int op) {
  sync_host_paint_context();
  return overlay_painter_->render_feature(pFeature, op);
}

int Rhi2dRenderDevice::RenderGeometry(const OGRGeometry* pGeom,
                                         const Style* pStyle, int op) {
  sync_host_paint_context();
  return overlay_painter_->render_geometry(pGeom, pStyle, op);
}

int Rhi2dRenderDevice::DrawMultiLineString(
    const OGRMultiLineString* pMultiLinestring) {
  return carto_draw_->draw_multi_line_string(pMultiLinestring);
}

int Rhi2dRenderDevice::DrawMultiPoint(const Style* pStyle,
                                         const OGRMultiPoint* pMultiPoint) {
  return carto_draw_->draw_multi_point(pStyle, pMultiPoint);
}

int Rhi2dRenderDevice::DrawMultiPolygon(
    const OGRMultiPolygon* pMultiPolygon) {
  return carto_draw_->draw_multi_polygon(pMultiPolygon);
}

int Rhi2dRenderDevice::DrawPoint(const Style* pStyle,
                                    const OGRPoint* pPoint) {
  return carto_draw_->draw_point(pStyle, pPoint);
}

int Rhi2dRenderDevice::DrawAnno(const char* szAnno, float fangel,
                                   float fCHeight, float fCWidth, float fCSpace,
                                   const OGRPoint* pPoint) {
  return carto_draw_->draw_anno(szAnno, fangel, fCHeight, fCWidth, fCSpace,
                                pPoint);
}

int Rhi2dRenderDevice::DrawSymbol(HICON hIcon, long lHeight, long lWidth,
                                     const OGRPoint* pPoint) {
  return carto_draw_->draw_symbol(hIcon, lHeight, lWidth, pPoint);
}

int Rhi2dRenderDevice::DrawLineSpline(const OGRLineString* pSpline) {
  return carto_draw_->draw_line_spline(pSpline);
}

int Rhi2dRenderDevice::DrawLineString(const OGRLineString* pLinestring) {
  return carto_draw_->draw_line_string(pLinestring);
}

int Rhi2dRenderDevice::DrawLinearRing(const OGRLinearRing* pLinearRing) {
  return carto_draw_->draw_linear_ring(pLinearRing);
}

int Rhi2dRenderDevice::DrawPloygon(const OGRPolygon* pPloygon) {
  return carto_draw_->draw_polygon(pPloygon);
}

int Rhi2dRenderDevice::DrawTin(const OGRTriangulatedSurface* pTin) {
  return carto_draw_->draw_tin(pTin);
}

int Rhi2dRenderDevice::DrawGrid(const plugin::detail::OrthoLattice* pGrid) {
  return carto_draw_->draw_grid(pGrid);
}

int Rhi2dRenderDevice::DrawFan(const OGRPolygon* pFan) {
  return carto_draw_->draw_fan(pFan);
}

int Rhi2dRenderDevice::DrawArc(const OGRLineString* pArc) {
  return carto_draw_->draw_arc(pArc);
}

int Rhi2dRenderDevice::DrawEllipse(float left, float top, float right,
                                      float bottom, bool bDP) {
  return carto_draw_->draw_ellipse(left, top, right, bottom, bDP);
}

int Rhi2dRenderDevice::DrawRect(const fRect& rect, bool bDP) {
  return carto_draw_->draw_rect(rect, bDP);
}

int Rhi2dRenderDevice::DrawLine(fPoint* pfPoints, int nCount, bool bDP) {
  return carto_draw_->draw_line(pfPoints, nCount, bDP);
}

int Rhi2dRenderDevice::DrawLine(const fPoint& ptA, const fPoint& ptB,
                                   bool bDP) {
  return carto_draw_->draw_line(ptA, ptB, bDP);
}

int Rhi2dRenderDevice::DrawText(const char* szAnno, float fangel,
                                   float fCHeight, float fCWidth, float fCSpace,
                                   const fPoint& point, bool bDP) {
  return carto_draw_->draw_text(szAnno, fangel, fCHeight, fCWidth, fCSpace,
                                point, bDP);
}

int Rhi2dRenderDevice::DrawImage(const char* szImageBuf, int nImageBufSize,
                                    const fRect& frect, long lCodeType,
                                    eRDBufferLayer eMRDBufLyr) {
  return buffer_image_.draw_image(szImageBuf, nImageBufSize, frect, lCodeType,
                                  eMRDBufLyr);
}

int Rhi2dRenderDevice::StrethImage(const char* szImageBuf, int nImageBufSize,
                                      const fRect& frect, long lCodeType,
                                      eRDBufferLayer eMRDBufLyr) {
  return buffer_image_.stretch_image(szImageBuf, nImageBufSize, frect,
                                     lCodeType, eMRDBufLyr);
}

int Rhi2dRenderDevice::SaveImage(const char* szFilePath,
                                    eRDBufferLayer eMRDBufLyr,
                                    bool bBgTransparent) {
  return buffer_image_.save_image(szFilePath, eMRDBufLyr, bBgTransparent);
}

int Rhi2dRenderDevice::Save2ImageBuf(char*& szImageBuf, long& lImageBufSize,
                                        long lCodeType,
                                        eRDBufferLayer eMRDBufLyr,
                                        bool bBgTransparent) {
  return buffer_image_.save2_image_buf(szImageBuf, lImageBufSize, lCodeType,
                                       eMRDBufLyr, bBgTransparent);
}

int Rhi2dRenderDevice::FreeImageBuf(char*& szImageBuf) {
  return buffer_image_.free_image_buf(szImageBuf);
}

}  // namespace detail
}  // namespace scenic
