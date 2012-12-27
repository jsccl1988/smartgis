/*
File:    cata_xdscatalog.h  

Desc:    SmtXCatalog,Smt DataSource Catalog 继承于SmtXCatalog

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _CATA_XDSCATALOG_H
#define _CATA_XDSCATALOG_H

#include "cata_xcatalog.h"

#include "sde_datasourcemgr.h"

using namespace Smt_SDEDevMgr;
// SmtXCatalog

namespace Smt_XCatalog
{
	class SMT_EXPORT_CLASS SmtDSXCatalog : public SmtXCatalog
	{
		DECLARE_DYNAMIC(SmtDSXCatalog)

	public:
		SmtDSXCatalog();
		virtual ~SmtDSXCatalog();

	public:
		//addtion
		virtual	bool				InitCreate(void) ;
		virtual	bool				EndDestory(void) ;

		virtual	bool				CreateContexMenu(void);

		CString						GetSelDSName() { return m_strSelDSName;}
		CString						GetDSSelLayerName() { return m_strSelDSLayerName;}
		void						UpdateCatalogTree(void);

	public:
		virtual BOOL				Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
		afx_msg int					OnCreate(LPCREATESTRUCT lpCreateStruct);
		afx_msg void				OnRButtonDown(UINT nFlags, CPoint point);

		afx_msg void				OnDsLayerCreate();
		afx_msg void				OnDsLayerDelete();
		afx_msg void				OnDsLayerProperty();
		afx_msg void				OnDsLayerLoadShp();
		afx_msg void				OnDsLayerLoadImage();
		afx_msg void				OnDsProperty();

		afx_msg void				OnDsDelete();
		afx_msg void				OnSvrDsCreate();
		afx_msg void				OnSvrDsAppend();
		afx_msg void				OnSvrDsSetActive();

		DECLARE_MESSAGE_MAP()

	private:
		void						AppendDSNode(SmtDataSource *pDS);

	private:
		CImageList					m_imgList;
		HTREEITEM					m_hRoot;				//根节点
		HTREEITEM					m_hDSCatalog;			//数据源节点

		CString						m_strSelDSName;			//选择数据源
		CString						m_strSelDSLayerName;	//选择数据源图层
	};
}
#if !defined(Export_SmtXCatalogCore)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtXCatalogCoreD.lib")
#       else
#          pragma comment(lib,"SmtXCatalogCore.lib")
#	    endif
#endif

#endif //_CATA_XDSCATALOG_H

