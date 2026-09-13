/*
File:    vw_xview.h 

Desc:    SmtXView,Smt View �̳���CView

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _VW_XVIEW_H
#define _VW_XVIEW_H
#if defined(XVIEW_EXPORTS)
#define XVIEW_EXPORT __declspec(dllexport)
#else
#define XVIEW_EXPORT __declspec(dllimport)
#endif


#include "base/core/core.h"
#include "base/core/listener.h"
#include "tool/gestures.h"
using namespace base;

namespace content {
class ViewHost;
}

#define CPtTolPt(cpt) (lPoint(cpt.x,cpt.y))

// SmtXView ��ͼ
namespace ui
{
	class XVIEW_EXPORT SmtXView : public CView,public SmtListener
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

		content::ViewHost*			view_host() { return m_pViewHost; }
		void						reset_view_host(content::ViewHost* host);
		bool						route_chrome_command(unsigned int msg);
		void						bind_draft_observer();
		void						dispatch_menu_command(unsigned int msg);
		virtual void				apply_workspace_draft(const tool::Draft& draft);

	public:
		virtual	int					notify(long nMsg,SmtListenerMsg &param);

	protected:
		HMENU						m_hContexMenu;
		HMENU						m_hMainMenu;

		BOOL						m_bActive;
		content::ViewHost*			m_pViewHost;

	protected:
		CDialog						*m_pBindDlg;
		UINT						m_unBindItemID;
	};
}

#if !defined(XVIEW_EXPORTS)
#if     defined( _DEBUG)
#          pragma comment(lib,"xviewD.lib")
#       else
#          pragma comment(lib,"xview.lib")
#	    endif
#endif

#endif //_VW_XVIEW_H