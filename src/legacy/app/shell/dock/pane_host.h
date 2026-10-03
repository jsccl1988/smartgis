// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_APP_SHELL_DOCK_PANE_HOST_H_
#define LEGACY_APP_SHELL_DOCK_PANE_HOST_H_

#pragma once

// Shared CWnd chrome for leftover Diagnostic dock panes (Console / RenderTrace).
// Plain child windows — must NOT derive from CBCGPDockingControlBar (nested
// docking bars inside AMBox / Feature Pack crash). Compose from diagnostic.*
// page host. Requires CWnd / CListBox / CDC from the including TU's PCH.

namespace legacy_app {
namespace detail {

inline constexpr int k_dock_pane_max_log_lines = 500;

// Child CWnd under a docking strip page (DiagnosticToolsDockBar).
inline BOOL create_dock_pane_child(CWnd* self, CWnd* parent, UINT id) {
  if (self == nullptr || parent == nullptr) {
    return FALSE;
  }
  return self->CreateEx(
      0, AfxRegisterWndClass(0), _T(""),
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
      CRect(0, 0, 0, 0), parent, id);
}

inline BOOL erase_dock_pane_bkgnd(CWnd* host, CDC* pDC, COLORREF color) {
  if (host == nullptr || pDC == nullptr) {
    return TRUE;
  }
  CRect rc;
  host->GetClientRect(&rc);
  pDC->FillSolidRect(&rc, color);
  return TRUE;
}

inline void append_list_line(CListBox& list, const CString& line,
                             int max_lines = k_dock_pane_max_log_lines) {
  if (!::IsWindow(list.m_hWnd)) {
    return;
  }
  const int idx = list.AddString(line);
  while (list.GetCount() > max_lines) {
    list.DeleteString(0);
  }
  list.SetCurSel(idx);
}

}  // namespace detail
}  // namespace legacy_app

#endif  // LEGACY_APP_SHELL_DOCK_PANE_HOST_H_
