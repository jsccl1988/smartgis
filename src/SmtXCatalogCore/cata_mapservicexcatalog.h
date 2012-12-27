/*
File:    cata_mapservicexcatalog.h  

Desc:    SmtMapServiceXCatalog,Smt MapService Catalog 继承于SmtXCatalog

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _CATA_MAPSERVICECATALOG_H
#define _CATA_MAPSERVICECATALOG_H

#include "cata_xcatalog.h"

#include "cata_mapservicemgr.h"
// SmtXCatalog

namespace Smt_XCatalog
{
	class SMT_EXPORT_CLASS SmtMapServiceXCatalog : public SmtXCatalog
	{
		DECLARE_DYNAMIC(SmtMapServiceXCatalog)

	public:
		SmtMapServiceXCatalog();
		virtual ~SmtMapServiceXCatalog();

	public:
		//addtion
		virtual	bool				InitCreate(void) ;
		virtual	bool				EndDestory(void) ;

		virtual	bool				CreateContexMenu(void);

		CString						GetSelMServiceName() { return m_strSelMServiceName;}
		void						UpdateMServiceCatalogTree(void);
		void						AppendMServiceNode(SmtMapService *pMService);

	public:
		virtual BOOL				Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
		afx_msg int					OnCreate(LPCREATESTRUCT lpCreateStruct);
		afx_msg void				OnRButtonDown(UINT nFlags, CPoint point);

		afx_msg void				OnLButtonDown(UINT nFlags, CPoint point);
		
		DECLARE_MESSAGE_MAP()

	public:
		afx_msg void				OnMServiceAdd();
		afx_msg void				OnMServiceDelete();

	private:
		CImageList					m_imgList;
		HTREEITEM					m_hRoot;				//根节点
		
		CString						m_strSelMServiceName;		//选择数据源
	};
}
#if !defined(Export_SmtXCatalogCore)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtXCatalogCoreD.lib")
#       else
#          pragma comment(lib,"SmtXCatalogCore.lib")
#	    endif
#endif

#endif //_CATA_MAPSERVICECATALOG_H

