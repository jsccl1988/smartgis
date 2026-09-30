// Copyright (c) 2010 CCL. All rights reserved.
// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef _GDI_RENDERDEVICE_H
#define _GDI_RENDERDEVICE_H

#include <cstdint>
#include <mutex>

#include "gis/kernel/geo/mesh/geometry.h"
#include "gis/model/feature/feature.h"
#include "gis/model/map/map.h"
#include "legacy/core/macros/macros.h"
#include "legacy/render/rhi2d/impl/gdi/paint/encode/command_encoder.h"
#include "legacy/render/rhi2d/impl/gdi/host/image_io.h"
#include "legacy/render/rhi2d/impl/gdi/host/ui_controller.h"
#include "legacy/render/rhi2d/impl/gdi/paint/canvas/layer_painter.h"
#include "legacy/render/rhi2d/impl/gdi/paint/canvas/paint_canvas.h"
#include "legacy/render/rhi2d/impl/gdi/paint/canvas/paint_context.h"
#include "legacy/render/rhi2d/impl/gdi/surface/surface_pool.h"
#include "legacy/render/rhi2d/public/device/renderdevice.h"

using namespace base;
using namespace gis;
using namespace geo;

namespace render {
class SmtGdiRenderThread;

// GDI map render device facade: owns buffers/viewport and forwards paint,
// style, schedule, and image work to composed helpers.
class SmtGdiRenderDevice : public SmtRenderDevice {
  friend class GdiUiController;
  friend class GdiImageIo;

 public:
  SmtGdiRenderDevice(HINSTANCE hInst);
  virtual ~SmtGdiRenderDevice(void);

  int Init(HWND hWnd, const char *logname);
  int Destroy(void);
  int Release(void);
  // Runs Release(); returns true when the device must not be deleted
  // (detached render worker still references Viewport members).
  bool release_may_leak();

  int Resize(int orgx, int orgy, int cx, int cy);

 public:
  int Lock();
  int Unlock();

  int Refresh(void);
  int Refresh(const SmtMap *pMap, fRect rect);
  int RefreshDirectly(const SmtMap *pMap, lRect rect, bool bRealTime = false);

  int ZoomMove(const SmtMap *pMap, fPoint dbfPointOffset,
               bool bRealTime = false);
  int ZoomScale(const SmtMap *pMap, lPoint orgPoint, float fscale,
                bool bRealTime = false);
  int ZoomToRect(const SmtMap *pMap, fRect rect, bool bRealTime = false);

  // Viewport + last-buffer StretchBlt only -- no tessellate. Pair with
  // ScheduleDelayedRedraw so the worker wakes after ~200 ms idle.
  // PreviewZoomScale also stretches vir_viewport2 (MapLibre transform preview).
  int PreviewZoomScale(lPoint orgPoint, float fscale) override;
  int PreviewZoomMove(fPoint dbfPointOffset) override;
  int ScheduleDelayedRedraw(const SmtMap *pMap) override;

  int Timer();

 private:
  // Stage a map FrameJob (viewport snapshot + map*). Timer submits unless
  // urgent forces the next tick / immediate idle wake.
  int stage_map_job(const SmtMap *pMap, int x, int y, int w, int h, int op,
                    bool urgent);
  // Wake the worker once when idle; arm present-on-published_gen.
  bool submit_staged_job();
  void invalidate_map_present();

  // Sync host_rc_ + canvas links from device viewport/windowport/pra.
  void sync_host_paint_context();

  // Shared by RefreshDirectly / Zoom* after viewport/windowport updates.
  int rerender_map(const SmtMap* map, bool realtime);

  // MAP/DYNAMIC/QUICK BeginRender body: encoder begin + optional clear + DC.
  // Returns false when |fail_if_busy| and the worker owns the shared front.
  bool begin_surface_encode_pass(GdiOwnedSurface& buf, COLORREF clear_color,
                                 bool clear, bool fail_if_busy);
  void end_surface_encode_pass(GdiOwnedSurface& buf);

 public:
  int LPToDP(float x, float y, long &X, long &Y) const;
  int DPToLP(LONG X, LONG Y, float &x, float &y) const;
  int LRectToDRect(const fRect &frect, lRect &lrect) const;
  int DRectToLRect(const lRect &lrect, fRect &frect) const;

  int ReRenderMapByProxy(const SmtMap *pMap, int x, int y, int w, int h,
                         int op = R2_COPYPEN);
  int ReRenderMapRealTime(const SmtMap *pMap, int x, int y, int w, int h,
                          int op = R2_COPYPEN);

  int RenderMap(void);
  int RenderMapToDC(HDC hdc) override;

 public:
  int BeginRender(eRDBufferLayer eMRDBufLyr, bool bClear = false,
                  const SmtStyle *pStyle = NULL, int op = R2_COPYPEN);
  int EndRender(eRDBufferLayer eMRDBufLyr);

  int RenderMap(const SmtMap *pMap, int op = R2_COPYPEN);
  int RenderLayer(const SmtLayer *pLayer,
                  int op = R2_COPYPEN);  // 1-16 R2_BLACK-R2_WHITE
  int RenderLayer(OGRLayer *pLayer, int op = R2_COPYPEN);
  int RenderLayer(const SmtRasterLayer *pLayer,
                  int op = R2_COPYPEN);  // 1-16 R2_BLACK-R2_WHITE
  int RenderLayer(const SmtTileLayer *pLayer,
                  int op = R2_COPYPEN);  // 1-16 R2_BLACK-R2_WHITE
  int RenderFeature(OGRFeature *pFeature, int op = R2_COPYPEN);
  int RenderGeometry(const OGRGeometry *pGeom, const SmtStyle *pStyle,
                     int op = R2_COPYPEN);

