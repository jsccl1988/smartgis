// DlgAbout.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "SmtAMDemCreater.h"
#include "DlgAbout.h"


// CDlgAbout �Ի���

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


// CDlgAbout ��Ϣ�������

void CDlgAbout::OnBnClickedOk()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	OnOK();
}
