// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_CEF_CEF_APP_H_
#define APP_CEF_CEF_APP_H_

#include "include/cef_app.h"
#include "include/cef_browser_process_handler.h"
#include "include/cef_render_process_handler.h"
#include "include/cef_v8.h"

namespace app {
namespace cef {

// Thin CefApp: browser + renderer hooks. Renderer only exposes
// window.smartgis_post(string) → ProcessMessage (no GIS pointers in V8).
class CefApp : public ::CefApp,
               public CefBrowserProcessHandler,
               public CefRenderProcessHandler {
 public:
  CefApp() = default;

  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }
  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
    return this;
  }

  void OnContextInitialized() override;

  void OnBeforeCommandLineProcessing(
      const CefString& process_type,
      CefRefPtr<CefCommandLine> command_line) override;

  void OnContextCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override;

 private:
  IMPLEMENT_REFCOUNTING(CefApp);
};

}  // namespace cef
}  // namespace app

#endif  // APP_CEF_CEF_APP_H_
