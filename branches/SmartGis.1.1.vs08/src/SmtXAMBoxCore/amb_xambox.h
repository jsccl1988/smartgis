/*
File:    amb_xambox.h  

Desc:    SmtXAMBox,Aux Module Box 继承于CTreeCtrl

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _AMB_XAMBOX_H
#define _AMB_XAMBOX_H

#include "smt_core.h"
#include "am_amodule.h"
// SmtXAMBox
using namespace Smt_AM;

namespace Smt_XAMBox
{
	class SMT_EXPORT_CLASS SmtXAMBox : public CTreeCtrl
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
		HTREEITEM					m_hRoot;				//根节点

		vSmtFuncItems				m_vFuncItems;
		SmtAuxModule				*m_pAModule;
	};
}

#if !defined(Export_SmtXAMBoxCore)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtXAMBoxCoreD.lib")
#       else
#          pragma comment(lib,"SmtXAMBoxCore.lib")
#	    endif
#endif

#endif //_AMB_XAMBOX_H

