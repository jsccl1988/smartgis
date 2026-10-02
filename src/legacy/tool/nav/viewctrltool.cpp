// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/tool/nav/viewctrltool.h"

#include "legacy/sys/sysmanager.h"
#include "legacy/tool/msg/msg.h"
#include "legacy/tool/resource.h"
#include "legacy/tool/nav/view_zoom_apply.h"
#include "legacy/tool/abi/t_iatoolmanager.h"

using namespace render;
using namespace base;
using namespace gis;
using namespace sys;

const string CST_STR_MAPVIEWCTRL_TOOL_NAME = "地图控制";

namespace tool {
SmtViewCtrlTool::SmtViewCtrlTool()
    : m_bCaptured(FALSE), m_usFlashed(0), m_viewMode(VM_ZoomOff) {
  set_name(CST_STR_MAPVIEWCTRL_TOOL_NAME.c_str());
}

SmtViewCtrlTool::~SmtViewCtrlTool() { UnRegisterMsg(); }

int SmtViewCtrlTool::Init(LPRENDERDEVICE pMrdRenderDevice, SmtMap* pOperSmtMap,
                          HWND hWnd, pfnToolCallBack pfnCallBack,
                          void* pToFollow) {
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

  SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ZOOMIN);
  SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ZOOMOUT);
  SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ZOOMMOVE);
  SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ZOOMRESTORE);
  SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ZOOMREFRESH);

  SMT_IATOOL_APPEND_MSG(GT_MSG_SET_VIEW_MODE);
  SMT_IATOOL_APPEND_MSG(GT_MSG_GET_VIEW_MODE);
  SMT_IATOOL_APPEND_MSG(GT_MSG_SET_SCALEDELT);
  SMT_IATOOL_APPEND_MSG(GT_MSG_GET_SCALEDELT);

  RegisterMsg();

  return SMT_ERR_NONE;
}

int SmtViewCtrlTool::AuxDraw() { return SMT_ERR_NONE; }

int SmtViewCtrlTool::Timer() { return SMT_ERR_NONE; }

int SmtViewCtrlTool::notify(long nMsg, SmtListenerMsg& param) {
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
        m_viewMode = eViewMode(*(ushort*)param.wParam);

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
        *(ushort*)param.wParam = m_viewMode;
      } break;
      case GT_MSG_SET_SCALEDELT: {
        m_fScaleDelt = *(double*)param.wParam;
      } break;
      case GT_MSG_GET_SCALEDELT: {
        *(double*)param.wParam = m_fScaleDelt;
      } break;
    }

    if (!m_workspace) {
      SetActive();
    }
  }

  return SMT_ERR_NONE;
}

int SmtViewCtrlTool::SetCursor(void) {
  switch (m_viewMode) {
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
      ::SetCursor(m_hCrossCursor);
      break;
  }

  return SMT_ERR_NONE;
}

int SmtViewCtrlTool::LButtonDown(uint nFlags, lPoint point) {
  // Do not early-out when m_workspace is bound: WindowProc already skipped
  // CView when Workspace consumed the message. Reaching here means view.pan
  // was not on the stack (or declined) — leftover ZoomMove must still pan.
  return SmtBaseTool::LButtonDown(nFlags, point);
}

int SmtViewCtrlTool::LButtonUp(uint nFlags, lPoint point) {
  return SmtBaseTool::LButtonUp(nFlags, point);
}

int SmtViewCtrlTool::MouseMove(uint nFlags, lPoint point) {
  return SmtBaseTool::MouseMove(nFlags, point);
}

int SmtViewCtrlTool::MouseWeel(uint nFlags, short zDelta, lPoint point) {
  (void)nFlags;
  // |point| is screen coordinates (MFC OnMouseWheel). Do not early-out when
  // m_workspace is set: OnMouseWheel only calls this after host dispatch
  // failed, so skipping ApplyWheel swallowed wheel zoom in Edit.
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
  apply_wheel_zoom(m_pRenderDevice, m_pOperMap, m_fScaleDelt, z_delta, point);
}

void SmtViewCtrlTool::ZoomMove(short mouse_status, base::lPoint point) {
  if (mouse_status != typeLButtonUp) {
    return;
  }
  m_bCaptured = FALSE;
  apply_pan_by_points(m_pRenderDevice, m_pOperMap, m_pntOrigin, point,
                      /*gesture_end=*/true);
}

void SmtViewCtrlTool::ZoomIn(short mouse_status, base::lPoint point) {
  if (mouse_status != typeLButtonUp) {
    return;
  }
  m_bCaptured = FALSE;
  m_pntCur = point;
  apply_zoom_in_by_points(m_pRenderDevice, m_pOperMap, m_fScaleDelt,
                          m_pntOrigin, point);
}

void SmtViewCtrlTool::ZoomOut(short mouse_status, base::lPoint point) {
  if (mouse_status != typeLButtonUp) {
    return;
  }
  apply_zoom_out_at_point(m_pRenderDevice, m_pOperMap, m_fScaleDelt, point);
}

void SmtViewCtrlTool::ZoomRestore() {
  apply_zoom_restore(m_pRenderDevice, m_pOperMap);
}

void SmtViewCtrlTool::ZoomRefresh() {
  apply_zoom_refresh(m_pRenderDevice, m_pOperMap);
}

void SmtViewCtrlTool::OnSetViewMode(void) { SetCursor(); }

void SmtViewCtrlTool::apply_draft(const tool::Draft& draft) {
  apply_view_draft(m_pRenderDevice, m_pOperMap, m_fScaleDelt, m_viewMode,
                   &m_pntOrigin, &m_bCaptured, draft);
}
}  // namespace tool
