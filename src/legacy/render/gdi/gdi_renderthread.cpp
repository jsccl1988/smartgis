#include "legacy/render/gdi/gdi_renderthread.h"

#include "base/core/api.h"
#include "legacy/render/bridge/leftover_record.h"
#include "legacy/render/gdi/map_carto2d.h"
#include "base/core/log.h"
#include "legacy/render/gdi/resource.h"
#include "ogrsf_frmts.h"
#include "base/carto/style_api.h"

namespace render {
SmtGdiRenderThread::SmtGdiRenderThread(HINSTANCE hInst, Viewport &virViewport1,
                                       Viewport &virViewport2)
    : m_bRendering(false),
      m_hWnd(NULL),
      m_hInst(m_hInst),
      m_hCurDC(NULL),
      m_virViewport1(virViewport1),
      m_virViewport2(virViewport2),
      m_bCurUseStyle(false),
      m_bLockStyle(false),
      m_labelPriority(5),
      m_isRiver(false),
      m_carto2d(new MapCarto2dFrame()) {
  ;
}

SmtGdiRenderThread::~SmtGdiRenderThread(void) {
  stop();
  delete m_carto2d;
  m_carto2d = nullptr;
}

void SmtGdiRenderThread::start() {
  if (m_thread.joinable()) {
    return;
  }
  m_stop = false;
  m_suspend = true;
  m_thread = std::thread([this]() { run(nullptr); });
}

void SmtGdiRenderThread::suspend() { m_suspend = true; }

void SmtGdiRenderThread::resume() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_suspend = false;
  }
  m_cv.notify_one();
}

void SmtGdiRenderThread::stop() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Drop map alias before join so a late RenderMap cannot UAF
    // layers freed by SmtApp::Destory after DestroyWindow returns.
    m_smtRC.pMap = nullptr;
    m_stop = true;
    m_suspend = false;
  }
  m_cv.notify_one();
  if (!m_thread.joinable()) {
    return;
  }
  // Do not PeekMessage/DispatchMessage here: stop() runs from
  // OnDestroy → EndDestory → device Release. Pumping inside
  // DestroyWindow re-enters window procs and yields STATUS_FATAL_APP_EXIT
  // (0xC000041D).
  // EndDestory runs on the HWND thread; RenderMap may wait on that same
  // HWND (GDI), so join() deadlocks and --self-test never ExitProcess.
  const DWORD hwnd_tid =
      m_hWnd ? ::GetWindowThreadProcessId(m_hWnd, nullptr) : 0;
  if (hwnd_tid != 0 && hwnd_tid == ::GetCurrentThreadId()) {
    m_thread.detach();
    return;
  }
  m_thread.join();
}

void SmtGdiRenderThread::run(void * /*pParam*/) {
  while (!m_stop) {
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_cv.wait(lock, [this]() { return !m_suspend || m_stop; });
    }
    if (m_stop) {
      break;
    }
    m_bRendering = true;
    RenderMap(m_smtRC.pMap, m_smtRC.orgx, m_smtRC.orgy, m_smtRC.width,
              m_smtRC.height, m_smtRC.op);
    m_bRendering = false;
    m_suspend = true;
  }
}

//////////////////////////////////////////////////////////////////////////
int SmtGdiRenderThread::Init(HWND hWnd, const char *logname) {
  if (hWnd == NULL || logname == NULL) return SMT_ERR_INVALID_PARAM;

  m_hWnd = hWnd;
  m_smtRenderBuf.SetWnd(m_hWnd);
  m_strLogName = logname;

  return SMT_ERR_NONE;
}

