#pragma once


// CDlgCreateMap �Ի���
#include "smt_bas_struct.h"

class CDlgCreateMap : public CDialog
{
	DECLARE_DYNAMIC(CDlgCreateMap)

public:
	CDlgCreateMap(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CDlgCreateMap();

// �Ի�������
	enum { IDD = IDD_DLG_CREATE_MAP };

protected:
	virtual void			DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��
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
