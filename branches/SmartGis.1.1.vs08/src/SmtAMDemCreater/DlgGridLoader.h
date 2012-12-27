#pragma once
#include "vw_3dxview.h"
using namespace Smt_XView;

// CDlgGridLoader 对话框

class CDlgGridLoader : public CDialog
{
	DECLARE_DYNAMIC(CDlgGridLoader)

public:
	CDlgGridLoader(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlgGridLoader();

// 对话框数据
	enum { IDD = IDD_DLG_GRIDLOADER };

protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	virtual BOOL		OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void		OnBnClickedBtnSelhmapfile();
	afx_msg void		OnBnClickedBtnSeltexfile();
	afx_msg void		OnBnClickedOk();
	afx_msg void		OnBnClickedCkUsetex();

	afx_msg void		OnEnChangeEditXscale();
	afx_msg void		OnEnChangeEditYscale();
	afx_msg void		OnEnChangeEditZscale();

	afx_msg void		OnEnChangeEditXstart();
	afx_msg void		OnEnChangeEditYstart();
	afx_msg void		OnEnChangeEditZstart();

protected:
	int					Init3DStuff(void);

	void				UpdateClrTypeCmb(void);

protected:
	CString				m_strHMapUrl;
	float				m_fXScale;
	float				m_fYScale;
	float				m_fZScale;

	float				m_fXStart;
	float				m_fYStart;
	float				m_fZStart;

	bool				m_bUseTex;
	CString				m_strTexUrl;
	CString				m_strTexDir;
	CString				m_strTexExt;
	CString				m_strTexName;
	CComboBox			m_cmbClrType;

protected:
	LP3DRENDERDEVICE	m_p3DRenderDevice;
	SmtScene			*m_pScene;

	Smt3DXView			*m_p3DView;
};
