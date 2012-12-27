#pragma once

#include "msvr_mapservice.h"
using namespace Smt_MapService;
// CDlgSvrCfgBase 对话框

class CDlgSvrCfgBase : public CDialog
{
	DECLARE_DYNAMIC(CDlgSvrCfgBase)

public:
	CDlgSvrCfgBase(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlgSvrCfgBase();

// 对话框数据
	enum { IDD = IDD_DLG_SVRCFG_BASE };

protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	virtual BOOL		OnInitDialog();

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void		OnShowWindow(BOOL bShow, UINT nStatus);

	afx_msg void		OnBnClickedBtnSelMDoc();
	afx_msg void		OnBnClickedBtnSelLog();
	afx_msg void		OnBnClickedBtnReviewMap();
	afx_msg void		OnBnClickedBtnCreate();

	afx_msg void		OnEnUpdateEditName();
	afx_msg void		OnEnUpdateEditMDoc();

	afx_msg void		OnEnUpdateEditXMin();
	afx_msg void		OnEnUpdateEditYMin();
	afx_msg void		OnEnUpdateEditXMax();
	afx_msg void		OnEnUpdateEditYMax();

	afx_msg void		OnCbnSelchangeCmbSRS();

public:
	void				SetMapService(SmtMapService		*pMapService) {m_pMapService = pMapService;}

public:
	bool				UpdateMapServiceUI(void);

protected:
	CString				m_strName;
	CString				m_strMapDoc;
	CString				m_strLog;
	Smt_Core::fRect		m_mapRect;

	SmtMapService		*m_pMapService;
};
