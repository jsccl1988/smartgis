#include "legacy/tool/group/3dviewctrltool.h"

#include <cstring>

#include "legacy/render/scene3d/bl3d_object.h"
#include "legacy/tool/group/defs.h"
#include "legacy/tool/group/resource.h"
#include "tool/legacy_msg.h"

const string CST_STR_3DVIEWCTRL_TOOL_NAME = "三维控制";

namespace tool {
Smt3DViewCtrlTool::Smt3DViewCtrlTool()
    : m_bIsDrag(false),
      m_viewMode(V3DM_Normal),
      m_nWinWidth(0),
      m_nWinHeight(0),
      m_fAngle(0),
      m_pLastTarget(NULL),
      m_pCamera(NULL) {
  set_name(CST_STR_3DVIEWCTRL_TOOL_NAME.c_str());
}

Smt3DViewCtrlTool::~Smt3DViewCtrlTool() {
  UnRegisterMsg();
  SMT_SAFE_DELETE(m_pCamera);
}

int Smt3DViewCtrlTool::Init(LP3DRENDERDEVICE p3DRenderDevice, SmtScene* pScene,
                            HWND hWnd, pfnToolCallBack pfnCallBack,
                            void* pToFollow) {
  if (SMT_ERR_NONE != SmtBase3DTool::Init(p3DRenderDevice, pScene, hWnd,
                                          pfnCallBack, pToFollow)) {
    return SMT_ERR_FAILURE;
  }

  Vector3 TCenter = Vector3(0, 0, 0);
  m_vOrgEye = TCenter + Vector3(0, 0, 100);
  m_vOrgTarget = TCenter;
  m_vOrgUp = Vector3(0.f, 1.f, 0.f);

  ReplaceCamera(render::View3dCameraKind::kPersp, 500.f);

  //////////////////////////////////////////////////////////////////////////
  // ���ù��
  UINT idCursors[] = {IDC_CURSOR_TRACEBALL, IDC_CURSOR_SPHERECAMERA,
                      IDC_CURSOR_FIRSTPERSON};

  int nCount = sizeof(idCursors) / sizeof(UINT);

  for (int i = 0; i < nCount; i++)
    m_hCursors[i] = ::LoadCursor(g_hInstance, MAKEINTRESOURCE(idCursors[i]));

  // ����ѡ���������
  m_matSel.SetAmbientValue(SmtColor(0, 0, 0));
  m_matSel.SetDiffuseValue(SmtColor(0, 0, 1));
  m_matSel.SetSpecularValue(SmtColor(0, 1, 1));
  m_matSel.SetEmissiveValue(SmtColor(0, 0, 1));
  m_matSel.SetShininessValue(50);

  append_func_items("轨迹球", GT_MSG_3DVIEW_TRACEBALL,
                    FIM_3DVIEW | FIM_3DMFMENU);
  append_func_items("球面相机", GT_MSG_3DVIEW_SPHERECAMERA,
                    FIM_3DVIEW | FIM_3DMFMENU);
  append_func_items("第一人称", GT_MSG_3DVIEW_FIRSTPERSON,
                    FIM_3DVIEW | FIM_3DMFMENU);
  append_func_items("复位", GT_MSG_3DVIEW_RESTORE, FIM_3DVIEW | FIM_3DMFMENU);
  // append_func_items("��ά��ͼ",GT_MSG_VIEW_ACTIVE,FIM_3DVIEW);

  SMT_IATOOL_APPEND_MSG(GT_MSG_3DVIEW_TRACEBALL);
  SMT_IATOOL_APPEND_MSG(GT_MSG_3DVIEW_SPHERECAMERA);
  SMT_IATOOL_APPEND_MSG(GT_MSG_3DVIEW_FIRSTPERSON);
  // SMT_IATOOL_APPEND_MSG(GT_MSG_3DVIEW_ACTIVE);

  SMT_IATOOL_APPEND_MSG(GT_MSG_3DVIEW_RESIZE);
  SMT_IATOOL_APPEND_MSG(GT_MSG_SET_3DVIEW_MODE);
  SMT_IATOOL_APPEND_MSG(GT_MSG_GET_3DVIEW_MODE);
  SMT_IATOOL_APPEND_MSG(GT_MSG_3DVIEW_RESTORE)

  RegisterMsg();

  return SMT_ERR_NONE;
}

void Smt3DViewCtrlTool::sync_org_pose() {
  if (!m_pCamera) {
    return;
  }
  m_vOrgEye = m_pCamera->GetEye();
  m_vOrgTarget = m_pCamera->GetTarget();
  m_vOrgUp = m_pCamera->GetUp();
}

int Smt3DViewCtrlTool::AuxDraw() { return SMT_ERR_NONE; }

void Smt3DViewCtrlTool::ReplaceCamera(render::View3dCameraKind kind,
                                      float step_div) {
  SMT_SAFE_DELETE(m_pCamera);
  if (!m_p3DRenderDevice) {
    return;
  }
  Viewport3D viewport = m_p3DRenderDevice->GetViewport();
  m_pCamera = render::make_view3d_camera(kind, m_p3DRenderDevice, viewport);
  if (!m_pCamera) {
    return;
  }
  m_pCamera->SetETU(m_vOrgEye, m_vOrgTarget, m_vOrgUp);
  if (m_pScene) {
    Aabb aabb = m_pScene->GetAabb();
    m_pCamera->SetMoveStep((aabb.vcMax - aabb.vcMin).length() / step_div);
    m_pScene->SetSceneCamera(m_pCamera);
  }
}

int Smt3DViewCtrlTool::Timer() { return SMT_ERR_NONE; }

int Smt3DViewCtrlTool::notify(long nMsg, SmtListenerMsg& param) {
  const char* cmd = tool::command_id_from_gt_msg(nMsg);
  // Bound: activate view3d.* on Workspace; leftover keeps camera mode only.
  const bool via_ws = tool::try_execute_gt_msg(m_workspace, nMsg);

  if (param.hSrcWnd != m_hWnd) {
    switch (nMsg) {
      case GT_MSG_3DVIEW_ACTIVE: {
        SetForegroundWindow(m_hWnd);
        if (!m_workspace) {
          SetActive();
        }
      } break;
      case GT_MSG_3DVIEW_RESTORE: {
        if (m_pScene) {
          Viewport3D viewport = m_p3DRenderDevice->GetViewport();

          Aabb aabb = m_pScene->GetAabb();
          m_vOrgEye = aabb.vcMax;
          m_vOrgTarget = (aabb.vcMax + aabb.vcMin) / 2.;
          m_vOrgUp = Vector3(0.f, 1.f, 0.f);

          m_pCamera->SetViewport(viewport);
          m_pCamera->SetETU(m_vOrgEye, m_vOrgTarget, m_vOrgUp);
          m_pCamera->SetMoveStep((aabb.vcMax - aabb.vcMin).length() / 100);
        }
      } break;
    }
  } else {
    if (cmd && std::strcmp(cmd, "view3d.trackball") == 0) {
      m_viewMode = V3DM_TraceBall;
      OnSetViewMode();
      if (!via_ws) {
        SetActive();
      }
      return SMT_ERR_NONE;
    }
    if (cmd && std::strcmp(cmd, "view3d.sphere") == 0) {
      if (m_viewMode != V3DM_ShpereCamera) {
        ReplaceCamera(render::View3dCameraKind::kArbv, 100.f);
        m_viewMode = V3DM_ShpereCamera;
      }
      OnSetViewMode();
      if (!via_ws) {
        SetActive();
      }
      return SMT_ERR_NONE;
    }
    if (cmd && std::strcmp(cmd, "view3d.fps") == 0) {
      if (m_viewMode != V3DM_FirstPerson) {
        RECT wndRect;
        lPoint point;
        GetWindowRect(m_hWnd, &wndRect);
        point.x = (wndRect.right + wndRect.left) >> 1;
        point.y = (wndRect.bottom + wndRect.top) >> 1;

        ReplaceCamera(render::View3dCameraKind::kFps, 100.f);
        if (m_pCamera) {
          ((SmtFPSCamera*)m_pCamera)->SetWinCenter(point);
        }

        m_viewMode = V3DM_FirstPerson;
      }
      OnSetViewMode();
      if (!via_ws) {
        SetActive();
      }
      return SMT_ERR_NONE;
    }
    if (cmd && std::strcmp(cmd, "view3d.full") == 0) {
      if (m_pScene) {
        Viewport3D viewport = m_p3DRenderDevice->GetViewport();

        Aabb aabb = m_pScene->GetAabb();
        m_vOrgEye = aabb.vcMax;
        m_vOrgTarget = (aabb.vcMax + aabb.vcMin) / 2.;
        m_vOrgUp = Vector3(0.f, 1.f, 0.f);

        m_pCamera->SetViewport(viewport);
        m_pCamera->SetETU(m_vOrgEye, m_vOrgTarget, m_vOrgUp);
        m_pCamera->SetMoveStep((aabb.vcMax - aabb.vcMin).length() / 100);
      }
      if (!via_ws) {
        SetActive();
      }
      return SMT_ERR_NONE;
    }
    switch (nMsg) {
      case GT_MSG_3DVIEW_RESIZE: {
        Viewport3D vp = m_p3DRenderDevice->GetViewport();
        if (vp.ulHeight > 0 && vp.ulWidth > 0) {
          m_nWinWidth = vp.ulWidth;
          m_nWinHeight = vp.ulHeight;
          if (m_pCamera) {
            m_pCamera->SetViewport(vp);
          }
        }
      } break;
      case GT_MSG_SET_3DVIEW_MODE: {
        m_viewMode = eView3DMode(*(ushort*)param.wParam);
        OnSetViewMode();
      } break;
      case GT_MSG_GET_3DVIEW_MODE: {
        *(ushort*)param.wParam = m_viewMode;
      } break;
      default:
        break;
    }
    if (!m_workspace) {
      SetActive();
    }
  }
  return SMT_ERR_NONE;
}

int Smt3DViewCtrlTool::SetCursor(void) {
  switch (m_viewMode) {  // Zoom mode select
    case V3DM_TraceBall:
      ::SetCursor(m_hCursors[CursorTraceBall]);
      break;
    case V3DM_ShpereCamera:
      ::SetCursor(m_hCursors[CursorSphereCamare]);
      break;
    case V3DM_FirstPerson:
      ::SetCursor(m_hCursors[CursorFirstPerson]);
      break;
    default:
      // All other zoom modes
      ::SetCursor(m_hCrossCursor);
      break;
  }
  return SMT_ERR_NONE;
}

int Smt3DViewCtrlTool::KeyDown(uint nChar, uint nRepCnt, uint nFlags) {
  (void)nRepCnt;
  (void)nFlags;
  // Bound: keys arrive as DraftKind::kKey via Workspace / apply_draft.
  if (m_workspace) {
    return SMT_ERR_NONE;
  }
  SetCursor();
  ApplyCameraKey(nChar);
  ApplyModeKey(nChar);
  return SMT_ERR_NONE;
}

int Smt3DViewCtrlTool::LButtonDown(uint nFlags, lPoint point) {
  if (m_workspace) {
    (void)nFlags;
    (void)point;
    return SMT_ERR_NONE;
  }
  return SmtBase3DTool::LButtonDown(nFlags, point);
}

int Smt3DViewCtrlTool::LButtonUp(uint nFlags, lPoint point) {
  if (m_workspace) {
    (void)nFlags;
    (void)point;
    return SMT_ERR_NONE;
  }
  return SmtBase3DTool::LButtonUp(nFlags, point);
}

int Smt3DViewCtrlTool::RButtonDown(uint nFlags, lPoint point) {
  if (m_workspace) {
    (void)nFlags;
    (void)point;
    return SMT_ERR_NONE;
  }
  return SmtBase3DTool::RButtonDown(nFlags, point);
}

int Smt3DViewCtrlTool::RButtonUp(uint nFlags, lPoint point) {
  if (m_workspace) {
    (void)nFlags;
    (void)point;
    return SMT_ERR_NONE;
  }
  return SmtBase3DTool::RButtonUp(nFlags, point);
}

int Smt3DViewCtrlTool::MouseMove(uint nFlags, lPoint point) {
  if (m_workspace) {
    (void)nFlags;
    (void)point;
    return SMT_ERR_NONE;
  }
  return SmtBase3DTool::MouseMove(nFlags, point);
}

int Smt3DViewCtrlTool::MouseWeel(uint nFlags, short zDelta, lPoint point) {
  if (m_workspace) {
    (void)nFlags;
    (void)zDelta;
    (void)point;
    return SMT_ERR_NONE;
  }
  return SmtBase3DTool::MouseWeel(nFlags, zDelta, point);
}

void Smt3DViewCtrlTool::OnSetViewMode(void) { SetCursor(); }

void Smt3DViewCtrlTool::RestoreLastTarget() {
  if (m_pLastTarget) {
    m_pLastTarget->SetMaterial(m_matLastTarget);
    m_pLastTarget = NULL;
  }
}

void Smt3DViewCtrlTool::ApplyCameraKey(uint nChar) {
  if (!m_pCamera) {
    return;
  }
  switch (nChar) {
    case 'W':
      m_pCamera->MoveForward();
      break;
    case 'S':
      m_pCamera->MoveBack();
      break;
    case 'A':
      m_pCamera->MoveLeft();
      break;
    case 'D':
      m_pCamera->MoveRight();
      break;
    case VK_UP:
      m_pCamera->MoveUp();
      break;
    case VK_DOWN:
      m_pCamera->MoveDown();
      break;
    case VK_LEFT:
      m_pCamera->Roll(PI / 120);
      break;
    case VK_RIGHT:
      m_pCamera->Roll(-PI / 120);
      break;
    case VK_PRIOR:
      m_pCamera->SetMoveStep(m_pCamera->GetMoveStep() + 10);
      break;
    case VK_NEXT:
      m_pCamera->SetMoveStep(m_pCamera->GetMoveStep() - 10);
      break;
    case 'K':
      if (m_p3DRenderDevice) {
        m_p3DRenderDevice->SetShadeMode(RSV_SHADE_TRIWIRE, 0,
                                        SmtColor(0., 1., 0., 1.0));
      }
      break;
    case 'J':
      if (m_p3DRenderDevice) {
        m_p3DRenderDevice->SetShadeMode(RSV_SHADE_SOLID, 0,
                                        SmtColor(0., 1., 0., 1.0));
      }
      break;
    case 'F':
      if (m_p3DRenderDevice) {
        m_p3DRenderDevice->SetFog(FM_LINEAR, SmtColor(0.8, 0.8, 0.8), 0.1, 30,
                                  100);
      }
      break;
    case 'G':
      if (m_p3DRenderDevice) {
        m_p3DRenderDevice->SetFog(FM_NONE, SmtColor(0.8, 0.8, 0.8), 0.1, 30,
                                  100);
      }
      break;
    case 'O':
      if (m_pScene) {
        m_pScene->SetShowNodeBox(!m_pScene->IsShowNodeBox());
      }
      break;
    default:
      break;
  }
}

void Smt3DViewCtrlTool::ApplyModeKey(uint nChar) {
  SmtListenerMsg param;
  param.hSrcWnd = m_hWnd;
  switch (nChar) {
    case 'C':
      notify(GT_MSG_3DVIEW_TRACEBALL, param);
      break;
    case 'V':
      notify(GT_MSG_3DVIEW_SPHERECAMERA, param);
      break;
    case 'B':
      notify(GT_MSG_3DVIEW_FIRSTPERSON, param);
      break;
    case 'Z':
      notify(GT_MSG_3DVIEW_RESTORE, param);
      break;
    case VK_ESCAPE:
      if (m_viewMode != V3DM_Normal && m_p3DRenderDevice) {
        ReplaceCamera(render::View3dCameraKind::kPersp, 500.f);
        m_viewMode = V3DM_Normal;
        OnSetViewMode();
      }
      break;
    default:
      break;
  }
}

void Smt3DViewCtrlTool::ApplyPick(const tool::Draft& draft) {
  if (!m_pScene || draft.points.empty()) {
    return;
  }
  lPoint pt(draft.points.back().x_px, draft.points.back().y_px);
  m_vSelTargetPtrs.clear();
  m_pScene->Select3DObject(m_vSelTargetPtrs, pt);
  if (m_vSelTargetPtrs.size() > 0) {
    m_pLastTarget = m_vSelTargetPtrs[0];
    m_matLastTarget = m_pLastTarget->GetMaterial();
    m_pLastTarget->SetMaterial(m_matSel);
  }
}

void Smt3DViewCtrlTool::apply_draft(const tool::Draft& draft) {
  if (draft.kind == tool::DraftKind::kKey) {
    ApplyCameraKey(draft.key);
    ApplyModeKey(draft.key);
    return;
  }
  if (draft.kind == tool::DraftKind::kWheel) {
    if (m_pCamera) {
      // Zoom toward look-at (no terrain ray-pick on leftover camera).
      m_pCamera->MoveEyeSmoothly(draft.wheel < 0);
    }
    return;
  }
  if (draft.kind == tool::DraftKind::kPick) {
    ApplyPick(draft);
    return;
  }
  if (draft.points.empty()) {
    return;
  }
  lPoint cur(draft.points.back().x_px, draft.points.back().y_px);
  if (m_viewMode == V3DM_FirstPerson) {
    if (m_pCamera) {
      ((SmtFPSCamera*)m_pCamera)->SetViewByMouse();
    }
    m_pntCur = cur;
    return;
  }
  if (draft.kind == tool::DraftKind::kPoint || draft.points.size() == 1) {
    m_bIsDrag = true;
    m_pntOrigin = m_pntCur = m_pntPre = cur;
    if (m_viewMode == V3DM_TraceBall) {
      TrackballProv(cur, m_vPrePos);
    }
    return;
  }
  lPoint pre(draft.points[0].x_px, draft.points[0].y_px);
  // Two-finger pan drafts carry kTouchPan so trackball orbit does not win.
  const bool touch_pan = tool::draft_flags::is_touch_pan(draft.flags);
  const bool orbit =
      !touch_pan &&
      ((draft.flags & (MK_RBUTTON | MK_MBUTTON)) != 0 ||
       m_viewMode == V3DM_TraceBall);
  if (!orbit && m_viewMode != V3DM_ShpereCamera &&
      m_viewMode != V3DM_FirstPerson && m_pCamera) {
    // Cesium / Google Earth: left-drag pans the look-at.
    const int dx = cur.x - pre.x;
    const int dy = cur.y - pre.y;
    if (dx <= -4) {
      m_pCamera->MoveRight();
    } else if (dx >= 4) {
      m_pCamera->MoveLeft();
    }
    if (dy <= -4) {
      m_pCamera->MoveDown();
    } else if (dy >= 4) {
      m_pCamera->MoveUp();
    }
    m_pntPre = pre;
    m_pntCur = cur;
    return;
  }
  if (orbit) {
    Vector3 curPos, deltV;
    TrackballProv(cur, curPos);
    if (!m_bIsDrag) {
      TrackballProv(pre, m_vPrePos);
    }
    deltV = (curPos - m_vPrePos);
    if ((deltV.x || deltV.y || deltV.z) && m_pScene) {
      m_fAngle = 90.0f * deltV.length();
      m_vAxis = m_vPrePos.cross(curPos);
      m_vPrePos = curPos;
      Matrix mat;
      mat.rotate_axis(m_vAxis, -m_fAngle * PI / 180.);
      m_pScene->TransWorld3DObjects(mat);
    }
    m_pntPre = pre;
    m_pntCur = cur;
    return;
  }
  switch (m_viewMode) {
    case V3DM_ShpereCamera:
      if (m_pCamera) {
        float deltx = static_cast<float>(cur.x - pre.x);
        float delty = static_cast<float>(pre.y - cur.y);
        ((SmtArbvCamera*)m_pCamera)->SetArbitMove(deltx, delty);
      }
      m_pntPre = pre;
      m_pntCur = cur;
      break;
    case V3DM_TraceBall: {
      Vector3 curPos, deltV;
      TrackballProv(cur, curPos);
      deltV = (curPos - m_vPrePos);
      if (deltV.x || deltV.y || deltV.z) {
        m_fAngle = 90.0f * deltV.length();
        m_vAxis = m_vPrePos.cross(curPos);
        m_vPrePos = curPos;
        if (m_pScene) {
          Matrix mat;
          mat.rotate_axis(m_vAxis, -m_fAngle * PI / 180.);
          m_pScene->TransWorld3DObjects(mat);
        }
      }
      m_pntPre = pre;
      m_pntCur = cur;
    } break;
    default:
      break;
  }
}

//////////////////////////////////////////////////////////////////////////
// ���ݶ�ά������x��y����һ����ά�����꣬��λ�����ŵ�v��
void Smt3DViewCtrlTool::TrackballProv(lPoint pos, Vector3& vec) {
  float d;
  vec.x = (2.0f * pos.x - m_nWinWidth) / m_nWinWidth;
  vec.y = (m_nWinHeight - 2.0f * pos.y) / m_nWinHeight;
  d = (float)sqrt(vec.x * vec.x + vec.y * vec.y);
  vec.z = (float)cos((PI / 2.0F) * ((d < 1.0) ? d : 1.0f));
  vec.normalize();
}
}  // namespace tool
