// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "host_window.h"

#include "detail/local_render.h"
#include "webview_host.h"

namespace app {
namespace webview2 {
namespace {

const wchar_t kHostClass[] = L"SmartGisWebHost";

void user_data_dir(wchar_t* out, size_t n) {
  GetModuleFileNameW(nullptr, out, static_cast<DWORD>(n));
  wchar_t* slash = wcsrchr(out, L'\\');
  if (slash) {
    slash[1] = 0;
  }
  wcscat_s(out, n, L"webview2_data");
}

}  // namespace

HostWindow::HostWindow() = default;

HostWindow::~HostWindow() {
  delete chrome_;
  chrome_ = nullptr;
  if (session_) {
    session_->SetObserver(nullptr);
    session_->Shutdown();
    delete session_;
    session_ = nullptr;
  }
}

bool HostWindow::create(HINSTANCE instance, const wchar_t* webview_fixed) {
  instance_ = instance;
  if (webview_fixed && webview_fixed[0]) {
    wcsncpy_s(webview_fixed_, webview_fixed, _TRUNCATE);
  }

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = &HostWindow::wnd_proc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpszClassName = kHostClass;
  wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  RegisterClassExW(&wc);

  hwnd_ = CreateWindowExW(
      WS_EX_APPWINDOW, kHostClass, L"SmartGIS Web",
      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
      1280, 800, nullptr, nullptr, instance, this);
  if (!hwnd_) {
    return false;
  }

  session_ = content::MapContents::Create();
  if (!session_) {
    return false;
  }
  session_->SetObserver(this);
  session_->StartRenderProcess();

  view_id_ = session_->OpenView(content::ViewKind::kMapEdit);
  if (!pane_.create(hwnd_, session_, view_id_)) {
    return false;
  }

  content::MapWidgetHostView* view =
      session_->AttachSurface(view_id_, content::PresentMode::kChildHwnd);
  if (!view) {
    view = session_->HostView(view_id_);
  }
  if (view) {
    content::MapWidgetHostView::CreateParams params;
    params.parent_hwnd = pane_.hwnd();
    view->Create(params, content::MapWidgetHostView::Preferences());
    view->SetPresentMode(content::PresentMode::kChildHwnd);
  }

  pane_.set_oop(session_->IsOopRender());
  const wchar_t* status = session_->PresentStatus();
  if (status && status[0]) {
    pane_.set_probe_text(status);
  } else {
    const auto probe = detail::probe_legacy_render_dlls();
    pane_.set_probe_text(detail::local_render_status_text(probe));
  }

  chrome_ = new WebViewChrome();
  wchar_t data[MAX_PATH];
  user_data_dir(data, MAX_PATH);
  const wchar_t* fixed = webview_fixed_[0] ? webview_fixed_ : nullptr;
  if (!chrome_->start(hwnd_, this, data, fixed)) {
    layout_fallback_map();
  }

  ShowWindow(hwnd_, SW_SHOW);
  UpdateWindow(hwnd_);
  return true;
}

int HostWindow::run_loop() {
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return static_cast<int>(msg.wParam);
}

void HostWindow::on_map_slot(int x, int y, int w, int h) {
  const UINT dpi = GetDpiForWindow(hwnd_);
  const float scale = dpi > 0 ? static_cast<float>(dpi) / 96.f : 1.f;
  pane_.move(x, y, w, h, scale);
}

void HostWindow::on_activate_tool(const char* tool_id) {
  if (session_) {
    session_->ActivateTool(view_id_, tool_id);
  }
}

void HostWindow::on_catalog_op(const char* json) {
  if (session_) {
    session_->CatalogCall(json);
  }
}

void HostWindow::OnFrameReady(uint32_t, uint32_t) {
  pane_.invalidate();
}

void HostWindow::OnRenderDied() {
  pane_.set_oop(false);
  pane_.set_probe_text(L"SmartGisRender.exe died — local map pane");
}

LRESULT CALLBACK HostWindow::wnd_proc(HWND hwnd,
                                      UINT msg,
                                      WPARAM wp,
                                      LPARAM lp) {
  HostWindow* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
    self = static_cast<HostWindow*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  } else {
    self = reinterpret_cast<HostWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (!self) {
    return DefWindowProcW(hwnd, msg, wp, lp);
  }
  return self->handle(msg, wp, lp);
}

LRESULT HostWindow::handle(UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_SIZE:
      on_size();
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(hwnd_, msg, wp, lp);
  }
}

void HostWindow::on_size() {
  RECT rc;
  GetClientRect(hwnd_, &rc);
  if (chrome_ && chrome_->is_ready()) {
    chrome_->set_bounds(rc);
  } else {
    layout_fallback_map();
  }
}

void HostWindow::layout_fallback_map() {
  RECT rc;
  GetClientRect(hwnd_, &rc);
  const int x = 8;
  const int y = 48;
  const int w = (rc.right - rc.left) - 16;
  const int h = (rc.bottom - rc.top) - 56;
  const UINT dpi = GetDpiForWindow(hwnd_);
  const float scale = dpi > 0 ? static_cast<float>(dpi) / 96.f : 1.f;
  pane_.move(x, y, w > 1 ? w : 1, h > 1 ? h : 1, scale);
}

}  // namespace webview2
}  // namespace app
