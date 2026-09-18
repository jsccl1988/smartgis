// Smt3DXView.cpp : 实锟斤拷锟侥硷拷
//

#include "stdafx.h"
#include "legacy/ui/xview/view_3d.h"

#include "base/core/api.h"
#include "base/core/listenermanager.h"
#include "base/core/log.h"
#include "content/public/view_host.h"
#include "plugin/legacy/module_manager.h"
#include "plugin/legacy/plugin_msg.h"
#include "sys/sysmanager.h"
#include "legacy/tool/t_iatoolmanager.h"
#include "legacy/ui/xview/view_chrome.h"
#include "legacy/ui/xview/view_core.h"
#include "legacy/render/model3d/cube.h"
#include "legacy/render/scene3d/map_to_scene.h"

using namespace render;
using namespace sys;
using namespace plugin;

// Smt3DXView

namespace ui {
IMPLEMENT_DYNCREATE(Smt3DXView, SmtXView)

Smt3DXView::Smt3DXView()
    : m_p3DRenderer(NULL),
      m_p3DRenderDevice(NULL),
      m_pScene(NULL),
      m_p3DViewCtrlTool(NULL) {
  ;
}

Smt3DXView::~Smt3DXView() { SMT_SAFE_DELETE(m_pScene); }

LP3DRENDERDEVICE Smt3DXView::GetRenderDevice(void) { return m_p3DRenderDevice; }

SmtScene *Smt3DXView::GetScene(void) { return m_pScene; }

BEGIN_MESSAGE_MAP(Smt3DXView, SmtXView)
ON_WM_SIZE()
ON_WM_CREATE()
ON_WM_DESTROY()
ON_WM_MOUSEMOVE()
ON_WM_TIMER()
ON_WM_KEYDOWN()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_RBUTTONDOWN()
ON_WM_MOUSEWHEEL()
ON_WM_LBUTTONDBLCLK()
ON_WM_RBUTTONDBLCLK()
ON_WM_RBUTTONUP()
ON_WM_CONTEXTMENU()
ON_WM_SETCURSOR()
ON_WM_ERASEBKGND()

END_MESSAGE_MAP()

// Smt3DXView 锟斤拷图

void Smt3DXView::OnDraw(CDC *pDC) {
  CDocument *pDoc = GetDocument();
  // TODO: 锟节达拷锟斤拷锟接伙拷锟狡达拷锟斤拷
  /*if (!m_bActive)
  return;*/

  if (m_p3DRenderDevice == NULL) {
    pDC->SetBkColor(0xFFFF0000);
  } else {
    SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();
    SmtSysPra sysPra = pSysMgr->get_sys_pra();

    m_p3DRenderDevice->SetClearColor(
        SmtColor(GetRValue(sysPra.l3DViewClearColor) / 255.,
                 GetGValue(sysPra.l3DViewClearColor) / 255.,
                 GetBValue(sysPra.l3DViewClearColor) / 255., 1));
    m_p3DRenderDevice->Clear(CLR_COLOR | CLR_ZBUFFER);
    m_p3DRenderDevice->BeginRender();

    if (m_pScene) m_pScene->Render();

    SmtIAToolManager *pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
    SmtBase3DTool *pTmpTool =
        dynamic_cast<SmtBase3DTool *>(pIAToolMgr->GetActiveIATool());
    if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd)) {
      pTmpTool->AuxDraw();
    }

    m_p3DRenderDevice->EndRender();
    m_p3DRenderDevice->SwapBuffers();
  }
}

// Smt3DXView 锟斤拷锟?
#ifdef _DEBUG
void Smt3DXView::AssertValid() const { SmtXView::AssertValid(); }

#ifndef _WIN32_WCE
void Smt3DXView::Dump(CDumpContext &dc) const { SmtXView::Dump(dc); }
#endif
#endif  //_DEBUG

// Smt3DXView 锟斤拷息锟斤拷锟斤拷锟斤拷锟斤拷

