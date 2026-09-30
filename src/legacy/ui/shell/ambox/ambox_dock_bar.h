// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _AMB_AMBOXMGR_H
#define _AMB_AMBOXMGR_H
#if defined(XAMBOX_EXPORTS)
#define XAMBOX_EXPORT __declspec(dllexport)
#else
#define XAMBOX_EXPORT __declspec(dllimport)
#endif

#include "legacy/core/macros/macros.h"
#include "legacy/plugin/runtime/auxmodule/module_manager.h"

#include <vector>

using namespace plugin;

namespace ui {

// Aux-module Outlook bar — Feature Pack CMFCOutlookBar (via CBCGPOutlookBar).
class XAMBOX_EXPORT SmtAMBoxMgrDocBar : public CBCGPOutlookBar {
 public:
  SmtAMBoxMgrDocBar();
  ~SmtAMBoxMgrDocBar() override;

  // Normalizes CP936 titles when process ACP is UTF-8, then adds an Outlook page.
  bool add_wnd(CWnd* pWnd, CString strTitle);

  CBCGPOutlookWnd* get_oner_wnd() {
    return DYNAMIC_DOWNCAST(CBCGPOutlookWnd, GetUnderlyingWindow());
  }

  bool UpdateAMBoxs(void);

 protected:
  bool CreateAMBox(SmtAuxModule* pAModule, int nID);

  DECLARE_MESSAGE_MAP()

 protected:
  std::vector<CWnd*> m_vWndPtrs;
};

}  // namespace ui

#if !defined(XAMBOX_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  //_AMB_AMBOXMGR_H
