// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_SHELL_AMBOX_OUTLOOK_BAR_H_
#define LEGACY_UI_SHELL_AMBOX_OUTLOOK_BAR_H_

#if defined(XAMBOX_EXPORTS)
#define XAMBOX_EXPORT __declspec(dllexport)
#else
#define XAMBOX_EXPORT __declspec(dllimport)
#endif

#include "legacy/core/macros/macros.h"
#include "legacy/plugin/runtime/auxmodule/module_manager.h"
#include "legacy/ui/shell/ambox/title.h"

#include <vector>

using namespace plugin;

namespace ui {

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

  // Re-apply ASCII Outlook face captions after Feature Pack LoadState may
  // restore corrupted registry BarName values (e.g. "DEM????").
  bool refresh_outlook_captions(void);

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

#endif  // LEGACY_UI_SHELL_AMBOX_OUTLOOK_BAR_H_
