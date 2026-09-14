// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/cef/cef_app.h"

#include "include/cef_command_line.h"
#include "include/cef_process_message.h"
#include "include/wrapper/cef_helpers.h"

namespace app {
namespace cef {
namespace {

constexpr char kBridgeMessageName[] = "SmartGisBridge";

class SmartGisPostHandler : public CefV8Handler {
 public:
  explicit SmartGisPostHandler(CefRefPtr<CefBrowser> browser)
      : browser_(browser) {}

  bool Execute(const CefString& name,
               CefRefPtr<CefV8Value> object,
               const CefV8ValueList& arguments,
               CefRefPtr<CefV8Value>& retval,
               CefString& exception) override {
    CEF_REQUIRE_RENDERER_THREAD();
    (void)object;
    (void)retval;
    if (name != "smartgis_post" || arguments.size() < 1 ||
        !arguments[0]->IsString()) {
      exception = "smartgis_post expects a string";
      return true;
    }
    if (!browser_) {
      return true;
    }
    CefRefPtr<CefProcessMessage> msg =
        CefProcessMessage::Create(kBridgeMessageName);
    msg->GetArgumentList()->SetString(0, arguments[0]->GetStringValue());
    browser_->GetMainFrame()->SendProcessMessage(PID_BROWSER, msg);
    return true;
  }

 private:
  CefRefPtr<CefBrowser> browser_;
  IMPLEMENT_REFCOUNTING(SmartGisPostHandler);
};

class SmartGisOnHostHandler : public CefV8Handler {
 public:
  bool Execute(const CefString& name,
               CefRefPtr<CefV8Value> object,
               const CefV8ValueList& arguments,
               CefRefPtr<CefV8Value>& retval,
               CefString& exception) override {
    (void)name;
    (void)object;
    (void)arguments;
    (void)retval;
    (void)exception;
    // Host → JS is delivered via ExecuteJavaScript in the browser process.
    return true;
  }

 private:
  IMPLEMENT_REFCOUNTING(SmartGisOnHostHandler);
};

}  // namespace

void CefApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();
}

void CefApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {
  (void)process_type;
  if (!command_line) {
    return;
  }
  // Avoid CEF's own GPU process crashes on some Debug/driver setups; map
  // frames still come from MapContents OOP GPU, not CEF compositor.
  command_line->AppendSwitch("disable-gpu");
  command_line->AppendSwitch("disable-gpu-compositing");
  command_line->AppendSwitch("disable-gpu-sandbox");
  command_line->AppendSwitchWithValue("use-gl", "disabled");
}

void CefApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefV8Context> context) {
  CEF_REQUIRE_RENDERER_THREAD();
  if (!frame || !frame->IsMain() || !context) {
    return;
  }
  CefRefPtr<CefV8Value> global = context->GetGlobal();
  CefRefPtr<CefV8Handler> post(new SmartGisPostHandler(browser));
  global->SetValue("smartgis_post",
                   CefV8Value::CreateFunction("smartgis_post", post),
                   V8_PROPERTY_ATTRIBUTE_NONE);
}

}  // namespace cef
}  // namespace app
