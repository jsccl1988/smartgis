// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_DIALOGS_DIALOGS_API_H_
#define LEGACY_UI_DIALOGS_DIALOGS_API_H_

#if defined(GUI_EXPORTS)
#define GUI_EXPORT __declspec(dllexport)
#else
#define GUI_EXPORT __declspec(dllimport)
#endif

#include "legacy/gis/feature/model_aliases.h"
#include "legacy/core/types/types.h"
#include "legacy/core/macros/macros.h"
#include "ogrsf_frmts.h"

using namespace base;
using namespace gis;

#ifdef _AFXEXT  // support MFC
CWnd GUI_EXPORT* SmtGetActiveWnd(void);
#endif  // _AFXEXT

// Export facade for leftover MFC modals (callers include this header only).
long GUI_EXPORT SmtInputTextDlg(string& strText);

// Retired: edit-param modal removed; inspect docks own preferences. ABI stub.
long GUI_EXPORT SmtEditParamSettingDlg(void);

long GUI_EXPORT SmtSelectOneDlg(uint& unID, vector<uint>& vIDs);

long GUI_EXPORT SmtShow2DFeatureInfoDlg(FeatureAdapter* pSmtFea = NULL);

// Edit OGR layer field schema (replaces SmtAttribute-based leftover).
long GUI_EXPORT SmtAttStructEditDlg(OGRLayer* layer, int nFixField = 0);

#if !defined(GUI_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  // LEGACY_UI_DIALOGS_DIALOGS_API_H_
