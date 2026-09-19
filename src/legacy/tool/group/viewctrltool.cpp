#include "legacy/tool/group/viewctrltool.h"

#include <assert.h>
#include <math.h>

#include <cstdint>

#include "base/core/api.h"
#include "legacy/tool/group/resource.h"
#include "legacy/tool/t_iatoolmanager.h"
#include "base/carto/style_api.h"
#include "base/carto/stylemanager.h"
#include "gis/feature/feature.h"
#include "gis/map/map.h"
#include "sys/sysmanager.h"
#include "tool/legacy_msg.h"

using namespace render;
using namespace base;
using namespace gis;
using namespace base;
using namespace sys;

const string CST_STR_MAPVIEWCTRL_TOOL_NAME = "地图控制";

namespace tool {
SmtViewCtrlTool::SmtViewCtrlTool()
    : m_bCaptured(FALSE), m_usFlashed(0), m_viewMode(VM_ZoomOff) {
  set_name(CST_STR_MAPVIEWCTRL_TOOL_NAME.c_str());
}

SmtViewCtrlTool::~SmtViewCtrlTool() { UnRegisterMsg(); }

int SmtViewCtrlTool::Init(LPRENDERDEVICE pMrdRenderDevice, SmtMap *pOperSmtMap,
                          HWND hWnd, pfnToolCallBack pfnCallBack,
                          void *pToFollow) {
  if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice, pOperSmtMap, hWnd,
                                        pfnCallBack, pToFollow)) {
    return SMT_ERR_FAILURE;
  }

  UINT idCursors[] = {IDC_CURSOR_ZOOMIN, IDC_CURSOR_ZOOMOUT,
                      IDC_CURSOR_ZOOMMOVE, IDC_CURSOR_IDENTIFY};

  int nCount = sizeof(idCursors) / sizeof(UINT);

  for (int i = 0; i < nCount; i++)
    m_hCursors[i] = ::LoadCursor(g_hInstance, MAKEINTRESOURCE(idCursors[i]));

  append_func_items("放大", GT_MSG_VIEW_ZOOMIN, FIM_2DVIEW | FIM_2DMFMENU);
  append_func_items("缩小", GT_MSG_VIEW_ZOOMOUT, FIM_2DVIEW | FIM_2DMFMENU);
  append_func_items("移动", GT_MSG_VIEW_ZOOMMOVE, FIM_2DVIEW | FIM_2DMFMENU);
  append_func_items("复位", GT_MSG_VIEW_ZOOMRESTORE, FIM_2DVIEW | FIM_2DMFMENU);
  append_func_items("刷新", GT_MSG_VIEW_ZOOMREFRESH, FIM_2DVIEW | FIM_2DMFMENU);
  // append_func_items("��ά��ͼ",GT_MSG_3DVIEW_ACTIVE,FIM_2DVIEW);

  SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ZOOMIN);
  SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ZOOMOUT);
  SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ZOOMMOVE);
  SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ZOOMRESTORE);
  SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ZOOMREFRESH);
  // SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ACTIVE);

  SMT_IATOOL_APPEND_MSG(GT_MSG_SET_VIEW_MODE);
  SMT_IATOOL_APPEND_MSG(GT_MSG_GET_VIEW_MODE);
  SMT_IATOOL_APPEND_MSG(GT_MSG_SET_SCALEDELT);
  SMT_IATOOL_APPEND_MSG(GT_MSG_GET_SCALEDELT);

  RegisterMsg();

  return SMT_ERR_NONE;
}

int SmtViewCtrlTool::AuxDraw() { return SMT_ERR_NONE; }

int SmtViewCtrlTool::Timer() { return SMT_ERR_NONE; }

