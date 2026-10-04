// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/app/stdafx.h"

#include "legacy/app/views/helper/status_coord.h"

#include "legacy/app/shell/frame/main_frame.h"

namespace legacy_app {
namespace helper {

void set_status_map_xy(float x, float y) {
  CMainFrame* pMain = static_cast<CMainFrame*>(AfxGetApp()->m_pMainWnd);
  if (!pMain) {
    return;
  }
  CString strXY, strLB;
  strXY.Format(_T("x=%.2f,y=%.2f"), x, y);
  // UTF-8 source + /execution-charset:.936 — avoid corrupted legacy bytes.
  strLB.Format(_T("经=%.2f,纬=%.2f"), x, y);
  pMain->SetStatusBarString(2, strXY);
  pMain->SetStatusBarString(1, strLB);
}

void set_status_scene_xyz(float x, float y, float z) {
  CMainFrame* pMain = static_cast<CMainFrame*>(AfxGetApp()->m_pMainWnd);
  if (!pMain) {
    return;
  }
  CString strXYZ;
  strXYZ.Format(_T("x=%.4f,y=%.4f,z=%.4f"), x, y, z);
  pMain->SetStatusBarString(2, strXYZ);
}

}  // namespace helper
}  // namespace legacy_app
