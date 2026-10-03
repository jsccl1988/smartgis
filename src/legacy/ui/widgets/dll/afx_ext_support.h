// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_WIDGETS_AFX_EXT_SUPPORT_H_
#define LEGACY_UI_WIDGETS_AFX_EXT_SUPPORT_H_

// Include from the sole AFX extension-DLL DllMain translation unit
// (dll/dll_main.cpp). Supplies mfcs140*.lib anchors without linking
// dllmodul (which would duplicate DllMain).

extern "C" {
int __afxForceEXCLUDE;
int __afxForceUSRDLL;
int __afxForceSTDAFX;
}

AFX_MODULE_STATE* AFXAPI AfxGetStaticModuleState() {
  return AfxGetModuleState();
}

#endif  // LEGACY_UI_WIDGETS_AFX_EXT_SUPPORT_H_
