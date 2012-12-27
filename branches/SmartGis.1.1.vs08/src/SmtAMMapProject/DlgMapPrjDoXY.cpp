// DlgMapPrjDoXY.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "SmtAMMapProject.h"
#include "DlgMapPrjDoXY.h"

#include "prj_projection.h"
#include "prj_lambertprj.h"
#include "prj_gaussprj.h"

// CDlgMapPrjDoXY �Ի���

IMPLEMENT_DYNAMIC(CDlgMapPrjDoXY, CDialog)

CDlgMapPrjDoXY::CDlgMapPrjDoXY(PrjX *pPrjX,CWnd* pParent /*=NULL*/)
: CDialog(CDlgMapPrjDoXY::IDD, pParent)
, m_fL(120)
, m_fB(36)
, m_fX(0)
, m_fY(0)
{
	m_pPrj = new Projection(pPrjX);
}

CDlgMapPrjDoXY::~CDlgMapPrjDoXY()
{
	SMT_SAFE_DELETE(m_pPrj);
}

void CDlgMapPrjDoXY::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_L, m_fL);
	DDX_Text(pDX, IDC_EDIT_B, m_fB);
	DDX_Text(pDX, IDC_EDIT_X, m_fX);
	DDX_Text(pDX, IDC_EDIT_Y, m_fY);
}


BEGIN_MESSAGE_MAP(CDlgMapPrjDoXY, CDialog)
	ON_BN_CLICKED(IDC_BTN_DOXY, &CDlgMapPrjDoXY::OnBnClickedBtnDoxy)
	ON_WM_CTLCOLOR()
	ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()


// CDlgMapPrjDoXY ��Ϣ�������
void CDlgMapPrjDoXY::OnBnClickedBtnDoxy()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	UpdateData(TRUE);
	m_pPrj->PrjLB2XY(m_fB,m_fL,m_fY,m_fX);
	UpdateData(FALSE);
}

BOOL CDlgMapPrjDoXY::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  �ڴ���Ӷ���ĳ�ʼ��
	return TRUE;  // return TRUE unless you set the focus to a control
	// �쳣: OCX ����ҳӦ���� FALSE
}

void CDlgMapPrjDoXY::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);

	// TODO: �ڴ˴������Ϣ����������
	CString str;
	int lScaleRuler = m_pPrj->GetPrjX()->GetScaleRuler();
	str.Format("1:%d",lScaleRuler);
	GetDlgItem(IDC_STATIC_SCALERULER)->SetWindowText(str);

}
