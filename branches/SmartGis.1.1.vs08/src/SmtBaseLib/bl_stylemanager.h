/*
File:    bl_stylemanager.h

Desc:    SmtStyleManager

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _BL_STYLEMANAGER_H
#define _BL_STYLEMANAGER_H

#include "bl_style.h"

namespace Smt_Base
{
	class SMT_EXPORT_CLASS SmtStyleManager
	{
	private:
		SmtStyleManager(void);

	public:
		virtual ~SmtStyleManager(void);

		void					SetDefaultStyle(const char * defName,
												SmtPenDesc        &penDesc,
												SmtBrushDesc      &brushDesc,
												SmtAnnotationDesc &annoDesc,
												SmtSymbolDesc     &symbolDesc);
		SmtStyle*				GetDefaultStyle(void);

	public:
		SmtStyle *				CreateStyle(const char *   defName,
											SmtPenDesc        &penDesc,
											SmtBrushDesc      &brushDesc,
											SmtAnnotationDesc &annoDesc,
											SmtSymbolDesc     &symbolDesc);

		void					DestroyStyle(const char * name);

		void					DestroyStyle(SmtStyle *pStyle);

		void					DestroyAllStyle();

		SmtStyle*				GetStyle(const char * name);

	public:
		static SmtStyleManager* GetSingletonPtr(void);

		static void				DestoryInstance(void);

	private:
		StylePtrList			m_StylePtrList;
		SmtStyle *				m_pDefaultStyle;

		static SmtStyleManager* m_pSingleton;
	};
}

#if !defined(Export_SmtBaseLib)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtBaseLibD.lib")
#       else
#          pragma comment(lib,"SmtBaseLib.lib")
#	    endif  
#endif

#endif //_BL_STYLEMANAGER_H