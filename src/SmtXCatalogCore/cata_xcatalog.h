/*
File:    cata_xcatalog.h  

Desc:    SmtXCatalog,Smt Catalog ¼Ì³ÐÓÚCTreeCtrl

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _CATA_XCATALOG_H
#define _CATA_XCATALOG_H

#include "smt_core.h"
// SmtXCatalog
namespace Smt_XCatalog
{
	class SMT_EXPORT_CLASS SmtXCatalog : public CTreeCtrl
	{
		DECLARE_DYNAMIC(SmtXCatalog)

	public:
		SmtXCatalog();
		virtual ~SmtXCatalog();

	public:
		//addtion
		virtual	bool				InitCreate(void) ;
		virtual	bool				EndDestory(void) ;

		virtual	bool				CreateContexMenu(void) { return true;}

	protected:
		DECLARE_MESSAGE_MAP()

	public:
		afx_msg int					OnCreate(LPCREATESTRUCT lpCreateStruct);

	protected:
		HMENU						m_hContexMenu;
	};
}
#if !defined(Export_SmtXCatalogCore)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtXCatalogCoreD.lib")
#       else
#          pragma comment(lib,"SmtXCatalogCore.lib")
#	    endif
#endif

#endif //_CATA_XCATALOG_H

