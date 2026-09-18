// smart_gis.cpp : Defines the class behaviors for the application.
//

#include "legacy/app/stdafx.h"
#include "legacy/app/smart_gis.h"
#include "legacy/app/main_frame.h"

#include "legacy/app/child_frame.h"
#include "legacy/app/smart_gis_doc.h"
#include "legacy/app/smart_gis_view.h"
#include "legacy/app/smart_data_source_view.h"
#include "legacy/app/smart_map_edit_view.h"
#include "legacy/app/smart_3d_view.h"

#include "sys/sysmanager.h"

#include "base/core/api.h"
#include "base/core/log.h"
#include "legacy/ui/xcatalog/mapmgr.h"
#include "legacy/tool/t_iatoolmanager.h"
#include "legacy/tool/group/defs.h"
#include "legacy/tool/t_msg.h"

using namespace base;
using namespace sdb;
using namespace sys;
using namespace ui;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About
class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

	// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	// Implementation
protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	ON_BN_CLICKED(IDOK, &CAboutDlg::OnBnClickedOk)
END_MESSAGE_MAP()

void CAboutDlg::OnBnClickedOk()
{
	OnOK();
}

//////////////////////////////////////////////////////////////////////
CMDITabOptions::CMDITabOptions()
{
	m_nMDITabsType = CMDITabOptions::MDITabsStandard;
	m_bMaximizeMDIChild = TRUE;
	m_bTabsOnTop = TRUE;
	m_bActiveTabCloseButton = FALSE;
	m_nTabsStyle = CBCGPTabWnd::STYLE_3D_ONENOTE;
	m_bTabsAutoColor = FALSE;
	m_bMDITabsIcons = TRUE;
	m_bMDITabsDocMenu = FALSE;
	m_bDragMDITabs = TRUE;
	m_bMDITabsContextMenu = TRUE;
	m_nMDITabsBorderSize = 2;
	m_bDisableMDIChildRedraw = TRUE;
	m_bFlatFrame = TRUE;
	m_bCustomTooltips = FALSE;
}

void CMDITabOptions::Load ()
{
	m_nMDITabsType =  (MDITabsType) theApp.GetInt (_T("ShowMDITabs"), TRUE);
	m_bMaximizeMDIChild = theApp.GetInt (_T("MaximizeMDIChild"), TRUE);
	m_bTabsOnTop = theApp.GetInt (_T("TabsOnTop"), TRUE);
	m_bActiveTabCloseButton = theApp.GetInt (_T("ActiveTabCloseButton"), FALSE);
	m_nTabsStyle = (CBCGPTabWnd::Style) theApp.GetInt (_T("TabsStyle"), CBCGPTabWnd::STYLE_3D_ONENOTE);
	m_bTabsAutoColor = theApp.GetInt (_T("TabsAutoColor"), FALSE);
	m_bMDITabsIcons = theApp.GetInt (_T("MDITabsIcons"), TRUE);
	m_bMDITabsDocMenu = theApp.GetInt (_T("MDITabsDocMenu"), FALSE);
	m_bDragMDITabs = theApp.GetInt (_T("DragMDITabs"), TRUE);
	m_bMDITabsContextMenu = theApp.GetInt (_T("MDITabsContextMenu"), TRUE);
	m_nMDITabsBorderSize = theApp.GetInt (_T("MDITabsBorderSize"), TRUE);
	m_bDisableMDIChildRedraw = theApp.GetInt (_T("DisableMDIChildRedraw"), TRUE);
	m_bFlatFrame = theApp.GetInt (_T("FlatFrame"), TRUE);
	m_bCustomTooltips = theApp.GetInt (_T("CustomTooltips"), FALSE);
}

