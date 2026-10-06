// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_WEB_WEBVIEW2_REPORT_BROWSER_H_
#define PLUGIN_RUNTIME_WEB_WEBVIEW2_REPORT_BROWSER_H_

#include <memory>
#include <string>

#include "plugin/runtime/web/report_browser.h"

struct ICoreWebView2;
struct ICoreWebView2Controller;
struct ICoreWebView2Environment;

namespace plugin {

// WebView2-backed ReportBrowser. Soft-fails when loader/Runtime is missing.
class PLUGIN_HOST_EXPORT WebView2ReportBrowser final : public ReportBrowser {
 public:
  WebView2ReportBrowser();
  ~WebView2ReportBrowser() override;

  void set_parent_hwnd(HWND parent) override;
  void set_bounds(int x, int y, int width, int height) override;
  bool navigate(std::string_view report_dir) override;
  bool post_json(std::string_view json) override;
  void close() override;

  bool is_available() const override;
  std::string status_text() const override;

 private:
  struct State;

  void ensure_environment();
  void apply_bounds();
  void navigate_pending();

  HWND parent_ = nullptr;
  int x_ = 0;
  int y_ = 0;
  int w_ = 0;
  int h_ = 0;
  bool available_ = false;
  std::string status_ = "webview2-init";
  std::string pending_url_;
  std::string pending_post_;
  std::unique_ptr<State> state_;
};

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_WEB_WEBVIEW2_REPORT_BROWSER_H_
