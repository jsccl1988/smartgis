// Copyright (c) 2010 CCL. All rights reserved.
// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef _GDI_RENDERDEVICE_H
#define _GDI_RENDERDEVICE_H

#include <cstdint>
#include <memory>
#include <mutex>

#include "plugin/product/world3d/scene/orthogrid/lattice/ortho_lattice.h"
#include "gis/feature/feature.h"
#include "gis/map/map.h"
#include "scenic/render/err.h"
#include "scenic/render/rhi2d/impl/common/host/buffer_image.h"
#include "scenic/render/rhi2d/impl/common/host/present_controller.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/carto_draw.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/frame/context.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/encode/command_encoder.h"
#include "scenic/render/rhi2d/impl/common/paint/map/map_painter.h"
#include "scenic/render/rhi2d/impl/common/surface/dib/owned.h"
#include "scenic/render/rhi2d/public/device/render_device.h"

using namespace base;
using namespace gis;

namespace scenic {
namespace detail {

class Rhi2dLayerTreeHost;

// GDI map render device facade: owns buffers/viewport and forwards paint,
// style, schedule, and image work to composed helpers.
class Rhi2dRenderDevice : public RenderDevice2d {
  friend class Rhi2dPresentController;
  friend class Rhi2dBufferImage;

 public:
  Rhi2dRenderDevice(HINSTANCE hInst);
  virtual ~Rhi2dRenderDevice(void);

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
  int Refresh(const Map *pMap, fRect rect);
  int RefreshDirectly(const Map *pMap, lRect rect, bool bRealTime = false);

  int ZoomMove(const Map *pMap, fPoint dbfPointOffset,
               bool bRealTime = false);
  int ZoomScale(const Map *pMap, lPoint orgPoint, float fscale,
                bool bRealTime = false);
  int ZoomToRect(const Map *pMap, fRect rect, bool bRealTime = false);

  // Viewport + last-buffer StretchBlt only -- no tessellate. Pair with
  // ScheduleDelayedRedraw so the worker wakes after ~200 ms idle.
  // PreviewZoomScale also stretches vir_viewport2 (MapLibre transform preview).
  int PreviewZoomScale(lPoint orgPoint, float fscale) override;
  int PreviewZoomMove(fPoint dbfPointOffset) override;
  int ScheduleDelayedRedraw(const Map *pMap) override;
  int ScheduleUrgentRedraw(const Map *pMap) override;

  int Timer();

 private:
  // Stage a map FrameJob (viewport snapshot + map*). Timer submits unless
  // urgent forces the next tick / immediate idle wake.
  int stage_map_job(const Map *pMap, int x, int y, int w, int h, int op,
                    bool urgent);
  // Wake the worker once when idle; arm present-on-published_gen.
  bool submit_staged_job();
  void invalidate_map_present();

  // Sync host_rc_ + canvas links from device viewport/windowport/options.
  void sync_host_paint_context();

  // Shared by RefreshDirectly / Zoom* after viewport/windowport updates.
  int rerender_map(const Map* map, bool realtime);

  // Stretch last published front around |org| (device px); clears pan-slide.
  void apply_stretch_preview(float org_x, float org_y);
  // Rubber-band focus in *current* windowport (call before fit).
  bool rubber_band_device_focus(const fRect& rect, float* org_x,
                                float* org_y) const;
  // First Edit / china bootstrap: cancel worker, sync paint, then Refresh.
  int paint_map_bootstrap_sync(const Map* map);

  // MAP/DYNAMIC/QUICK BeginRender body: encoder begin + optional clear + DC.
  // Returns false when |fail_if_busy| and the worker owns the shared front.
  bool begin_surface_encode_pass(Rhi2dOwnedSurface& buf, COLORREF clear_color,
                                 bool clear, bool fail_if_busy);
  void end_surface_encode_pass(Rhi2dOwnedSurface& buf);

 public:
  int LPToDP(float x, float y, long &X, long &Y) const;
  int DPToLP(LONG X, LONG Y, float &x, float &y) const;
  int LRectToDRect(const fRect &frect, lRect &lrect) const;
  int DRectToLRect(const lRect &lrect, fRect &frect) const;

  int ReRenderMapByProxy(const Map *pMap, int x, int y, int w, int h,
                         int op = R2_COPYPEN);
  int ReRenderMapRealTime(const Map *pMap, int x, int y, int w, int h,
                          int op = R2_COPYPEN);

  int RenderMap(void);
  int RenderMapToDC(HDC hdc) override;

 public:
  int BeginRender(eRDBufferLayer eMRDBufLyr, bool bClear = false,
                  const Style *pStyle = nullptr, int op = R2_COPYPEN);
  int EndRender(eRDBufferLayer eMRDBufLyr);

  int RenderMap(const Map *pMap, int op = R2_COPYPEN);
  int RenderLayer(const MapLayer *pLayer, int op = R2_COPYPEN);
  int RenderLayer(OGRLayer *pLayer, int op = R2_COPYPEN);
  int RenderLayer(const datasource::OgrRasterLayer *pLayer,
                  int op = R2_COPYPEN);
  int RenderLayer(const tile::ProviderTileLayer *pLayer, int op = R2_COPYPEN);
  int RenderFeature(OGRFeature *pFeature, int op = R2_COPYPEN);
  int RenderGeometry(const OGRGeometry *pGeom, const Style *pStyle,
                     int op = R2_COPYPEN);

