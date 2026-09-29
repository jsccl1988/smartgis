// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/app/stdafx.h"
#include "legacy/app/shell/dock/diagnostic.h"

namespace {

enum {
  kIdTabs = 5400,
  kIdConsolePane = 5401,
  kIdTracePane = 5402,
};

}  // namespace

BEGIN_MESSAGE_MAP(DiagnosticToolsDockBar, CBCGPDockingControlBar)
ON_WM_CREATE()
ON_WM_SIZE()
END_MESSAGE_MAP()

DiagnosticToolsDockBar::DiagnosticToolsDockBar() = default;

DiagnosticToolsDockBar::~DiagnosticToolsDockBar() = default;

int DiagnosticToolsDockBar::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CBCGPDockingControlBar::OnCreate(lpCreateStruct) == -1) {
    return -1;
  }

  CRect r(0, 0, 0, 0);
  if (!tabs_.Create(CBCGPTabWnd::STYLE_3D, r, this, kIdTabs)) {
    return -1;
  }
  tabs_.EnableTabSwap(FALSE);
  tabs_.SetFlatFrame(TRUE);

  if (!console_.Create(&tabs_, kIdConsolePane) ||
      !trace_.Create(&tabs_, kIdTracePane)) {
    return -1;
  }

  tabs_.AddTab(&console_, _T("Console"), (UINT)-1, FALSE);
  tabs_.AddTab(&trace_, _T("RenderTrace"), (UINT)-1, FALSE);
  tabs_.SetActiveTab(0);
  return 0;
}

void DiagnosticToolsDockBar::layout_children(int cx, int cy) {
  if (::IsWindow(tabs_.m_hWnd)) {
    tabs_.SetWindowPos(nullptr, 0, 0, cx, cy, SWP_NOZORDER);
  }
}

void DiagnosticToolsDockBar::OnSize(UINT nType, int cx, int cy) {
  CBCGPDockingControlBar::OnSize(nType, cx, cy);
  if (cx > 0 && cy > 0) {
    layout_children(cx, cy);
  }
}