LRESULT Smt3DXView::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
  if (dispatch_chrome_message(view_host(), message, wParam, lParam)) {
    if (message == WM_MOUSEWHEEL) {
      return TRUE;
    }
    return 0;
  }
  return CView::WindowProc(message, wParam, lParam);
}

void Smt3DXView::OnSize(UINT nType, int cx, int cy) {
  SmtXView::OnSize(nType, cx, cy);

  if (m_p3DRenderDevice) {
    Viewport3D viewport;
    CRect rect;
    GetClientRect(&rect);
    if (rect.Width() <= 0 || rect.Height() <= 0) {
      return;
    }
    apply_view3d_viewport(&viewport, static_cast<ulong>(rect.Width()),
                          static_cast<ulong>(rect.Height()));

    // set viewport
    m_p3DRenderDevice->SetViewport(viewport);

    m_p3DRenderDevice->MatrixModeSet(MM_PROJECTION);
    m_p3DRenderDevice->MatrixLoadIdentity();
    m_p3DRenderDevice->SetPerspective(
        viewport.fFovy, (float)viewport.ulWidth / (float)viewport.ulHeight,
        viewport.fZNear, viewport.fZFar);
    m_p3DRenderDevice->MatrixModeSet(MM_MODELVIEW);
    m_p3DRenderDevice->MatrixLoadIdentity();

    if (m_pScene) {
      if (SmtPerspCamera* cam = m_pScene->GetSceneCamera()) {
        cam->SetViewport(viewport);
      }
    }

    if (m_p3DViewCtrlTool) {
      SmtListenerMsg param;
      param.hSrcWnd = m_hWnd;
      m_p3DViewCtrlTool->notify(GT_MSG_3DVIEW_RESIZE, param);
    }
  }
}

int Smt3DXView::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (SmtXView::OnCreate(lpCreateStruct) == -1) return -1;

  // TODO:  锟节达拷锟斤拷锟斤拷锟斤拷专锟矫的达拷锟斤拷锟斤拷锟斤拷
  SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();

  SmtSysPra sysPra = pSysMgr->get_sys_pra();

  m_uiRefreshTimer = SetTimer(69, sysPra.l3DViewRefreshTime, 0);

  return 0;
}

void Smt3DXView::OnDestroy() {
  KillTimer(69);
  SmtXView::OnDestroy();
}

void Smt3DXView::OnMouseMove(UINT nFlags, CPoint point) {
  Vector3 vOrg;

  if (m_pScene)
    m_pScene->Transform2DTo3D(vOrg, m_vCursor3DPos, lPoint(point.x, point.y));

  SmtXView::OnMouseMove(nFlags, point);
}

void Smt3DXView::OnTimer(UINT_PTR nIDEvent) {
  // TODO: restored after encoding merge
  if (nIDEvent == 69)  // Refresh timer
  {
    if (m_pScene) m_pScene->Update();

    SmtIAToolManager *pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
    SmtBase3DTool *pTmpTool =
        dynamic_cast<SmtBase3DTool *>(pIAToolMgr->GetActiveIATool());
    if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd)) {
      pTmpTool->Timer();
    }

    if (m_bActive) PostMessage(WM_PAINT);
  }

  SmtXView::OnTimer(nIDEvent);
}

void Smt3DXView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {
  SmtIAToolManager *pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
  SmtBase3DTool *pTmpTool =
      dynamic_cast<SmtBase3DTool *>(pIAToolMgr->GetActiveIATool());
  if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd)) {
    pTmpTool->KeyDown(nChar, nRepCnt, nFlags);
  }

  SmtXView::OnKeyDown(nChar, nRepCnt, nFlags);
}

void Smt3DXView::OnLButtonDown(UINT nFlags, CPoint point) {
  SmtXView::OnLButtonDown(nFlags, point);
}

void Smt3DXView::OnLButtonUp(UINT nFlags, CPoint point) {
  SmtXView::OnLButtonUp(nFlags, point);
}

void Smt3DXView::OnRButtonDown(UINT nFlags, CPoint point) {
  SmtXView::OnRButtonDown(nFlags, point);
}

