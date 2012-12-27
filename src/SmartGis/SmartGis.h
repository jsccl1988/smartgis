// SmartGis.h : SmartGis 应用程序的主头文件
//
#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含“stdafx.h”以生成 PCH 文件"
#endif

#include "resource.h"       // 主符号


// CSmartGisApp:
// 有关此类的实现，请参阅 SmartGis.cpp
//

#include "smt_core.h"
#include "app_smtapp.h"
#include "smt_env_struct.h"

using namespace Smt_Core;
using namespace Smt_App;

class CMDITabOptions
{
public:
	CMDITabOptions();

	enum MDITabsType
	{
						None,
						MDITabsStandard,
						MDITabbedGroups
	};
public:
	void				Load ();
	void				Save ();

	BOOL				IsMDITabsDisabled () const {return m_nMDITabsType == CMDITabOptions::None;}

public:
	MDITabsType			m_nMDITabsType;
	BOOL				m_bMaximizeMDIChild;
	BOOL				m_bTabsOnTop;
	BOOL				m_bActiveTabCloseButton;
	CBCGPTabWnd::Style	m_nTabsStyle;
	BOOL				m_bTabsAutoColor;
	BOOL				m_bMDITabsIcons;
	BOOL				m_bMDITabsDocMenu;
	BOOL				m_bDragMDITabs;
	BOOL				m_bMDITabsContextMenu;
	int					m_nMDITabsBorderSize;
	BOOL				m_bDisableMDIChildRedraw;
	BOOL				m_bFlatFrame;
	BOOL				m_bCustomTooltips;
};

class CSmartGisApp : public CWinApp,public CBCGPWorkspace,public SmtApp
{
public:
	CSmartGisApp();

// 重写
public:
	virtual BOOL				InitInstance();
	virtual int					ExitInstance();

	virtual void				PreLoadState ();

// 实现
	afx_msg void				OnAppAbout();
	DECLARE_MESSAGE_MAP()

public:
	//////////////////////////////////////////////////////////////////////////
	//
	inline	CMultiDocTemplate*	GetEditViewDocTemplate() { return m_pEditViewDocTemplate;}
	inline	CMultiDocTemplate*	GetDataViewDocTemplate() { return m_pDataViewDocTemplate;}
	inline	CMultiDocTemplate*	Get3DViewDocTemplate() { return m_p3DViewDocTemplate;}

	//////////////////////////////////////////////////////////////////////////
	CView*						GetActiveDocView(CRuntimeClass* pViewClass);
	CView*						GetActiveView(void);
	CDocument*					GetActiveDoc(void);

public:
	CMDITabOptions				m_Options;

protected:
	CMultiDocTemplate*			m_pEditViewDocTemplate;
	CMultiDocTemplate*			m_pDataViewDocTemplate;
	CMultiDocTemplate*			m_p3DViewDocTemplate;

};

extern CSmartGisApp theApp;