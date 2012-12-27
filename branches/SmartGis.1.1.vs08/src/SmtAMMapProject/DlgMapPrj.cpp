// DlgMapPrj.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtAMMapProject.h"
#include "DlgMapPrj.h"

#include "DlgMapPrjDoGrid.h"
#include "DlgMapPrjDoXY.h"

#include "prj_projection.h"
#include "prj_lambertprj.h"
#include "prj_gaussprj.h"

using namespace Smt_Prj;


// CDlgMapPrj 对话框

IMPLEMENT_DYNAMIC(CDlgMapPrj, CDialog)

CDlgMapPrj::CDlgMapPrj(CWnd* pParent /*=NULL*/)
: CDialog(CDlgMapPrj::IDD, pParent)
{
	m_pDlgDoGrid       = NULL;
	m_pDlgDoXY         = NULL;
	m_pPrjX            = new GaussPrj(IUGG1975);
}

CDlgMapPrj::~CDlgMapPrj()
{
	SMT_SAFE_DELETE(m_pDlgDoGrid);
	SMT_SAFE_DELETE(m_pDlgDoXY);
	SMT_SAFE_DELETE(m_pPrjX);
}

void CDlgMapPrj::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TAB_MAPPRJ, m_tabMapPrj);
}


BEGIN_MESSAGE_MAP(CDlgMapPrj, CDialog)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_MAPPRJ, &CDlgMapPrj::OnTcnSelchangeTabMapprj)
	ON_WM_CTLCOLOR()
END_MESSAGE_MAP()


// CDlgMapPrj 消息处理程序

void CDlgMapPrj::OnTcnSelchangeTabMapprj(NMHDR *pNMHDR, LRESULT *pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	int ipage = m_tabMapPrj.GetCurSel();
	switch(ipage)
	{
	case 0:
		m_pDlgDoGrid->ShowWindow(SW_SHOW);
		m_pDlgDoXY->ShowWindow(SW_HIDE);
		break;
	case 1:
		m_pDlgDoGrid->ShowWindow(SW_HIDE);
		m_pDlgDoXY->ShowWindow(SW_SHOW);
		break;
	default:
		break;
	}

	*pResult = 0;
}

BOOL CDlgMapPrj::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化
	m_tabMapPrj.InsertItem(0,"经纬网");  
	m_tabMapPrj.InsertItem(1,"单点转换");  
	m_pDlgDoGrid = new CDlgMapPrjDoGrid(m_pPrjX);
	m_pDlgDoXY   = new CDlgMapPrjDoXY(m_pPrjX);

	m_pDlgDoGrid->Create(IDD_DLG_PRJ_DOGRID,&m_tabMapPrj);
	m_pDlgDoXY->Create(IDD_DLG_PRJ_DOXY,&m_tabMapPrj);

	CRect rc;
	m_tabMapPrj.GetClientRect(rc);
	rc.top += 20;
	rc.bottom -= 0;
	rc.left += 2;
	rc.right -= 2;

	m_pDlgDoGrid->MoveWindow(&rc);
	m_pDlgDoXY->MoveWindow(&rc);


	m_pDlgDoGrid->ShowWindow(SW_SHOW);
	m_pDlgDoXY->ShowWindow(SW_HIDE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}
