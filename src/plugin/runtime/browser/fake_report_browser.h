// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_BROWSER_FAKE_REPORT_BROWSER_H_
#define PLUGIN_RUNTIME_BROWSER_FAKE_REPORT_BROWSER_H_

#include <string>
#include <vector>

#include "plugin/runtime/browser/report_browser.h"

namespace plugin {

// In-memory ReportBrowser for unit tests and Runtime-less hosts.
class PLUGIN_HOST_EXPORT FakeReportBrowser final : public ReportBrowser {
 public:
  FakeReportBrowser();
  ~FakeReportBrowser() override;

  void set_parent_hwnd(HWND parent) override;
  void set_bounds(int x, int y, int width, int height) override;
  bool navigate(std::string_view report_dir) override;
  bool post_json(std::string_view json) override;
  void close() override;

  bool is_available() const override;
  std::string status_text() const override;

  HWND parent_hwnd() const { return parent_; }
  const std::string& last_dir() const { return last_dir_; }
  const std::vector<std::string>& posts() const { return posts_; }
  int navigate_count() const { return navigate_count_; }

  void set_available(bool on) { available_ = on; }

 private:
  HWND parent_ = nullptr;
  int x_ = 0;
  int y_ = 0;
  int w_ = 0;
  int h_ = 0;
  bool available_ = true;
  bool open_ = false;
  std::string last_dir_;
  std::string status_ = "fake-ready";
  std::vector<std::string> posts_;
  int navigate_count_ = 0;
};

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_BROWSER_FAKE_REPORT_BROWSER_H_
