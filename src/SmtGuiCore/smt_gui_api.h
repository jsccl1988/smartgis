/*
File:    smt_gui_api.h

Desc:    GUI API

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_GUI_API_H
#define _SMT_GUI_API_H

#include "smt_core.h"
#include "gis_feature.h"

using namespace Smt_Core;
using namespace Smt_GIS;

#ifdef _AFXEXT			//support MFC
//////////////////////////////////////////////////////////////////////////
//mfc
CWnd	SMT_EXPORT_API	*SmtGetActiveWnd(void);
#endif // _AFXEXT


//////////////////////////////////////////////////////////////////////////
long	SMT_EXPORT_API	SmtInputTextDlg(string &strText);

long	SMT_EXPORT_API	SmtEditParamSettingDlg(void);

long	SMT_EXPORT_API	SmtSelectOneDlg(uint &unID,vector<uint> &vIDs);

long	SMT_EXPORT_API	SmtShow2DFeatureInfoDlg(SmtFeature *pSmtFea = NULL);

long	SMT_EXPORT_API	SmtAttStructEditDlg(SmtAttribute *&pSmtAtt,int nFixField = 0);

#if !defined(Export_SmtGuiCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtGuiCoreD.lib")
#       else
#          pragma comment(lib,"SmtGuiCore.lib")
#	    endif  
#endif

#endif //_SMT_GUI_API_H