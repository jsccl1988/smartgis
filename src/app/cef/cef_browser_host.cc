// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/cef/cef_browser_host.h"

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/wrapper/cef_helpers.h"

#include <cstring>

namespace app {
namespace cef {
namespace {

constexpr char kBridgeMessageName[] = "SmartGisBridge";

}  // namespace

CefBrowserHost::CefBrowserHost() = default;
CefBrowserHost::~CefBrowserHost() = default;

bool CefBrowserHost::create(HWND parent,
                            const RectPx& rect,
                            const std::wstring& url) {
  parent_ = parent;
  if (!parent_ || !rect.is_valid()) {
    return false;
  }

  CefWindowInfo info;
  info.SetAsChild(parent_,
                  CefRect(rect.x, rect.y, rect.w > 0 ? rect.w : 1,
                          rect.h > 0 ? rect.h : 1));

  CefBrowserSettings settings;
  const CefString cef_url(url);
  return ::CefBrowserHost::CreateBrowser(info, this, cef_url, settings, nullptr,
                                         nullptr);
}

void CefBrowserHost::destroy() {
  if (browser_) {
    browser_->GetHost()->CloseBrowser(true);
    browser_ = nullptr;
  }
}

void CefBrowserHost::resize(const RectPx& rect) {
  if (!browser_ || !rect.is_valid()) {
    return;
  }
  HWND hwnd = browser_->GetHost()->GetWindowHandle();
  if (hwnd) {
    SetWindowPos(hwnd, nullptr, rect.x, rect.y, rect.w, rect.h,
                 SWP_NOZORDER | SWP_NOACTIVATE);
  }
}

bool CefBrowserHost::wait_load(uint32_t timeout_ms) {
  const DWORD end = GetTickCount() + timeout_ms;
  while (!load_ok_ && !load_failed_ && GetTickCount() < end) {
    CefDoMessageLoopWork();
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        return load_ok_;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    Sleep(10);
  }
  return load_ok_;
}

bool CefBrowserHost::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_UI_THREAD();
  (void)browser;
  (void)frame;
  (void)source_process;
  if (!message || message->GetName() != kBridgeMessageName) {
    return false;
  }
  const CefString json = message->GetArgumentList()->GetString(0);
  if (bridge_) {
    bridge_->handle_json(json.ToString());
  }
  return true;
}

void CefBrowserHost::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  browser_ = browser;
  if (bridge_) {
    bridge_->set_post_json(&CefBrowserHost::post_json_to_browser, this);
  }
}

bool CefBrowserHost::DoClose(CefRefPtr<CefBrowser> browser) {
  (void)browser;
  return false;
}

void CefBrowserHost::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  (void)browser;
  browser_ = nullptr;
}

void CefBrowserHost::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               int httpStatusCode) {
  (void)browser;
  (void)httpStatusCode;
  if (frame && frame->IsMain()) {
    load_ok_ = true;
  }
}

void CefBrowserHost::OnLoadError(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 ErrorCode errorCode,
                                 const CefString& errorText,
                                 const CefString& failedUrl) {
  (void)browser;
  (void)errorCode;
  (void)errorText;
  (void)failedUrl;
  if (frame && frame->IsMain()) {
    load_failed_ = true;
  }
}

void CefBrowserHost::post_json_to_browser(void* user, const std::string& json) {
  auto* self = static_cast<CefBrowserHost*>(user);
  if (self) {
    self->post_json(json);
  }
}

void CefBrowserHost::post_json(const std::string& json) {
  if (!browser_ || !browser_->GetMainFrame()) {
    return;
  }
  // Escape for JS string literal.
  std::string escaped;
  escaped.reserve(json.size() + 8);
  for (char c : json) {
    if (c == '\\' || c == '\'') {
      escaped.push_back('\\');
    }
    if (c == '\n') {
      escaped += "\\n";
      continue;
    }
    if (c == '\r') {
      continue;
    }
    escaped.push_back(c);
  }
  const std::string script =
      "window.smartgis_on_host && window.smartgis_on_host('" + escaped + "');";
  browser_->GetMainFrame()->ExecuteJavaScript(script, browser_->GetMainFrame()->GetURL(),
                                              0);
}

}  // namespace cef
}  // namespace app
