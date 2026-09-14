// dlg_select_one.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "legacy_ui/gui/dlg_select_one.h"


// CDlgSelectOne �Ի���

IMPLEMENT_DYNAMIC(CDlgSelectOne, CDialog)

CDlgSelectOne::CDlgSelectOne(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgSelectOne::IDD, pParent)
{

}

CDlgSelectOne::~CDlgSelectOne()
{
}

void CDlgSelectOne::DoDataExchange(CDataExchange* pDX)
{
	DDX_Control(pDX,IDC_CMB_OBJID,m_cmbIDs);
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CDlgSelectOne, CDialog)
	ON_BN_CLICKED(IDOK, &CDlgSelectOne::OnBnClickedOk)
END_MESSAGE_MAP()


// CDlgSelectOne ��Ϣ��������

void CDlgSelectOne::OnBnClickedOk()
{
	// TODO: �ڴ����ӿؼ�֪ͨ�����������
	UpdateData(TRUE);
	CString strSelID;
	m_cmbIDs.GetLBText(m_cmbIDs.GetCurSel(),strSelID);
	m_unSelID = atoi(strSelID);
	OnOK();
}

BOOL CDlgSelectOne::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  �ڴ����Ӷ���ĳ�ʼ��
	UpdataIDCmb();
	return TRUE;  // return TRUE unless you set the focus to a control
	// �쳣: OCX ����ҳӦ���� FALSE
}

void CDlgSelectOne::UpdataIDCmb(void)
{
	CString strID;
	m_cmbIDs.ResetContent();
	for (int i = 0; i < m_vIDs.size();i++)
	{
		strID.Format("%d",m_vIDs[i]);
		m_cmbIDs.AddString(strID);
	}

	if (m_vIDs.size() > 0 )
	{
		m_cmbIDs.SetCurSel(0);
	}
}
