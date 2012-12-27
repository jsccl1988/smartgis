#pragma once

#include "vw_2dxview.h"
using namespace Smt_XView;
#include "Resource.h"
// CDlg2DXView �Ի���

class CDlg2DXView : public CDialog
{
	DECLARE_DYNAMIC(CDlg2DXView)

public:
	CDlg2DXView(string	 strMDocFile,CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CDlg2DXView();

// �Ի�������
	enum { IDD = IDD_DLG_2DXVIEW };

protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��
	virtual BOOL		OnInitDialog();

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void		OnDestroy();
	
protected:
	BOOL				InitGreateXView(void);

protected:
	Smt2DXView			*m_p2DXView;
	SmtMap				m_smtMap;
	string				m_strMDocFile;
};