int SmtViewCtrlTool::notify(long nMsg, SmtListenerMsg &param) {
  if (param.hSrcWnd != m_hWnd) {
    switch (nMsg) {
      case GT_MSG_VIEW_ZOOMRESTORE: {
        tool::try_execute_gt_msg(m_workspace, nMsg);
        m_viewMode = VM_ZoomRestore;
        ZoomRestore();
      } break;
      case GT_MSG_VIEW_ZOOMREFRESH: {
        tool::try_execute_gt_msg(m_workspace, nMsg);
        m_viewMode = VM_ZoomRefresh;
        ZoomRefresh();
      } break;
      case GT_MSG_VIEW_ACTIVE: {
        SetForegroundWindow(m_hWnd);
      } break;
    }
  } else {
    switch (nMsg) {
      case GT_MSG_VIEW_ZOOMIN: {
        tool::try_execute_gt_msg(m_workspace, nMsg);
        m_viewMode = VM_ZoomIn;
        OnSetViewMode();
      } break;
      case GT_MSG_VIEW_ZOOMOUT: {
        tool::try_execute_gt_msg(m_workspace, nMsg);
        m_viewMode = VM_ZoomOut;
        OnSetViewMode();
      } break;
      case GT_MSG_VIEW_ZOOMMOVE: {
        tool::try_execute_gt_msg(m_workspace, nMsg);
        m_viewMode = VM_ZoomMove;
        OnSetViewMode();
      } break;
      case GT_MSG_VIEW_ZOOMRESTORE: {
        tool::try_execute_gt_msg(m_workspace, nMsg);
        m_viewMode = VM_ZoomRestore;
        ZoomRestore();
      } break;
      case GT_MSG_VIEW_ZOOMREFRESH: {
        tool::try_execute_gt_msg(m_workspace, nMsg);
        m_viewMode = VM_ZoomRefresh;
        ZoomRefresh();
      } break;
      case GT_MSG_SET_VIEW_MODE: {
        m_viewMode = eViewMode(*(ushort *)param.wParam);

        switch (m_viewMode) {
          case VM_ZoomRestore:
            ZoomRestore();
            break;
          case VM_ZoomRefresh:
            ZoomRefresh();
            break;
          default:
            OnSetViewMode();
            break;
        }
      } break;
      case GT_MSG_GET_VIEW_MODE: {
        *(ushort *)param.wParam = m_viewMode;
      } break;
      case GT_MSG_SET_SCALEDELT: {
        m_fScaleDelt = *(double *)param.wParam;
      } break;
      case GT_MSG_GET_SCALEDELT: {
        *(double *)param.wParam = m_fScaleDelt;
      } break;
    }

    // Workspace owns exclusive input when bound; skip leftover SetActive.
    if (!m_workspace) {
      SetActive();
    }
  }

  return SMT_ERR_NONE;
}

int SmtViewCtrlTool::SetCursor(void) {
  switch (m_viewMode) {  // Zoom mode select
    case VM_ZoomOff:
      ::SetCursor(::LoadCursor(NULL, IDC_ARROW));
      break;
    case VM_ZoomIn:
      ::SetCursor(m_hCursors[CursorLoupePlus]);
      break;
    case VM_ZoomOut:
      ::SetCursor(m_hCursors[CursorLoupeMinus]);
      break;
    case VM_ZoomMove:
      ::SetCursor(m_hCursors[CursorMove]);
      break;
    case VM_ZoomRestore:
      ::SetCursor(m_hCrossCursor);
      break;
    case VM_ZoomRefresh:
      ::SetCursor(m_hCrossCursor);
      break;
    default:
      // All other zoom modes
      ::SetCursor(m_hCrossCursor);
      break;
  }

  return SMT_ERR_NONE;
}

int SmtViewCtrlTool::LButtonDown(uint nFlags, lPoint point) {
  if (m_workspace) {
    (void)nFlags;
    (void)point;
    return SMT_ERR_NONE;
  }
  return SmtBaseTool::LButtonDown(nFlags, point);
}

int SmtViewCtrlTool::LButtonUp(uint nFlags, lPoint point) {
  if (m_workspace) {
    (void)nFlags;
    (void)point;
    return SMT_ERR_NONE;
  }
  return SmtBaseTool::LButtonUp(nFlags, point);
}

int SmtViewCtrlTool::MouseMove(uint nFlags, lPoint point) {
  if (m_workspace) {
    (void)nFlags;
    (void)point;
    return SMT_ERR_NONE;
  }
  return SmtBaseTool::MouseMove(nFlags, point);
}

int SmtViewCtrlTool::MouseWeel(uint nFlags, short zDelta, lPoint point) {
  (void)nFlags;
  // Bound: wheel is owned by Workspace always-on / Draft observer.
  if (m_workspace) {
    return SMT_ERR_NONE;
  }
  POINT pnt;
  pnt.x = point.x;
  pnt.y = point.y;
  if (m_hWnd) {
    ScreenToClient(m_hWnd, &pnt);
  }
  ApplyWheel(zDelta, lPoint(pnt.x, pnt.y));
  return SMT_ERR_NONE;
}

void SmtViewCtrlTool::ApplyWheel(int z_delta, lPoint point) {
  if (!m_pRenderDevice) {
    return;
  }
  // |point| is client pixels. Positive wheel zooms toward the cursor.
  float fScale;
  if (z_delta < 0) {
    fScale = 1 + m_fScaleDelt;
  } else
    fScale = 1 - m_fScaleDelt;

  m_pRenderDevice->PreviewZoomScale(point, fScale);
  m_pRenderDevice->Refresh();
  m_pRenderDevice->ScheduleDelayedRedraw(m_pOperMap);
}

//////////////////////////////////////////////////////////////////////////
void SmtViewCtrlTool::ZoomMove(short mouse_status, base::lPoint point) {
  if (mouse_status != typeLButtonUp || !m_pRenderDevice) {
    return;
  }
  m_bCaptured = FALSE;
  lPoint curOrgPos;
  curOrgPos.x = 0;
  curOrgPos.y = 0;
  m_pRenderDevice->SetCurDrawingOrg(curOrgPos);

  float x1, y1, x2, y2;
  m_pRenderDevice->DPToLP(m_pntOrigin.x, m_pntOrigin.y, x1, y1);
  m_pRenderDevice->DPToLP(point.x, point.y, x2, y2);

  fPoint doffset(x2 - x1, y2 - y1);
  m_pRenderDevice->PreviewZoomMove(doffset);
  m_pRenderDevice->Refresh();
  m_pRenderDevice->ScheduleDelayedRedraw(m_pOperMap);
}

