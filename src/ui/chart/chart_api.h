/*
File:    sta_api.h

Desc:    SmartGis ͳ��ͼ����API

Version: Version 1.0

Writter:  �´���

Date:    2012.8.22

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _STA_API_H
#define _STA_API_H
#if defined(STAT_CHART_EXPORTS)
#define STAT_CHART_EXPORT __declspec(dllexport)
#else
#define STAT_CHART_EXPORT __declspec(dllimport)
#endif


#include "base/core/core.h"
#include "ui/chart/chart.h"

using namespace ui;

long		STAT_CHART_EXPORT		SmtPlot(const vPoints &points,const char * szTitle,const char * szPanelTitle,
										const char * szXTitle,const char * szXUnit,
										const char * szYTitle,const char * szYUnit);

#if !defined(STAT_CHART_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"stat_chartD.lib")
#       else
#          pragma comment(lib,"stat_chart.lib")
#	    endif  
#endif

#endif //_STA_API_H
