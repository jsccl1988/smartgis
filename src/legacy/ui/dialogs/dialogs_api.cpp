// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy/ui/dialogs/dialogs_api.h"

#include "legacy/ui/dialogs/gis/att_struct_dialog.h"
#include "legacy/ui/dialogs/gis/feature_info_dialog.h"
#include "legacy/ui/dialogs/toolkit/input_text_dialog.h"
#include "legacy/ui/dialogs/toolkit/select_one_dialog.h"

CWnd* SmtGetActiveWnd(void) { return CWnd::FromHandle(::GetActiveWindow()); }

long SmtInputTextDlg(string& strText) {
  AFX_MANAGE_STATE(AfxGetStaticModuleState());

  CDlgInputText dlg;
  if (dlg.DoModal() == IDOK) {
    strText = (LPCTSTR)dlg.m_strText;
  }
  return SMT_ERR_NONE;
}

long SmtEditParamSettingDlg(void) {
  // Preferences live on inspect docks (SysConfig / EditConfig). No modal.
  return SMT_ERR_NONE;
}

long SmtSelectOneDlg(uint& unID, vector<uint>& vIDs) {
  if (vIDs.size() < 2) {
    return SMT_ERR_INVALID_PARAM;
  }

  AFX_MANAGE_STATE(AfxGetStaticModuleState());

  CDlgSelectOne dlg(SmtGetActiveWnd());
  dlg.set_id_list(vIDs);
  if (dlg.DoModal() == IDOK) {
    unID = dlg.selected_id();
  }
  return SMT_ERR_NONE;
}

long SmtShow2DFeatureInfoDlg(SmtFeature* pSmtFea) {
  if (NULL == pSmtFea) {
    return SMT_ERR_INVALID_PARAM;
  }

  AFX_MANAGE_STATE(AfxGetStaticModuleState());

  CDlg2DFeatureInfo dlg(SmtGetActiveWnd());
  dlg.set_feature(pSmtFea);
  if (dlg.DoModal() == IDOK) {
    ;
  }

  return SMT_ERR_NONE;
}

long GUI_EXPORT SmtAttStructEditDlg(OGRLayer* layer, int nFixField) {
  if (NULL == layer) {
    return SMT_ERR_INVALID_PARAM;
  }

  AFX_MANAGE_STATE(AfxGetStaticModuleState());

  CDlgAttStructSet dlg(SmtGetActiveWnd());
  dlg.set_ogr_layer(layer, nFixField);
  if (dlg.DoModal() == IDOK) {
    dlg.apply_to_ogr_layer();
  }

  return SMT_ERR_NONE;
}