void CMDITabOptions::Save ()
{
	theApp.WriteInt (_T("ShowMDITabs"), m_nMDITabsType);
	theApp.WriteInt (_T("MaximizeMDIChild"), m_bMaximizeMDIChild);
	theApp.WriteInt (_T("TabsOnTop"), m_bTabsOnTop);
	theApp.WriteInt (_T("ActiveTabCloseButton"), m_bActiveTabCloseButton);
	theApp.WriteInt (_T("TabsStyle"), m_nTabsStyle);
	theApp.WriteInt (_T("TabsAutoColor"), m_bTabsAutoColor);
	theApp.WriteInt (_T("MDITabsIcons"), m_bMDITabsIcons);
	theApp.WriteInt (_T("MDITabsDocMenu"), m_bMDITabsDocMenu);
	theApp.WriteInt (_T("DragMDITabs"), m_bDragMDITabs);
	theApp.WriteInt (_T("MDITabsContextMenu"), m_bMDITabsContextMenu);
	theApp.WriteInt (_T("MDITabsBorderSize"), m_nMDITabsBorderSize);
	theApp.WriteInt (_T("DisableMDIChildRedraw"), m_bDisableMDIChildRedraw);
	theApp.WriteInt (_T("FlatFrame"), m_bFlatFrame);
	theApp.WriteInt (_T("CustomTooltips"), m_bCustomTooltips);
}

//////////////////////////////////////////////////////////////////////
// CSmartGisApp

BEGIN_MESSAGE_MAP(CSmartGisApp, CWinAppEx)
	ON_COMMAND(ID_APP_ABOUT, &CSmartGisApp::OnAppAbout)
	// Standard file document commands (disabled for MDI bootstrap)
//	ON_COMMAND(ID_FILE_NEW, &CWinApp::OnFileNew)
//	ON_COMMAND(ID_FILE_OPEN, &CWinApp::OnFileOpen)
	// Standard print setup command
	ON_COMMAND(ID_FILE_PRINT_SETUP, &CWinApp::OnFilePrintSetup)
END_MESSAGE_MAP()


// CSmartGisApp construction

CSmartGisApp::CSmartGisApp()
{
	m_pEditViewDocTemplate = NULL;
	m_pDataViewDocTemplate = NULL;
	m_p3DViewDocTemplate = NULL;
}


// The one and only CSmartGisApp object

CSmartGisApp theApp;


// CSmartGisApp initialization

