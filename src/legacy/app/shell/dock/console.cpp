// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/app/stdafx.h"
#include "legacy/app/shell/dock/console.h"

#include <algorithm>
#include <string>

#include "base/log/log_sink.h"
#include "content/browser/debug/debug_agent.h"

namespace {

constexpr UINT_PTR kConsoleTimerId = 43;
constexpr UINT kConsoleTimerMs = 200;
constexpr int kBtnW = 64;
constexpr int kBtnH = 24;
constexpr int kInputH = 22;
constexpr int kPad = 4;
constexpr int kMaxLogLines = 500;

enum {
  kIdClear = 5201,
  kIdSubmit = 5202,
  kIdLog = 5203,
  kIdInput = 5204,
};

}  // namespace

BEGIN_MESSAGE_MAP(DebugConsolePane, CWnd)
ON_WM_CREATE()
ON_WM_SIZE()
ON_WM_TIMER()
ON_WM_DESTROY()
ON_BN_CLICKED(kIdClear, OnBnClear)
ON_BN_CLICKED(kIdSubmit, OnBnSubmit)
END_MESSAGE_MAP()

DebugConsolePane::DebugConsolePane() = default;

DebugConsolePane::~DebugConsolePane() {
  drop_log_subscription();
}

BOOL DebugConsolePane::Create(CWnd* parent, UINT id) {
  return CWnd::CreateEx(
      0, AfxRegisterWndClass(0), _T(""),
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, CRect(0, 0, 0, 0),
      parent, id);
}

int DebugConsolePane::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CWnd::OnCreate(lpCreateStruct) == -1) {
    return -1;
  }

  CRect r(0, 0, 0, 0);
  if (!btn_clear_.Create(_T("Clear"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, r,
                         this, kIdClear) ||
      !btn_submit_.Create(_T("Run"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, r,
                          this, kIdSubmit) ||
      !log_list_.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER |
                            LBS_NOINTEGRALHEIGHT | LBS_NOTIFY,
                        r, this, kIdLog) ||
      !input_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, r, this,
                     kIdInput)) {
    return -1;
  }

  ensure_log_subscription();
  seed_from_tail();
  SetTimer(kConsoleTimerId, kConsoleTimerMs, nullptr);
  append_line(_T("[Console] ready — key [legacy.flow] + process logs"));
  return 0;
}

void DebugConsolePane::OnDestroy() {
  KillTimer(kConsoleTimerId);
  drop_log_subscription();
  CWnd::OnDestroy();
}

void DebugConsolePane::layout_children(int cx, int cy) {
  int x = kPad;
  const int y = kPad;
  btn_clear_.SetWindowPos(nullptr, x, y, kBtnW, kBtnH, SWP_NOZORDER);
  x += kBtnW + kPad;
  btn_submit_.SetWindowPos(nullptr, x, y, kBtnW, kBtnH, SWP_NOZORDER);

  const int list_top = y + kBtnH + kPad;
  const int list_h = (std::max)(40, cy - list_top - kInputH - 2 * kPad);
  log_list_.SetWindowPos(nullptr, kPad, list_top, cx - 2 * kPad, list_h,
                         SWP_NOZORDER);
  input_.SetWindowPos(nullptr, kPad, list_top + list_h + kPad, cx - 2 * kPad,
                      kInputH, SWP_NOZORDER);
}

void DebugConsolePane::OnSize(UINT nType, int cx, int cy) {
  CWnd::OnSize(nType, cx, cy);
  if (cx > 0 && cy > 0 && ::IsWindow(log_list_.m_hWnd)) {
    layout_children(cx, cy);
  }
}

void DebugConsolePane::append_line(const CString& line) {
  if (!::IsWindow(log_list_.m_hWnd)) {
    return;
  }
  const int idx = log_list_.AddString(line);
  while (log_list_.GetCount() > kMaxLogLines) {
    log_list_.DeleteString(0);
  }
  log_list_.SetCurSel(idx);
}

void DebugConsolePane::drain_pending() {
  if (!::IsWindow(m_hWnd)) {
    return;
  }
  std::vector<std::string> batch;
  {
    std::lock_guard<std::mutex> lock(pending_mu_);
    batch.swap(pending_);
  }
  for (const auto& s : batch) {
    append_line(CString(s.c_str()));
  }
}

void DebugConsolePane::OnTimer(UINT_PTR nIDEvent) {
  if (nIDEvent == kConsoleTimerId) {
    drain_pending();
  }
  CWnd::OnTimer(nIDEvent);
}

void DebugConsolePane::ensure_log_subscription() {
  if (log_sub_id_) {
    return;
  }
  log_sub_id_ = base::log_sink().subscribe([this](const base::LogEntry& e) {
    const bool is_flow =
        e.message.find("[legacy.flow]") != std::string::npos ||
        e.message.find("[flow]") != std::string::npos;
    const bool keep =
        is_flow || e.level <= base::LogLevel::kWarning ||
        e.level == base::LogLevel::kInfo || e.level == base::LogLevel::kNotice;
    if (!keep) {
      return;
    }
    std::string line =
        std::string(base::log_level_name(e.level)) + " " + e.message;
    std::lock_guard<std::mutex> lock(pending_mu_);
    pending_.push_back(std::move(line));
    if (pending_.size() > 200) {
      pending_.erase(pending_.begin(),
                     pending_.begin() +
                         static_cast<std::ptrdiff_t>(pending_.size() - 200));
    }
  });
}

void DebugConsolePane::drop_log_subscription() {
  if (!log_sub_id_) {
    return;
  }
  base::log_sink().unsubscribe(log_sub_id_);
  log_sub_id_ = 0;
}

void DebugConsolePane::seed_from_tail() {
  const auto tail = base::log_sink().snapshot_tail(80);
  for (const auto& e : tail) {
    const bool is_flow =
        e.message.find("[legacy.flow]") != std::string::npos ||
        e.message.find("[flow]") != std::string::npos;
    if (!is_flow && e.level > base::LogLevel::kInfo) {
      continue;
    }
    append_line(CString(
        (std::string(base::log_level_name(e.level)) + " " + e.message).c_str()));
  }
}

void DebugConsolePane::OnBnClear() {
  {
    std::lock_guard<std::mutex> lock(pending_mu_);
    pending_.clear();
  }
  if (::IsWindow(log_list_.m_hWnd)) {
    log_list_.ResetContent();
  }
  // Drop subscription briefly so clear does not re-enter the pane.
  drop_log_subscription();
  base::log_sink().clear();
  ensure_log_subscription();
  append_line(_T("[Clear]"));
}

void DebugConsolePane::OnBnSubmit() {
  CString text;
  input_.GetWindowText(text);
  input_.SetWindowText(_T(""));
  if (text.IsEmpty()) {
    return;
  }
  CT2CA utf8(text);
  const std::string line(utf8 ? static_cast<const char*>(utf8) : "");
  append_line(CString(("> " + line).c_str()));

  try {
    if (!content::debug_agent().is_running()) {
      content::debug_agent().start();
    }
    const std::string out = content::debug_agent().exec_line(line);
    if (!out.empty()) {
      append_line(CString(out.c_str()));
    }
  } catch (...) {
    append_line(_T("[error] debug_agent failed"));
  }
}
