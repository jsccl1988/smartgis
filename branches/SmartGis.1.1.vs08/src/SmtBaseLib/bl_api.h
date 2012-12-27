/*
File:    bl_api.h

Desc:    SmtBaseLib»ù´¡API

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _BL_API_H
#define _BL_API_H

#include "smt_core.h"
#include "smt_bas_struct.h"
#include "bl_envelope.h"
#include "bl_bas_struct.h"
#include "bl_style.h"

//////////////////////////////////////////////////////////////////////////
void		SMT_EXPORT_API		ViewportToRect(Smt_Core::lRect &lrect,const Smt_Base::Viewport &viewport);
void		SMT_EXPORT_API		WindowportToRect(Smt_Core::fRect &frect,const Smt_Base::Windowport &windowport);

void		SMT_EXPORT_API		EnvelopeToRect(Smt_Core::fRect &frect,const Smt_Base::Envelope &env);
void		SMT_EXPORT_API		RectToEnvelope(Smt_Base::Envelope &env,const Smt_Core::fRect &frect);


void		SMT_EXPORT_API		AnnoDescToLogFont(LOGFONT &lf,const Smt_Base::SmtAnnotationDesc &annoDesc);
void		SMT_EXPORT_API		LogFontToAnnoDesc(Smt_Base::SmtAnnotationDesc &annoDesc,const LOGFONT &lf);

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_API_H