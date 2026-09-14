#pragma once

#include "legacy/ui/xview/view_2d.h"
using namespace ui;

// CDlg2DXView �Ի���

class CDlg2DXView : public CDialog
{
	DECLARE_DYNAMIC(CDlg2DXView)

public:
	CDlg2DXView(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CDlg2DXView();

// �Ի�������
	enum { IDD = IDD_DLG_2DXVIEW };

protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��
	virtual BOOL		OnInitDialog();

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void		OnDestroy();
	afx_msg void		OnBnClickedBtnSave();

protected:
	BOOL				InitGreateXView(void);

protected:
	Smt2DXView			*m_p2DXView;
};
