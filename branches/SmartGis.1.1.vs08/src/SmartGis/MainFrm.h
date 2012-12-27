// MainFrm.h : CMainFrame 类的接口
//
#pragma once
#include "cata_dsxcatalog.h"
#include "cata_mapdocxcatalog.h"
#include "cata_3dobjxcatalog.h"

#include "amb_amboxmgrdocbar.h"
#include "TabbedWndDockBar.h"
#include "StackedWndDockBar.h"

using namespace Smt_XCatalog;
using namespace Smt_XAMBox;

#define CMainWnd CBCGPMDIFrameWnd

class CMainFrame : public CMainWnd
{
	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame();

// 属性
public:

// 操作
public:

// 重写
public:
	void						UpdateMDITabs (BOOL bResetMDIChild);
	CBCGPMDIChildWnd*			CreateDocumentWindow (LPCTSTR lpcszDocName);
	virtual BOOL				PreCreateWindow(CREATESTRUCT& cs);

// 实现
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:  // 控件条嵌入成员
	CBCGPMenuBar				m_wndMenuBar;  // New menu bar
//	CBCGPToolBar				m_wndToolBar; // Application toolbar
	CBCGPStatusBar				m_wndStatusBar;
	TabbedWndDockBar			m_wndCatalogDocBar;
	SmtAMBoxMgrDocBar			m_wndAMBoxMgrDocBar;
	
// 生成的消息映射函数
protected:
	afx_msg int					OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void				OnDestroy();
	
	afx_msg void				OnAppLook(UINT id);
	afx_msg LRESULT				OnGetTabToolTip(WPARAM wp, LPARAM lp);
	afx_msg void				OnWndMapedit();
	afx_msg void				OnWndMapdata();
	afx_msg void				OnWnd3d();

	DECLARE_MESSAGE_MAP()
	
public:
	void						SetStatusBarString(UINT index,CString str);

private:
	void						InitStatusBar(void);
	bool						InitCatalogDockBar(void);
	bool						InitAMBoxMgrDockBar(void);

private:
	//
	bool						InitMapDocCatalog(void);
	bool						Init3DObjCatalog(void);
	bool						InitDSCatalog(void);

protected:
	UINT						m_nAppLook;

private:
	//
	SmtMapDocXCatalog			*m_pMapDocCatalog;
	Smt3DObjXCatalog			*m_p3DObjCatalog;
	SmtDSXCatalog				*m_pDSCatalog;
};


