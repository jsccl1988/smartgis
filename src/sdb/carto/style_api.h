/*
File:    bl_api.h

Desc:    SmtBaseLib����API

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _BL_API_H
#define _BL_API_H

#include "base/core/core.h"
#include "base/core/bas_struct.h"
#include "sdb/carto/envelope.h"
#include "sdb/carto/style_bas_struct.h"
#include "sdb/carto/style.h"

//////////////////////////////////////////////////////////////////////////
void		STYLE_EXPORT		viewport_to_rect(base::lRect &lrect,const base::Viewport &viewport);
void		STYLE_EXPORT		windowport_to_rect(base::fRect &frect,const base::Windowport &windowport);

void		STYLE_EXPORT		envelope_to_rect(base::fRect &frect,const base::Envelope &env);
void		STYLE_EXPORT		rect_to_envelope(base::Envelope &env,const base::fRect &frect);


void		STYLE_EXPORT		anno_desc_to_log_font(LOGFONT &lf,const base::SmtAnnotationDesc &annoDesc);
void		STYLE_EXPORT		log_font_to_anno_desc(base::SmtAnnotationDesc &annoDesc,const LOGFONT &lf);

#if !defined(STYLE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"platform_d.lib")
#       else
#          pragma comment(lib,"platform.lib")
#	    endif  
#endif

#endif //_SMT_API_H