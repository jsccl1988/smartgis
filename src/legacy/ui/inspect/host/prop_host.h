// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_INSPECT_HOST_PROP_HOST_H_
#define LEGACY_UI_INSPECT_HOST_PROP_HOST_H_

// Shared CWnd chrome for leftover inspect prop-list pages (AMBox Outlook).
// Compose with legacy_ui::create_vs_prop_list / size_prop_list_inset from
// widgets/prop. Requires CBCGPPropList / CWnd from the including TU's PCH.

#include "legacy/ui/widgets/prop/prop_list.h"

namespace legacy_ui {

inline constexpr int k_prop_host_border = 1;

// Plain child CWnd — must NOT derive from CBCGPDockingControlBar (nested
// docking bars inside Outlook crash on close).
inline BOOL create_prop_host_child(CWnd* self, CWnd* parent, UINT id) {
  if (self == nullptr || parent == nullptr) {
    return FALSE;
  }
  return self->CreateEx(
      0, AfxRegisterWndClass(0), _T(""),
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
      CRect(0, 0, 0, 0), parent, id);
}

inline void paint_prop_host_border(CWnd* host,
                                   CBCGPPropList& list,
                                   int border = k_prop_host_border) {
  if (host == nullptr) {
    return;
  }
  CPaintDC dc(host);
  CRect rect;
  list.GetWindowRect(rect);
  host->ScreenToClient(rect);
  rect.InflateRect(border, border);
  dc.Draw3dRect(rect, ::GetSysColor(COLOR_3DSHADOW),
                ::GetSysColor(COLOR_3DSHADOW));
}

inline BOOL erase_prop_host_bkgnd(CWnd* host, CDC* pDC) {
  if (host == nullptr || pDC == nullptr) {
    return TRUE;
  }
  CRect rc;
  host->GetClientRect(&rc);
  pDC->FillSolidRect(&rc, ::GetSysColor(COLOR_WINDOW));
  return TRUE;
}

inline void size_prop_host(CBCGPPropList& list,
                           int cx,
                           int cy,
                           int border = k_prop_host_border) {
  if (cx > 0 && cy > 0 && ::IsWindow(list.GetSafeHwnd())) {
    size_prop_list_inset(list, cx, cy, border);
  }
}

inline void add_prop_options(CBCGPProp* prop, const char* const* options) {
  if (prop == nullptr || options == nullptr) {
    return;
  }
  for (int i = 0; options[i] != nullptr; ++i) {
    prop->AddOption(options[i]);
  }
}

// Returns matching option index, or -1 when absent.
inline int option_index(const CString& value, const char* const* options) {
  if (options == nullptr) {
    return -1;
  }
  for (int i = 0; options[i] != nullptr; ++i) {
    if (value == options[i]) {
      return i;
    }
  }
  return -1;
}

}  // namespace legacy_ui

#endif  // LEGACY_UI_INSPECT_HOST_PROP_HOST_H_