BOOL Smt3DXView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) {
  return SmtXView::OnMouseWheel(nFlags, zDelta, pt);
}

void Smt3DXView::OnLButtonDblClk(UINT nFlags, CPoint point) {
  SmtXView::OnLButtonDblClk(nFlags, point);
}

void Smt3DXView::OnRButtonDblClk(UINT nFlags, CPoint point) {
  SmtXView::OnRButtonDblClk(nFlags, point);
}

void Smt3DXView::OnRButtonUp(UINT nFlags, CPoint point) {
  SmtXView::OnRButtonUp(nFlags, point);
}

BOOL Smt3DXView::OnEraseBkgnd(CDC *pDC) {
  // TODO: restored after encoding merge
  return FALSE;
}

void Smt3DXView::OnContextMenu(CWnd *pWnd, CPoint point) {
  // TODO: restored after encoding merge
  SmtIAToolManager *pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
  SmtBase3DTool *pTmpTool =
      dynamic_cast<SmtBase3DTool *>(pIAToolMgr->GetActiveIATool());
  if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd)) {
    if (!pTmpTool->IsEnableContexMenu()) return;
  }

  if (!m_hContexMenu || ::GetMenuItemCount(m_hContexMenu) <= 0) {
    return;
  }
  ::TrackPopupMenu(m_hContexMenu,
                   TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, point.x,
                   point.y, 0, this->m_hWnd, NULL);
}

BOOL Smt3DXView::OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message) {
  // TODO: restored after encoding merge
  if (nHitTest != HTCLIENT)
    return SmtXView::OnSetCursor(pWnd, nHitTest, message);

  SmtIAToolManager *pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
  SmtBase3DTool *pTmpTool =
      dynamic_cast<SmtBase3DTool *>(pIAToolMgr->GetActiveIATool());
  if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd)) {
    return pTmpTool->SetCursor();
  }

  return TRUE;
}

//////////////////////////////////////////////////////////////////////////
bool Smt3DXView::InitCreate(void) { return SmtXView::InitCreate(); }

bool Smt3DXView::CreateContexMenu() {
  SmtXView::CreateContexMenu();
// 3dview ctrl menu
  append_listener_menu(m_hContexMenu, m_p3DViewCtrlTool, FIM_3DVIEW, false);

  //////////////////////////////////////////////////////////////////////////
  // am menu
  ::AppendMenuA(m_hContexMenu, MF_SEPARATOR, 0, NULL);
  SmtAModuleManager *pAModuleMgr = SmtAModuleManager::get_singleton_ptr();
  if (pAModuleMgr) {
    for (int i = 0; i < pAModuleMgr->get_a_module_count(); i++) {
      SmtAuxModule *pAModule = pAModuleMgr->get_a_module(i);
      attach_listener_popup(m_hContexMenu, pAModule, FIM_3DVIEW,
                            pAModule->get_name(), i + 3, MF_BYPOSITION);
    }
  }

  LOGGING(LOG_INFO, "Init 3DView ContexMenu ok!");

  return true;
}

bool Smt3DXView::CreateMainMenu() {
  SmtXView::CreateMainMenu();
  // view ctrl menu
  attach_listener_popup(m_hMainMenu, m_p3DViewCtrlTool, FIM_3DMFMENU,
                        m_p3DViewCtrlTool->get_name());

  //////////////////////////////////////////////////////////////////////////
  // am menu
  ::AppendMenuA(m_hMainMenu, MF_SEPARATOR, 0, NULL);
  SmtAModuleManager *pAModuleMgr = SmtAModuleManager::get_singleton_ptr();
  if (pAModuleMgr) {
    for (int i = 0; i < pAModuleMgr->get_a_module_count(); i++) {
      SmtAuxModule *pAModule = pAModuleMgr->get_a_module(i);
      attach_listener_popup(m_hMainMenu, pAModule, FIM_3DMFMENU,
                            pAModule->get_name());
    }
  }

  LOGGING(LOG_INFO, "Init 2DView MainMenu OK!");

  return true;
}