int SmtGdiRenderThread::Resize(int orgx, int orgy, int cx, int cy) {
  if (cx < 0 || cy < 0) return SMT_ERR_FAILURE;

  if (IsRendering()) return SMT_ERR_FAILURE;

  if (is_equal(m_smtRC.viewport.m_fVOX, orgx, dEPSILON) &&
      is_equal(m_smtRC.viewport.m_fVOY, orgy, dEPSILON) &&
      is_equal(m_smtRC.viewport.m_fVHeight, cy, dEPSILON) &&
      is_equal(m_smtRC.viewport.m_fVWidth, cx, dEPSILON)) {
    return SMT_ERR_FAILURE;
  }

  m_smtRC.viewport.m_fVOX = orgx;
  m_smtRC.viewport.m_fVOY = orgy;
  m_smtRC.viewport.m_fVHeight = cy;
  m_smtRC.viewport.m_fVWidth = cx;

  if (is_equal(m_smtRC.windowport.m_fWWidth, 0, dEPSILON) ||
      is_equal(m_smtRC.windowport.m_fWHeight, 0, dEPSILON)) {
    m_smtRC.fblc = 1.f;
  } else {
    float xblc, yblc;
    xblc = m_smtRC.viewport.m_fVWidth / m_smtRC.windowport.m_fWWidth;
    yblc = m_smtRC.viewport.m_fVHeight / m_smtRC.windowport.m_fWHeight;
    m_smtRC.fblc = (xblc > yblc) ? yblc : xblc;
  }

  if (SMT_ERR_NONE == m_smtRenderBuf.SetBufSize(m_smtRC.viewport.m_fVWidth,
                                                m_smtRC.viewport.m_fVHeight) &&
      SMT_ERR_NONE == m_smtRenderBuf.SwapBuf(
                          m_smtRC.viewport.m_fVOX, m_smtRC.viewport.m_fVWidth,
                          m_smtRC.viewport.m_fVWidth,
                          m_smtRC.viewport.m_fVHeight, 0, 0)) {
    return SMT_ERR_NONE;
  }

  return SMT_ERR_FAILURE;
}

int SmtGdiRenderThread::ShareBuf(SmtRenderBuf &smtSharedBuf) {
  return m_smtSharedBuf.ShareBuf(smtSharedBuf);
}

//////////////////////////////////////////////////////////////////////////
int SmtGdiRenderThread::LPToDP(float x, float y, LONG &X, LONG &Y) const {
  if (is_equal(m_smtRC.windowport.m_fWWidth, 0, dEPSILON) &&
      is_equal(m_smtRC.windowport.m_fWHeight, 0, dEPSILON) &&
      is_equal(m_smtRC.viewport.m_fVWidth, 0, dEPSILON) &&
      is_equal(m_smtRC.viewport.m_fVHeight, 0, dEPSILON)) {
    X = x;
    Y = y;

    return SMT_ERR_FAILURE;
  }

  X = LONG(m_smtRC.viewport.m_fVOX +
           (x - m_smtRC.windowport.m_fWOX) * m_smtRC.fblc + 0.5);
  Y = LONG(m_smtRC.viewport.m_fVOY +
           (y - m_smtRC.windowport.m_fWOY) * m_smtRC.fblc + 0.5);

  Y = m_smtRC.viewport.m_fVHeight - Y;

  return SMT_ERR_FAILURE;
}

int SmtGdiRenderThread::DPToLP(LONG X, LONG Y, float &x, float &y) const {
  if (is_equal(m_smtRC.windowport.m_fWWidth, 0, dEPSILON) &&
      is_equal(m_smtRC.windowport.m_fWHeight, 0, dEPSILON) &&
      is_equal(m_smtRC.viewport.m_fVWidth, 0, dEPSILON) &&
      is_equal(m_smtRC.viewport.m_fVHeight, 0, dEPSILON)) {
    x = X;
    y = Y;

    return SMT_ERR_FAILURE;
  }

  Y = m_smtRC.viewport.m_fVHeight - Y;

  x = (X - m_smtRC.viewport.m_fVOX) / m_smtRC.fblc + m_smtRC.windowport.m_fWOX;
  y = (Y - m_smtRC.viewport.m_fVOY) / m_smtRC.fblc + m_smtRC.windowport.m_fWOY;

  return SMT_ERR_FAILURE;
}

int SmtGdiRenderThread::LRectToDRect(const fRect &frect, lRect &lrect) const {
  LPToDP(frect.lb.x, frect.lb.y, lrect.lb.x, lrect.lb.y);
  LPToDP(frect.rt.x, frect.rt.y, lrect.rt.x, lrect.rt.y);

  return SMT_ERR_FAILURE;
}

