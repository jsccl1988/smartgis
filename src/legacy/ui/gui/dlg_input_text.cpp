// dlg_input_text.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "legacy/ui/gui/dlg_input_text.h"


// CDlgInputText �Ի���

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


// CDlgInputText ��Ϣ��������

void CDlgInputText::OnBnClickedOk()
{
	// TODO: �ڴ����ӿؼ�֪ͨ�����������
	UpdateData(TRUE);

	OnOK();
}
