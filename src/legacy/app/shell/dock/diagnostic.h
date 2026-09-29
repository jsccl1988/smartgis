// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_APP_SHELL_DOCK_DIAGNOSTIC_H_
#define LEGACY_APP_SHELL_DOCK_DIAGNOSTIC_H_

#pragma once

#include "legacy/app/shell/dock/console.h"
#include "legacy/app/shell/dock/render_trace.h"
#include "legacy/core/macros/macros.h"

// Bottom Diagnostic Tools strip (Views DiagnosticToolsPanel parity):
// tabs Console | RenderTrace. One CBCGPDockingControlBar on the main frame
// bottom — never nested inside AMBox.
class DiagnosticToolsDockBar : public CBCGPDockingControlBar {
 public:
  DiagnosticToolsDockBar();
  ~DiagnosticToolsDockBar() override;

  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnSize(UINT nType, int cx, int cy);

  DECLARE_MESSAGE_MAP()

 private:
  void layout_children(int cx, int cy);

  CBCGPTabWnd tabs_;
  DebugConsolePane console_;
  RenderTracePane trace_;
};

#endif  // LEGACY_APP_SHELL_DOCK_DIAGNOSTIC_H_