 public:
  int DrawMultiLineString(const OGRMultiLineString *pMultiLinestring);
  int DrawMultiPoint(const Style *pStyle, const OGRMultiPoint *pMultiPoint);
  int DrawMultiPolygon(const OGRMultiPolygon *pMultiPolygon);

  int DrawPoint(const Style *pStyle, const OGRPoint *pPoint);
  int DrawAnno(const char *szAnno, float fangel, float fCHeight, float fCWidth,
               float fCSpace, const OGRPoint *pPoint);
  int DrawSymbol(HICON hIcon, long lHeight, long lWhidth,
                 const OGRPoint *pPoint);

  int DrawLineString(const OGRLineString *pLinestring);
  int DrawLineSpline(const OGRLineString *pSpline);
  int DrawLinearRing(const OGRLinearRing *pLinearRing);
  int DrawPloygon(const OGRPolygon *pPloygon);

  int DrawTin(const OGRTriangulatedSurface *pTin);
  int DrawGrid(const plugin::detail::OrthoLattice* pGrid);
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
  int PrepareForDrawing(const Style *pStyle, int nDrawMode = R2_COPYPEN);
  int EndDrawing();

 public:
  Rhi2dCartoDraw &carto_draw() { return *carto_draw_; }
  const Rhi2dCartoDraw &carto_draw() const { return *carto_draw_; }
  Rhi2dPresentController &present() { return present_; }
  const Rhi2dPresentController &present() const { return present_; }
  Rhi2dBufferImage &buffer_image() { return buffer_image_; }
  const Rhi2dBufferImage &buffer_image() const { return buffer_image_; }
  std::mutex &shared_front_mutex() { return shared_front_mu_; }

  // Last completed MAP/DYNAMIC/QUICK pass (Task 3 wire; Draw* record is Task
  // 4).
  const Rhi2dCommandBuffer &last_pass() const { return last_pass_; }

 protected:
  Rhi2dPresentController present_;
  Rhi2dBufferImage buffer_image_;

  Viewport vir_viewport1_;
  Viewport vir_viewport2_;

  Rhi2dOwnedSurface map_front_;
  Rhi2dOwnedSurface dynamic_buf_;
  // QUICK encode target (BeginRender). Not the FrameJob paint-back (that is
  // LayerTreeHost::back_buf_).
  Rhi2dOwnedSurface raster_back_;
  Rhi2dOwnedSurface compose_buf_;

  // True after EndRender(MRD_BL_DYNAMIC) so compose can skip an empty
  // color-key scan (full-frame TransparentBlt is ~1s in Debug).
  bool dynamic_overlay_live_ = false;

  // HWND-thread CartoDraw for BeginRender / Draw* (encoder sync pen).
  // Map FrameJob paint owns a separate CartoDraw on LayerTreeHost.
  std::unique_ptr<Rhi2dCartoDraw> carto_draw_;
  // BeginRender overlay only (RenderLayer/Feature/Geometry into host
  // encoder). Full-map frames go through layer_tree_host_->paint_map_sync /
  // submit_frame ? never this painter.
  std::unique_ptr<Rhi2dPainter> overlay_painter_;
  RenderContext host_rc_;

 protected:
  // cc frame owner (commit/activate/draw + sole map painter + back_buf).
  // Published map front stays on this device (map_front_); LTH shared_buf_
  // is share_from(map_front_).
  Rhi2dLayerTreeHost* layer_tree_host_;

  // Set when HWND-thread stop detaches a still-running worker. Destroy then
  // leaks this device until process exit instead of UAF on Viewport refs.
  bool m_leak_on_close_ = false;

  // Serializes UI Refresh/OnDraw compose against worker publish to the
  // shared map front (same HBITMAP via share_from). Without this, wheel /
  // ZoomScale Refresh races FrameJob and fail-fasts (0xC0000409).
  std::mutex shared_front_mu_;

  std::mutex lock_;

  // Appended after legacy fields so TUs that touch layer_tree_host_ keep
  // stable offsets (Task 3 wire; Draw* record is Task 4). DIRECT uses HWND DC only.
  Rhi2dCommandEncoder pass_encoder_;
  Rhi2dCommandBuffer last_pass_;

  // Windowport/fblc of the last published map front (MapLibre preview baseline).
  // Appended at end to avoid shifting Rhi2dOwnedSurface member offsets.
  Windowport painted_windowport_{};
  double painted_fblc_ = 1.0;
  bool has_painted_preview_ = false;

  // Snapshot current windowport as the painted baseline (Timer present / settle).
  void note_painted_preview_baseline();
  uint64_t map_published_generation() const override;
};
}  // namespace detail
}  // namespace scenic

#if !defined(SCENIC_IMPL_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  //_GDI_RENDERDEVICE_H
