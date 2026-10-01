// Copyright (c) 2010 CCL. All rights reserved.
// Abstract 2D map render device. Buffer layers (eRDBufferLayer):
// Map=0, Dynamic=1, Quick=2, Direct=immediate DC (no persistent buffer).
#ifndef _RD_RENDERDEVICE_H
#define _RD_RENDERDEVICE_H

#include "gis/kernel/geo/mesh/geometry.h"
#include "gis/model/feature/feature.h"
#include "gis/model/map/map.h"
#include "legacy/gis/present/carto/style.h"
#include "legacy/gis/present/carto/style_bas_struct.h"
#include "legacy/core/macros/macros.h"
#include "legacy/render/legacy_render_export.h"

class OGRFeature;
class OGRLayer;

using namespace base;
using namespace gis;
using namespace base;
using namespace geo;

namespace render {
enum eRDBufferLayer { MRD_BL_MAP, MRD_BL_DYNAMIC, MRD_BL_QUICK, MRD_BL_DIRECT };

class SmtRenderDevice {
 public:
  SmtRenderDevice(HINSTANCE hInst)
      : m_rBaseApi(RD_GDI),
        m_hInst(hInst),
        m_strLogName(""),
        m_hWnd(nullptr),
        m_nMapMode(MM_TEXT),
        m_fblc(1.) {
    ;
  }

  virtual ~SmtRenderDevice(void) {}

 public:
  virtual int Init(HWND hWnd, const char *logname) = 0;
  virtual int Destroy(void) = 0;
  virtual int Release(void) = 0;

  virtual int Resize(int orgx, int orgy, int cx, int cy) = 0;

  void SetViewport(const Viewport &viewport) { m_Viewport = viewport; }
  Viewport GetViewport(void) const { return m_Viewport; }

  void SetWindowport(const Windowport &windowport) {
    m_Windowport = windowport;
  }
  Windowport GetWindowport(void) const { return m_Windowport; }

  void SetCurDrawingOrg(const lPoint &ptPos) { m_curDrawingOrg = ptPos; }
  lPoint GetCurDrawingOrg(void) const { return m_curDrawingOrg; }

  void SetMapMode(int nMode) { m_nMapMode = nMode; }
  int GetMapMode(void) const { return m_nMapMode; }

  void SetRenderOptions(const Smt2DRenderOptions &rdOptions) { m_rdOptions = rdOptions; }
  Smt2DRenderOptions GetRenderOptions(void) const { return m_rdOptions; }

  inline double GetBlc(void) const { return m_fblc; }

 public:
  virtual int Lock() = 0;
  virtual int Unlock() = 0;

  virtual int Refresh(void) = 0;
  virtual int Refresh(const SmtMap *pMap, fRect rect) = 0;
  virtual int RefreshDirectly(const SmtMap *pMap, lRect rect,
                              bool bRealTime = false) = 0;

  virtual int ZoomMove(const SmtMap *pMap, fPoint dbfPointOffset,
                       bool bRealTime = false) = 0;
  virtual int ZoomScale(const SmtMap *pMap, lPoint orgPoint, float fscale,
                        bool bRealTime = false) = 0;
  virtual int ZoomToRect(const SmtMap *pMap, fRect rect,
                         bool bRealTime = false) = 0;

  // StretchBlt last map buffer; full tessellate is scheduled separately.
  virtual int PreviewZoomScale(lPoint orgPoint, float fscale) {
    return ZoomScale(nullptr, orgPoint, fscale);
  }
  virtual int PreviewZoomMove(fPoint dbfPointOffset) {
    return ZoomMove(nullptr, dbfPointOffset);
  }
  virtual int ScheduleDelayedRedraw(const SmtMap *pMap) {
    (void)pMap;
    return Refresh();
  }

  virtual int Timer() = 0;

 public:
  virtual int LPToDP(float x, float y, long &X, long &Y) const = 0;
  virtual int DPToLP(LONG X, LONG Y, float &x, float &y) const = 0;
  virtual int LRectToDRect(const fRect &frect, lRect &lrect) const = 0;
  virtual int DRectToLRect(const lRect &lrect, fRect &frect) const = 0;

 public:
  virtual int BeginRender(eRDBufferLayer eMRDBufLyr, bool bClear = false,
                          const SmtStyle *pStyle = nullptr,
                          int op = R2_COPYPEN) = 0;
  virtual int EndRender(eRDBufferLayer eMRDBufLyr) = 0;

 public:
  virtual int RenderMap(void) = 0;
  virtual int RenderMap(const SmtMap *pMap, int op = R2_COPYPEN) = 0;
  virtual int RenderLayer(const SmtLayer *pLayer, int op = R2_COPYPEN) = 0;
  virtual int RenderLayer(OGRLayer *pLayer, int op = R2_COPYPEN) = 0;
  virtual int RenderLayer(const SmtRasterLayer *pLayer,
                          int op = R2_COPYPEN) = 0;
  virtual int RenderLayer(const SmtTileLayer *pLayer, int op = R2_COPYPEN) = 0;
  virtual int RenderFeature(OGRFeature *pFeature, int op = R2_COPYPEN) = 0;
  virtual int RenderGeometry(const OGRGeometry *pGeom, const SmtStyle *pStyle,
                             int op = R2_COPYPEN) = 0;

