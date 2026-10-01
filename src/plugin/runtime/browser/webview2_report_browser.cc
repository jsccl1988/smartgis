// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/browser/webview2_report_browser.h"

#include <atomic>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wrl/client.h>
#include <wrl/event.h>

#include "WebView2.h"

namespace plugin {
namespace {

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

std::wstring utf8_to_wide(std::string_view utf8) {
  if (utf8.empty()) {
    return {};
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                    static_cast<int>(utf8.size()), nullptr, 0);
  if (n <= 0) {
    return {};
  }
  std::wstring out(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                      out.data(), n);
  return out;
}

}  // namespace

struct WebView2ReportBrowser::State {
  ComPtr<ICoreWebView2Environment> env;
  ComPtr<ICoreWebView2Controller> controller;
  ComPtr<ICoreWebView2> webview;
  std::atomic<bool> ready{false};
  std::atomic<bool> creating{false};
};

WebView2ReportBrowser::WebView2ReportBrowser()
    : state_(std::make_unique<State>()) {
  available_ = true;
  status_ = "webview2-pending";
}

WebView2ReportBrowser::~WebView2ReportBrowser() {
  close();
}

void WebView2ReportBrowser::set_parent_hwnd(HWND parent) {
  parent_ = parent;
  if (parent_) {
    ensure_environment();
  }
}

void WebView2ReportBrowser::set_bounds(int x, int y, int width, int height) {
  x_ = x;
  y_ = y;
  w_ = width > 0 ? width : 1;
  h_ = height > 0 ? height : 1;
  apply_bounds();
}

void WebView2ReportBrowser::ensure_environment() {
  if (!parent_ || !state_ || state_->ready.load() ||
      state_->creating.load()) {
    return;
  }
  state_->creating.store(true);
  HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
      nullptr, nullptr, nullptr,
      Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(result) || !env || !state_) {
              available_ = false;
              status_ = "webview2-env-failed";
              state_->creating.store(false);
              return S_OK;
            }
            state_->env = env;
            env->CreateCoreWebView2Controller(
                parent_,
                Callback<
                    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [this](HRESULT result2,
                           ICoreWebView2Controller* controller) -> HRESULT {
                      if (FAILED(result2) || !controller || !state_) {
                        available_ = false;
                        status_ = "webview2-controller-failed";
                        state_->creating.store(false);
                        return S_OK;
                      }
                      state_->controller = controller;
                      controller->get_CoreWebView2(&state_->webview);
                      if (!state_->webview) {
                        available_ = false;
                        status_ = "webview2-core-failed";
                        state_->creating.store(false);
                        return S_OK;
                      }
                      EventRegistrationToken token = {};
                      state_->webview->add_NavigationStarting(
                          Callback<ICoreWebView2NavigationStartingEventHandler>(
                              [](ICoreWebView2*,
                                 ICoreWebView2NavigationStartingEventArgs*
                                     args) -> HRESULT {
                                if (!args) {
                                  return S_OK;
                                }
                                LPWSTR uri = nullptr;
                                args->get_Uri(&uri);
                                if (uri) {
                                  char narrow[2048] = {};
                                  WideCharToMultiByte(CP_UTF8, 0, uri, -1,
                                                      narrow, sizeof(narrow) - 1,
                                                      nullptr, nullptr);
                                  CoTaskMemFree(uri);
                                  if (ReportBrowser::is_blocked_navigation_url(
                                          narrow)) {
                                    args->put_Cancel(TRUE);
                                  }
                                }
                                return S_OK;
                              })
                              .Get(),
                          &token);
                      state_->ready.store(true);
                      state_->creating.store(false);
                      available_ = true;
                      status_ = "webview2-ready";
                      apply_bounds();
                      navigate_pending();
                      return S_OK;
                    })
                    .Get());
            return S_OK;
          })
          .Get());
  if (FAILED(hr)) {
    available_ = false;
    status_ = "webview2-create-failed";
    state_->creating.store(false);
  }
}

void WebView2ReportBrowser::apply_bounds() {
  if (!state_ || !state_->controller) {
    return;
  }
  RECT rc = {x_, y_, x_ + w_, y_ + h_};
  state_->controller->put_Bounds(rc);
  state_->controller->put_IsVisible(TRUE);
}

void WebView2ReportBrowser::navigate_pending() {
  if (!state_ || !state_->webview || pending_url_.empty()) {
    return;
  }
  const std::wstring uri = utf8_to_wide(pending_url_);
  state_->webview->Navigate(uri.c_str());
  status_ = "navigated";
  if (!pending_post_.empty()) {
    const std::wstring msg = utf8_to_wide(pending_post_);
    state_->webview->PostWebMessageAsJson(msg.c_str());
    pending_post_.clear();
  }
}

bool WebView2ReportBrowser::navigate(std::string_view report_dir) {
  if (!is_path_allowed(report_dir)) {
    status_ = "path-not-allowed";
    return false;
  }
  const std::string url = file_url_for_report_dir(report_dir);
  if (url.empty()) {
    status_ = "missing-index.html";
    return false;
  }
  pending_url_ = url;
  if (!parent_) {
    status_ = "no-parent-hwnd";
    return false;
  }
  ensure_environment();
  if (state_ && state_->ready.load()) {
    navigate_pending();
    return true;
  }
  status_ = "webview2-pending-nav";
  return available_;
}

bool WebView2ReportBrowser::post_json(std::string_view json) {
  if (json.empty()) {
    return false;
  }
  if (state_ && state_->ready.load() && state_->webview) {
    const std::wstring msg = utf8_to_wide(json);
    const HRESULT hr = state_->webview->PostWebMessageAsJson(msg.c_str());
    return SUCCEEDED(hr);
  }
  pending_post_ = std::string(json);
  return available_;
}

void WebView2ReportBrowser::close() {
  pending_url_.clear();
  pending_post_.clear();
  if (state_) {
    if (state_->controller) {
      state_->controller->Close();
    }
    state_->webview.Reset();
    state_->controller.Reset();
    state_->env.Reset();
    state_->ready.store(false);
    state_->creating.store(false);
  }
  status_ = "closed";
}

bool WebView2ReportBrowser::is_available() const {
  return available_;
}

std::string WebView2ReportBrowser::status_text() const {
  return status_;
}

}  // namespace plugin
