// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/app/stdafx.h"
#include "legacy/app/shell/dock/render_trace.h"

#include <vector>

#include "base/trace/event/process_trace.h"
#include "base/trace/log/frame_log.h"
#include "legacy/app/shell/dock/dock_child.h"

namespace {

constexpr UINT_PTR kRenderTraceTimerId = 42;
constexpr UINT kRenderTraceTimerMs = 400;
constexpr int kBtnW = 64;
constexpr int kBtnH = 24;
constexpr int kPad = 4;

enum {
  kIdRecord = 5101,
  kIdStop = 5102,
  kIdClear = 5103,
  kIdRefresh = 5104,
  kIdLog = 5105,
};

}  // namespace

BEGIN_MESSAGE_MAP(RenderTracePane, CWnd)
ON_WM_CREATE()
ON_WM_SIZE()
ON_WM_TIMER()
ON_WM_DESTROY()
ON_BN_CLICKED(kIdRecord, &RenderTracePane::OnBnRecord)
ON_BN_CLICKED(kIdStop, &RenderTracePane::OnBnStop)
ON_BN_CLICKED(kIdClear, &RenderTracePane::OnBnClear)
ON_BN_CLICKED(kIdRefresh, &RenderTracePane::OnBnRefresh)
END_MESSAGE_MAP()

RenderTracePane::RenderTracePane() = default;

RenderTracePane::~RenderTracePane() = default;

BOOL RenderTracePane::Create(CWnd* parent, UINT id) {
  return legacy_app::detail::create_dock_pane_child(this, parent, id);
}

int RenderTracePane::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CWnd::OnCreate(lpCreateStruct) == -1) {
    return -1;
  }

  CRect r(0, 0, 0, 0);
  if (!btn_record_.Create(_T("Record"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          r, this, kIdRecord) ||
      !btn_stop_.Create(_T("Stop"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, r,
                        this, kIdStop) ||
      !btn_clear_.Create(_T("Clear"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, r,
                         this, kIdClear) ||
      !btn_refresh_.Create(_T("Refresh"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           r, this, kIdRefresh) ||
      !log_list_.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER |
                            LBS_NOINTEGRALHEIGHT | LBS_NOTIFY,
                        r, this, kIdLog)) {
    return -1;
  }

  if (base::trace::tracing_enabled()) {
    armed_ = true;
    SetTimer(kRenderTraceTimerId, kRenderTraceTimerMs, nullptr);
  }
  return 0;
}

void RenderTracePane::OnDestroy() {
  KillTimer(kRenderTraceTimerId);
  CWnd::OnDestroy();
}

void RenderTracePane::layout_children(int cx, int cy) {
  int x = kPad;
  const int y = kPad;
  btn_record_.SetWindowPos(nullptr, x, y, kBtnW, kBtnH, SWP_NOZORDER);
  x += kBtnW + kPad;
  btn_stop_.SetWindowPos(nullptr, x, y, kBtnW, kBtnH, SWP_NOZORDER);
  x += kBtnW + kPad;
  btn_clear_.SetWindowPos(nullptr, x, y, kBtnW, kBtnH, SWP_NOZORDER);
  x += kBtnW + kPad;
  btn_refresh_.SetWindowPos(nullptr, x, y, kBtnW + 8, kBtnH, SWP_NOZORDER);

  const int list_top = y + kBtnH + kPad;
  log_list_.SetWindowPos(nullptr, kPad, list_top, cx - 2 * kPad,
                         cy - list_top - kPad, SWP_NOZORDER);
}

void RenderTracePane::OnSize(UINT nType, int cx, int cy) {
  CWnd::OnSize(nType, cx, cy);
  if (cx > 0 && cy > 0 && ::IsWindow(log_list_.m_hWnd)) {
    layout_children(cx, cy);
  }
}

void RenderTracePane::append_line(const CString& line) {
  legacy_app::detail::append_list_line(log_list_, line);
}

void RenderTracePane::append_new_frame_lines() {
  if (!::IsWindow(log_list_.m_hWnd)) {
    return;
  }
  std::vector<base::trace::Trace::Event> events;
  base::trace::for_each_process_trace_event(
      [](void* ctx, int tid, base::trace::Trace::time_point begin,
         base::trace::Trace::time_point end, const char* name, const char* cat,
         base::trace::Trace::Event::Kind kind, int64_t counter_value) {
        auto* out = static_cast<std::vector<base::trace::Trace::Event>*>(ctx);
        base::trace::Trace::Event ev;
        ev.tid = tid;
        ev.begin = begin;
        ev.end = end;
        ev.name = name ? name : "";
        ev.cat = cat ? cat : "";
        ev.kind = kind;
        ev.counter_value = counter_value;
        out->push_back(std::move(ev));
      },
      &events);

  const auto lines = base::trace::format_trace_frame_log_lines(events);
  for (const auto& line : lines) {
    if (line.frame_end <= last_frame_end_) {
      continue;
    }
    last_frame_end_ = line.frame_end;
    append_line(CString(line.text.c_str()));
  }
}

void RenderTracePane::OnTimer(UINT_PTR nIDEvent) {
  if (nIDEvent == kRenderTraceTimerId && armed_) {
    append_new_frame_lines();
  }
  CWnd::OnTimer(nIDEvent);
}

void RenderTracePane::OnBnRecord() {
  base::trace::process_trace().clear();
  last_frame_end_ = {};
  if (::IsWindow(log_list_.m_hWnd)) {
    log_list_.ResetContent();
  }
  base::trace::set_tracing_enabled(true);
  armed_ = true;
  SetTimer(kRenderTraceTimerId, kRenderTraceTimerMs, nullptr);
  append_line(_T("[Record] tracing armed"));
}

void RenderTracePane::OnBnStop() {
  base::trace::set_tracing_enabled(false);
  armed_ = false;
  KillTimer(kRenderTraceTimerId);
  append_new_frame_lines();
  append_line(_T("[Stop] tracing off"));
}

void RenderTracePane::OnBnClear() {
  base::trace::process_trace().clear();
  last_frame_end_ = {};
  if (::IsWindow(log_list_.m_hWnd)) {
    log_list_.ResetContent();
  }
  append_line(_T("[Clear]"));
}

void RenderTracePane::OnBnRefresh() { append_new_frame_lines(); }