void SmtViewCtrlTool::ZoomIn(short mouse_status, base::lPoint point) {
  if (mouse_status != typeLButtonUp || !m_pRenderDevice) {
    return;
  }
  m_bCaptured = FALSE;
  m_pntCur = point;
  if (m_pntOrigin != point) {
    fRect frt;
    lRect lrt;
    lrt.lb.x = min(m_pntOrigin.x, m_pntCur.x);
    lrt.lb.y = max(m_pntOrigin.y, m_pntCur.y);
    lrt.rt.x = max(m_pntOrigin.x, m_pntCur.x);
    lrt.rt.y = min(m_pntOrigin.y, m_pntCur.y);

    m_pRenderDevice->DRectToLRect(lrt, frt);
    m_pRenderDevice->ZoomToRect(m_pOperMap, frt);
  } else {
    m_pRenderDevice->ZoomScale(m_pOperMap, point, 1 - m_fScaleDelt);
  }
  m_pRenderDevice->Refresh();
}

void SmtViewCtrlTool::ZoomOut(short mouse_status, base::lPoint point) {
  if (mouse_status != typeLButtonUp || !m_pRenderDevice) {
    return;
  }
  m_pRenderDevice->ZoomScale(m_pOperMap, point, 1 + m_fScaleDelt);
  m_pRenderDevice->Refresh();
}

void SmtViewCtrlTool::ZoomRestore() {
  if (m_pOperMap == NULL || m_pRenderDevice == NULL) {
    return;
  }

  Envelope envelope;
  fRect frt;

  // Prefer whole-map envelope so OGR-only layers (no leftover SmtLayer)
  // still frame. Fall back to active leftover when map envelope is empty.
  m_pOperMap->CalEnvelope();
  m_pOperMap->get_envelope(envelope);
  if (!envelope.is_init()) {
    SmtLayer *pLayer = m_pOperMap->GetActiveLayer();
    if (!pLayer) {
      return;
    }
    pLayer->CalEnvelope();
    pLayer->get_envelope(envelope);
    if (!envelope.is_init()) {
      return;
    }
  }

  envelope_to_rect(frt, envelope);

  float fWidthDiv = frt.width() / 40;
  float fHeightDiv = frt.height() / 40;
  if (fWidthDiv < 1e-6f) {
    fWidthDiv = 0.01f;
  }
  if (fHeightDiv < 1e-6f) {
    fHeightDiv = 0.01f;
  }

  frt.rt.x += fWidthDiv;
  frt.rt.y += fHeightDiv;
  frt.lb.x -= fWidthDiv;
  frt.lb.y -= fHeightDiv;

  // Proxy re-render only. Calling Refresh() while the worker still holds
  // shared GDI buffers races SMT_THREAD_SAFE mutexes / GetDC and has hung
  // the UI thread (Wait/UserRequest) after ZOOMRESTORE.
  m_pRenderDevice->ZoomToRect(m_pOperMap, frt, false);
}

void SmtViewCtrlTool::ZoomRefresh() {
  fRect frt;
  if (m_pOperMap != NULL) {
    Envelope envelope;
    fRect frt;

    SmtLayer *pLayer = m_pOperMap->GetActiveLayer();
    if (pLayer) {
      pLayer->CalEnvelope();
      pLayer->get_envelope(envelope);
      envelope_to_rect(frt, envelope);
      m_pRenderDevice->Refresh(m_pOperMap, frt);
    }
  }
}

void SmtViewCtrlTool::OnSetViewMode(void) { SetCursor(); }

void SmtViewCtrlTool::apply_draft(const tool::Draft &draft) {
  if (draft.kind == tool::DraftKind::kWheel) {
    lPoint pt{};
    if (!draft.points.empty()) {
      pt.x = draft.points.back().x_px;
      pt.y = draft.points.back().y_px;
    }
    ApplyWheel(draft.wheel, pt);
    return;
  }
  if (!m_pRenderDevice || draft.points.empty()) {
    return;
  }
  lPoint end(draft.points.back().x_px, draft.points.back().y_px);
  if (draft.points.size() >= 2) {
    m_pntOrigin.x = draft.points[0].x_px;
    m_pntOrigin.y = draft.points[0].y_px;
  }
  m_bCaptured = TRUE;
  switch (m_viewMode) {
    case VM_ZoomIn:
      ZoomIn(typeLButtonUp, end);
      break;
    case VM_ZoomOut:
      ZoomOut(typeLButtonUp, end);
      break;
    case VM_ZoomMove:
      ZoomMove(typeLButtonUp, end);
      break;
    default:
      // Empty-map / navigate: left-drag pans (Google / Baidu 2D).
      ZoomMove(typeLButtonUp, end);
      break;
  }
}
}  // namespace tool
