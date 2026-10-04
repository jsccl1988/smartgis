// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_APP_SHELL_RENDER_TRACE_H_
#define LEGACY_APP_SHELL_RENDER_TRACE_H_

#pragma once

#include <chrono>

// Plain CWnd RenderTrace pane (Views RenderTracePanel parity). Not a docking
// bar — hosted inside DiagnosticToolsDockBar tabs.
class RenderTracePane : public CWnd {
 public:
  RenderTracePane();
  ~RenderTracePane() override;

  BOOL Create(CWnd* parent, UINT id);

  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnTimer(UINT_PTR nIDEvent);
  afx_msg void OnDestroy();
  afx_msg void OnBnRecord();
  afx_msg void OnBnStop();
  afx_msg void OnBnClear();
  afx_msg void OnBnRefresh();

  DECLARE_MESSAGE_MAP()

 private:
  void layout_children(int cx, int cy);
  void append_new_frame_lines();
  void append_line(const CString& line);

  CButton btn_record_;
  CButton btn_stop_;
  CButton btn_clear_;
  CButton btn_refresh_;
  CListBox log_list_;
  std::chrono::steady_clock::time_point last_frame_end_{};
  bool armed_ = false;
};

#endif  // LEGACY_APP_SHELL_RENDER_TRACE_H_
