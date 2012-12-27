/*
File:    sta_api.h

Desc:    SmartGis Í³¼ÆÍ¼»ù´¡API

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.8.22

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _STA_API_H
#define _STA_API_H

#include "smt_core.h"
#include "sta_chart.h"

using namespace Smt_StaDiagram;

long		SMT_EXPORT_API		SmtPlot(const vPoints &points,const char * szTitle,const char * szPanelTitle,
										const char * szXTitle,const char * szXUnit,
										const char * szYTitle,const char * szYUnit);

#if !defined(Export_SmtStaDiagram)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtStaDiagramD.lib")
#       else
#          pragma comment(lib,"SmtStaDiagram.lib")
#	    endif  
#endif

#endif //_STA_API_H