bool Smt3DXView::EndDestory(void) {
SmtGroupToolFactory::DestoryGroup3DTool(m_p3DViewCtrlTool);

  LOGGING(LOG_INFO, "Destory Group3DTools ok!");

  if (m_p3DRenderDevice) {
    m_p3DRenderDevice->Destroy();
    m_p3DRenderDevice = NULL;
  }

  LOGGING(LOG_INFO, "Destory 3D RenderDevice ok!");

  SMT_SAFE_DELETE(m_p3DRenderer);

  return SmtXView::EndDestory();
}

//////////////////////////////////////////////////////////////////////////
bool Smt3DXView::CreateRender(void) {
SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();
SmtSysPra sysPra = pSysMgr->get_sys_pra();
  string str3DRenderDevice = sysPra.str3DRenderDeviceName;

  m_p3DRenderer = new Smt3DRenderer(AfxGetInstanceHandle());

  if (SMT_OK != m_p3DRenderer->CreateDevice(str3DRenderDevice.c_str()))
    return false;

  m_p3DRenderDevice = m_p3DRenderer->GetDevice();
  if (m_p3DRenderDevice == NULL) return false;

  if (SMT_OK != m_p3DRenderDevice->Init(m_hWnd, "SmtGL3DRenderDevice"))
    return false;

  SMT_SAFE_DELETE(m_pScene);
  m_pScene = new SmtScene();

  // 锟斤拷锟斤拷锟斤拷锟斤拷
  m_pScene->Set3DRenderDevice(m_p3DRenderDevice);

  if (m_pScene->Setup() != SMT_ERR_NONE) return false;

  const int seeded = seed_sample_map_into_scene(m_p3DRenderDevice, m_pScene);
  if (seeded > 0) {
    LOGGING(LOG_INFO, "Seeded %d OGR features into 3D scene.", seeded);
  } else {
    // Fallback so 3D1 is never an empty black HWND when china_plp is missing.
    Vector3 cube_center(0.f, 0.f, 0.f);
    SmtCube* cube = new SmtCube(m_p3DRenderDevice, cube_center, 40.f);
    SmtMaterial mat;
    Vector3 pos(0.f, 0.f, 0.f);
    if (cube->Init(pos, mat) == SMT_ERR_NONE &&
        cube->Create(m_p3DRenderDevice) == SMT_ERR_NONE) {
      cube->SetVisible(true);
      m_pScene->Add3DObject(cube);
      LOGGING(LOG_INFO, "Seeded default SmtCube into 3D scene.");
    } else {
      SMT_SAFE_DELETE(cube);
      LOGGING(LOG_INFO, "Failed to seed default SmtCube.");
    }
  }

  LOGGING(LOG_INFO, "Init 3D RenderDevice ok!");

  return Setup();
}

