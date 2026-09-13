/*
File:    cata_xcatalog.h  

Desc:    SmtXCatalog,Smt Catalog �̳���CTreeCtrl

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _CATA_XCATALOG_H
#define _CATA_XCATALOG_H
#if defined(XCATALOG_EXPORTS)
#define XCATALOG_EXPORT __declspec(dllexport)
#else
#define XCATALOG_EXPORT __declspec(dllimport)
#endif


#include "base/core/core.h"
// SmtXCatalog
namespace ui
{
	class XCATALOG_EXPORT SmtXCatalog : public CTreeCtrl
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
#if !defined(XCATALOG_EXPORTS)
#if     defined( _DEBUG)
#          pragma comment(lib,"xcatalogD.lib")
#       else
#          pragma comment(lib,"xcatalog.lib")
#	    endif
#endif

#endif //_CATA_XCATALOG_H