  virtual int DrawMultiLineString(
      const OGRMultiLineString *pMultiLinestring) = 0;
  virtual int DrawMultiPoint(const SmtStyle *pStyle,
                             const OGRMultiPoint *pMultiPoint) = 0;
  virtual int DrawMultiPolygon(const OGRMultiPolygon *pMultiPolygon) = 0;

  virtual int DrawPoint(const SmtStyle *pStyle, const OGRPoint *pPoint) = 0;
  virtual int DrawAnno(const char *szAnno, float fangel, float fCHeight,
                       float fCWidth, float fCSpace,
                       const OGRPoint *pPoint) = 0;
  virtual int DrawSymbol(HICON hIcon, long lHeight, long lWhidth,
                         const OGRPoint *pPoint) = 0;

  virtual int DrawLineString(const OGRLineString *pLinestring) = 0;
  virtual int DrawLineSpline(const OGRLineString *pSpline) = 0;
  virtual int DrawLinearRing(const OGRLinearRing *pLinearRing) = 0;
  virtual int DrawPloygon(const OGRPolygon *pPloygon) = 0;

  virtual int DrawTin(const SmtTin *pTin) = 0;
  virtual int DrawGrid(const SmtGrid *pGrid) = 0;
  virtual int DrawArc(const OGRLineString *pArc) = 0;
  virtual int DrawFan(const OGRPolygon *pFan) = 0;

 public:
  virtual int DrawEllipse(float left, float top, float right, float bottom,
                          bool bDP = false) = 0;
  virtual int DrawRect(const fRect &rect, bool bDP = false) = 0;
  virtual int DrawLine(fPoint *pfPoints, int nCount, bool bDP = false) = 0;
  virtual int DrawLine(const fPoint &ptA, const fPoint &ptB,
                       bool bDP = false) = 0;
  virtual int DrawText(const char *szAnno, float fangel, float fCHeight,
                       float fCWidth, float fCSpace, const fPoint &point,
                       bool bDP = false) = 0;
  virtual int DrawImage(const char *szImageBuf, int nImageBufSize,
                        const fRect &frect, long lCodeType,
                        eRDBufferLayer eMRDBufLyr = MRD_BL_MAP) = 0;
  virtual int StrethImage(const char *szImageBuf, int nImageBufSize,
                          const fRect &frect, long lCodeType,
                          eRDBufferLayer eMRDBufLyr = MRD_BL_MAP) = 0;

 public:
  virtual int SaveImage(const char *szFilePath,
                        eRDBufferLayer eMRDBufLyr = MRD_BL_MAP,
                        bool bBgTransparent = false) = 0;
  virtual int Save2ImageBuf(char *&szImageBuf, long &lImageBufSize,
                            long lCodeType,
                            eRDBufferLayer eMRDBufLyr = MRD_BL_MAP,
                            bool bBgTransparent = false) = 0;
  virtual int FreeImageBuf(char *&szImageBuf) = 0;

  // Appended at end of the vtable so mid-list inserts do not shift SaveImage
  // / StrethImage slots for stale TUs. Present composite buffers to a paint
  // DC (WM_PAINT). Default falls back to RenderMap()'s GetDC path; GDI
  // overrides BitBlt into hdc so EndPaint cannot discard the frame.
  virtual int RenderMapToDC(HDC hdc) {
    (void)hdc;
    return RenderMap();
  }

 protected:
  HINSTANCE m_hInst;
  RenderBaseApi m_rBaseApi;
  string m_strLogName;

  Viewport m_Viewport;
  Windowport m_Windowport;
  float m_fblc;

  HWND m_hWnd;
  int m_nMapMode;
  Smt2DRenderOptions m_rdOptions;

  lPoint m_curDrawingOrg;
};

typedef SmtRenderDevice *LPRENDERDEVICE;

#ifdef __cplusplus
extern "C" {
#endif
// Exported from legacy_rhi2d_{gdi,gdiplus,skia}.dll (not legacy_render).
int LEGACY_RHI2D_DEVICE_EXPORT CreateRenderDevice(HINSTANCE hInst,
                                                  LPRENDERDEVICE &pMrdDevice);
int LEGACY_RHI2D_DEVICE_EXPORT DestroyRenderDevice(LPRENDERDEVICE &pMrdDevice);

typedef HRESULT (*_CreateRenderDevice)(HINSTANCE hInst,
                                       LPRENDERDEVICE &pMrdDevice);
typedef HRESULT (*_DestroyRenderDevice)(LPRENDERDEVICE &pMrdDevice);

#ifdef __cplusplus
}
#endif
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_RD_RENDERDEVICE_H