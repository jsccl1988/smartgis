// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_PLUGIN_DEM_DEM_DLG_HELPERS_H_
#define LEGACY_PLUGIN_DEM_DEM_DLG_HELPERS_H_

#include "legacy/core/util/path.h"
#include "legacy/core/listener/listener_manager.h"
#include "legacy/plugin/product/dem/shell/resource.h"
#include "legacy/render/scene3d/scene/scene.h"
#include "legacy/ui/map/viewport/view_3d.h"

// Shared helpers for CDlgGridLoader / CDlgTinLoader (identical 3D bind + tex
// UI).
namespace dem_dlg {

inline int bind_sys_3d_view(ui::Smt3DXView*& view, LP3DRENDERDEVICE& device,
                            SmtScene*& scene) {
  base::SmtListenerMsg msg_param;
  msg_param.lParam = reinterpret_cast<LPARAM>(&view);
  smt_post_listener_msg(SMT_LISTENER_MSG_BROADCAST, SMT_MSG_GET_SYS_3DVIEW,
                        msg_param);

  if (!view || !view->GetSafeHwnd()) {
    return SMT_ERR_FAILURE;
  }

  device = view->GetRenderDevice();
  scene = view->GetScene();
  if (!device || !scene) {
    return SMT_ERR_FAILURE;
  }
  return SMT_ERR_NONE;
}

inline void disable_tex_controls(CWnd* dlg, bool* use_tex) {
  if (!dlg || !use_tex) {
    return;
  }
  CButton* ck = static_cast<CButton*>(dlg->GetDlgItem(IDC_CK_USETEX));
  CEdit* url = static_cast<CEdit*>(dlg->GetDlgItem(IDC_EDIT_TEX_URL));
  CButton* btn = static_cast<CButton*>(dlg->GetDlgItem(IDC_BTN_SELTEXFILE));
  if (ck && url && btn) {
    ck->SetCheck(0);
    url->EnableWindow(FALSE);
    btn->EnableWindow(FALSE);
    *use_tex = true;
  }
}

inline void apply_usetex_toggle(CWnd* dlg, bool* use_tex) {
  if (!dlg || !use_tex) {
    return;
  }
  CButton* ck = static_cast<CButton*>(dlg->GetDlgItem(IDC_CK_USETEX));
  CEdit* url = static_cast<CEdit*>(dlg->GetDlgItem(IDC_EDIT_TEX_URL));
  CButton* btn = static_cast<CButton*>(dlg->GetDlgItem(IDC_BTN_SELTEXFILE));
  if (ck && url && btn) {
    *use_tex = (ck->GetCheck() == 1);
    url->EnableWindow(*use_tex);
    btn->EnableWindow(*use_tex);
  }
}

inline bool pick_bmp_texture(CString* url, CString* dir, CString* name,
                             CString* ext) {
  if (!url || !dir || !name || !ext) {
    return false;
  }
  static char BASED_CODE sz_filter[] = "Data Files (*.bmp)|*.bmp";
  CFileDialog dlg(true, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                  sz_filter, NULL);
  if (dlg.DoModal() == IDCANCEL) {
    return false;
  }
  char path[_MAX_PATH];
  char file_name[_MAX_PATH];
  char title[_MAX_PATH];
  char file_ext[_MAX_PATH];
  split_file_name(dlg.GetPathName(), path, file_name, title, file_ext);
  *url = dlg.GetPathName();
  *dir = path;
  *name = title;
  *ext = file_ext;
  return true;
}

}  // namespace dem_dlg

#endif  // LEGACY_PLUGIN_DEM_DEM_DLG_HELPERS_H_
