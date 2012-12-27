#pragma once
#include "afxwin.h"

#include "gis_sde.h"

using namespace Smt_GIS;
// CDlgCreateDS 对话框

class CDlgCreateDS : public CDialog
{
	DECLARE_DYNAMIC(CDlgCreateDS)

public:
	CDlgCreateDS(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlgCreateDS();

	// 对话框数据
	enum { IDD = IDD_DLG_CREATE_DS };

protected:
	virtual void				DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	virtual BOOL				OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void				OnBnClickedOk();
	afx_msg void				OnBnClickedBtnSeldb();
	afx_msg void				OnBnClickedBtnSelpath();
	afx_msg void				OnEnKillfocusEditDsDbseveice();
	afx_msg void				OnEnKillfocusEditDsDb();
	afx_msg void				OnEnKillfocusEditDsUserid();
	afx_msg void				OnEnKillfocusEditDsPwd();
	afx_msg void				OnEnKillfocusEditDsName();
	afx_msg void				OnEnKillfocusEditDsFilePath();

	afx_msg void				OnCbnSelchangeCmbProvider();

	afx_msg void				OnTvnSelchangedDstypeTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void				OnNMClickDstypeTree(NMHDR *pNMHDR, LRESULT *pResult);

private:
	void						UpdateProviderCmb(void);
	void						UpdateDSTypeTree(void);

public:
	CString						m_strSelTypeName;
	
protected:
	CTreeCtrl					m_typeTree;
	HTREEITEM					m_hTypeRoot;

public:
	CComboBox					m_cmbProvider;

public:
	CString						m_strDSName;
	UINT						m_unType;
	UINT						m_unProvider;

	CString						m_strFilePath;
	CString						m_strFileName;

	CString						m_strService;
	CString						m_strDBName;

	CString						m_strUserid;
	CString						m_strUserpwd;

	SmtDataSourceInfo			m_dsInfo;
};
