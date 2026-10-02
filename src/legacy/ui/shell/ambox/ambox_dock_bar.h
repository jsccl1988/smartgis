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

// Decode leftover CP936 or UTF-8 AM titles for TreeCtrl captions.
XAMBOX_EXPORT CString ambox_title_for_display(const char* name);

// ASCII-only Outlook page caption (Outlook faces paint CJK as '?').
XAMBOX_EXPORT CString ambox_outlook_caption(const char* name);

// Aux-module Outlook bar — Feature Pack CMFCOutlookBar (via CBCGPOutlookBar).
// Do NOT nest CBCGPDockingControlBar pages in a Tab host (Feature Pack crash).
class XAMBOX_EXPORT SmtAMBoxMgrDocBar : public CBCGPOutlookBar {
 public:
  SmtAMBoxMgrDocBar();
  ~SmtAMBoxMgrDocBar() override;

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
