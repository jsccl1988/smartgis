/*
File:    cata_mapdocxcatalog.h  

Desc:    SmtMapDocXCatalog,Smt MapDoc Catalog 锟教筹拷锟斤拷SmtXCatalog

Version: Version 1.0

Writter:  锟铰达拷锟斤拷

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _CATA_MAPDOCXCATALOG_H
#define _CATA_MAPDOCXCATALOG_H
#if defined(XCATALOG_EXPORTS)
#define XCATALOG_EXPORT __declspec(dllexport)
#else
#define XCATALOG_EXPORT __declspec(dllimport)
#endif


#include "ui/xcatalog/xcatalog.h"

// SmtXCatalog

namespace ui
{
	class XCATALOG_EXPORT SmtMapDocXCatalog : public SmtXCatalog
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
		bool						UpdateMapTree();    //锟斤拷锟斤拷图锟斤拷锟斤拷

	protected:
		DECLARE_MESSAGE_MAP()

	protected:
		CImageList					m_imgList;
		HTREEITEM					m_hRoot;             //锟斤拷锟节碉拷
		HTREEITEM					m_hMap;              //锟斤拷图锟斤拷锟节碉拷
		CString						m_strSelLayerName;
	};
}
#if !defined(XCATALOG_EXPORTS)
#if     defined( _DEBUG)
#          pragma comment(lib,"ui_legacy_d.lib")
#       else
#          pragma comment(lib,"ui_legacy.lib")
#	    endif
#endif

#endif //_CATA_MAPDOCXCATALOG_H

