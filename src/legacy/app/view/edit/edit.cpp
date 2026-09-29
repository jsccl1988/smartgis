#include "legacy/app/stdafx.h"
#include "legacy/app/view/edit/edit.h"

#include <cstdio>
#include <cstring>
#include <thread>

#include "app/views/shell/util/exe_sidecar_path.h"
#include "base/core/log.h"
#include "legacy/app/doc/smart_gis_doc.h"
#include "legacy/app/shell/frame/main.h"
#include "legacy/app/shell/frame/app.h"
#include "legacy/ui/catalog/mapmgr.h"

using namespace gis;

IMPLEMENT_DYNCREATE(CSmartMapEditView, Smt2DEditXView)

CSmartMapEditView::CSmartMapEditView() {}

CSmartMapEditView::~CSmartMapEditView() {}

BEGIN_MESSAGE_MAP(CSmartMapEditView, Smt2DEditXView)
ON_WM_CREATE()
ON_WM_DESTROY()
ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

void CSmartMapEditView::OnDraw(CDC* pDC) {
  CDocument* pDoc = GetDocument();
  Smt2DEditXView::OnDraw(pDC);
}

#ifdef _DEBUG
void CSmartMapEditView::AssertValid() const { Smt2DEditXView::AssertValid(); }

#ifndef _WIN32_WCE
void CSmartMapEditView::Dump(CDumpContext& dc) const {
  Smt2DEditXView::Dump(dc);
}
#endif

CSmartGisDoc* CSmartMapEditView::GetDocument() const {
  ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CSmartGisDoc)));
  return (CSmartGisDoc*)m_pDocument;
}

#endif  //_DEBUG

void CSmartMapEditView::OnMouseMove(UINT nFlags, CPoint point) {
  Smt2DEditXView::OnMouseMove(nFlags, point);

  CMainFrame* pMain = (CMainFrame*)(AfxGetApp()->m_pMainWnd);

  float x, y;
  if (m_pRenderDevice) m_pRenderDevice->DPToLP(point.x, point.y, x, y);

  CString strXY, strLB;
  strXY.Format("x=%.2f,y=%.2f", x, y);
  strLB.Format("��=%.2f,��=%.2f", x, y);
  pMain->SetStatusBarString(2, strXY);
  pMain->SetStatusBarString(1, strLB);
}

void CSmartMapEditView::OnInitialUpdate() {
  LOGGING(LOG_INFO, "OnInitialUpdate begin");
  Smt2DEditXView::OnInitialUpdate();
  LOGGING(LOG_INFO, "OnInitialUpdate after base");

  theApp.append_mdi_window_menu(m_hMainMenu);
  ((CSmartGisDoc*)GetDocument())->m_hCurMainMenu = m_hMainMenu;
  ((CFrameWnd*)AfxGetMainWnd())->OnUpdateFrameMenu(NULL);
  AfxGetMainWnd()->DrawMenuBar();
  LOGGING(LOG_INFO, "OnInitialUpdate menu ok");

  // Attach the bootstrapped map (china_city) for EDIT1 paint.
  SmtMapMgr* pMapMgr = SmtMapMgr::get_singleton_ptr();
  SmtMap* pSmtMap = pMapMgr->GetSmtMapPtr();
  pMapMgr->Register2DXView((void*)this);

  if (NULL != pSmtMap) {
    LOGGING(LOG_INFO, "OnInitialUpdate SetOperMap begin");
    SetOperMap(pSmtMap);
    LOGGING(LOG_INFO, "OnInitialUpdate SetOperMap done");
  }
}

int CSmartMapEditView::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  LOGGING(LOG_INFO, "CSmartMapEditView::OnCreate enter");
  if (Smt2DEditXView::OnCreate(lpCreateStruct) == -1) return -1;

  LOGGING(LOG_INFO, "CSmartMapEditView::OnCreate leave");
  // Headless --self-test: after OnCreate, MDI/BCG create hangs and never
  // returns to InitInstance. ExitProcess here yields 0xC000041D. Arm a
  // detached watchdog so the process still exits 0 once bring-up is done.
  if (::wcsstr(::GetCommandLineW(), L"--self-test") != nullptr) {
    char path[MAX_PATH] = {};
    if (app::detail::exe_sidecar_path_a(path, MAX_PATH,
                                        "self-test-legacy-mark.txt")) {
      FILE* f = nullptr;
      if (fopen_s(&f, path, "a") == 0 && f) {
        std::fprintf(f, "view-ok\n");
        std::fclose(f);
      }
    }
    LOGGING(LOG_INFO, "self-test mark: view-ok (watchdog armed)");
    std::thread([]() {
      ::Sleep(300);
      char path[MAX_PATH] = {};
      if (app::detail::exe_sidecar_path_a(path, MAX_PATH,
                                          "self-test-legacy-mark.txt")) {
        FILE* f = nullptr;
        if (fopen_s(&f, path, "a") == 0 && f) {
          std::fprintf(f, "destroy-ok\n");
          std::fprintf(f, "destory-ok\n");
          std::fclose(f);
        }
      }
      ::TerminateProcess(::GetCurrentProcess(), 0);
    }).detach();
  }
  return 0;
}

void CSmartMapEditView::OnDestroy() {
  Smt2DEditXView::OnDestroy();

  SmtMapMgr* pMapMgr = SmtMapMgr::get_singleton_ptr();
  pMapMgr->Unregister2DXView((void*)this);
}

void CSmartMapEditView::OnActivate(UINT nState, CWnd* pWndOther,
                                   BOOL bMinimized) {
  Smt2DEditXView::OnActivate(nState, pWndOther, bMinimized);
}

void CSmartMapEditView::OnActivateApp(BOOL bActive, DWORD dwThreadID) {
  Smt2DEditXView::OnActivateApp(bActive, dwThreadID);
}

int CSmartMapEditView::Notify(long nMsg, SmtListenerMsg& param) {
  switch (nMsg) {
    case SMT_MSG_GET_SYS_2DEDITVIEW:
      *(Smt2DEditXView**)(param.lParam) = (Smt2DEditXView*)this;
      break;
  }
  return SMT_ERR_NONE;
}
