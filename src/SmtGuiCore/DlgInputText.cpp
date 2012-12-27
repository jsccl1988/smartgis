// DlgInputText.cpp : 实现文件
//

#include "stdafx.h"
#include "DlgInputText.h"


// CDlgInputText 对话框

IMPLEMENT_DYNAMIC(CDlgInputText, CDialog)

CDlgInputText::CDlgInputText(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgInputText::IDD, pParent)
{

}

CDlgInputText::~CDlgInputText()
{
}

void CDlgInputText::DoDataExchange(CDataExchange* pDX)
{
	DDX_Text(pDX,IDC_EDIT_TEXT,m_strText);
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CDlgInputText, CDialog)
	ON_BN_CLICKED(IDOK, &CDlgInputText::OnBnClickedOk)
END_MESSAGE_MAP()


// CDlgInputText 消息处理程序

void CDlgInputText::OnBnClickedOk()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);

	OnOK();
}
