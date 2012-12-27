#pragma once


// CDlgCreateMap 对话框
#include "smt_bas_struct.h"

class CDlgCreateMap : public CDialog
{
	DECLARE_DYNAMIC(CDlgCreateMap)

public:
	CDlgCreateMap(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlgCreateMap();

// 对话框数据
	enum { IDD = IDD_DLG_CREATE_MAP };

protected:
	virtual void			DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	virtual BOOL            OnInitDialog();
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void            OnBnClickedOk();

public:
	CString                 GetMapName(void) { return m_strMapName;}
	Smt_Core::fRect			GetMapRect(void) {return m_mapRect;}


protected:
	CString                 m_strMapName;

protected:
	Smt_Core::fRect			m_mapRect;
};
