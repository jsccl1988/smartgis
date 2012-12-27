#pragma once

#include "smt_core.h"
#include "Container.h"
#include "cata_mapservicexcatalog.h"
#include "DlgSvrCfgTile.h"
#include "DlgSvrCfgBase.h"

using namespace Smt_XCatalog;

// CDlgMapServiceMgr 对话框
#define		SMT_MSG_SELCHANGE_MAPSERVIVCE				(WM_USER+100)

class CDlgMapServiceMgr : public CDialog
{
	DECLARE_DYNAMIC(CDlgMapServiceMgr)

public:
	CDlgMapServiceMgr(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlgMapServiceMgr();

// 对话框数据
	enum { IDD = IDD_DLG_MAPSERVICEMGR };

protected:
	virtual void			DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	virtual BOOL			OnInitDialog();

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void			OnTcnSelchangeTabServiceCfg(NMHDR *pNMHDR, LRESULT *pResult);

	afx_msg void			OnBnClickedBtnRunConSvr();

	afx_msg void			OnBnClickedBtnInstallSvr();
	afx_msg void			OnBnClickedBtnStartSvr();
	afx_msg void			OnBnClickedBtnStopSvr();
	afx_msg void			OnBnClickedBtnUninstallSvr();
	afx_msg void			OnBnClickedBtnRestartSvr();

	afx_msg void			OnSelChangeMapService(void);

	afx_msg void            OnBnClickedOk();

protected:
	bool					CreateServiceTree();
	bool					CreateServiceCfgTab();

	void                    InitSetWnd();

protected:
	CContainer				m_contMapService;
	SmtMapServiceXCatalog	m_cataMapService;
	
	CTabCtrl                m_tabServiceCfg;
	CDlgSvrCfgBase			*m_pDlgSvrCfgBase;
	CDlgSvrCfgTile			*m_pDlgSvrCfgTile;

	vector<CWnd*>			m_vWnds;
};
