// DlgAbout.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtAMDemCreater.h"
#include "DlgAbout.h"


// CDlgAbout 对话框

IMPLEMENT_DYNAMIC(CDlgAbout, CDialog)

CDlgAbout::CDlgAbout(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgAbout::IDD, pParent)
{

}

CDlgAbout::~CDlgAbout()
{
}

void CDlgAbout::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CDlgAbout, CDialog)
	ON_BN_CLICKED(IDOK, &CDlgAbout::OnBnClickedOk)
END_MESSAGE_MAP()


// CDlgAbout 消息处理程序

void CDlgAbout::OnBnClickedOk()
{
	// TODO: 在此添加控件通知处理程序代码
	OnOK();
}
