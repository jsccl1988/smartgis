#pragma once


// CDlgInputText �Ի���
#include "legacy/ui/gui/resource.h"
class CDlgInputText : public CDialog
{
	DECLARE_DYNAMIC(CDlgInputText)

public:
	CDlgInputText(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CDlgInputText();

// �Ի�������
	enum { IDD = IDD_DLG_INPUTTEXT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedOk();

public:
	CString				m_strText;
};