BOOL CSmartGisApp::InitInstance()
{
	// InitCommonControlsEx required for ComCtl32 v6 visual styles.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinAppEx::InitInstance();

	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}

	// Legacy MFC chrome: remain DPI-unaware (see BcgGlobalData::SetDPIAware).
	globalData.SetDPIAware ();

	AfxEnableControlContainer();
	SetRegistryKey(_T("SmartGIS"));
	LoadStdProfileSettings(4);


	SetRegistryBase (_T("Settings"));

	m_Options.Load ();

	m_Options.m_nMDITabsType = CMDITabOptions::MDITabbedGroups;
	m_Options.m_bCustomTooltips = true;
	m_Options.m_bFlatFrame = true;
	m_Options.m_bTabsOnTop = false;
	m_Options.m_bMDITabsIcons =false;
	m_Options.m_nTabsStyle = CBCGPTabWnd::STYLE_3D_ONENOTE;
	m_Options.m_bDisableMDIChildRedraw = true;

	// Initialize all Managers for usage. They are automatically constructed
	// if not yet present
	InitContextMenuManager();
	InitKeyboardManager();
	InitTooltipManager();

	CBCGPToolTipParams params;
	params.m_bVislManagerTheme = TRUE;

	GetTooltipManager ()->SetTooltipParams (
		BCGP_TOOLTIP_TYPE_ALL,
		RUNTIME_CLASS (CBCGPToolTipCtrl),
		&params);

	if(!SmtApp::Init()) 
		return FALSE;

	// ע��Ӧ�ó�����ĵ�ģ�塣�ĵ�ģ��
	// �������ĵ�����ܴ��ں���ͼ֮�������

	m_pEditViewDocTemplate = new CMultiDocTemplate(
		IDR_MENU_EDITVIEW,
		RUNTIME_CLASS(CSmartGisDoc),			// document class
		RUNTIME_CLASS(CChildFrame),				// frame class
		RUNTIME_CLASS(CSmartMapEditView));		// view class
	AddDocTemplate(m_pEditViewDocTemplate);

	m_pDataViewDocTemplate = new CMultiDocTemplate(
		IDR_MENU_DSVIEW,
		RUNTIME_CLASS(CSmartGisDoc),			// document class
		RUNTIME_CLASS(CChildFrame),				// frame class
		RUNTIME_CLASS(CSmartDataSourceView));	// view class
	AddDocTemplate(m_pDataViewDocTemplate);

	m_p3DViewDocTemplate = new CMultiDocTemplate(
		IDR_MENU_3DVIEW,
			RUNTIME_CLASS(CSmartGisDoc),		// document class
			RUNTIME_CLASS(CChildFrame),			// frame class
		RUNTIME_CLASS(CSmart3DView));			// view class
	
	AddDocTemplate(m_p3DViewDocTemplate);

	// ������ MDI ��ܴ���
	CMainFrame* pMainFrame = new CMainFrame;
	if (!pMainFrame || !pMainFrame->LoadFrame(IDR_MAINFRAME))
	{
		delete pMainFrame;
		return FALSE;
	}

	m_pMainWnd = pMainFrame;
	// �������к�׺ʱ�ŵ��� DragAcceptFiles
	//  �� MDI Ӧ�ó����У���Ӧ������ m_pMainWnd ֮����������


	// ������׼������DDE�����ļ�������������
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);


	// ��������������ָ����������
	// �� /RegServer��/Register��/Unregserver �� /Unregister ����Ӧ�ó����򷵻� FALSE��
	cmdInfo.m_nShellCommand = CCommandLineInfo::FileNothing;
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	const bool self_test = wcsstr(GetCommandLineW(), L"--self-test") != nullptr;
	if (self_test) {
		// Headless smoke: bootstrap China PLP map, open one EDIT frame without
		// activating/showing (avoids BCG InitialUpdate abort), paint via
		// SetOperMap, then tear down.
		auto mark = [](const char* step) {
			char path[MAX_PATH] = {};
			if (::GetModuleFileNameA(nullptr, path, MAX_PATH) == 0) {
				return;
			}
			if (char* slash = strrchr(path, '\\')) {
				slash[1] = '\0';
			}
			strncat_s(path, "self-test-legacy-mark.txt", _TRUNCATE);
			FILE* f = nullptr;
			if (fopen_s(&f, path, "a") == 0 && f) {
				std::fprintf(f, "%s\n", step);
				std::fclose(f);
			}
			LOGGING(LOG_INFO, "self-test mark: %s", step);
		};
		{
			char path[MAX_PATH] = {};
			if (::GetModuleFileNameA(nullptr, path, MAX_PATH) != 0) {
				if (char* slash = strrchr(path, '\\')) {
					slash[1] = '\0';
				}
				strncat_s(path, "self-test-legacy-mark.txt", _TRUNCATE);
				DeleteFileA(path);
			}
		}
		mark("delay-init");
		if (!SmtApp::DelayInit()) {
			mark("delay-init-fail");
			return FALSE;
		}
		{
			SmtMap* map = SmtMapMgr::get_singleton_ptr()
			                  ? SmtMapMgr::get_singleton_ptr()->GetSmtMapPtr()
			                  : nullptr;
			const int layers = map ? map->GetLayerCount() : 0;
			long long feats = 0;
			if (map && layers > 0) {
				if (OGRLayer* ogr = map->GetOgrLayer(0)) {
					feats = ogr->GetFeatureCount(1);
				}
			}
			if (layers < 1 || feats < 3) {
				mark("map-empty");
				SmtApp::Destory();
				::ExitProcess(11);
			}
			mark("china-plp-ok");
		}
		// BCG/MDI child create hangs if the main frame stays hidden.
		if (m_pMainWnd) {
			m_pMainWnd->ShowWindow(SW_SHOW);
			m_pMainWnd->UpdateWindow();
		}
		mark("open-edit");
		// Prefer OpenDocumentFile (same as interactive InitInstance). CreateNewFrame
		// + manual InitialUpdate hangs after OnCreate under this BCG bring-up.
		CDocument* doc = m_pEditViewDocTemplate->OpenDocumentFile(NULL);
		if (!doc) {
			mark("doc-fail");
			SmtApp::Destory();
			::ExitProcess(12);
		}
		mark("view-ok");
		if (POSITION pos = doc->GetFirstViewPosition()) {
			if (CView* view = doc->GetNextView(pos)) {
				if (CFrameWnd* frame = view->GetParentFrame()) {
					frame->DestroyWindow();
				}
			}
		}
		mark("destroy-ok");
		SmtApp::Destory();
		mark("destory-ok");
		::ExitProcess(0);
	}

	// Bootstrap map + OGR sample BEFORE MDI views so SetOperMap sees real
	// geometry on first OnInitialUpdate (avoids white panes / late attach).
	LOGGING(LOG_INFO, "InitInstance: DelayInit begin...");
	if (!SmtApp::DelayInit()) {
		LOGGING(LOG_INFO, "InitInstance: DelayInit failed.");
		return FALSE;
	}
	LOGGING(LOG_INFO, "InitInstance: DelayInit ok.");

	// Show early: BCG/MDI child creation while the frame is hidden has hung
	// after 2D CreateContexMenu in practice.
	m_pMainWnd->ShowWindow(SW_SHOW);
	m_pMainWnd->UpdateWindow();
	LOGGING(LOG_INFO, "InitInstance: main window shown.");

	LOGGING(LOG_INFO, "InitInstance: opening Edit view...");
	CDocument* edit_doc = m_pEditViewDocTemplate->OpenDocumentFile(NULL);
	LOGGING(LOG_INFO, "InitInstance: Edit view doc=%p", edit_doc);

	// Interactive: restore the leftover 3D MDI child so china_plp is visible
	// without hunting a replaced RC menu. Data stays on-demand. --self-test
	// never reaches here (2D-only; BCG 3D bring-up can hang).
	if (edit_doc) {
		LOGGING(LOG_INFO, "InitInstance: opening 3D view...");
		const BOOL ok3d = open_mdi_view(m_p3DViewDocTemplate);
		LOGGING(LOG_INFO, "InitInstance: 3D view ok=%d", static_cast<int>(ok3d));
	}

	if (SmtMapMgr* map_mgr = SmtMapMgr::get_singleton_ptr()) {
		map_mgr->Update2DXView();
	}
	{
		SmtListenerMsg param;
		param.hSrcWnd = NULL;
		post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,
				 SMT_MSG_KEY(GT_MSG_VIEW_ZOOMRESTORE, NULL), param);
	}

	m_pMainWnd->ShowWindow(SW_SHOWMAXIMIZED);
	pMainFrame->ShowWindow(SW_SHOWMAXIMIZED);
	pMainFrame->UpdateWindow();
	LOGGING(LOG_INFO, "InitInstance: main window shown maximized.");

	return TRUE;
}

