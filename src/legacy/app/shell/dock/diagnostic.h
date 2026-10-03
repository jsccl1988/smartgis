// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_APP_SHELL_DOCK_DIAGNOSTIC_H_
#define LEGACY_APP_SHELL_DOCK_DIAGNOSTIC_H_

#pragma once

#include "legacy/app/shell/dock/console.h"
#include "legacy/app/shell/dock/render_trace.h"

// Bottom Diagnostic Tools strip (Views DiagnosticToolsPanel parity):
// Console | RenderTrace pages as plain CWnd children. Uses lightweight
// header buttons instead of CMFCTabCtrl — Feature Pack tabs + member CWnd
// panes crash on tab / button clicks with the dock manager.
class DiagnosticToolsDockBar : public CBCGPDockingControlBar {
 public:
  DiagnosticToolsDockBar();
  ~DiagnosticToolsDockBar() override;

  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);
  afx_msg void OnBnConsole();
  afx_msg void OnBnTrace();

  DECLARE_MESSAGE_MAP()

 private:
  void layout_children(int cx, int cy);
  void show_page(int page);
  void fill_opaque_client(CDC* pDC);

  CButton btn_console_;
  CButton btn_trace_;
  DebugConsolePane console_;
  RenderTracePane trace_;
  CFont ui_font_;
  int active_page_ = 0;
};

#endif  // LEGACY_APP_SHELL_DOCK_DIAGNOSTIC_H_
