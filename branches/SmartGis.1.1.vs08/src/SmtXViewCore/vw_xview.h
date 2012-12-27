/*
File:    vw_xview.h 

Desc:    SmtXView,Smt View ¼Ì³ÐÓÚCView

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _VW_XVIEW_H
#define _VW_XVIEW_H

#include "smt_core.h"
#include "smt_listener.h"
using namespace Smt_Core;

#define CPtTolPt(cpt) (lPoint(cpt.x,cpt.y))

// SmtXView ÊÓÍ¼
namespace Smt_XView
{
	class SMT_EXPORT_CLASS SmtXView : public CView,public SmtListener
	{
		DECLARE_DYNCREATE(SmtXView)

	protected:
		SmtXView();						 
		virtual ~SmtXView();

	public:
		long						BindWind(HWND hWnd);
		long						BindDlgItem(CDialog *pDlg,UINT nItemID);
		long 						UnbindWind(void);

	public:
		virtual void				OnInitialUpdate();
		virtual void				OnDraw(CDC* pDC);   
		virtual void				OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView);
		virtual BOOL				PreTranslateMessage(MSG* pMsg); 
		virtual LRESULT				WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
		virtual void				PostNcDestroy();
		virtual void				OnActivateFrame(UINT nState, CFrameWnd* pDeactivateFrame);

#ifdef _DEBUG
		virtual void				AssertValid() const;
#ifndef _WIN32_WCE
		virtual void				Dump(CDumpContext& dc) const;
#endif
#endif

	protected:
		DECLARE_MESSAGE_MAP()
	public:
		afx_msg int					OnCreate(LPCREATESTRUCT lpCreateStruct);
		afx_msg void				OnDestroy();

	public:
		//addtion
		virtual	bool				InitCreate(void) ;
		virtual	bool				EndDestory(void) ;

		virtual	bool				CreateMainMenu(void) ;
		virtual	bool				CreateContexMenu(void);

		virtual bool				CreateRender(void) { return true;}
		virtual bool				CreateTools(void) { return true;}

	public:
		virtual	int					Notify(long nMsg,SmtListenerMsg &param);

	protected:
		HMENU						m_hContexMenu;
		HMENU						m_hMainMenu;

		BOOL						m_bActive;

	protected:
		CDialog						*m_pBindDlg;
		UINT						m_unBindItemID;
	};
}

#if !defined(Export_SmtXViewCore)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtXViewCoreD.lib")
#       else
#          pragma comment(lib,"SmtXViewCore.lib")
#	    endif
#endif

#endif //_VW_XVIEW_H