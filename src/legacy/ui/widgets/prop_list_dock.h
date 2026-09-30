// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_COMMON_PROP_LIST_DOCK_H_
#define LEGACY_UI_COMMON_PROP_LIST_DOCK_H_

// Requires CBCGPPropList / CWnd from the including TU's stdafx / BCGP headers.
// Do not pull mfc_ext_support.h here (avoids duplicate AFX module-state symbols).

namespace legacy_ui {

inline bool create_vs_prop_list(CBCGPPropList& list,
                                CWnd* parent,
                                UINT id = 1) {
  if (!parent) {
    return false;
  }
  CRect rect_dummy;
  rect_dummy.SetRectEmpty();
  if (!list.Create(WS_VISIBLE | WS_CHILD, rect_dummy, parent, id)) {
    return false;
  }
  list.EnableHeaderCtrl(FALSE);
  list.EnableDescriptionArea();
  list.SetVSDotNetLook();
  return true;
}

inline void size_prop_list_inset(CBCGPPropList& list,
                                 int cx,
                                 int cy,
                                 int border = 1) {
  list.SetWindowPos(nullptr, border, border, cx - 2 * border, cy - 2 * border,
                    SWP_NOACTIVATE | SWP_NOZORDER);
}

}  // namespace legacy_ui

#endif  // LEGACY_UI_COMMON_PROP_LIST_DOCK_H_
