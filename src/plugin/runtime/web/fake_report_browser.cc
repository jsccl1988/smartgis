// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/web/fake_report_browser.h"

namespace plugin {

FakeReportBrowser::FakeReportBrowser() = default;
FakeReportBrowser::~FakeReportBrowser() = default;

void FakeReportBrowser::set_parent_hwnd(HWND parent) {
  parent_ = parent;
}

void FakeReportBrowser::set_bounds(int x, int y, int width, int height) {
  x_ = x;
  y_ = y;
  w_ = width;
  h_ = height;
}

bool FakeReportBrowser::navigate(std::string_view report_dir) {
  if (!available_) {
    status_ = "fake-unavailable";
    return false;
  }
  if (!is_path_allowed(report_dir)) {
    status_ = "path-not-allowed";
    return false;
  }
  const std::string url = file_url_for_report_dir(report_dir);
  if (url.empty()) {
    status_ = "missing-index.html";
    return false;
  }
  last_dir_ = std::string(report_dir);
  open_ = true;
  ++navigate_count_;
  status_ = "navigated";
  return true;
}

bool FakeReportBrowser::post_json(std::string_view json) {
  if (!open_ || !available_) {
    return false;
  }
  posts_.emplace_back(json);
  return true;
}

void FakeReportBrowser::close() {
  open_ = false;
  last_dir_.clear();
  status_ = "closed";
}

bool FakeReportBrowser::is_available() const {
  return available_;
}

std::string FakeReportBrowser::status_text() const {
  return status_;
}

}  // namespace plugin
