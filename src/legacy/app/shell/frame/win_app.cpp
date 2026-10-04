#include "legacy/app/stdafx.h"

// Leftover CWinAppEx + MDI doc-template wiring.
// Map/session bootstrap is SmtApp in bootstrap/; endgame host is src/app/views.

#include "app/views/shell/util/exe_sidecar_path.h"
#include "base/core/log.h"
#include "legacy/app/views/document/document.h"
#include "legacy/app/shell/frame/child_frame.h"
#include "legacy/app/shell/frame/main_frame.h"
#include "legacy/app/shell/showcase/map2d_showcase.h"
#include "legacy/app/shell/showcase/scene3d_showcase.h"
#include "legacy/app/shell/frame/mdi_tab_options.h"
#include "legacy/app/shell/frame/win_app.h"
#include "legacy/app/views/viewport/scene3d_view.h"
#include "legacy/app/views/viewport/data_view.h"
#include "legacy/app/views/viewport/edit_view.h"
#include "legacy/core/util/menu.h"
#include "legacy/plugin/runtime/auxmodule/mfc_module.h"
#include "legacy/sys/sysmanager.h"
#include "legacy/ui/catalog/map/mapmgr.h"
#include "ogrsf_frmts.h"

using namespace base;
using namespace gis;
using namespace sys;
using namespace ui;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

class CAboutDlg : public CDialog {
public:
  CAboutDlg();

  enum { IDD = IDD_ABOUTBOX };

protected:
  virtual void DoDataExchange(CDataExchange *pDX);

protected:
  DECLARE_MESSAGE_MAP()
public:
  afx_msg void OnBnClickedOk();
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD) {}

