/*
File:    cata_3dobjxcatalog.h  

Desc:    Smt3DObjXCatalog,Smt 3D Object Catalog 锟教筹拷锟斤拷SmtXCatalog

Version: Version 1.0

Writter:  锟铰达拷锟斤拷

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _CATA_3DOBJXCATALOG_H
#define _CATA_3DOBJXCATALOG_H
#if defined(XCATALOG_EXPORTS)
#define XCATALOG_EXPORT __declspec(dllexport)
#else
#define XCATALOG_EXPORT __declspec(dllimport)
#endif


#include "legacy/ui/xcatalog/xcatalog.h"
// SmtXCatalog

namespace ui
{
	class XCATALOG_EXPORT Smt3DObjXCatalog : public SmtXCatalog
	{
		DECLARE_DYNAMIC(Smt3DObjXCatalog)

	public:
		Smt3DObjXCatalog();
		virtual ~Smt3DObjXCatalog();

	public:
		//addtion
		virtual	bool				InitCreate(void) ;
		virtual	bool				EndDestory(void) ;

		virtual	bool				CreateContexMenu(void);

	public:
		afx_msg int					OnCreate(LPCREATESTRUCT lpCreateStruct);
		afx_msg void				OnLButtonDown(UINT nFlags, CPoint point);
		afx_msg void				OnRButtonDown(UINT nFlags, CPoint point);

		afx_msg void				On3DObjMgrSetVisible();
		afx_msg void				On3DObjMgrSetHide();
		afx_msg void				On3DObjMgrRemove();
		afx_msg void				On3DObjMgrReCreateSceneTree();
		
	public:
		CString						GetSelObjName(void) { return m_strSelObjName;}
		bool						Update3DObjTree();    //锟斤拷锟斤拷锟斤拷维锟斤拷锟斤拷锟斤拷

	protected:
		DECLARE_MESSAGE_MAP()

	protected:
		CImageList					m_imgList;
		HTREEITEM					m_hRoot;             //锟斤拷锟节碉拷
		CString						m_strSelObjName;
	};
}

#if !defined(XCATALOG_EXPORTS)
#if     defined( _DEBUG)
#          pragma comment(lib,"ui_legacy_d.lib")
#       else
#          pragma comment(lib,"ui_legacy.lib")
#	    endif
#endif

#endif //_CATA_3DOBJXCATALOG_H

