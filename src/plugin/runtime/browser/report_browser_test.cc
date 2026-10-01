// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/browser/fake_report_browser.h"
#include "plugin/runtime/browser/report_browser.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  using plugin::FakeReportBrowser;
  using plugin::ReportBrowser;

  expect(ReportBrowser::is_blocked_navigation_url("https://evil.example"),
         "block https");
  expect(ReportBrowser::is_blocked_navigation_url("http://evil.example"),
         "block http");
  expect(!ReportBrowser::is_blocked_navigation_url("file:///C:/tmp/index.html"),
         "allow file");

  const auto tmp = std::filesystem::temp_directory_path() / "smt_report_browser_test";
  std::error_code ec;
  std::filesystem::create_directories(tmp, ec);
  {
    std::ofstream out(tmp / "index.html");
    out << "<html><body>ok</body></html>";
  }

  FakeReportBrowser browser;
  browser.add_allowed_root(tmp.string());
  expect(browser.navigate(tmp.string()), "navigate allowed");
  expect(browser.navigate_count() == 1, "navigate once");
  expect(browser.post_json(R"({"a":1})"), "post json");
  expect(browser.posts().size() == 1, "one post");

  FakeReportBrowser denied;
  denied.add_allowed_root((tmp / "other").string());
  expect(!denied.navigate(tmp.string()), "deny outside root");
  expect(denied.status_text() == "path-not-allowed", "deny status");

  const std::string url = ReportBrowser::file_url_for_report_dir(tmp.string());
  expect(!url.empty() && url.find("file:///") == 0, "file url");

  browser.close();
  expect(browser.status_text() == "closed", "closed");

  if (g_fails) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("plugin_report_browser_test OK\n");
  return 0;
}
