// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_MFC_EXT_SUPPORT_H
#define SMT_MFC_EXT_SUPPORT_H

// Include from the single DllMain translation unit of each AFX
// extension DLL. Supplies mfcs140*.lib anchors without linking
// dllmodul (which would duplicate DllMain).

extern "C" {
int __afxForceEXCLUDE;
int __afxForceUSRDLL;
int __afxForceSTDAFX;
}

AFX_MODULE_STATE* AFXAPI AfxGetStaticModuleState() {
  return AfxGetModuleState();
}

#endif
