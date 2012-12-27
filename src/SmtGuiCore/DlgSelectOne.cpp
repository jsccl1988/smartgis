// DlgSelectOne.cpp : 实现文件
//

#include "stdafx.h"
#include "DlgSelectOne.h"


// CDlgSelectOne 对话框

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


// CDlgSelectOne 消息处理程序

void CDlgSelectOne::OnBnClickedOk()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);
	CString strSelID;
	m_cmbIDs.GetLBText(m_cmbIDs.GetCurSel(),strSelID);
	m_unSelID = atoi(strSelID);
	OnOK();
}

BOOL CDlgSelectOne::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化
	UpdataIDCmb();
	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
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
