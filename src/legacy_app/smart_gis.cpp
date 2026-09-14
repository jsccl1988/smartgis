// smart_gis.cpp : ����Ӧ�ó��������Ϊ��
//

#include "legacy_app/stdafx.h"
#include "legacy_app/smart_gis.h"
#include "legacy_app/main_frame.h"

#include "legacy_app/child_frame.h"
#include "legacy_app/smart_gis_doc.h"
#include "legacy_app/smart_gis_view.h"
#include "legacy_app/smart_data_source_view.h"
#include "legacy_app/smart_map_edit_view.h"
#include "legacy_app/smart_3d_view.h"

#include "sys/sysmanager.h"

#include "base/core/api.h"
#include "legacy_ui/xcatalog/mapmgr.h"

using namespace base;
using namespace sdb;
using namespace sys;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// ����Ӧ�ó��򡰹��ڡ��˵���� CAboutDlg �Ի���
class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

	// �Ի�������
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	// ʵ��
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
	// TODO: �ڴ����ӿؼ�֪ͨ�����������
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
	// �����ļ��ı�׼�ĵ�����
//	ON_COMMAND(ID_FILE_NEW, &CWinApp::OnFileNew)
//	ON_COMMAND(ID_FILE_OPEN, &CWinApp::OnFileOpen)
	// ��׼��ӡ��������
	ON_COMMAND(ID_FILE_PRINT_SETUP, &CWinApp::OnFilePrintSetup)
END_MESSAGE_MAP()


// CSmartGisApp ����

CSmartGisApp::CSmartGisApp()
{
	// TODO: �ڴ˴����ӹ�����룬
	// ��������Ҫ�ĳ�ʼ�������� InitInstance ��

	m_pEditViewDocTemplate = NULL;
	m_pDataViewDocTemplate = NULL;
	m_p3DViewDocTemplate = NULL;
}


// Ψһ��һ�� CSmartGisApp ����

CSmartGisApp theApp;


// CSmartGisApp ��ʼ��

BOOL CSmartGisApp::InitInstance()
{
	// ���һ�������� Windows XP �ϵ�Ӧ�ó����嵥ָ��Ҫ
	// ʹ�� ComCtl32.dll �汾 6 ����߰汾�����ÿ��ӻ���ʽ��
	//����Ҫ InitCommonControlsEx()�����򣬽��޷��������ڡ�
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// ��������Ϊ��������Ҫ��Ӧ�ó�����ʹ�õ�
	// �����ؼ��ࡣ
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinAppEx::InitInstance();

	// ��ʼ�� OLE ��
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}

	globalData.SetDPIAware ();

	AfxEnableControlContainer();
	// ��׼��ʼ��
	// ���δʹ����Щ���ܲ�ϣ����С
	// ���տ�ִ���ļ��Ĵ�С����Ӧ�Ƴ�����
	// ����Ҫ���ض���ʼ������
	// �������ڴ洢���õ�ע�����
	// TODO: Ӧ�ʵ��޸ĸ��ַ�����
	// �����޸�Ϊ��˾����֯��
	SetRegistryKey(_T("Ӧ�ó��������ɵı���Ӧ�ó���"));
	LoadStdProfileSettings(4);  // ���ر�׼ INI �ļ�ѡ��(���� MRU)


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

	//����3���մ���
	m_pDataViewDocTemplate->OpenDocumentFile(NULL);
	m_pEditViewDocTemplate->OpenDocumentFile(NULL);
	m_p3DViewDocTemplate->OpenDocumentFile(NULL);
	
	if(!SmtApp::DelayInit()) 
		return FALSE;

	// �������ѳ�ʼ���������ʾ����������и���
	m_pMainWnd->ShowWindow(SW_SHOWMAXIMIZED);
	pMainFrame->ShowWindow(SW_SHOWMAXIMIZED);
	pMainFrame->UpdateWindow();

	if (wcsstr(GetCommandLineW(), L"--self-test")) {
		if (!m_pMainWnd || !::IsWindow(m_pMainWnd->m_hWnd))
			return FALSE;
		// Skip Run(): MFC can sit on a modal/plugin pump after ShowWindow.
		::ExitProcess(0);
	}

	return TRUE;
}

int CSmartGisApp::ExitInstance()
{
	// TODO: �ڴ�����ר�ô����/����û���
	SmtApp::Destory();

	m_Options.Save ();

	CleanState();
	BCGCBProCleanUp();

	return CWinAppEx::ExitInstance();
}

void CSmartGisApp::PreLoadState ()
{
	//GetContextMenuManager()->AddMenu (_T("My menu"), IDR_CONTEXT_MENU);

	// TODO: add another context menus here
}

// �������жԻ����Ӧ�ó�������
void CSmartGisApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}


// CSmartGisApp ��Ϣ��������
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


