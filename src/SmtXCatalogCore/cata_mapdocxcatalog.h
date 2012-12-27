/*
File:    cata_mapdocxcatalog.h  

Desc:    SmtMapDocXCatalog,Smt MapDoc Catalog 继承于SmtXCatalog

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _CATA_MAPDOCXCATALOG_H
#define _CATA_MAPDOCXCATALOG_H

#include "cata_xcatalog.h"

// SmtXCatalog

namespace Smt_XCatalog
{
	class SMT_EXPORT_CLASS SmtMapDocXCatalog : public SmtXCatalog
	{
		DECLARE_DYNAMIC(SmtMapDocXCatalog)

	public:
		SmtMapDocXCatalog();
		virtual ~SmtMapDocXCatalog();

	public:
		//addtion
		virtual	bool				InitCreate(void) ;
		virtual	bool				EndDestory(void) ;

		virtual	bool				CreateContexMenu(void);

	public:
		afx_msg int					OnCreate(LPCREATESTRUCT lpCreateStruct);
		afx_msg void				OnLButtonDown(UINT nFlags, CPoint point);
		afx_msg void				OnRButtonDown(UINT nFlags, CPoint point);

		afx_msg void				OnLayerMgrAppend();
		afx_msg void				OnLayerMgrRemove();
		afx_msg void				OnLayerMgrActive();
		afx_msg void				OnLayerMgrProperty();
		afx_msg void				OnLayerMgrReCalcMBR();
		afx_msg void				OnLayerMgrAttstruct();

		afx_msg void				OnMapMgrNew();
		afx_msg void				OnMapMgrOpen();
		afx_msg void				OnMapMgrClose();
		afx_msg void				OnMapMgrSave();
		afx_msg void				OnMapMgrSaveas();

	public:
		CString						GetMapSelLayerName(void) { return m_strSelLayerName;}
		bool						UpdateMapTree();    //更新图层树

	protected:
		DECLARE_MESSAGE_MAP()

	protected:
		CImageList					m_imgList;
		HTREEITEM					m_hRoot;             //根节点
		HTREEITEM					m_hMap;              //地图集节点
		CString						m_strSelLayerName;
	};
}
#if !defined(Export_SmtXCatalogCore)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtXCatalogCoreD.lib")
#       else
#          pragma comment(lib,"SmtXCatalogCore.lib")
#	    endif
#endif

#endif //_CATA_MAPDOCXCATALOG_H

