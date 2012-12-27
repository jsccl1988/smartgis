// DlgSvrCfgBase.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtAMMapServiceMgr.h"
#include "DlgSvrCfgBase.h"
#include "msvr_mgr_api.h"
#include "cata_mapservicemgr.h"
#include "bl_api.h"

using namespace Smt_Core;
using namespace Smt_XCatalog;
// CDlgSvrCfgBase 对话框

IMPLEMENT_DYNAMIC(CDlgSvrCfgBase, CDialog)

CDlgSvrCfgBase::CDlgSvrCfgBase(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgSvrCfgBase::IDD, pParent)
	, m_pMapService(NULL)
	, m_strName(_T(""))
	, m_strLog(_T(""))
{
	m_mapRect.lb.x = 0;
	m_mapRect.lb.y = 0;
	m_mapRect.rt.x = 500;
	m_mapRect.rt.y = 500;
}

CDlgSvrCfgBase::~CDlgSvrCfgBase()
{

}

void CDlgSvrCfgBase::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_NAME, m_strName);
	DDX_Text(pDX, IDC_EDIT_XMIN, m_mapRect.lb.x);
	DDX_Text(pDX, IDC_EDIT_YMIN, m_mapRect.lb.y);
	DDX_Text(pDX, IDC_EDIT_XMAX, m_mapRect.rt.x);
	DDX_Text(pDX, IDC_EDIT_YMAX, m_mapRect.rt.y);
	DDX_Text(pDX, IDC_EDIT_MDOC, m_strMapDoc);
	DDX_Text(pDX, IDC_EDIT_LOG, m_strLog);
}


BEGIN_MESSAGE_MAP(CDlgSvrCfgBase, CDialog)
	ON_BN_CLICKED(IDC_BTN_SEL_MDOC, &CDlgSvrCfgBase::OnBnClickedBtnSelMDoc)
	ON_BN_CLICKED(IDC_BTN_SEL_LOG, &CDlgSvrCfgBase::OnBnClickedBtnSelLog)
	ON_BN_CLICKED(IDC_BTN_REVIEWMAP, &CDlgSvrCfgBase::OnBnClickedBtnReviewMap)
	ON_BN_CLICKED(IDC_BTN_CREATE,  &CDlgSvrCfgBase::OnBnClickedBtnCreate)
	ON_EN_UPDATE(IDC_EDIT_NAME, &CDlgSvrCfgBase::OnEnUpdateEditName)
	ON_EN_UPDATE(IDC_EDIT_MDOC, &CDlgSvrCfgBase::OnEnUpdateEditMDoc)
	ON_EN_UPDATE(IDC_EDIT_XMIN, &CDlgSvrCfgBase::OnEnUpdateEditXMin)
	ON_EN_UPDATE(IDC_EDIT_YMIN, &CDlgSvrCfgBase::OnEnUpdateEditYMin)
	ON_EN_UPDATE(IDC_EDIT_XMAX, &CDlgSvrCfgBase::OnEnUpdateEditXMax)
	ON_EN_UPDATE(IDC_EDIT_YMAX, &CDlgSvrCfgBase::OnEnUpdateEditYMax)
	ON_CBN_SELCHANGE(IDC_CMB_SRS, &CDlgSvrCfgBase::OnCbnSelchangeCmbSRS)
	ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()


// CDlgSvrCfgBase 消息处理程序
BOOL CDlgSvrCfgBase::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化
	if (!UpdateMapServiceUI())
		return FALSE;

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

//////////////////////////////////////////////////////////////////////////
void CDlgSvrCfgBase::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);

	// TODO: 在此处添加消息处理程序代码
	if (bShow)
	{
		UpdateMapServiceUI();
	}
}

bool CDlgSvrCfgBase::UpdateMapServiceUI(void)
{
	if (NULL == m_pMapService)
		return true;

	Envelope	envMap;
	m_strName	= m_pMapService->GetName().c_str();
	m_strMapDoc	= m_pMapService->GetMapDoc().c_str();
	
	m_pMapService->GetEnvelope(envMap);
	EnvelopeToRect(m_mapRect,envMap);

	UpdateData(FALSE);

	return true;
}

