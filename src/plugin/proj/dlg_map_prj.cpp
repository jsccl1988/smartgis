// dlg_map_prj.cpp : implementation file

#include "stdafx.h"
#include "plugin/proj/map_project.h"
#include "plugin/proj/dlg_map_prj.h"

#include "plugin/proj/dlg_map_prj_do_grid.h"
#include "plugin/proj/dlg_map_prj_do_xy.h"

IMPLEMENT_DYNAMIC(CDlgMapPrj, CDialog)

CDlgMapPrj::CDlgMapPrj(CWnd* pParent /*=NULL*/)
: CDialog(CDlgMapPrj::IDD, pParent)
{
	m_pDlgDoGrid       = NULL;
	m_pDlgDoXY         = NULL;
}

CDlgMapPrj::~CDlgMapPrj()
{
	SMT_SAFE_DELETE(m_pDlgDoGrid);
	SMT_SAFE_DELETE(m_pDlgDoXY);
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

void CDlgMapPrj::sync_xy_scale()
{
	if (m_pDlgDoXY && m_pDlgDoGrid)
		m_pDlgDoXY->SetScaleRuler(m_pDlgDoGrid->ScaleRuler());
}

void CDlgMapPrj::OnTcnSelchangeTabMapprj(NMHDR *pNMHDR, LRESULT *pResult)
{
	int ipage = m_tabMapPrj.GetCurSel();
	switch(ipage)
	{
	case 0:
		m_pDlgDoGrid->ShowWindow(SW_SHOW);
		m_pDlgDoXY->ShowWindow(SW_HIDE);
		break;
	case 1:
		sync_xy_scale();
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

	m_tabMapPrj.InsertItem(0,"��γ��");
	m_tabMapPrj.InsertItem(1,"����ת��");
	m_pDlgDoGrid = new CDlgMapPrjDoGrid();
	m_pDlgDoXY   = new CDlgMapPrjDoXY();

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

	sync_xy_scale();
	m_pDlgDoGrid->ShowWindow(SW_SHOW);
	m_pDlgDoXY->ShowWindow(SW_HIDE);

	return TRUE;
}
