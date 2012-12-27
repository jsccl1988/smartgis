#pragma once


// CDlgInputText 对话框
#include "resource.h"
class CDlgInputText : public CDialog
{
	DECLARE_DYNAMIC(CDlgInputText)

public:
	CDlgInputText(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlgInputText();

// 对话框数据
	enum { IDD = IDD_DLG_INPUTTEXT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedOk();

public:
	CString				m_strText;
};