void CDlgSvrCfgBase::OnBnClickedBtnSelMDoc()
{
	if (NULL == m_pMapService)
		return;

	UpdateData(TRUE);

	static char BASED_CODE szFilter[] = "Data Files (*.mdoc)|*.mdoc||" ;

	CFileDialog dlg( TRUE , NULL , NULL , OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT , szFilter , NULL ) ;

	if( dlg.DoModal() == IDCANCEL ||
		dlg.GetPathName().IsEmpty())
	{
		return;
	}

	m_strMapDoc = dlg.GetPathName();
	UpdateData(FALSE);

	m_pMapService->SetMapDoc((LPCTSTR)m_strMapDoc);
	if (m_pMapService->IsCreate())
	{
		m_pMapService->Destroy();
	}
}

void CDlgSvrCfgBase::OnBnClickedBtnSelLog()
{
	if (NULL == m_pMapService)
		return;

	UpdateData(TRUE);

	static char BASED_CODE szFilter[] = "Data Files (*.mdoc)|*.mdoc||" ;

	CFileDialog dlg(FALSE , NULL , NULL , OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT , NULL , NULL ) ;

	if( dlg.DoModal() == IDCANCEL ||
		dlg.GetPathName().IsEmpty())
	{
		return;
	}

	m_strLog = dlg.GetPathName();

	UpdateData(FALSE);
}

void CDlgSvrCfgBase::OnBnClickedBtnReviewMap()
{
	if (NULL == m_pMapService)
		return;

	UpdateData(TRUE);

	MSVR_MGR_ViewMap((LPCTSTR)m_strMapDoc);
}

void CDlgSvrCfgBase::OnBnClickedBtnCreate()
{
	if (NULL == m_pMapService)
		return;

	UpdateData(TRUE);

	if (!m_pMapService->IsCreate())
	{
		if (SMT_ERR_NONE == m_pMapService->Init(m_strLog) && 
			SMT_ERR_NONE == m_pMapService->Create())
		{
			Envelope	envMap;
			m_pMapService->GetEnvelope(envMap);
			EnvelopeToRect(m_mapRect,envMap);

			UpdateData(FALSE);
		}
		else
		{
			::MessageBox(::GetActiveWindow(), "创建地图服务失败，请检查设置参数!", "提示", MB_OK);
		}
	}
}

void CDlgSvrCfgBase::OnEnUpdateEditName()
{
	if (NULL == m_pMapService)
		return;

	UpdateData(TRUE);
	m_pMapService->SetName((LPCTSTR)m_strName);
	SmtMapServiceMgr *pMServiceMgr = SmtMapServiceMgr::GetSingletonPtr();
	pMServiceMgr->UpdateMServiceCatalog();
}

void CDlgSvrCfgBase::OnEnUpdateEditMDoc()
{
	if (NULL == m_pMapService)
		return;

	UpdateData(TRUE);

	m_pMapService->SetMapDoc((LPCTSTR)m_strMapDoc);
	if (m_pMapService->IsCreate())
	{
		m_pMapService->Destroy();
	}
}

void CDlgSvrCfgBase::OnEnUpdateEditXMin()
{
	if (NULL == m_pMapService)
		return;

	UpdateData(TRUE);
}

void CDlgSvrCfgBase::OnEnUpdateEditYMin()
{
	if (NULL == m_pMapService)
		return;

	UpdateData(TRUE);
}

void CDlgSvrCfgBase::OnEnUpdateEditXMax()
{
	if (NULL == m_pMapService)
		return;

	UpdateData(TRUE);
}

void CDlgSvrCfgBase::OnEnUpdateEditYMax()
{
	if (NULL == m_pMapService)
		return;

	UpdateData(TRUE);
}

void CDlgSvrCfgBase::OnCbnSelchangeCmbSRS()
{
	if (NULL == m_pMapService)
		return;

	UpdateData(TRUE);
}