// SmartGis.cpp : 定义应用程序的类行为。
//

#include "stdafx.h"
#include "SmartGis.h"
#include "MainFrm.h"

#include "ChildFrm.h"
#include "SmartGisDoc.h"
#include "SmartGisView.h"
#include "SmartDataSourceView.h"
#include "SmartMapEditView.h"
#include "Smart3DView.h"

#include "sys_sysmanager.h"

#include "smt_api.h"
#include "cata_mapmgr.h"

using namespace Smt_Core;
using namespace Smt_GIS;
using namespace Smt_Sys;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// 用于应用程序“关于”菜单项的 CAboutDlg 对话框
class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

	// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	// 实现
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
	// TODO: 在此添加控件通知处理程序代码
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

BEGIN_MESSAGE_MAP(CSmartGisApp, CWinApp)
	ON_COMMAND(ID_APP_ABOUT, &CSmartGisApp::OnAppAbout)
	// 基于文件的标准文档命令
//	ON_COMMAND(ID_FILE_NEW, &CWinApp::OnFileNew)
//	ON_COMMAND(ID_FILE_OPEN, &CWinApp::OnFileOpen)
	// 标准打印设置命令
	ON_COMMAND(ID_FILE_PRINT_SETUP, &CWinApp::OnFilePrintSetup)
END_MESSAGE_MAP()


// CSmartGisApp 构造

CSmartGisApp::CSmartGisApp()
{
	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中

	m_pEditViewDocTemplate = NULL;
	m_pDataViewDocTemplate = NULL;
	m_p3DViewDocTemplate = NULL;
}


// 唯一的一个 CSmartGisApp 对象

CSmartGisApp theApp;


// CSmartGisApp 初始化

BOOL CSmartGisApp::InitInstance()
{
	// 如果一个运行在 Windows XP 上的应用程序清单指定要
	// 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
	//则需要 InitCommonControlsEx()。否则，将无法创建窗口。
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// 将它设置为包括所有要在应用程序中使用的
	// 公共控件类。
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	// 初始化 OLE 库
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}

	globalData.SetDPIAware ();

	AfxEnableControlContainer();
	// 标准初始化
	// 如果未使用这些功能并希望减小
	// 最终可执行文件的大小，则应移除下列
	// 不需要的特定初始化例程
	// 更改用于存储设置的注册表项
	// TODO: 应适当修改该字符串，
	// 例如修改为公司或组织名
	SetRegistryKey(_T("应用程序向导生成的本地应用程序"));
	LoadStdProfileSettings(4);  // 加载标准 INI 文件选项(包括 MRU)


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

	// 注册应用程序的文档模板。文档模板
	// 将用作文档、框架窗口和视图之间的连接

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

	// 创建主 MDI 框架窗口
	CMainFrame* pMainFrame = new CMainFrame;
	if (!pMainFrame || !pMainFrame->LoadFrame(IDR_MAINFRAME))
	{
		delete pMainFrame;
		return FALSE;
	}

	m_pMainWnd = pMainFrame;
	// 仅当具有后缀时才调用 DragAcceptFiles
	//  在 MDI 应用程序中，这应在设置 m_pMainWnd 之后立即发生


	// 分析标准外壳命令、DDE、打开文件操作的命令行
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);


	// 调度在命令行中指定的命令。如果
	// 用 /RegServer、/Register、/Unregserver 或 /Unregister 启动应用程序，则返回 FALSE。
	cmdInfo.m_nShellCommand = CCommandLineInfo::FileNothing;
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	//创建3个空窗口
	m_pDataViewDocTemplate->OpenDocumentFile(NULL);
	m_pEditViewDocTemplate->OpenDocumentFile(NULL);
	m_p3DViewDocTemplate->OpenDocumentFile(NULL);
	
	if(!SmtApp::DelayInit()) 
		return FALSE;

	// 主窗口已初始化，因此显示它并对其进行更新
	m_pMainWnd->ShowWindow(SW_SHOWMAXIMIZED);
	pMainFrame->ShowWindow(SW_SHOWMAXIMIZED);
	pMainFrame->UpdateWindow();

	return TRUE;
}

int CSmartGisApp::ExitInstance()
{
	// TODO: 在此添加专用代码和/或调用基类
	SmtApp::Destory();

	m_Options.Save ();

	CleanState();
	BCGCBProCleanUp();

	return CWinApp::ExitInstance();
}

void CSmartGisApp::PreLoadState ()
{
	//GetContextMenuManager()->AddMenu (_T("My menu"), IDR_CONTEXT_MENU);

	// TODO: add another context menus here
}

// 用于运行对话框的应用程序命令
void CSmartGisApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}


// CSmartGisApp 消息处理程序
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


