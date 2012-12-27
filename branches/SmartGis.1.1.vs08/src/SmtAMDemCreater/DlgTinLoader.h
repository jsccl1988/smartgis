#pragma once
#include "afxwin.h"
#include "vw_3dxview.h"

#include "GridCtrlSupport.h"

using namespace Smt_XView;

// CDlgTinLoader 对话框

class CDlgTinLoader : public CDialog
{
	DECLARE_DYNAMIC(CDlgTinLoader)

public:
	CDlgTinLoader(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlgTinLoader();

// 对话框数据
	enum { IDD = IDD_DLG_TINLOADER };

protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	virtual BOOL		OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void		OnBnClickedBtnSelvertexfile();
	afx_msg void		OnBnClickedBtnSeltexfile();
	afx_msg void		OnBnClickedOk();
	afx_msg void		OnBnClickedRadioTab();
	afx_msg void		OnBnClickedRadioSpace();
	afx_msg void		OnBnClickedRadioComma();
	
	afx_msg void		OnBnClickedCkUsetex();
	afx_msg void		OnBnClickedCkGen2DTin();

	afx_msg void		OnEnChangeEditXscale();
	afx_msg void		OnEnChangeEditYscale();
	afx_msg void		OnEnChangeEditZscale();

protected:
	//////////////////////////////////////////////////////////////////////////
	//1.
	void				OnSelVertexFile(void);

	//////////////////////////////////////////////////////////////////////////
	//2.
	void				UpdateColVGrid(void);

	void				UpdateColVGridHead(void);
	void				UpdateColVGridContent(void);

	//////////////////////////////////////////////////////////////////////////
	//3.
	void				UpdateClrTypeCmb(void);

	//4.
	void				UpdateXYZCmb(void);

	//5.
	void				Update2DTinLayerCmb(void);

private:
	void				OnChangeSeparator(void);
	long				SplitString(vector<string> &vFldVal,string strContent);
	
protected:
	int					Init3DStuff(void);

private:

	CString				m_strVertexUrl;

	int					m_nSeparator;
	UINT				m_nHeadSkip;
	UINT				m_nLineSkip;

	CGridCtrl			m_colvGrid;
	vector<string>		m_vTextBuf;
	int					m_nCol;

	CComboBox			m_cmbX;
	CComboBox			m_cmbY;
	CComboBox			m_cmbZ;

	int					m_iX;
	int					m_iY;
	int					m_iZ;

	float				m_fXScale;
	float				m_fYScale;
	float				m_fZScale;

	bool				m_bUseTex;
	CString				m_strTexUrl;
	CString				m_strTexDir;
	CString				m_strTexExt;
	CString				m_strTexName;
	CComboBox			m_cmbClrType;

	bool				m_bGen2DTin;
	CComboBox			m_cmbTinLayer;

protected:
	LP3DRENDERDEVICE	m_p3DRenderDevice;
	SmtScene			*m_pScene;

	Smt3DXView			*m_p3DView;
};
