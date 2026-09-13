// main_frame.h : CMainFrame ��Ľӿ�
//
#pragma once
#include "ui/xcatalog/dsxcatalog.h"
#include "ui/xcatalog/mapdocxcatalog.h"
#include "ui/xcatalog/3dobjxcatalog.h"

#include "ui/xambox/amb_amboxmgrdocbar.h"
#include "ui/mfc_ex/tabbed_wnd_dock_bar.h"
#include "ui/mfc_ex/stacked_wnd_dock_bar.h"

using namespace ui;
using namespace ui;

#define CMainWnd CBCGPMDIFrameWnd

class CMainFrame : public CMainWnd
{
	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame();

// ����
public:

// ����
public:

// ��д
public:
	void						UpdateMDITabs (BOOL bResetMDIChild);
	CBCGPMDIChildWnd*			CreateDocumentWindow (LPCTSTR lpcszDocName);
	virtual BOOL				PreCreateWindow(CREATESTRUCT& cs);

// ʵ��
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:  // �ؼ���Ƕ���Ա
	CBCGPMenuBar				m_wndMenuBar;  // New menu bar
//	CBCGPToolBar				m_wndToolBar; // Application toolbar
	CBCGPStatusBar				m_wndStatusBar;
	TabbedWndDockBar			m_wndCatalogDocBar;
	SmtAMBoxMgrDocBar			m_wndAMBoxMgrDocBar;
	
// ���ɵ���Ϣӳ�亯��
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