void CAboutDlg::DoDataExchange(CDataExchange *pDX) {
  CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
ON_BN_CLICKED(IDOK, &CAboutDlg::OnBnClickedOk)
END_MESSAGE_MAP()

void CAboutDlg::OnBnClickedOk() { OnOK(); }

BEGIN_MESSAGE_MAP(CSmartGisApp, CWinAppEx)
ON_COMMAND(ID_APP_ABOUT, &CSmartGisApp::OnAppAbout)
//	ON_COMMAND(ID_FILE_NEW, &CWinApp::OnFileNew)
//	ON_COMMAND(ID_FILE_OPEN, &CWinApp::OnFileOpen)
ON_COMMAND(ID_FILE_PRINT_SETUP, &CWinApp::OnFilePrintSetup)
END_MESSAGE_MAP()

CSmartGisApp::CSmartGisApp() {
  m_pEditViewDocTemplate = NULL;
  m_pDataViewDocTemplate = NULL;
  m_p3DViewDocTemplate = NULL;
}

CSmartGisApp theApp;

BOOL CSmartGisApp::InitInstance() {
  const wchar_t *cmdline = GetCommandLineW();
  const bool self_test = wcsstr(cmdline, L"--self-test") != nullptr;
  const bool map2d_showcase = wcsstr(cmdline, L"--map2d-showcase") != nullptr;
  const bool scene3d_showcase =
      wcsstr(cmdline, L"--scene3d-showcase") != nullptr;
  auto early_mark = [self_test](const char *step) {
    if (!self_test) {
      return;
    }
    FILE *f = nullptr;
    if (fopen_s(&f, "self-test-legacy-mark.txt", "a") == 0 && f) {
      std::fprintf(f, "%s\n", step);
      std::fclose(f);
    }
  };
  if (self_test) {
    DeleteFileA("self-test-legacy-mark.txt");
    early_mark("init-enter");
  }

  // InitCommonControlsEx required for ComCtl32 v6 visual styles.
  INITCOMMONCONTROLSEX InitCtrls;
  InitCtrls.dwSize = sizeof(InitCtrls);
  InitCtrls.dwICC = ICC_WIN95_CLASSES;
  InitCommonControlsEx(&InitCtrls);
  early_mark("after-common-controls");

  CWinAppEx::InitInstance();
  early_mark("after-winappex");

  if (!AfxOleInit()) {
    AfxMessageBox(IDP_OLE_INIT_FAILED);
    return FALSE;
  }
  early_mark("after-ole");

  // Legacy MFC shell: remain DPI-unaware (see BcgGlobalData::SetDPIAware).
  globalData.SetDPIAware();
  early_mark("after-dpi");

  AfxEnableControlContainer();
  SetRegistryKey(_T("SmartGIS"));
  LoadStdProfileSettings(4);

  SetRegistryBase(_T("Settings"));

  // Feature Pack persists Outlook page titles under DockablePaneAdapter-*.
  // Older UTF-8 AM names were saved as literal '?' (DEM????). Wipe those
  // BarName values before LoadFrame so LoadState cannot paint garbage faces.
  {
    HKEY settings = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER,
                        L"Software\\SmartGIS\\SmartGis\\Settings", 0,
                        KEY_READ | KEY_WRITE, &settings) == ERROR_SUCCESS) {
      wchar_t sub[256];
      DWORD sub_cch = 256;
      DWORD index = 0;
      while (::RegEnumKeyExW(settings, index, sub, &sub_cch, nullptr, nullptr,
                             nullptr, nullptr) == ERROR_SUCCESS) {
        if (wcsncmp(sub, L"DockablePaneAdapter-", 20) == 0) {
          HKEY pane = nullptr;
          if (::RegOpenKeyExW(settings, sub, 0, KEY_READ | KEY_WRITE, &pane) ==
              ERROR_SUCCESS) {
            wchar_t name[256] = {};
            DWORD name_bytes = sizeof(name);
            DWORD type = 0;
            if (::RegQueryValueExW(pane, L"BarName", nullptr, &type,
                                   reinterpret_cast<LPBYTE>(name),
                                   &name_bytes) == ERROR_SUCCESS &&
                type == REG_SZ) {
              if (wcschr(name, L'?') != nullptr) {
                ::RegDeleteValueW(pane, L"BarName");
              }
            }
            ::RegCloseKey(pane);
          }
        }
        ++index;
        sub_cch = 256;
      }
      ::RegCloseKey(settings);
    }
  }

  m_Options.Load();

  m_Options.m_nMDITabsType = CMDITabOptions::MDITabbedGroups;
  m_Options.m_bCustomTooltips = true;
  m_Options.m_bFlatFrame = true;
  m_Options.m_bTabsOnTop = false;
  m_Options.m_bMDITabsIcons = false;
  m_Options.m_nTabsStyle = CBCGPTabWnd::STYLE_3D_ONENOTE;
  m_Options.m_bDisableMDIChildRedraw = true;

  // Initialize all Managers for usage. They are automatically constructed
  // if not yet present
  InitContextMenuManager();
  InitKeyboardManager();
  InitTooltipManager();
  early_mark("after-managers");

  CBCGPToolTipParams params;
  params.m_bVislManagerTheme = TRUE;

  GetTooltipManager()->SetTooltipParams(
      BCGP_TOOLTIP_TYPE_ALL, RUNTIME_CLASS(CBCGPToolTipCtrl), &params);

  if (!SmtApp::Init())
    return FALSE;
  // Plugin CWinApp objects can leave this thread on a DLL module state whose
  // resource handle was null (afxwin1.inl AfxGetResourceHandle assert).
  restore_exe_mfc_module_state();
  early_mark("after-smtapp-init");

  // --map2d-showcase[=china]: headless GDI paint + BMP sidecar, then exit
  // before BCG MDI (same hang avoidance as --self-test).
  if (map2d_showcase) {
    const int rc = legacy_app::run_map2d_showcase_china(*this);
    ::TerminateProcess(::GetCurrentProcess(), static_cast<UINT>(rc));
    return FALSE;
  }

  // --scene3d-showcase[=mode]: leftover GL/D3D stereo + BMP (china|terrain|
  // cube|sphere|water|pointcloud|northarray), then exit before BCG MDI.
  if (scene3d_showcase) {
    const int rc = legacy_app::run_scene3d_showcase(*this);
    ::TerminateProcess(::GetCurrentProcess(), static_cast<UINT>(rc));
    return FALSE;
  }

  // --self-test: validate China map bootstrap then exit before BCG MDI
  // doc-templates / LoadFrame. Creating CMultiDocTemplate currently hangs
  // headless under this BCG build; smoke only needs exit 0 + mark file.
  if (self_test) {
    auto mark = [](const char *step) {
      char path[MAX_PATH] = {};
      FILE *f = nullptr;
      if (app::detail::exe_sidecar_path_a(path, MAX_PATH,
                                          "self-test-legacy-mark.txt") &&
          fopen_s(&f, path, "a") == 0 && f) {
        std::fprintf(f, "%s\n", step);
        std::fclose(f);
      } else if (fopen_s(&f, "self-test-legacy-mark.txt", "a") == 0 && f) {
        std::fprintf(f, "%s\n", step);
        std::fclose(f);
      }
      LOGGING(LOG_INFO, "self-test mark: %s", step);
    };
    mark("delay-init");
    if (!SmtApp::DelayInit()) {
      mark("delay-init-fail");
      SmtApp::Destory();
      ::ExitProcess(10);
    }
    Map *map = SmtMapMgr::get_singleton_ptr()
                      ? SmtMapMgr::get_singleton_ptr()->GetSmtMapPtr()
                      : nullptr;
    const int layers = map ? map->GetLayerCount() : 0;
    long long feats = 0;
    if (map && layers > 0) {
      if (OGRLayer *ogr = map->GetOgrLayer(0)) {
        feats = ogr->GetFeatureCount(/*bForce=*/0);
        if (feats < 0) {
          ogr->ResetReading();
          feats = 0;
          while (feats < 3) {
            OGRFeature *feat = ogr->GetNextFeature();
            if (!feat) {
              break;
            }
            ++feats;
            OGRFeature::DestroyFeature(feat);
          }
        }
      }
    }
    if (layers < 1 || feats < 3) {
      mark("map-empty");
      SmtApp::Destory();
      ::ExitProcess(11);
    }
    mark("china-plp-ok");
    // Do not call SmtApp::Destory() / ExitProcess: DLL_PROCESS_DETACH can
    // deadlock the loader lock under this MFC/BCG stack. TerminateProcess
    // matches the edit-view watchdog and returns 0 to exe_smoke.
    mark("destory-ok");
    ::TerminateProcess(::GetCurrentProcess(), 0);
  }

  m_pEditViewDocTemplate = new CMultiDocTemplate(
      IDR_MENU_EDITVIEW, RUNTIME_CLASS(CSmartGisDoc),
      RUNTIME_CLASS(CChildFrame), RUNTIME_CLASS(CSmartMapEditView));
  AddDocTemplate(m_pEditViewDocTemplate);

  m_pDataViewDocTemplate = new CMultiDocTemplate(
      IDR_MENU_DSVIEW, RUNTIME_CLASS(CSmartGisDoc), RUNTIME_CLASS(CChildFrame),
      RUNTIME_CLASS(CSmartDataSourceView));
  AddDocTemplate(m_pDataViewDocTemplate);

  m_p3DViewDocTemplate = new CMultiDocTemplate(
      IDR_MENU_3DVIEW, RUNTIME_CLASS(CSmartGisDoc), RUNTIME_CLASS(CChildFrame),
      RUNTIME_CLASS(CSmart3DView));

  AddDocTemplate(m_p3DViewDocTemplate);

  CMainFrame *pMainFrame = new CMainFrame;
  if (!pMainFrame || !pMainFrame->LoadFrame(IDR_MAINFRAME)) {
    delete pMainFrame;
    return FALSE;
  }

  m_pMainWnd = pMainFrame;

  CCommandLineInfo cmdInfo;
  ParseCommandLine(cmdInfo);

  cmdInfo.m_nShellCommand = CCommandLineInfo::FileNothing;
  if (!ProcessShellCommand(cmdInfo)) {
    return FALSE;
  }

  // Show shell before china_city / OGR bootstrap so the message pump is
  // not blocked on a hidden frame.
  m_pMainWnd->ShowWindow(SW_SHOW);
  m_pMainWnd->UpdateWindow();
  LOGGING(LOG_INFO, "InitInstance: main window shown.");

  LOGGING(LOG_INFO, "InitInstance: DelayInit begin...");
  if (!SmtApp::DelayInit()) {
    LOGGING(LOG_INFO, "InitInstance: DelayInit failed.");
    return FALSE;
  }
  LOGGING(LOG_INFO, "InitInstance: DelayInit ok.");

  // Do not OpenDocumentFile synchronously here. BCG MDI tabbed groups
  // deadlock inside CView::OnInitialUpdate when InitInstance has no outer
  // message pump (title bar shows 未响应, UI thread Wait/UserRequest, CPU
  // idle). Post after return so the pump can nest safely; SetOperMap then
  // frames/paints the bootstrapped china_city map.
  //
  // Only auto-open Edit. Queuing Data + 3D right after Edit still hangs the
  // UI thread inside CreateNewFrame / InitialUpdateFrame for the second MDI
  // child (Edit OnInitialUpdate completes; Data never logs begin). User can
  // open Data/3D from 窗口(&W) once Edit is responsive.
  LOGGING(LOG_INFO, "InitInstance: posting deferred Edit view open...");
  pMainFrame->PostMessage(WM_COMMAND, ID_WND_MAPEDIT, 0);

  m_pMainWnd->ShowWindow(SW_SHOWMAXIMIZED);
  pMainFrame->ShowWindow(SW_SHOWMAXIMIZED);
  pMainFrame->UpdateWindow();
  // LoadState may have re-applied stale Outlook BarName ('?'); force ASCII.
  pMainFrame->refresh_am_outlook_captions();
  LOGGING(LOG_INFO, "InitInstance: main window shown maximized.");

  return TRUE;
}