bool Smt3DXView::Setup(void) {
SmtLight lgt;

  lgt.SetType(LGT_DIRECTIONAL);
  lgt.SetAmbientValue(SmtColor(0., 0., 0., 1.0));
  lgt.SetDiffuseValue(SmtColor(1.0, 1.0, 1.0, 1.0));
  lgt.SetSpecularValue(SmtColor(0., 0., 0., 1));
  lgt.SetPoistion(Vector4(1.0, 1.0, 1.0, 0.0));
  lgt.SetDirection(Vector4(0.0, -1.0, -1.0, 0));

  // clear clr
  // m_p3DRenderDevice->SetClearColor(SmtColor(0.9,0.9,0.9));
  m_p3DRenderDevice->SetClearColor(SmtColor(0., 0., 0.));
  m_p3DRenderDevice->SetDepthClearValue(1.0f);
  m_p3DRenderDevice->SetStencilClearValue(0);

  // light stuff
  m_p3DRenderDevice->SetLight(0, &lgt);

  lgt.SetPoistion(Vector4(1.0, 1.0, 1.0, 0.0));
  lgt.SetDirection(Vector4(-1.0, -1.0, -1.0, 0));

  m_p3DRenderDevice->SetLight(1, &lgt);

  // fog stuff
  m_p3DRenderDevice->SetFog(FM_NONE, SmtColor(0.8, 0.8, 0.8), 0.1, 30, 100);

  m_p3DRenderDevice->SetBackfaceCulling(RSV_CULL_NONE);
  m_p3DRenderDevice->SetDepthBufferMode(RSV_DEPTH_READWRITE);
  m_p3DRenderDevice->SetShadeMode(RSV_SHADE_SOLID, 0,
                                  SmtColor(1., 1., 1., 1.0));
  m_p3DRenderDevice->SetAmbientLight(SmtColor(1., 1., 1., 1.0));

  string strAppPath = get_app_path();
  string strTextureDir = strAppPath + "rs\\texture\\";
  string strTexturePath = "";
  SmtTexture *pTexture = NULL;

  //
  strTexturePath = strTextureDir + "terrain-1.bmp";
  pTexture = m_p3DRenderDevice->CreateTexture("terrain");
  pTexture->Load(strTexturePath.c_str());

  //
  strTexturePath = strTextureDir + "earth-1.tga";
  pTexture = m_p3DRenderDevice->CreateTexture("earth");
  pTexture->Load(strTexturePath.c_str());

  //
  strTexturePath = strTextureDir + "water.bmp";
  pTexture = m_p3DRenderDevice->CreateTexture("seawater");
  pTexture->Load(strTexturePath.c_str());

  //
  strTexturePath = strTextureDir + "rbed.bmp";
  pTexture = m_p3DRenderDevice->CreateTexture("rbed");
  pTexture->Load(strTexturePath.c_str());

  return true;
}

bool Smt3DXView::CreateTools(void) {
SmtGroupToolFactory::CreateGroup3DTool(m_p3DViewCtrlTool,
                                         GroupTool3DType::GTT_3DViewControl);

  if (NULL == m_p3DViewCtrlTool) return false;

  if (m_p3DViewCtrlTool->Init(m_p3DRenderDevice, m_pScene, m_hWnd) !=
      SMT_ERR_NONE)
    return false;

  m_p3DViewCtrlTool->SetActive();

  if (m_p3DRenderDevice && m_pScene) {
    Viewport3D vp = m_p3DRenderDevice->GetViewport();
    const ulong w = vp.ulWidth > 0 ? vp.ulWidth : 1;
    const ulong h = vp.ulHeight > 0 ? vp.ulHeight : 1;
    apply_view3d_viewport(&vp, w, h);
    if (SmtPerspCamera* cam = m_pScene->GetSceneCamera()) {
      frame_persp_camera_to_aabb(cam, &vp, m_pScene->GetAabb());
    }
    if (w > 1 && h > 1) {
      m_p3DRenderDevice->SetViewport(vp);
    }
  }

  LOGGING(LOG_INFO, "Init Group3DTools ok!");

  return true;
}

BOOL Smt3DXView::OnCommand(WPARAM wParam, LPARAM lParam) {
  // TODO: restored after encoding merge
  unsigned int unMsg = (wParam);
  if (unMsg >= SMT_MSG_CMD_BEGIN && unMsg <= SMT_MSG_CMD_END) {
    dispatch_menu_command(unMsg);
  } else if (unMsg >= SMT_MSG_USER_BEGIN && unMsg <= SMT_MSG_USER_END) {
    dispatch_menu_command(unMsg);
  }

  return SmtXView::OnCommand(wParam, lParam);
}

void Smt3DXView::apply_workspace_draft(const tool::Draft &draft) {
  SmtIAToolManager *mgr = SmtIAToolManager::get_singleton_ptr();
  SmtBase3DTool *tool =
      mgr ? dynamic_cast<SmtBase3DTool *>(mgr->GetActiveIATool()) : NULL;
  if (tool && tool->GetOwnerWnd() == m_hWnd) {
    tool->apply_draft(draft);
  }
}
}  // namespace ui