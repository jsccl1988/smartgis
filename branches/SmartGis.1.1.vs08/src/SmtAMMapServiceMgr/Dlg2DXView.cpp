// DlgXView.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtAMMapServiceMgr.h"
#include "Dlg2DXView.h"

#include "cata_mapmgr.h"
#include "gis_api.h"
#include "sde_datasourcemgr.h"

using namespace Smt_GIS;
using namespace Smt_SDEDevMgr;
using namespace Smt_XCatalog;

// CDlg2DXView 对话框

IMPLEMENT_DYNAMIC(CDlg2DXView, CDialog)

CDlg2DXView::CDlg2DXView(string	 strMDocFile,CWnd* pParent /*=NULL*/)
	: CDialog(CDlg2DXView::IDD, pParent)
	,m_p2DXView(NULL)
	,m_strMDocFile(strMDocFile)
{

}

CDlg2DXView::~CDlg2DXView()
{
	SMT_SAFE_DELETE(m_p2DXView);
}

void CDlg2DXView::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CDlg2DXView, CDialog)
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CDlg2DXView 消息处理程序

BOOL CDlg2DXView::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化
	if (!InitGreateXView())
		return FALSE;

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void CDlg2DXView::OnDestroy()
{
	CDialog::OnDestroy();

	// TODO: 在此处添加消息处理程序代码
	//m_p2DXView->UnbindWind();
}

//////////////////////////////////////////////////////////////////////////
BOOL CDlg2DXView::InitGreateXView(void)
{
	m_p2DXView = new Smt2DXView();

	m_p2DXView->BindDlgItem(this, IDC_XVIEW_CONTAINER);

	if (m_p2DXView->GetSafeHwnd())
		m_p2DXView->OnInitialUpdate();
	else
		return FALSE;

	if (SmtMapMgr::OpenMap(&m_smtMap,m_strMDocFile.c_str()))
	{
		m_p2DXView->SetOperMap(&m_smtMap);
		//m_p2DXView->SetOperMap(SmtMapMgr::GetSingletonPtr()->GetSmtMapPtr());
	}

	return TRUE;
}