int CSmartGisApp::ExitInstance() {
  SmtApp::Destory();

  m_Options.Save();

  CleanState();
  BCGCBProCleanUp();

  return CWinAppEx::ExitInstance();
}

void CSmartGisApp::restore_mfc_module_state() {
  restore_exe_mfc_module_state();
}

void CSmartGisApp::PreLoadState() {
  // GetContextMenuManager()->AddMenu (_T("My menu"), IDR_CONTEXT_MENU);
}

void CSmartGisApp::OnAppAbout() {
  CAboutDlg aboutDlg;
  aboutDlg.DoModal();
}

CView *CSmartGisApp::GetActiveDocView(CRuntimeClass *pViewClass) {
  CDocument *pDoc =
      ((CMainFrame *)m_pMainWnd)->GetActiveFrame()->GetActiveDocument();
  if (pDoc == NULL)
    return NULL;

  CView *pView;
  POSITION pos = pDoc->GetFirstViewPosition();
  while (pos != NULL) {
    pView = pDoc->GetNextView(pos);
    if (pView->IsKindOf(pViewClass))
      return pView;
  }
  return NULL;
}

CView *CSmartGisApp::GetActiveView(void) {
  CView *pView = ((CMainFrame *)m_pMainWnd)->GetActiveFrame()->GetActiveView();
  return pView;
}