int CSmartGisApp::ExitInstance()
{
	SmtApp::Destory();

	m_Options.Save ();

	CleanState();
	BCGCBProCleanUp();

	return CWinAppEx::ExitInstance();
}

void CSmartGisApp::PreLoadState ()
{
	//GetContextMenuManager()->AddMenu (_T("My menu"), IDR_CONTEXT_MENU);
}

// App About command
void CSmartGisApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}


// CSmartGisApp message handlers
CView *CSmartGisApp::GetActiveDocView(CRuntimeClass* pViewClass)
{
	CDocument* pDoc = ((CMainFrame* )m_pMainWnd)->GetActiveFrame()->GetActiveDocument();
	if (pDoc == NULL) 
		return NULL;

	CView* pView;
	POSITION pos = pDoc->GetFirstViewPosition();
	while (pos != NULL)
	{
		pView = pDoc->GetNextView(pos);
		if (pView->IsKindOf(pViewClass)) return pView;
	}
	return NULL;
}

CView *CSmartGisApp::GetActiveView(void)
{
	CView* pView = ((CMainFrame* )m_pMainWnd)->GetActiveFrame()->GetActiveView();
	return pView;
}

CDocument * CSmartGisApp::GetActiveDoc(void)
{
	CDocument* pDoc = ((CMainFrame* )m_pMainWnd)->GetActiveFrame()->GetActiveDocument();
	return pDoc;
}

void CSmartGisApp::append_mdi_window_menu(HMENU menu)
{
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

BOOL CSmartGisApp::open_mdi_view(CDocTemplate* tmpl)
{
	if (tmpl == NULL)
		return FALSE;

	CDocument* doc = NULL;
	CMDIChildWnd* child = NULL;
	if (m_pMainWnd) {
		CFrameWnd* frame = DYNAMIC_DOWNCAST(CFrameWnd, m_pMainWnd);
		CFrameWnd* active = frame ? frame->GetActiveFrame() : NULL;
		if (active && active != frame) {
			child = DYNAMIC_DOWNCAST(CMDIChildWnd, active);
			doc = active->GetActiveDocument();
		}
	}

	if (doc) {
		CFrameWnd* created = tmpl->CreateNewFrame(doc, child);
		if (created) {
			tmpl->InitialUpdateFrame(created, doc);
			return TRUE;
		}
	}

	return tmpl->OpenDocumentFile(NULL) != NULL;
}


