// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/app/stdafx.h"
#include "legacy/app/shell/dock/diagnostic.h"

#include <algorithm>

namespace {

enum {
  kIdBtnConsole = 5400,
  kIdBtnTrace = 5401,
  kIdConsolePane = 5402,
  kIdTracePane = 5403,
};

constexpr int kTabH = 28;
constexpr int kPad = 4;
constexpr int kBtnW = 100;

}  // namespace

BEGIN_MESSAGE_MAP(DiagnosticToolsDockBar, CBCGPDockingControlBar)
ON_WM_CREATE()
ON_WM_SIZE()
ON_BN_CLICKED(kIdBtnConsole, &DiagnosticToolsDockBar::OnBnConsole)
ON_BN_CLICKED(kIdBtnTrace, &DiagnosticToolsDockBar::OnBnTrace)
END_MESSAGE_MAP()

DiagnosticToolsDockBar::DiagnosticToolsDockBar() = default;

DiagnosticToolsDockBar::~DiagnosticToolsDockBar() = default;

int DiagnosticToolsDockBar::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CBCGPDockingControlBar::OnCreate(lpCreateStruct) == -1) {
    return -1;
  }

  CRect r(0, 0, 0, 0);
  if (!btn_console_.Create(_T("Console"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           r, this, kIdBtnConsole) ||
      !btn_trace_.Create(_T("RenderTrace"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         r, this, kIdBtnTrace) ||
      !console_.Create(this, kIdConsolePane) ||
      !trace_.Create(this, kIdTracePane)) {
    return -1;
  }

  // Views-like chrome: YaHei UI on page tabs.
  if (ui_font_.GetSafeHandle() == NULL) {
    ui_font_.CreateFont(
        -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, _T("Microsoft YaHei UI"));
  }
  if (ui_font_.GetSafeHandle() != NULL) {
    btn_console_.SetFont(&ui_font_);
    btn_trace_.SetFont(&ui_font_);
  }

  show_page(0);
  return 0;
}

void DiagnosticToolsDockBar::show_page(int page) {
  active_page_ = page;
  const BOOL show_console = (page == 0) ? TRUE : FALSE;
  if (::IsWindow(console_.m_hWnd)) {
    console_.ShowWindow(show_console ? SW_SHOW : SW_HIDE);
  }
  if (::IsWindow(trace_.m_hWnd)) {
    trace_.ShowWindow(show_console ? SW_HIDE : SW_SHOW);
  }
  // Re-layout after show: a zero-size first paint + later ShowWindow left the
  // RenderTrace child buttons/list at 0x0 and could AV on BN_CLICKED.
  CRect rc;
  GetClientRect(&rc);
  if (rc.Width() > 0 && rc.Height() > 0) {
    layout_children(rc.Width(), rc.Height());
  }
}

void DiagnosticToolsDockBar::OnBnConsole() { show_page(0); }

void DiagnosticToolsDockBar::OnBnTrace() { show_page(1); }

void DiagnosticToolsDockBar::layout_children(int cx, int cy) {
  int x = kPad;
  btn_console_.SetWindowPos(nullptr, x, kPad, kBtnW, kTabH, SWP_NOZORDER);
  x += kBtnW + kPad;
  btn_trace_.SetWindowPos(nullptr, x, kPad, kBtnW + 16, kTabH, SWP_NOZORDER);

  const int page_top = kPad + kTabH + kPad;
  const int page_h = (std::max)(0, cy - page_top - kPad);
  const int page_w = (std::max)(0, cx - 2 * kPad);
  if (::IsWindow(console_.m_hWnd)) {
    console_.SetWindowPos(nullptr, kPad, page_top, page_w, page_h, SWP_NOZORDER);
  }
  if (::IsWindow(trace_.m_hWnd)) {
    trace_.SetWindowPos(nullptr, kPad, page_top, page_w, page_h, SWP_NOZORDER);
  }
}

void DiagnosticToolsDockBar::OnSize(UINT nType, int cx, int cy) {
  CBCGPDockingControlBar::OnSize(nType, cx, cy);
  if (cx > 0 && cy > 0) {
    layout_children(cx, cy);
  }
}
