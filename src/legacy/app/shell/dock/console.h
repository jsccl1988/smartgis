// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_APP_SHELL_DOCK_CONSOLE_H_
#define LEGACY_APP_SHELL_DOCK_CONSOLE_H_

#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "legacy/core/macros/macros.h"

// Plain CWnd Console pane (Views DebugConsolePanel parity). Must NOT derive
// from CBCGPDockingControlBar — nested docking bars inside AMBox crash.
class DebugConsolePane : public CWnd {
 public:
  DebugConsolePane();
  ~DebugConsolePane() override;

  BOOL Create(CWnd* parent, UINT id);

  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnTimer(UINT_PTR nIDEvent);
  afx_msg void OnDestroy();
  afx_msg void OnBnClear();
  afx_msg void OnBnSubmit();

  DECLARE_MESSAGE_MAP()

 private:
  void layout_children(int cx, int cy);
  void append_line(const CString& line);
  void drain_pending();
  void ensure_log_subscription();
  void drop_log_subscription();
  void seed_from_tail();

  CButton btn_clear_;
  CButton btn_submit_;
  CListBox log_list_;
  CEdit input_;

  std::mutex pending_mu_;
  std::vector<std::string> pending_;
  std::uint64_t log_sub_id_ = 0;
};

#endif  // LEGACY_APP_SHELL_DOCK_CONSOLE_H_