CDocument *CSmartGisApp::GetActiveDoc(void) {
  CDocument *pDoc =
      ((CMainFrame *)m_pMainWnd)->GetActiveFrame()->GetActiveDocument();
  return pDoc;
}

void CSmartGisApp::append_mdi_window_menu(HMENU menu) {
  if (menu == NULL)
    return;

  HMENU popup = ::CreatePopupMenu();
  if (popup == NULL)
    return;

  ::AppendMenuA(popup, MF_STRING, ID_WND_MAPEDIT, "地图编辑窗口");
  ::AppendMenuA(popup, MF_STRING, ID_WND_MAPDATA, "地图数据窗口");
  ::AppendMenuA(popup, MF_STRING, ID_WND_3D, "三维窗口(&3)");
  if (!insert_popup_menu(menu, 0, popup, "窗口(&W)", MF_BYPOSITION))
    ::DestroyMenu(popup);
}

BOOL CSmartGisApp::open_mdi_view(CDocTemplate *tmpl) {
  if (tmpl == NULL)
    return FALSE;

  // Prefer activating an existing view of this template's class. A second
  // CreateNewFrame for 3D (same CSmartGisDoc, another CSmart3DView) re-inits
  // D3D + seeds thousands of OGR features and ends in heap corruption
  // (0xC0000374) after 窗口→三维窗口 is clicked again.
  // MFC here has no public GetViewClass(); map by the known template pointers.
  CRuntimeClass *view_class = NULL;
  if (tmpl == m_pEditViewDocTemplate) {
    view_class = RUNTIME_CLASS(CSmartMapEditView);
  } else if (tmpl == m_pDataViewDocTemplate) {
    view_class = RUNTIME_CLASS(CSmartDataSourceView);
  } else if (tmpl == m_p3DViewDocTemplate) {
    view_class = RUNTIME_CLASS(CSmart3DView);
  }
  if (view_class != NULL) {
    POSITION tmpl_pos = GetFirstDocTemplatePosition();
    while (tmpl_pos != NULL) {
      CDocTemplate *scan = GetNextDocTemplate(tmpl_pos);
      if (scan == NULL) {
        continue;
      }
      POSITION doc_pos = scan->GetFirstDocPosition();
      while (doc_pos != NULL) {
        CDocument *scan_doc = scan->GetNextDoc(doc_pos);
        if (scan_doc == NULL) {
          continue;
        }
        POSITION view_pos = scan_doc->GetFirstViewPosition();
        while (view_pos != NULL) {
          CView *view = scan_doc->GetNextView(view_pos);
          if (view == NULL || !view->IsKindOf(view_class)) {
            continue;
          }
          if (CFrameWnd *frame = view->GetParentFrame()) {
            LOGGING(LOG_INFO, "open_mdi_view: activate existing view");
            frame->ActivateFrame(SW_SHOW);
            return TRUE;
          }
        }
      }
    }
  }

  CDocument *doc = NULL;
  CMDIChildWnd *child = NULL;
  if (m_pMainWnd) {
    CFrameWnd *frame = DYNAMIC_DOWNCAST(CFrameWnd, m_pMainWnd);
    CFrameWnd *active = frame ? frame->GetActiveFrame() : NULL;
    if (active && active != frame) {
      child = DYNAMIC_DOWNCAST(CMDIChildWnd, active);
      doc = active->GetActiveDocument();
    }
  }

  // First 窗口→三维窗口 on an open 2D map used CreateNewFrame(shared
  // CSmartGisDoc). That re-enters GDAL/SQLite + D3D on the same heap as the
  // 2D view and dies with 0xC0000374 in sqlite3_free during WM_CREATE.
  if (doc && tmpl == m_p3DViewDocTemplate) {
    LOGGING(LOG_INFO, "open_mdi_view: 3D OpenDocumentFile (do not share 2D doc)");
    return tmpl->OpenDocumentFile(NULL) != NULL;
  }

  if (doc) {
    LOGGING(LOG_INFO, "open_mdi_view: CreateNewFrame on existing doc");
    CFrameWnd *created = tmpl->CreateNewFrame(doc, child);
    if (created) {
      tmpl->InitialUpdateFrame(created, doc);
      // Maximize 3D after first present settles — doing it inside
      // InitialUpdateFrame nesting crashed Feature Pack under OpenGL.
      if (tmpl == m_p3DViewDocTemplate) {
        if (CMDIChildWnd *mdi = DYNAMIC_DOWNCAST(CMDIChildWnd, created)) {
          mdi->PostMessage(WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        }
      }
      LOGGING(LOG_INFO, "open_mdi_view: InitialUpdateFrame done");
      return TRUE;
    }
  }

  LOGGING(LOG_INFO, "open_mdi_view: OpenDocumentFile(NULL)");
  return tmpl->OpenDocumentFile(NULL) != NULL;
}
