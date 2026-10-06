// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/panels/report_panel.h"

#include <memory>
#include <utility>

#include "plugin/runtime/web/report_browser.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/primitives/text/label.h"

namespace app {
namespace {

constexpr wchar_t kReportHostClass[] = L"SmartGisReportHost";

LRESULT CALLBACK report_host_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                      LPARAM lparam) {
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace

ReportPanel::ReportPanel() {
  auto layout = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kVertical);
  layout->set_between_child_spacing(4);
  set_layout_manager(std::move(layout));
  set_preferred_size({280, 200});

  auto status = std::make_unique<ui::views::Label>("Report (idle)");
  status_ = status.get();
  add_child(std::move(status));
}

ReportPanel::~ReportPanel() {
  close_report();
  if (native_view() && IsWindow(native_view())) {
    DestroyWindow(native_view());
    set_native_view(nullptr);
  }
}

void ReportPanel::set_browser(std::unique_ptr<plugin::ReportBrowser> browser) {
  browser_ = std::move(browser);
  refresh_status();
}

bool ReportPanel::ensure_host_hwnd() {
  if (native_view() && IsWindow(native_view())) {
    return true;
  }
  ui::views::Widget* w = widget();
  if (!w || !w->hwnd()) {
    set_status_text("no-widget-hwnd");
    return false;
  }
  static bool registered = false;
  HINSTANCE inst = GetModuleHandleW(nullptr);
  if (!registered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = report_host_wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kReportHostClass;
    registered = RegisterClassExW(&wc) != 0 ||
                 GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  }
  const ui::views::Rect& b = bounds();
  HWND hwnd = CreateWindowExW(
      0, kReportHostClass, L"ReportHost",
      WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, b.x,
      b.y + 22, b.width > 0 ? b.width : 100, b.height > 22 ? b.height - 22 : 80,
      w->hwnd(), nullptr, inst, nullptr);
  if (!hwnd) {
    set_status_text("host-hwnd-create-failed");
    return false;
  }
  set_native_view(hwnd);
  return true;
}

bool ReportPanel::open_report(std::string_view report_dir) {
  if (!browser_) {
    set_status_text("no-browser");
    return false;
  }
  if (!ensure_host_hwnd()) {
    return false;
  }
  browser_->set_parent_hwnd(native_view());
  sync_browser_bounds();
  const bool ok = browser_->navigate(report_dir);
  refresh_status();
  return ok;
}

bool ReportPanel::post_json(std::string_view json) {
  if (!browser_) {
    return false;
  }
  return browser_->post_json(json);
}

void ReportPanel::close_report() {
  if (browser_) {
    browser_->close();
  }
  refresh_status();
}

void ReportPanel::add_allowed_root(std::string root) {
  if (browser_) {
    browser_->add_allowed_root(std::move(root));
  }
}

void ReportPanel::set_status_text(std::string text) {
  if (status_) {
    status_->set_text(std::move(text));
  }
}

void ReportPanel::layout() {
  ui::views::View::layout();
  sync_browser_bounds();
}

HWND ReportPanel::create_native_view(HWND) {
  // Lazy: host HWND is created on first open_report (avoids horizon init AV).
  return nullptr;
}

void ReportPanel::sync_browser_bounds() {
  if (!browser_ || !native_view() || !IsWindow(native_view())) {
    return;
  }
  ui::views::Widget* w = widget();
  if (w && w->hwnd()) {
    const ui::views::Rect& b = bounds();
    SetWindowPos(native_view(), nullptr, b.x, b.y + 22,
                 b.width > 0 ? b.width : 1,
                 b.height > 22 ? b.height - 22 : 1,
                 SWP_NOZORDER | SWP_NOACTIVATE);
  }
  RECT rc = {};
  GetClientRect(native_view(), &rc);
  browser_->set_bounds(0, 0, rc.right - rc.left, rc.bottom - rc.top);
}

void ReportPanel::refresh_status() {
  if (!status_) {
    return;
  }
  if (!browser_) {
    status_->set_text("Report (no backend)");
    return;
  }
  status_->set_text(browser_->status_text());
}

}  // namespace app
