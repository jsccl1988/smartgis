/*
File:    smt_gui_api.h

Desc:    GUI API

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_GUI_API_H
#define _SMT_GUI_API_H
#if defined(GUI_EXPORTS)
#define GUI_EXPORT __declspec(dllexport)
#else
#define GUI_EXPORT __declspec(dllimport)
#endif


#include "base/core/core.h"
#include "sdb/feature/feature.h"

using namespace base;
using namespace sdb;

#ifdef _AFXEXT			//support MFC
//////////////////////////////////////////////////////////////////////////
//mfc
CWnd	GUI_EXPORT	*SmtGetActiveWnd(void);
#endif // _AFXEXT


//////////////////////////////////////////////////////////////////////////
long	GUI_EXPORT	SmtInputTextDlg(string &strText);

long	GUI_EXPORT	SmtEditParamSettingDlg(void);

long	GUI_EXPORT	SmtSelectOneDlg(uint &unID,vector<uint> &vIDs);

long	GUI_EXPORT	SmtShow2DFeatureInfoDlg(SmtFeature *pSmtFea = NULL);

long	GUI_EXPORT	SmtAttStructEditDlg(SmtAttribute *&pSmtAtt,int nFixField = 0);

#if !defined(GUI_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"guiD.lib")
#       else
#          pragma comment(lib,"gui.lib")
#	    endif  
#endif

#endif //_SMT_GUI_API_H