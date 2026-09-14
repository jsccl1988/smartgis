/*
File:    cata_xdscatalog.h  

Desc:    SmtXCatalog,Smt DataSource Catalog 锟教筹拷锟斤拷SmtXCatalog

Version: Version 1.0

Writter:  锟铰达拷锟斤拷

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _CATA_XDSCATALOG_H
#define _CATA_XDSCATALOG_H
#if defined(XCATALOG_EXPORTS)
#define XCATALOG_EXPORT __declspec(dllexport)
#else
#define XCATALOG_EXPORT __declspec(dllimport)
#endif


#include "legacy_ui/xcatalog/xcatalog.h"

#include "sdb/datasource/mgr/datasourcemgr.h"

using namespace sdb;
// SmtXCatalog

namespace ui
{
	class XCATALOG_EXPORT SmtDSXCatalog : public SmtXCatalog
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
		void						AppendDSNode(SmtDataSource pDS);

	private:
		CImageList					m_imgList;
		HTREEITEM					m_hRoot;				//锟斤拷锟节碉拷
		HTREEITEM					m_hDSCatalog;			//锟斤拷锟斤拷源锟节碉拷

		CString						m_strSelDSName;			//选锟斤拷锟斤拷锟斤拷源
		CString						m_strSelDSLayerName;	//选锟斤拷锟斤拷锟斤拷源图锟斤拷
	};
}
#if !defined(XCATALOG_EXPORTS)
#if     defined( _DEBUG)
#          pragma comment(lib,"ui_legacy_d.lib")
#       else
#          pragma comment(lib,"ui_legacy.lib")
#	    endif
#endif

#endif //_CATA_XDSCATALOG_H