int SmtGdiRenderThread::DRectToLRect(const lRect &lrect, fRect &frect) const {
  DPToLP(lrect.lb.x, lrect.lb.y, frect.lb.x, frect.lb.y);
  DPToLP(lrect.rt.x, lrect.rt.y, frect.rt.x, frect.rt.y);

  return SMT_ERR_FAILURE;
}

//////////////////////////////////////////////////////////////////////////
int SmtGdiRenderThread::RenderMap(const SmtMap *pMap, int x, int y, int w,
                                  int h, int op) {
  if (w == 0 || h == 0) return SMT_ERR_INVALID_PARAM;

  m_smtRenderBuf.ClearBuf(x, y, w,
                          h /*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);
  m_hCurDC = m_smtRenderBuf.PrepareDC();
  m_carto2d->reset(m_smtRC.fblc, w, h);

  if (pMap != NULL) {
    for (int i = 0; i < pMap->GetLayerCount(); ++i) {
      if (!pMap->IsLayerVisible(i)) {
        continue;
      }
      if (OGRLayer *ogr = const_cast<OGRLayer *>(pMap->GetOgrLayer(i))) {
        RenderLayer(ogr, op);
      } else {
        RenderLayer(pMap->GetLeftoverLayer(i), op);
      }
    }
    // After GDI draw: Null recording must not precede BitBlt (white canvas).
    render::scene::leftover_record_map_frame(
        m_hWnd, static_cast<uint32_t>(w), static_cast<uint32_t>(h), pMap);
  }

  m_smtRenderBuf.EndDC();
  m_hCurDC = NULL;

  m_smtSharedBuf.ClearBuf(x, y, w,
                          h /*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);
  m_smtRenderBuf.SwapBuf(m_smtSharedBuf, x, y, w, h, x, y, w, h,
                         BLT_TRANSPARENT,
                         SRCCOPY /*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);

  m_virViewport1 = m_smtRC.viewport;
  m_virViewport2 = m_smtRC.viewport;

  return SMT_ERR_FAILURE;
}

int SmtGdiRenderThread::SwapBuf(int destOrgx, int destOrgy, int destW,
                                int destH, int srcOrgx, int srcOrgy, int op) {
  if (!IsRendering())
    return m_smtRenderBuf.SwapBuf(destOrgx, destOrgy, destW, destH, srcOrgx,
                                  srcOrgy, op);

  return SMT_ERR_FAILURE;
}

int SmtGdiRenderThread::SwapBuf(int destOrgx, int destOrgy, int destW,
                                int destH, int srcOrgx, int srcOrgy, int srcW,
                                int srcH, eSwapType type, int op,
                                COLORREF clr) {
  if (!IsRendering())
    return m_smtRenderBuf.SwapBuf(destOrgx, destOrgy, destW, destH, srcOrgx,
                                  srcOrgy, srcW, srcH, type, op, clr);

  return SMT_ERR_FAILURE;
}

int SmtGdiRenderThread::SwapBuf(SmtRenderBuf &rbTarget, int destOrgx,
                                int destOrgy, int destW, int destH, int srcOrgx,
                                int srcOrgy, int op) {
  if (!IsRendering())
    return m_smtRenderBuf.SwapBuf(destOrgx, destOrgy, destW, destH, srcOrgx,
                                  srcOrgy, op);

  return SMT_ERR_FAILURE;
}

int SmtGdiRenderThread::SwapBuf(SmtRenderBuf &rbTarget, int destOrgx,
                                int destOrgy, int destW, int destH, int srcOrgx,
                                int srcOrgy, int srcW, int srcH, eSwapType type,
                                int op, COLORREF clr) {
  if (!IsRendering())
    return m_smtRenderBuf.SwapBuf(rbTarget, destOrgx, destOrgy, destW, destH,
                                  srcOrgx, srcOrgy, srcW, srcH, type, op, clr);

  return SMT_ERR_FAILURE;
}
}  // namespace render