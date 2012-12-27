/*
File:    cata_3dobjxcatalog.h  

Desc:    Smt3DObjXCatalog,Smt 3D Object Catalog 继承于SmtXCatalog

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _CATA_3DOBJXCATALOG_H
#define _CATA_3DOBJXCATALOG_H

#include "cata_xcatalog.h"
// SmtXCatalog

namespace Smt_XCatalog
{
	class SMT_EXPORT_CLASS Smt3DObjXCatalog : public SmtXCatalog
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
		bool						Update3DObjTree();    //更新三维对象树

	protected:
		DECLARE_MESSAGE_MAP()

	protected:
		CImageList					m_imgList;
		HTREEITEM					m_hRoot;             //根节点
		CString						m_strSelObjName;
	};
}

#if !defined(Export_SmtXCatalogCore)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtXCatalogCoreD.lib")
#       else
#          pragma comment(lib,"SmtXCatalogCore.lib")
#	    endif
#endif

#endif //_CATA_3DOBJXCATALOG_H

