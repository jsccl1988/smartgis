// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_CEF_CEF_BROWSER_HOST_H_
#define APP_CEF_CEF_BROWSER_HOST_H_

#include <atomic>
#include <string>

#include "app/cef/chrome_bridge.h"
#include "app/cef/layout_host.h"

#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {
namespace cef {

// Creates a CEF browser HWND inside the chrome rect and routes ProcessMessage
// JSON into ChromeBridge.
class CefBrowserHost : public CefClient,
                       public CefLifeSpanHandler,
                       public CefLoadHandler {
 public:
  CefBrowserHost();
  ~CefBrowserHost() override;

  bool create(HWND parent, const RectPx& rect, const std::wstring& url);
  void destroy();
  void resize(const RectPx& rect);
  bool load_ok() const { return load_ok_; }
  bool wait_load(uint32_t timeout_ms);

  void set_bridge(ChromeBridge* bridge) { bridge_ = bridge; }
  CefRefPtr<CefBrowser> browser() const { return browser_; }

  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  bool DoClose(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override;
  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override;

  static void post_json_to_browser(void* user, const std::string& json);

 private:
  void post_json(const std::string& json);

  HWND parent_ = nullptr;
  CefRefPtr<CefBrowser> browser_;
  ChromeBridge* bridge_ = nullptr;
  std::atomic<bool> load_ok_{false};
  std::atomic<bool> load_failed_{false};

  IMPLEMENT_REFCOUNTING(CefBrowserHost);
};

}  // namespace cef
}  // namespace app

#endif  // APP_CEF_CEF_BROWSER_HOST_H_
