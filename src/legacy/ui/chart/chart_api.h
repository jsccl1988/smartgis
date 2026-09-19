/*
File:    sta_api.h

Desc:    SmartGis 统锟斤拷图锟斤拷锟斤拷API

Version: Version 1.0

Writter:  锟铰达拷锟斤拷

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
#include "legacy/ui/chart/chart.h"

using namespace ui;

long STAT_CHART_EXPORT SmtPlot(const vPoints &points, const char *szTitle,
                               const char *szPanelTitle, const char *szXTitle,
                               const char *szXUnit, const char *szYTitle,
                               const char *szYUnit);

#if !defined(STAT_CHART_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  //_STA_API_H