 public:
  int DrawMultiLineString(const OGRMultiLineString *pMultiLinestring);
  int DrawMultiPoint(const SmtStyle *pStyle, const OGRMultiPoint *pMultiPoint);
  int DrawMultiPolygon(const OGRMultiPolygon *pMultiPolygon);

  int DrawPoint(const SmtStyle *pStyle, const OGRPoint *pPoint);
  int DrawAnno(const char *szAnno, float fangel, float fCHeight, float fCWidth,
               float fCSpace, const OGRPoint *pPoint);
  int DrawSymbol(HICON hIcon, long lHeight, long lWhidth,
                 const OGRPoint *pPoint);

  int DrawLineString(const OGRLineString *pLinestring);
  int DrawLineSpline(const OGRLineString *pSpline);
  int DrawLinearRing(const OGRLinearRing *pLinearRing);
  int DrawPloygon(const OGRPolygon *pPloygon);

  int DrawTin(const SmtTin *pTin);
  int DrawGrid(const SmtGrid *pGrid);
  int DrawArc(const OGRLineString *pArc);
  int DrawFan(const OGRPolygon *pFan);

 public:
  int DrawEllipse(float left, float top, float right, float bottom,
                  bool bDP = false);
  int DrawRect(const fRect &rect, bool bDP = false);
  int DrawLine(fPoint *pfPoints, int nCount, bool bDP = false);
  int DrawLine(const fPoint &ptA, const fPoint &ptB, bool bDP = false);
  int DrawText(const char *szAnno, float fangel, float fCHeight, float fCWidth,
               float fCSpace, const fPoint &point, bool bDP = false);
  int DrawImage(const char *szImageBuf, int nImageBufSize, const fRect &frect,
                long lCodeType, eRDBufferLayer eMRDBufLyr = MRD_BL_MAP);
  int StrethImage(const char *szImageBuf, int nImageBufSize, const fRect &frect,
                  long lCodeType, eRDBufferLayer eMRDBufLyr = MRD_BL_MAP);

 public:
  int SaveImage(const char *szFilePath, eRDBufferLayer eMRDBufLyr = MRD_BL_MAP,
                bool bBgTransparent = false);
  int Save2ImageBuf(char *&szImageBuf, long &lImageBufSize, long lCodeType,
                    eRDBufferLayer eMRDBufLyr = MRD_BL_MAP,
                    bool bBgTransparent = false);
  int FreeImageBuf(char *&szImageBuf);

 protected:
  int PrepareForDrawing(const SmtStyle *pStyle, int nDrawMode = R2_COPYPEN);
  int EndDrawing();

 public:
  detail::GdiPaintCanvas &canvas() { return paint_canvas_; }
  const detail::GdiPaintCanvas &canvas() const { return paint_canvas_; }
  GdiUiController &ui() { return ui_controller_; }
  const GdiUiController &ui() const { return ui_controller_; }
  std::mutex &shared_front_mutex() { return shared_front_mu_; }

  // Last completed MAP/DYNAMIC/QUICK pass (Task 3 wire; Draw* record is Task
  // 4).
  const GdiCommandBuffer &last_pass() const { return last_pass_; }

 protected:
  GdiUiController ui_controller_;
  GdiImageIo image_io_;

  Viewport vir_viewport1_;
  Viewport vir_viewport2_;

  GdiOwnedSurface map_front_;
  GdiOwnedSurface dynamic_buf_;
  GdiOwnedSurface raster_back_;
  GdiOwnedSurface compose_buf_;

  // Host UI-thread paint instance (not shared with the worker).
  // layer_painter_ back = quick; shared_front = map (must differ).
  detail::GdiPaintCanvas paint_canvas_;
  detail::GdiLayerPainter layer_painter_;
  SmtRenderContex host_rc_;

 protected:
  SmtGdiRenderThread *render_thread_;

  // Set when HWND-thread stop detaches a still-running worker. Destroy then
  // leaks this device until process exit instead of UAF on Viewport refs.
  bool m_leak_on_close_ = false;

  // Serializes UI Refresh/OnDraw compose against worker publish to the
  // shared map front (same HBITMAP via share_from). Without this, wheel /
  // ZoomScale Refresh races FrameJob and fail-fasts (0xC0000409).
  std::mutex shared_front_mu_;

  std::mutex lock_;

  // Appended after legacy fields so TUs that touch render_thread_ keep stable
  // offsets (Task 3 wire; Draw* record is Task 4). DIRECT uses HWND DC only.
  detail::GdiCommandEncoder pass_encoder_;
  GdiCommandBuffer last_pass_;

  // Windowport/fblc of the last published map front (MapLibre preview baseline).
  // Appended at end to avoid shifting GdiOwnedSurface member offsets.
  Windowport painted_windowport_{};
  double painted_fblc_ = 1.0;
  bool has_painted_preview_ = false;

  // Snapshot current windowport as the painted baseline (Timer present / settle).
  void note_painted_preview_baseline();
};
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_GDI_RENDERDEVICE_H
