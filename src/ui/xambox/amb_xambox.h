/*
File:    amb_xambox.h  

Desc:    SmtXAMBox,Aux Module Box 锟教筹拷锟斤拷CTreeCtrl

Version: Version 1.0

Writter:  锟铰达拷锟斤拷

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _AMB_XAMBOX_H
#define _AMB_XAMBOX_H
#if defined(XAMBOX_EXPORTS)
#define XAMBOX_EXPORT __declspec(dllexport)
#else
#define XAMBOX_EXPORT __declspec(dllimport)
#endif


#include "base/core/core.h"
#include "plugin/legacy/module.h"
// SmtXAMBox
using namespace plugin;

namespace ui
{
	class XAMBOX_EXPORT SmtXAMBox : public CTreeCtrl
	{
		DECLARE_DYNAMIC(SmtXAMBox)

	public:
		SmtXAMBox(SmtAuxModule *pAModule);
		virtual ~SmtXAMBox();

	public:
		//addtion
		virtual	bool				InitCreate(void) ;
		virtual	bool				EndDestory(void) ;
		virtual	bool				CreateContexMenu(void);

	public:
		virtual BOOL				Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
		afx_msg int					OnCreate(LPCREATESTRUCT lpCreateStruct);
		afx_msg void				OnRButtonDown(UINT nFlags, CPoint point);
		afx_msg void				OnLButtonDown(UINT nFlags, CPoint point);
		afx_msg void				OnLButtonUp(UINT nFlags, CPoint point);

	public:
		bool						UpdateAMBoxTree(void);

	protected:
		DECLARE_MESSAGE_MAP()

	protected:
		HMENU						m_hContexMenu;

	protected:
		CImageList					m_imgList;
		HTREEITEM					m_hRoot;				//锟斤拷锟节碉拷

		vSmtFuncItems				m_vFuncItems;
		SmtAuxModule				*m_pAModule;
	};
}

#if !defined(XAMBOX_EXPORTS)
#if     defined( _DEBUG)
#          pragma comment(lib,"ui_legacy_d.lib")
#       else
#          pragma comment(lib,"ui_legacy.lib")
#	    endif
#endif

#endif //_AMB_XAMBOX_H

